// The Xenos shader prelude, included twice by common_hlsl.h: once per
// XE_CONSTANTS_FORM(pointer, ubo) choice. Text outside those marks is shared.
R"HLSL(
// ---- Xenos shader prelude (LostOdysseyRecomp) ----
#define FLT_MIN asfloat(0xff7fffff)
#define FLT_MAX asfloat(0x7f7fffff)

)HLSL" XE_POINTER_GUARD R"HLSL(
ByteAddressBuffer xeVertexArena : register(t0, space0);
struct XePushConstants
{
    uint64_t VertexShaderConstants;
    uint64_t SharedConstants;
    uint64_t PixelShaderConstants;
};
)HLSL"
R"HLSL(// Some Android drivers misread the middle 64-bit push member. Preserve
// the 24-byte CPU layout, but load each address as two 32-bit words.
struct XePushConstantWords
{
    uint2 VertexShaderConstants;
    uint2 SharedConstants;
    uint2 PixelShaderConstants;
};
[[vk::push_constant]] ConstantBuffer<XePushConstantWords> xePushWords;
uint64_t XeDeviceAddress(uint2 words)
{
    return uint64_t(words.x) | (uint64_t(words.y) << 32u);
}
XePushConstants XeLoadPushConstants()
{
    XePushConstants addresses;
    addresses.VertexShaderConstants = XeDeviceAddress(xePushWords.VertexShaderConstants);
    addresses.SharedConstants = XeDeviceAddress(xePushWords.SharedConstants);
    addresses.PixelShaderConstants = XeDeviceAddress(xePushWords.PixelShaderConstants);
    return addresses;
}
#define xePush (XeLoadPushConstants())
// The renderer keeps every upload ring, plus 4 KiB of slack, inside one 4 GiB
// window, so constant reads add their offset to the low address word only.
uint64_t XeBankAddress(uint2 words, uint offset)
{
    return uint64_t(words.x + offset) | (uint64_t(words.y) << 32u);
}
#define xeShared(offset) XeBankAddress(xePushWords.SharedConstants, offset)
)HLSL"
R"HLSL(#ifdef XE_PIXEL_SHADER
#define XE_CONSTANT_WORDS xePushWords.PixelShaderConstants
#else
#define XE_CONSTANT_WORDS xePushWords.VertexShaderConstants
#endif
#define xeNdcScale       vk::RawBufferLoad<float4>(xeShared(160))
#define xeNdcOffset      vk::RawBufferLoad<float4>(xeShared(176))
#define xeHalfPixelOffset vk::RawBufferLoad<float2>(xeShared(192))
#define xeVtxFmt         vk::RawBufferLoad<uint>(xeShared(200))
#define xeFlags          vk::RawBufferLoad<uint>(xeShared(204))
#define xeAlphaTest      vk::RawBufferLoad<float4>(xeShared(208))
#define xeColorMax       vk::RawBufferLoad<float4>(xeShared(224))
#define xeTransfer       vk::RawBufferLoad<uint4>(xeShared(240))
#else
cbuffer XeConstants : register(b0, )HLSL" XE_CONSTANTS_FORM("space0", "space5") R"HLSL()
{
    float4 c[256];
};

cbuffer XeShared : register(b1, )HLSL" XE_CONSTANTS_FORM("space0", "space5") R"HLSL()
{
    uint4 xeBools[2];
    uint4 xeLoops[8];
    float4 xeNdcScale;
    float4 xeNdcOffset;
    float2 xeHalfPixelOffset;
    uint xeVtxFmt;          // PA_CL_VTE_CNTL bits 8..10: xy already /w, z already /w, w is 1/w
    uint xeFlags;           // bit0: alpha test enable, bit5: signed format
    float4 xeAlphaTest;     // x = reference, y = compare function
    float4 xeColorMax;      // per-channel range of the bound EDRAM format
    uint4 xeTransfer;       // x = source EDRAM class, y = destination class (transfer blit)
    uint4 xeVfetchOffset[24]; // byte offset of each vertex fetch slot inside its buffer
    uint4 xeSamplerIndex[8];  // sampler palette index per texture fetch slot
    uint4 xeTextureInfo[8];   // source signs (8 bits), then fetch swizzle (12 bits)
    uint4 xeTextureSize[8];
)HLSL" XE_CONSTANTS_FORM("", "    uint2 xeVertexArenaAddress; // device address of the vertex arena\n") R"HLSL(};
#endif

// Guest draws do not use the transfer-blit words. Keep the shared layout
// unchanged for the other producers of this constant block.
#define xePointState xeTransfer

float XeClampPointSize(float size)
{
    float minimum = float(xePointState.y & 0xffffu) * 0.125;
    float maximum = float(xePointState.y >> 16) * 0.125;
    maximum = min(max(maximum, 1.0), max(asfloat(xePointState.z), 1.0));
    minimum = min(max(minimum, 1.0), maximum);
    return clamp(size, minimum, maximum);
}

float XeDefaultPointSize()
{
    // Xenos stores half width and height. Vulkan points are square; use the
    // larger dimension so neither axis is smaller than the guest requests.
    uint halfWidth = xePointState.x >> 16;
    uint halfHeight = xePointState.x & 0xffffu;
    return XeClampPointSize(float(max(halfWidth, halfHeight)) * 0.125);
}

uint XeVfetchOffset(uint slot)
{
)HLSL" XE_POINTER_GUARD R"HLSL(
    return vk::RawBufferLoad<uint>(xeShared(256 + slot * 4));
#else
    return xeVfetchOffset[slot >> 2][slot & 3];
#endif
}

#ifdef __spirv__
SamplerState xeSamplers[64] : register(s0, space4);
#else
SamplerState xeSamplers[64] : register(s0, space0);
#endif

uint XeSamplerIndex(uint slot)
{
)HLSL" XE_POINTER_GUARD R"HLSL(
    return vk::RawBufferLoad<uint>(xeShared(640 + slot * 4));
#else
    return xeSamplerIndex[slot >> 2][slot & 3];
#endif
}

SamplerState XeSampler(uint slot)
{
    return xeSamplers[XeSamplerIndex(slot)];
}

float4 XeConst(int index)
{
)HLSL" XE_POINTER_GUARD R"HLSL(
    return vk::RawBufferLoad<float4>(XeBankAddress(XE_CONSTANT_WORDS, uint(clamp(index, 0, 255)) * 16u));
#else
    return c[clamp(index, 0, 255)];
#endif
}

// Xbox 360 piecewise gamma decode, as used by Xenia's PWLGammaToLinear.
float XeGammaToLinear(float gamma)
{
    gamma = saturate(gamma);
    float scale = gamma >= (192.0 / 255.0) ? 8.0 :
        (gamma >= (96.0 / 255.0) ? 4.0 : (gamma >= (64.0 / 255.0) ? 2.0 : 1.0));
    float offset = scale == 8.0 ? -1024.0 : (scale == 4.0 ? -256.0 : (scale == 2.0 ? -64.0 : 0.0));
    float decoded = gamma * (255.0 * scale) + offset;
    return (decoded + trunc(decoded * (scale / 1024.0))) / 1023.0;
}

float4 XeDecodeTexture(float4 value, uint info)
{
#ifndef __spirv__
    // SPIR-V gets the R/B swap of a resolved texture from its image view.
    if (info & (1u << 20)) value = value.bgra;
#endif
    // Signed float formats already arrive signed from the host resource.
    // Gamma and unsigned-bias operate on source channels, before swizzling.
    [unroll] for (uint i = 0; i < 4; ++i)
    {
        uint sign = (info >> (i * 2)) & 3u;
        if (sign == 3u) value[i] = XeGammaToLinear(value[i]);
        else if (sign == 2u) value[i] = value[i] * 2.0 - 1.0;
    }
    float4 result;
    [unroll] for (uint j = 0; j < 4; ++j)
    {
        uint component = (info >> (8u + j * 3u)) & 7u;
        result[j] = component < 4u ? value[component] : (component == 5u ? 1.0 : 0.0);
    }
    return result;
}

#ifdef __spirv__
// Specialization constant N is fetch slot N's texture-info word. The default
// 0xFFFFFFFF is never a real word (they have 21 bits) and reads the shared
// block, so one module serves the generic and the specialised pipelines.
[[vk::constant_id(0)]] const uint xeTextureInfoSpec0 = 0xFFFFFFFFu;
[[vk::constant_id(1)]] const uint xeTextureInfoSpec1 = 0xFFFFFFFFu;
[[vk::constant_id(2)]] const uint xeTextureInfoSpec2 = 0xFFFFFFFFu;
[[vk::constant_id(3)]] const uint xeTextureInfoSpec3 = 0xFFFFFFFFu;
[[vk::constant_id(4)]] const uint xeTextureInfoSpec4 = 0xFFFFFFFFu;
[[vk::constant_id(5)]] const uint xeTextureInfoSpec5 = 0xFFFFFFFFu;
[[vk::constant_id(6)]] const uint xeTextureInfoSpec6 = 0xFFFFFFFFu;
[[vk::constant_id(7)]] const uint xeTextureInfoSpec7 = 0xFFFFFFFFu;
[[vk::constant_id(8)]] const uint xeTextureInfoSpec8 = 0xFFFFFFFFu;
[[vk::constant_id(9)]] const uint xeTextureInfoSpec9 = 0xFFFFFFFFu;
[[vk::constant_id(10)]] const uint xeTextureInfoSpec10 = 0xFFFFFFFFu;
[[vk::constant_id(11)]] const uint xeTextureInfoSpec11 = 0xFFFFFFFFu;
[[vk::constant_id(12)]] const uint xeTextureInfoSpec12 = 0xFFFFFFFFu;
[[vk::constant_id(13)]] const uint xeTextureInfoSpec13 = 0xFFFFFFFFu;
[[vk::constant_id(14)]] const uint xeTextureInfoSpec14 = 0xFFFFFFFFu;
[[vk::constant_id(15)]] const uint xeTextureInfoSpec15 = 0xFFFFFFFFu;
[[vk::constant_id(16)]] const uint xeTextureInfoSpec16 = 0xFFFFFFFFu;
[[vk::constant_id(17)]] const uint xeTextureInfoSpec17 = 0xFFFFFFFFu;
[[vk::constant_id(18)]] const uint xeTextureInfoSpec18 = 0xFFFFFFFFu;
[[vk::constant_id(19)]] const uint xeTextureInfoSpec19 = 0xFFFFFFFFu;
[[vk::constant_id(20)]] const uint xeTextureInfoSpec20 = 0xFFFFFFFFu;
[[vk::constant_id(21)]] const uint xeTextureInfoSpec21 = 0xFFFFFFFFu;
[[vk::constant_id(22)]] const uint xeTextureInfoSpec22 = 0xFFFFFFFFu;
[[vk::constant_id(23)]] const uint xeTextureInfoSpec23 = 0xFFFFFFFFu;
[[vk::constant_id(24)]] const uint xeTextureInfoSpec24 = 0xFFFFFFFFu;
[[vk::constant_id(25)]] const uint xeTextureInfoSpec25 = 0xFFFFFFFFu;
[[vk::constant_id(26)]] const uint xeTextureInfoSpec26 = 0xFFFFFFFFu;
[[vk::constant_id(27)]] const uint xeTextureInfoSpec27 = 0xFFFFFFFFu;
[[vk::constant_id(28)]] const uint xeTextureInfoSpec28 = 0xFFFFFFFFu;
[[vk::constant_id(29)]] const uint xeTextureInfoSpec29 = 0xFFFFFFFFu;
[[vk::constant_id(30)]] const uint xeTextureInfoSpec30 = 0xFFFFFFFFu;
[[vk::constant_id(31)]] const uint xeTextureInfoSpec31 = 0xFFFFFFFFu;

uint XeTextureInfoSpec(uint slot)
{
    switch (slot)
    {
    case 0u: return xeTextureInfoSpec0;
    case 1u: return xeTextureInfoSpec1;
    case 2u: return xeTextureInfoSpec2;
    case 3u: return xeTextureInfoSpec3;
    case 4u: return xeTextureInfoSpec4;
    case 5u: return xeTextureInfoSpec5;
    case 6u: return xeTextureInfoSpec6;
    case 7u: return xeTextureInfoSpec7;
    case 8u: return xeTextureInfoSpec8;
    case 9u: return xeTextureInfoSpec9;
    case 10u: return xeTextureInfoSpec10;
    case 11u: return xeTextureInfoSpec11;
    case 12u: return xeTextureInfoSpec12;
    case 13u: return xeTextureInfoSpec13;
    case 14u: return xeTextureInfoSpec14;
    case 15u: return xeTextureInfoSpec15;
    case 16u: return xeTextureInfoSpec16;
    case 17u: return xeTextureInfoSpec17;
    case 18u: return xeTextureInfoSpec18;
    case 19u: return xeTextureInfoSpec19;
    case 20u: return xeTextureInfoSpec20;
    case 21u: return xeTextureInfoSpec21;
    case 22u: return xeTextureInfoSpec22;
    case 23u: return xeTextureInfoSpec23;
    case 24u: return xeTextureInfoSpec24;
    case 25u: return xeTextureInfoSpec25;
    case 26u: return xeTextureInfoSpec26;
    case 27u: return xeTextureInfoSpec27;
    case 28u: return xeTextureInfoSpec28;
    case 29u: return xeTextureInfoSpec29;
    case 30u: return xeTextureInfoSpec30;
    case 31u: return xeTextureInfoSpec31;
    }
    return 0xFFFFFFFFu;
}
#endif

float4 XeTextureResult(float4 value, uint slot)
{
    )HLSL" XE_POINTER_GUARD R"HLSL(
    uint info = vk::RawBufferLoad<uint>(xeShared(768 + slot * 4));
#else
    uint info = xeTextureInfo[slot >> 2][slot & 3];
#endif
#ifdef __spirv__
    const uint specialised = XeTextureInfoSpec(slot);
    if (specialised != 0xFFFFFFFFu) info = specialised;
#endif
    return XeDecodeTexture(value, info);
}

bool XeBool(uint index)
{
    )HLSL" XE_POINTER_GUARD R"HLSL(
    return ((vk::RawBufferLoad<uint>(xeShared(((index >> 5) & 7) * 4)) >> (index & 31)) & 1u) != 0u;
#else
    return ((xeBools[(index >> 7) & 1][(index >> 5) & 3] >> (index & 31)) & 1u) != 0u;
#endif
}

uint XeLoopConst(uint id)
{
    )HLSL" XE_POINTER_GUARD R"HLSL(
    return vk::RawBufferLoad<uint>(xeShared(32 + (id & 31) * 4));
#else
    return xeLoops[(id >> 2) & 7][id & 3];
#endif
}

)HLSL"
// Desktop Vulkan and Metal take the same device-address vertex fetch as
// Android, so one SPIR-V pack serves all three.
R"HLSL(
#ifdef __spirv__
// Android GPUs may cap storage-buffer descriptors at 128 MiB. Keep the
// existing arena and recycling policy, but fetch through its device address.
// The shared constant block appends this address at byte 1024 on this variant.
struct XeVertexDeviceBuffer
{
    uint unused;
    // Branch-free so the arena address stays a uniform load the driver can
    // hoist out of the per-vertex code. The arena is exactly 1 GiB, so the
    // clamped address is always inside it; out-of-range fetches still read 0.
    uint64_t Address(uint a, uint limit) {
        return )HLSL" XE_CONSTANTS_FORM("vk::RawBufferLoad<uint64_t>(xeShared(1024), 8)",
            "(uint64_t(xeVertexArenaAddress.x) | (uint64_t(xeVertexArenaAddress.y) << 32u))") R"HLSL( + uint64_t(min(a, limit));
    }
    uint Load(uint a) {
        uint v = vk::RawBufferLoad<uint>(Address(a, 1073741824u - 4u), 4);
        return a > 1073741824u - 4u ? 0u : v;
    }
    uint2 Load2(uint a) {
        uint2 v = vk::RawBufferLoad<uint2>(Address(a, 1073741824u - 8u), 4);
        return a > 1073741824u - 8u ? uint2(0u, 0u) : v;
    }
    uint3 Load3(uint a) {
        uint3 v = vk::RawBufferLoad<uint3>(Address(a, 1073741824u - 12u), 4);
        return a > 1073741824u - 12u ? uint3(0u, 0u, 0u) : v;
    }
    uint4 Load4(uint a) {
        uint4 v = vk::RawBufferLoad<uint4>(Address(a, 1073741824u - 16u), 4);
        return a > 1073741824u - 16u ? uint4(0u, 0u, 0u, 0u) : v;
    }
};
static const XeVertexDeviceBuffer xeVertexDeviceBuffer = (XeVertexDeviceBuffer)0;
#define xeVertexArena xeVertexDeviceBuffer
#define ByteAddressBuffer XeVertexDeviceBuffer
#endif
)HLSL"
R"HLSL(// ---- vertex fetch ----
// Data is little-endian after the CPU applied the fetch constant's endian swap.
float XeNorm(uint v, uint bits, bool sgn, bool nrm)
{
    if (sgn)
    {
        int sv = int(v << (32u - bits)) >> (32u - bits);
        return nrm ? max(float(sv) / float((1u << (bits - 1u)) - 1u), -1.0) : float(sv);
    }
    return nrm ? float(v) / float((1u << bits) - 1u) : float(v);
}

float4 XeVF_8_8_8_8(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint v = b.Load(a);
    return float4(XeNorm(v & 0xFF, 8, sgn, nrm), XeNorm((v >> 8) & 0xFF, 8, sgn, nrm),
                  XeNorm((v >> 16) & 0xFF, 8, sgn, nrm), XeNorm(v >> 24, 8, sgn, nrm));
}

float4 XeVF_2_10_10_10(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint v = b.Load(a);
    return float4(XeNorm(v & 0x3FF, 10, sgn, nrm), XeNorm((v >> 10) & 0x3FF, 10, sgn, nrm),
                  XeNorm((v >> 20) & 0x3FF, 10, sgn, nrm), XeNorm(v >> 30, 2, sgn, nrm));
}

float4 XeVF_10_11_11(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint v = b.Load(a);
    return float4(XeNorm(v & 0x7FF, 11, sgn, nrm), XeNorm((v >> 11) & 0x7FF, 11, sgn, nrm),
                  XeNorm(v >> 22, 10, sgn, nrm), 0.0);
}

float4 XeVF_11_11_10(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint v = b.Load(a);
    return float4(XeNorm(v & 0x3FF, 10, sgn, nrm), XeNorm((v >> 10) & 0x7FF, 11, sgn, nrm),
                  XeNorm(v >> 21, 11, sgn, nrm), 0.0);
}

float4 XeVF_16_16(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint v = b.Load(a);
    return float4(XeNorm(v & 0xFFFF, 16, sgn, nrm), XeNorm(v >> 16, 16, sgn, nrm), 0.0, 0.0);
}

float4 XeVF_16_16_16_16(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint2 v = b.Load2(a);
    return float4(XeNorm(v.x & 0xFFFF, 16, sgn, nrm), XeNorm(v.x >> 16, 16, sgn, nrm),
                  XeNorm(v.y & 0xFFFF, 16, sgn, nrm), XeNorm(v.y >> 16, 16, sgn, nrm));
}

float4 XeVF_16_16_FLOAT(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint v = b.Load(a);
    return float4(f16tof32(v & 0xFFFF), f16tof32(v >> 16), 0.0, 0.0);
}

float4 XeVF_16_16_16_16_FLOAT(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint2 v = b.Load2(a);
    return float4(f16tof32(v.x & 0xFFFF), f16tof32(v.x >> 16), f16tof32(v.y & 0xFFFF), f16tof32(v.y >> 16));
}

float4 XeVF_32(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint v = b.Load(a);
    return float4(sgn ? float(int(v)) : float(v), 0.0, 0.0, 0.0);
}

float4 XeVF_32_32(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint2 v = b.Load2(a);
    return float4(sgn ? float2(int2(v)) : float2(v), 0.0, 0.0);
}

float4 XeVF_32_32_32_32(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint4 v = b.Load4(a);
    return sgn ? float4(int4(v)) : float4(v);
}

float4 XeVF_32_FLOAT(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    return float4(asfloat(b.Load(a)), 0.0, 0.0, 0.0);
}

float4 XeVF_32_32_FLOAT(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    return float4(asfloat(b.Load2(a)), 0.0, 0.0);
}

float4 XeVF_32_32_32_FLOAT(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    return float4(asfloat(b.Load3(a)), 0.0);
}

float4 XeVF_32_32_32_32_FLOAT(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    return asfloat(b.Load4(a));
}

)HLSL"
R"HLSL(
#ifdef __spirv__
#undef ByteAddressBuffer
#endif
)HLSL"
R"HLSL(// ---- texture fetch ----
float2 XeTextureDimensions(Texture2D<float4> t, uint slot)
{
    )HLSL" XE_POINTER_GUARD R"HLSL(
    uint packed = vk::RawBufferLoad<uint>(xeShared(896 + slot * 4));
#else
    uint packed = xeTextureSize[slot >> 2][slot & 3];
#endif
    if (packed != 0) return float2(packed & 65535u, packed >> 16);
    uint2 dims;
    t.GetDimensions(dims.x, dims.y);
    return float2(dims);
}
float4 XeTex2D(Texture2D<float4> t, SamplerState s, float2 uv, float2 offset, uint slot, bool denormalized)
{
    float2 dims = XeTextureDimensions(t, slot);
    return XE_SAMPLE(t, s, (denormalized ? uv / dims : uv) + offset / dims);
}

float4 XeTex3D(Texture3D<float4> t, SamplerState s, float3 uvw)
{
    return XE_SAMPLE(t, s, uvw);
}

float4 XeTex2DLevelZero(Texture2D<float4> t, SamplerState s, float2 uv, float2 offset, uint slot, bool denormalized)
{
    float2 dims = XeTextureDimensions(t, slot);
    return t.SampleLevel(s, (denormalized ? uv / dims : uv) + offset / dims, 0.0);
}

float4 XeTex3DLevelZero(Texture3D<float4> t, SamplerState s, float3 uvw)
{
    return t.SampleLevel(s, uvw, 0.0);
}

struct CubeMapData
{
    float3 cubeMapDirections[2];
    uint cubeMapIndex;
};

float4 XeTexCube(TextureCube<float4> t, SamplerState s, float3 coord, inout CubeMapData cubeMapData)
{
    return XE_SAMPLE(t, s, cubeMapData.cubeMapDirections[uint(coord.z) & 1]);
}

float4 XeTexCubeLevelZero(TextureCube<float4> t, SamplerState s, float3 coord, inout CubeMapData cubeMapData)
{
    return t.SampleLevel(s, cubeMapData.cubeMapDirections[uint(coord.z) & 1], 0.0);
}

float2 XeWeights2D(Texture2D<float4> t, float2 uv, float2 offset, uint slot, bool denormalized)
{
    return select(isnan(uv), 0.0, frac((denormalized ? uv : uv * XeTextureDimensions(t, slot)) + offset - 0.5));
}

float4 cube(float4 value, inout CubeMapData cubeMapData)
{
    uint index = cubeMapData.cubeMapIndex;
    cubeMapData.cubeMapDirections[index & 1] = value.xyz;
    ++cubeMapData.cubeMapIndex;
    return float4(0.0, 0.0, 0.0, index);
}

float4 dst(float4 src0, float4 src1)
{
    return float4(1.0, src0.y * src1.y, src0.z, src1.w);
}

float4 max4(float4 src0)
{
    return max(max(src0.x, src0.y), max(src0.z, src0.w));
}
// ---- end prelude ----
)HLSL"
