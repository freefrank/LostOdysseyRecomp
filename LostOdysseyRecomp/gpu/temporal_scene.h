#pragma once
#include "temporal_math.h"
#include <bit>
#include <cstdint>

namespace gpu::temporal
{
// Verified position-transform slots from scene captures, including all three
// tire layers in Map3 frame 24389 and battle terrain/objects/skinned layers in
// frames 2871 and 26786, Map16 ground/material passes in frame 18420, and
// static/skinned scene paths in captures 17624-17626 and 21480-21482.
// A slot alone never
// authorizes jitter: renderer also checks viewport/VTE, ordered scene allocation,
// and exact unmodified camera bits. Fullscreen/postprocess/UI shaders are absent.
inline int PositionVPSlot(uint64_t shader) {
    switch(shader) {
    case 0x702c643defe73320ull:case 0x97f07e5d73418e64ull:case 0x99c2b4b0960a9ccdull:return 0;
    case 0xb030ab4e17a20783ull:case 0xf1b330b3ceea9a3bull:case 0xf7fd88506d704a3dull:return 4;
    case 0x03184cec350bc14eull:case 0x3621e6e696f914c5ull:case 0x4053f2a21dbb92ddull:
    case 0xa27a7234977e0d4aull:case 0xbfe5f796efa9ae95ull:case 0xc13cdd857c57fed9ull:case 0xf4577672c6ee5dd9ull:
    case 0xff9da3984ce8d094ull:case 0x8b5577db3ced3327ull:case 0x400df7c5a60819f5ull:case 0x08dcef32bd434f8cull:
    case 0xfcbb75d0feb3fcb9ull:
    case 0xf63bf6e0d52519a8ull:case 0xc1e8406a5c2ab764ull:case 0x8fe60c14bb586399ull:
    case 0x9f2ddb46a977510bull:case 0x6cbe49f383e54f69ull:
    // Live 4K telemetry + microcode: c7-c10 feed oPos; same camera/depth allocation.
    case 0x0b786a899598ce18ull:
    // Capture 2813-2815: exact c7 camera and position output, 48 draws/frame.
    case 0xe8ec18f1d3eac4dfull:case 0x1ea46291cb1c7298ull:
    case 0x7d403bdef896a97full:case 0x45ed0948b6b701a7ull:return 7;
    case 0x1da1ddc75da8e994ull:case 0x22557143e0f243ddull:case 0x4c87bb5b986defc8ull:case 0xa6c8c11c6dd07144ull:
    case 0xe8c0d438c690c784ull:case 0x576d669b2ad3c898ull:
    case 0x188061ace0615678ull:case 0xdc7f83af67c53ba1ull:case 0x68014a17a2a9a4bdull:return 8;
    case 0x31bde3e2770db187ull:case 0x7e8492365edcf556ull:return 230;
    case 0x118a37c0d32c0477ull:case 0x3148f81d65d3b5f4ull:case 0xb7557072899a63a1ull:case 0xc84ca5209e98e743ull:
    case 0x0eb223d33f8e8e0cull:case 0x1e9017d2b296f480ull:
    case 0x87a76ceaf1eaec11ull:case 0x81bc335604d04e8bull:return 233;
    default:return -1;
    }
}
// Ordered observations from one renderer frame. This associates selected draw
// constants with their actual depth allocation and a later pre-UI scene copy;
// it does not discover shaders, object motion, jitter or camera cuts.
struct SceneAnchor
{
    std::array<uint32_t, 16> vpBits{};
    Viewport viewport{};
    uint64_t depthAllocation = 0;
};
struct SceneResolve
{
    uint64_t frame = 0, ordinal = 0;
    uint32_t address = 0, format = 0, width = 0, height = 0;
    bool fullExtent = false;
};
class SceneObservation
{
public:
    enum class Rejection { None, InvalidCamera, AmbiguousCamera, PartialDepth, RepeatedDepth, InvalidColor, RepeatedColor };
    void Reset(uint64_t frame) { *this = SceneObservation(); frame_ = frame; }
    void ObserveCamera(const SceneAnchor& anchor)
    {
        ++draws_;
        if (draws_ == 1)
        {
            anchor_ = anchor;
            Matrix vp{};
            for (size_t i = 0; i < vp.size(); ++i) vp[i] = std::bit_cast<float>(anchor.vpBits[i]);
            const auto& v = anchor.viewport;
            if (!anchor.depthAllocation || v.x != 0 || v.y != 0 ||
                !Camera::Create(vp, v)) Reject(Rejection::InvalidCamera);
        }
        else if (anchor.vpBits != anchor_.vpBits || anchor.depthAllocation != anchor_.depthAllocation ||
            !SameViewport(anchor.viewport, anchor_.viewport)) Reject(Rejection::AmbiguousCamera);
    }
    void ObserveDepth(uint64_t sourceAllocation, const SceneResolve& resolve)
    {
        // Shadow/other view resolves cannot substitute for the selected allocation.
        if (!draws_ || sourceAllocation != anchor_.depthAllocation) return;
        if (depth_.ordinal) { Reject(Rejection::RepeatedDepth); return; }
        if (resolve.frame != frame_ || !resolve.ordinal || !resolve.fullExtent ||
            resolve.width != anchor_.viewport.width || resolve.height != anchor_.viewport.height)
        { Reject(Rejection::PartialDepth); return; }
        depth_ = resolve;
    }
    bool ObserveColor(const SceneResolve& resolve)
    {
        ++copies_;
        if (copies_ > 1) Reject(Rejection::RepeatedColor);
        if (!depth_.ordinal || resolve.frame != frame_ || resolve.ordinal <= depth_.ordinal ||
            !resolve.fullExtent || resolve.width != depth_.width || resolve.height != depth_.height)
            Reject(Rejection::InvalidColor);
        color_ = resolve;
        return Ready();
    }
    bool Ready() const { return rejection_ == Rejection::None && draws_ && depth_.ordinal && color_.ordinal; }
    uint64_t Frame() const { return frame_; }
    uint32_t Draws() const { return draws_; }
    uint32_t Copies() const { return copies_; }
    Rejection Reason() const { return rejection_; }
    const SceneAnchor& Anchor() const { return anchor_; }
    const SceneResolve& Depth() const { return depth_; }
    const SceneResolve& Color() const { return color_; }
private:
    static bool SameViewport(const Viewport& a, const Viewport& b)
    {
        return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height &&
            a.ndcYSign == b.ndcYSign && a.halfPixelNdcX == b.halfPixelNdcX && a.halfPixelNdcY == b.halfPixelNdcY;
    }
    void Reject(Rejection why) { if (rejection_ == Rejection::None) rejection_ = why; }
    uint64_t frame_ = 0;
    uint32_t draws_ = 0, copies_ = 0;
    Rejection rejection_ = Rejection::None;
    SceneAnchor anchor_{};
    SceneResolve depth_{}, color_{};
};
}
