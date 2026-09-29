#pragma once
#include "legacy_draw_state.h"
#include <cstddef>
#include <type_traits>

namespace gpu::renderer
{
struct DrawScalarField { uint16_t index; size_t offset; };
// A compatibility map, not a backend register reader. Native and legacy state
// writes update the same named scalar fields as they execute on the GPU owner.
#define LO_DRAW_FIELD(reg, group, type, field) DrawScalarField{reg, offsetof(DrawState, group) + offsetof(type, field)}
inline constexpr DrawScalarField kDrawScalarFields[] = {
    LO_DRAW_FIELD(0x2000, targets, TargetState, surfaceInfo),
    {0x2001, offsetof(DrawState, targets) + offsetof(TargetState, colorInfo)},
    {0x2003, offsetof(DrawState, targets) + offsetof(TargetState, colorInfo) + 4},
    {0x2004, offsetof(DrawState, targets) + offsetof(TargetState, colorInfo) + 8},
    {0x2005, offsetof(DrawState, targets) + offsetof(TargetState, colorInfo) + 12},
    LO_DRAW_FIELD(0x2002, targets, TargetState, depthInfo),
    LO_DRAW_FIELD(0x2208, pipeline, PipelineState, modeControl),
    LO_DRAW_FIELD(0x2200, pipeline, PipelineState, depthControl),
    LO_DRAW_FIELD(0x2201, pipeline, PipelineState, blendControl),
    LO_DRAW_FIELD(0x2202, pipeline, PipelineState, colorControl),
    LO_DRAW_FIELD(0x2104, pipeline, PipelineState, colorMask),
    LO_DRAW_FIELD(0x2205, pipeline, PipelineState, modeCull),
    LO_DRAW_FIELD(0x210D, pipeline, PipelineState, stencilRefMask),
    LO_DRAW_FIELD(0x210C, pipeline, PipelineState, stencilRefMaskBack),
    LO_DRAW_FIELD(0x2204, pipeline, PipelineState, clipControl),
    LO_DRAW_FIELD(0x2280, pipeline, PipelineState, pointSize),
    LO_DRAW_FIELD(0x2281, pipeline, PipelineState, pointMinMax),
    LO_DRAW_FIELD(0x210E, pipeline, PipelineState, alphaRef),
    {0x2380, offsetof(DrawState, pipeline) + offsetof(PipelineState, polygonOffset)},
    {0x2381, offsetof(DrawState, pipeline) + offsetof(PipelineState, polygonOffset) + 4},
    {0x2382, offsetof(DrawState, pipeline) + offsetof(PipelineState, polygonOffset) + 8},
    {0x2383, offsetof(DrawState, pipeline) + offsetof(PipelineState, polygonOffset) + 12},
    LO_DRAW_FIELD(0x2080, viewport, ViewportState, windowOffset),
    LO_DRAW_FIELD(0x2081, viewport, ViewportState, scissorTL),
    LO_DRAW_FIELD(0x2082, viewport, ViewportState, scissorBR),
    LO_DRAW_FIELD(0x2206, viewport, ViewportState, transformControl),
    LO_DRAW_FIELD(0x2302, viewport, ViewportState, vertexControl),
    {0x210F, offsetof(DrawState, viewport) + offsetof(ViewportState, scaleOffset)},
    {0x2110, offsetof(DrawState, viewport) + offsetof(ViewportState, scaleOffset) + 4},
    {0x2111, offsetof(DrawState, viewport) + offsetof(ViewportState, scaleOffset) + 8},
    {0x2112, offsetof(DrawState, viewport) + offsetof(ViewportState, scaleOffset) + 12},
    {0x2113, offsetof(DrawState, viewport) + offsetof(ViewportState, scaleOffset) + 16},
    {0x2114, offsetof(DrawState, viewport) + offsetof(ViewportState, scaleOffset) + 20},
    LO_DRAW_FIELD(0x2318, resolve, ResolveState, control),
    LO_DRAW_FIELD(0x2319, resolve, ResolveState, destinationBase),
    LO_DRAW_FIELD(0x231A, resolve, ResolveState, destinationPitch),
    LO_DRAW_FIELD(0x231B, resolve, ResolveState, destinationInfo),
    LO_DRAW_FIELD(0x200C, resolve, ResolveState, colorClear),
    LO_DRAW_FIELD(0x200B, resolve, ResolveState, depthClear),
    {0x2102, offsetof(DrawState, baseVertex)}
};
#undef LO_DRAW_FIELD
static_assert(std::is_standard_layout_v<DrawState>);
inline constexpr auto kDrawScalarOffsets = [] {
    std::array<uint16_t, 0x400> result{};
    result.fill(0xffff);
    for (auto field : kDrawScalarFields) result[field.index - 0x2000] = uint16_t(field.offset);
    return result;
}();
// CP-worker-owned. External MMIO callers signal invalidation on the CP's
// atomic flag; they never mutate this object or its borrowed bank views.
class ExecutionDrawState
{
public:
    void Reset() { initialized_ = false; }
    void Observe(uint32_t index, uint32_t value)
    {
        if (!initialized_ || index < 0x2000 || index >= 0x2400) return;
        const auto offset = kDrawScalarOffsets[index - 0x2000];
        if (offset != 0xffff) std::memcpy(reinterpret_cast<uint8_t*>(&state_) + offset, &value, 4);
    }
    DrawState ForDraw(const DrawInfo& draw, DrawWords registers,
        ShaderBinding vertexShader, ShaderBinding pixelShader)
    {
        if (!initialized_) {
            state_ = CaptureLegacyDrawState({}, registers, {}, {});
            // Seed scalar storage from native values, not a transient fallback.
            const auto native = registers.NativeValues();
            for (auto field : kDrawScalarFields) {
                const auto value = native.Read(field.index);
                std::memcpy(reinterpret_cast<uint8_t*>(&state_) + field.offset, &value, 4);
            }
            initialized_ = true;
        }
        DrawState result = state_;
        result.draw = draw;
        result.vertexShader = vertexShader;
        result.pixelShader = pixelShader;
        // Legacy supports plain, unobserved MMIO stores when its native value
        // is zero. Keep that narrow fallback without reparsing nonzero state.
        // Do not store a fallback in state_: subsequent direct zero writes must
        // be observed too. This is also why these groups stay revision-untracked.
        for (auto field : kDrawScalarFields) {
            auto* destination = reinterpret_cast<uint8_t*>(&result) + field.offset;
            uint32_t value;
            std::memcpy(&value, destination, 4);
            if (value == 0) {
                value = registers.Read(field.index);
                std::memcpy(destination, &value, 4);
            }
        }
        return result;
    }
private:
    bool initialized_ = false;
    DrawState state_;
};
}
