#pragma once
#include "draw_state.h"

namespace gpu::renderer
{
    // The only register-to-draw mapping. No allocations or full snapshots:
    // scalar groups are copied by value, large banks are borrowed until Draw
    // returns. This adapter must execute on the same owner as register writes.
    inline DrawState CaptureLegacyDrawState(const DrawInfo& draw, DrawWords registers,
        ShaderBinding vertexShader, ShaderBinding pixelShader)
    {
        DrawState state;
        state.draw = draw;
        state.vertexShader = vertexShader;
        state.pixelShader = pixelShader;
        auto r = [&](uint32_t index) { return registers.Read(index); };
        auto f = [&](uint32_t index) { return registers.ReadFloat(index); };
        state.targets.surfaceInfo = r(0x2000);
        state.targets.colorInfo = {r(0x2001), r(0x2003), r(0x2004), r(0x2005)};
        state.targets.depthInfo = r(0x2002);
        state.pipeline.modeControl = r(0x2208);
        state.pipeline.depthControl = r(0x2200);
        state.pipeline.blendControl = r(0x2201);
        state.pipeline.colorControl = r(0x2202);
        state.pipeline.colorMask = r(0x2104);
        state.pipeline.modeCull = r(0x2205);
        state.pipeline.stencilRefMask = r(0x210D);
        state.pipeline.stencilRefMaskBack = r(0x210C);
        state.pipeline.clipControl = r(0x2204);
        state.pipeline.pointSize = r(0x2280);
        state.pipeline.pointMinMax = r(0x2281);
        state.pipeline.alphaRef = f(0x210E);
        state.pipeline.polygonOffset = {f(0x2380), f(0x2381), f(0x2382), f(0x2383)};
        state.viewport.windowOffset = r(0x2080);
        state.viewport.scissorTL = r(0x2081);
        state.viewport.scissorBR = r(0x2082);
        state.viewport.transformControl = r(0x2206);
        state.viewport.vertexControl = r(0x2302);
        for (uint32_t i = 0; i < 6; ++i) state.viewport.scaleOffset[i] = f(0x210F + i);
        state.resolve.control = r(0x2318);
        state.resolve.destinationBase = r(0x2319);
        state.resolve.destinationPitch = r(0x231A);
        state.resolve.destinationInfo = r(0x231B);
        state.resolve.colorClear = r(0x200C);
        state.resolve.depthClear = r(0x200B);
        state.baseVertex = int32_t(r(0x2102));
        state.vertexConstants = registers.Subspan(0x4000, 1024);
        state.pixelConstants = registers.Subspan(0x4400, 1024);
        state.fetchConstants = registers.Subspan(0x4800, 192);
        state.boolConstants = registers.Subspan(0x4900, 8);
        state.loopConstants = registers.Subspan(0x4908, 32);
        state.legacyTraceRegisters = registers;
        return state;
    }
}
