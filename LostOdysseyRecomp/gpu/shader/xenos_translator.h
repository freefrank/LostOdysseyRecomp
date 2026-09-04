#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Translates raw Xenos shader microcode (as loaded by IM_LOAD / IM_LOAD_IMMEDIATE,
// big-endian dwords in guest memory) to HLSL for DXC (Shader Model 6).
//
// Resource binding contract shared with the draw pipeline:
//   b0 space0 : float4 c[256]     ALU constants of this stage (VS: regs 0x4000.., PS: 0x4400..)
//   b1 space0 : XeSharedConstants (bool/loop constants, half-pixel offset, viewport info)
//   t0..t95  space0 : ByteAddressBuffer  vertex fetch slots (little-endian dwords after CPU swap)
//   t0..t31  space1 : Texture2D           texture fetch slots
//   t0..t31  space2 : Texture3D
//   t0..t31  space3 : TextureCube
//   s0..s31  space0 : SamplerState
// Interpolators are always 16 float4 TEXCOORD0..15; the vertex shader writes all.
namespace xenos
{
    struct TranslatedShader
    {
        bool isPixelShader = false;
        std::string hlsl;
        std::string errors;                 // non-fatal translation notes
        uint32_t vertexFetchSlotsUsed = 0;  // bitmask over the 96 slots is too wide; low 32 slots
        uint64_t vertexFetchSlotMask[2] = {};
        uint32_t textureSlotMask = 0;       // 32 texture fetch slots
        uint8_t textureDimension[32] = {};  // TextureDimension per slot
        bool writesDepth = false;
        uint32_t colorTargetsWritten = 0;   // bitmask oC0..oC3
        bool usesPointSize = false;
    };

    // dwords: microcode in *host* byte order (already swapped from big-endian).
    TranslatedShader TranslateShader(const uint32_t* dwords, uint32_t dwordCount, bool isPixelShader);

    // HLSL prelude (constant buffers, fetch helpers) prepended to every shader.
    const char* GetShaderCommonHlsl();
}
