#pragma once
#include "temporal_math.h"
#include "motion_frame.h"
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string_view>

namespace gpu::temporal {
// SR-only fallback. Camera motion is an estimate for unknown moving geometry,
// never evidence that an object is stationary. Legacy TAA remains unchanged.
inline bool SrHybridMotionEnabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("LO_SR_HYBRID_MV");
        return !value || std::string_view(value) != "0";
    }();
    return enabled;
}
struct SrHybridConstants {
    float inverseCurrent[16]{}, previous[16]{};
    float currentNdc[4]{}, previousRaster[4]{}, jitterExtent[4]{};
    uint32_t options[4]{}; // previous camera usable, geometry usable, reserved
};
static_assert(sizeof(SrHybridConstants) == 192);
inline bool SrHybridGeometryMatches(const MotionFrameView& motion,
    uint64_t frame, uint64_t epoch, uint64_t allocation, uint32_t width, uint32_t height) {
    return motion.ready && motion.frame == frame && motion.epoch == epoch &&
        motion.depthAllocation == allocation && motion.width == width && motion.height == height &&
        motion.velocity && motion.reactive;
}
inline std::optional<SrHybridConstants> MakeSrHybridConstants(const Camera& current,
    const Camera* previous, uint32_t width, uint32_t height, double jx, double jy, bool reset) {
    if (!width || !height || !std::isfinite(jx) || !std::isfinite(jy) ||
        std::abs(jx) > 16 || std::abs(jy) > 16) return {};
    const auto full = [width,height](const Viewport& v) {
        return v.x == 0 && v.y == 0 && v.width == width && v.height == height;
    };
    if (!full(current.Raster()) || (previous && !full(previous->Raster()))) return {};
    const auto& c = current.Raster();
    const auto& p = previous ? previous->Raster() : c;
    const auto& previousVP = previous ? previous->VP() : current.VP();
    SrHybridConstants result{};
    for (unsigned i = 0; i < 16; ++i) {
        result.inverseCurrent[i] = float(current.InverseVP()[i]);
        result.previous[i] = float(previousVP[i]);
        if (!std::isfinite(result.inverseCurrent[i]) || !std::isfinite(result.previous[i])) return {};
    }
    result.currentNdc[0] = float(2 / c.width);
    result.currentNdc[1] = float(-2 / (c.height * c.ndcYSign));
    result.currentNdc[2] = float(-1 - c.halfPixelNdcX);
    result.currentNdc[3] = float((1 - c.halfPixelNdcY) / c.ndcYSign);
    result.previousRaster[0] = float(p.width * .5);
    result.previousRaster[1] = float(-p.height * .5 * p.ndcYSign);
    result.previousRaster[2] = float((1 + p.halfPixelNdcX) * p.width * .5);
    result.previousRaster[3] = float((1 - p.halfPixelNdcY) * p.height * .5);
    result.jitterExtent[0] = float(jx); result.jitterExtent[1] = float(jy);
    result.jitterExtent[2] = float(width); result.jitterExtent[3] = float(height);
    result.options[0] = previous && !reset ? 1u : 0u;
    for (float f : result.currentNdc) if (!std::isfinite(f)) return {};
    for (float f : result.previousRaster) if (!std::isfinite(f)) return {};
    return result;
}
inline constexpr const char* kSrHybridMotionShader = R"HLSL(
Texture2D<float> sceneDepth : register(t0);
Texture2D<float2> geometricMotion : register(t1);
Texture2D<float> geometricInvalidity : register(t2);
#ifdef __spirv__
[[vk::binding(3,0)]]
#endif
cbuffer Parameters : register(b0) {
    row_major float4x4 inverseCurrent;
    row_major float4x4 previous;
    float4 currentNdc, previousRaster, jitterExtent;
    uint4 options;
};
float4 vertex(uint id : SV_VertexID) : SV_Position {
    float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2,-2) + float2(-1,1), 0, 1);
}
struct Output { float2 motion : SV_Target0; float uncertainty : SV_Target1; };
bool validW(float4 p) {
    float m = max(max(abs(p.x),abs(p.y)),max(abs(p.z),abs(p.w)));
    return all(isfinite(p)) && m > 0 && p.w > 1e-6 * m;
}
Output pixel(float4 position : SV_Position) {
    Output o; o.motion = 0; o.uncertainty = 1;
    // Reset deliberately does not reuse either previous geometry or camera.
    if (options.x == 0) return o;
    int3 at = int3(position.xy,0);
    float depth = sceneDepth.Load(at);
    if (!isfinite(depth) || depth <= 0 || depth > 1) return o;
    if (options.y != 0) {
        float invalidity = geometricInvalidity.Load(at);
        float2 mv = geometricMotion.Load(at);
        if (isfinite(invalidity) && invalidity < .5 && all(isfinite(mv)) && all(abs(mv) <= 65504)) {
            o.motion = mv; o.uncertainty = 0; return o;
        }
    }
    // Depth was rasterized with current jitter. Remove it once; previous jitter
    // does not belong in a geometric motion vector.
    float2 currentPixel = position.xy - jitterExtent.xy;
    float2 ndc = currentPixel * currentNdc.xy + currentNdc.zw;
    float4 world = mul(float4(ndc,1-depth,1),inverseCurrent);
    if (!validW(world)) return o;
    float4 clip = mul(world / world.w,previous);
    if (!validW(clip)) return o;
    float previousDepth = 1 - clip.z / clip.w;
    float2 previousPixel = clip.xy / clip.w * previousRaster.xy + previousRaster.zw;
    float2 mv = previousPixel - currentPixel;
    if (!isfinite(previousDepth) || previousDepth <= 0 || previousDepth > 1 ||
        any(previousPixel < .5) || any(previousPixel > jitterExtent.zw - .5) ||
        !all(isfinite(mv)) || any(abs(mv) > 65504)) return o;
    o.motion = mv;
    // A reconstructed camera vector is not a validated object vector. The SDK
    // adapter uses this as a current-frame bias, not as a guaranteed rejection.
    return o;
}
)HLSL";
} // namespace gpu::temporal
