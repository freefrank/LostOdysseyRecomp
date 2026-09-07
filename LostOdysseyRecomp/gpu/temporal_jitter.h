#pragma once
#include "temporal_scene.h"

namespace gpu::temporal
{
struct JitterSample
{
    uint32_t phase = 0;
    double pixelX = 0, pixelY = 0;
    float ndcX = 0, ndcY = 0;
};

inline JitterSample FrameJitter(uint64_t frame, double width, double height)
{
    if (!std::isfinite(width) || !std::isfinite(height) || width < 1 || height < 1) return {};
    const auto halton = [](uint32_t n, uint32_t base) {
        double value = 0, fraction = 1;
        while (n) { fraction /= base; value += fraction * (n % base); n /= base; }
        return value - .5;
    };
    const uint32_t phase = uint32_t(frame % 32 + 1);
    const double x = halton(phase, 2), y = halton(phase, 3);
    // Guest NDC Y is up; the supported host viewport is Y down.
    return {phase, x, y, float(2 * float(x) / width), float(-2 * float(y) / height)};
}

// Classify the guest camera before the independent, host-only lighting depth
// bias is added to ndcOffset.z. A biased layer still uses the same XY camera.
inline bool IsJitterViewport(const Viewport& viewport, uint32_t vte,
    const float* ndcScale, const float* guestNdcOffset)
{
    return viewport.x == 0 && viewport.y == 0 && viewport.width == 1280 && viewport.height == 720 &&
        vte == 0x43f && ndcScale[0] == 1 && ndcScale[1] == 1 && ndcScale[2] == -1 &&
        guestNdcOffset[0] == 0 && guestNdcOffset[1] == 0 && guestNdcOffset[2] == 1;
}

enum class JitterRejection
{
    None, Disabled, UnknownShader, IncompatibleViewport, MissingCamera,
    CameraMismatch, DepthMismatch, InvalidExtent, InvalidConstants, ShadowDepthMismatch
};

inline bool IsSceneDepthSample(uint64_t frame, const SceneResolve* sceneDepth, const SceneResolve* sampledDepth)
{
    return sceneDepth && sampledDepth && sceneDepth->frame == frame && sampledDepth->frame == frame &&
        sceneDepth->ordinal && sceneDepth->ordinal == sampledDepth->ordinal &&
        sceneDepth->address == sampledDepth->address && sceneDepth->format == sampledDepth->format &&
        sceneDepth->fullExtent && sampledDepth->fullExtent &&
        sceneDepth->width == sampledDepth->width && sceneDepth->height == sampledDepth->height;
}

inline bool IsFullSceneDepthFetch(uint32_t fetchSize, uint32_t fetchControl,
    uint32_t resolvedGuestWidth, uint32_t resolvedGuestHeight)
{
    // Match GetTexture's 2D logical size decode. A smaller fetch creates a
    // cropped view with a different normalized UV domain despite sharing the
    // parent resolve's address/frame/ordinal. Other dimensions have no verified
    // screen-depth reconstruction contract for this shadow shader pair.
    return ((fetchControl >> 9) & 3) == 1 &&
        (fetchSize & 0x1fff) + 1 == resolvedGuestWidth &&
        ((fetchSize >> 13) & 0x1fff) + 1 == resolvedGuestHeight;
}

struct DrawJitter
{
    int slot = -1;
    JitterSample sample{};
    JitterRejection rejection = JitterRejection::Disabled;
    bool applied = false, shadowCompensated = false;
};

// Modify temporary upload copies only. The unmodified guest VP remains the
// camera identity, and the PS adjustment is authorized by an exact shader pair.
inline DrawJitter ApplyDrawJitter(uint64_t vs, uint64_t ps, uint64_t frame,
    bool enabled, bool compatibleViewport, const SceneAnchor* anchor,
    uint64_t depthAllocation, const Viewport& rasterViewport,
    uint32_t* vsConstants, uint32_t* psConstants,
    const SceneResolve* sceneDepth = nullptr, const SceneResolve* sampledDepth = nullptr)
{
    DrawJitter result;
    result.slot = PositionVPSlot(vs);
    if (!enabled) return result;
    const auto reject = [&](JitterRejection why) { result.rejection = why; return result; };
    if (result.slot < 0) return reject(JitterRejection::UnknownShader);
    if (!compatibleViewport) return reject(JitterRejection::IncompatibleViewport);
    if (!anchor) return reject(JitterRejection::MissingCamera);
    auto* vp = vsConstants + result.slot * 4;
    if (!std::equal(anchor->vpBits.begin(), anchor->vpBits.end(), vp))
        return reject(JitterRejection::CameraMismatch);
    if (depthAllocation && anchor->depthAllocation != depthAllocation)
        return reject(JitterRejection::DepthMismatch);
    if (rasterViewport.x != anchor->viewport.x || rasterViewport.y != anchor->viewport.y ||
        rasterViewport.width != anchor->viewport.width || rasterViewport.height != anchor->viewport.height)
        return reject(JitterRejection::IncompatibleViewport);
    result.sample = FrameJitter(frame, rasterViewport.width, rasterViewport.height);
    if (!result.sample.phase) return reject(JitterRejection::InvalidExtent);
    const bool shadow = vs == 0x99c2b4b0960a9ccdull && ps == 0xd55a20d004031279ull;
    if (shadow && !IsSceneDepthSample(frame, sceneDepth, sampledDepth))
        return reject(JitterRejection::ShadowDepthMismatch);
    for (unsigned i = 0; i < 16; ++i)
        if (!std::isfinite(std::bit_cast<float>(vp[i]))) return reject(JitterRejection::InvalidConstants);
    if (shadow)
        for (unsigned i = 2 * 4; i < 5 * 4; ++i)
            if (!std::isfinite(std::bit_cast<float>(psConstants[i]))) return reject(JitterRejection::InvalidConstants);

    for (unsigned row = 0; row < 4; ++row)
    {
        auto* values = vp + row * 4;
        const float w = std::bit_cast<float>(values[3]);
        values[0] = std::bit_cast<uint32_t>(std::bit_cast<float>(values[0]) + result.sample.ndcX * w);
        values[1] = std::bit_cast<uint32_t>(std::bit_cast<float>(values[1]) + result.sample.ndcY * w);
    }
    if (shadow)
    {
        // VS 99c2 passes its jittered clip position to both oPos and o0. PS d55
        // samples scene depth at i0.xy/i0.w, then reconstructs (before swizzling):
        // linearDepth * (ndcX*c2 + ndcY*c3 + c4) + c5.
        // Keep the jittered depth lookup, but remove the offset from that ray.
        // c0/c1 (depth UV/linearization), c2/c3 and c5 stay byte-for-byte intact.
        for (unsigned component = 0; component < 4; ++component)
        {
            const float x = std::bit_cast<float>(psConstants[2 * 4 + component]);
            const float y = std::bit_cast<float>(psConstants[3 * 4 + component]);
            const float origin = std::bit_cast<float>(psConstants[4 * 4 + component]);
            psConstants[4 * 4 + component] = std::bit_cast<uint32_t>(
                origin - result.sample.ndcX * x - result.sample.ndcY * y);
        }
        result.shadowCompensated = true;
    }
    result.applied = true;
    result.rejection = JitterRejection::None;
    return result;
}
}
