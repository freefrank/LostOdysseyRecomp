#pragma once

// Shared verbatim by translation and the offline/runtime pack contract.
namespace xenos {
inline constexpr char kShaderCommonHlsl[] = R"HLSL(
// ---- Xenos shader prelude (LostOdysseyRecomp) ----
#define FLT_MIN asfloat(0xff7fffff)
#define FLT_MAX asfloat(0x7f7fffff)

#ifdef __spirv__
ByteAddressBuffer xeVertexArena : register(t0, space0);
struct XePushConstants
{
    uint64_t VertexShaderConstants;
    uint64_t SharedConstants;
    uint64_t PixelShaderConstants;
};
[[vk::push_constant]] ConstantBuffer<XePushConstants> xePush;
#ifdef XE_PIXEL_SHADER
#define XE_CONSTANTS_ADDRESS xePush.PixelShaderConstants
#else
#define XE_CONSTANTS_ADDRESS xePush.VertexShaderConstants
#endif
#define xeNdcScale       vk::RawBufferLoad<float4>(xePush.SharedConstants + 160)
#define xeNdcOffset      vk::RawBufferLoad<float4>(xePush.SharedConstants + 176)
#define xeHalfPixelOffset vk::RawBufferLoad<float2>(xePush.SharedConstants + 192)
#define xeVtxFmt         vk::RawBufferLoad<uint>(xePush.SharedConstants + 200)
#define xeFlags          vk::RawBufferLoad<uint>(xePush.SharedConstants + 204)
#define xeAlphaTest      vk::RawBufferLoad<float4>(xePush.SharedConstants + 208)
#define xeColorMax       vk::RawBufferLoad<float4>(xePush.SharedConstants + 224)
#define xeTransfer       vk::RawBufferLoad<uint4>(xePush.SharedConstants + 240)
#else
cbuffer XeConstants : register(b0, space0)
{
    float4 c[256];
};

cbuffer XeShared : register(b1, space0)
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
};
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
#ifdef __spirv__
    return vk::RawBufferLoad<uint>(xePush.SharedConstants + 256 + slot * 4);
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
#ifdef __spirv__
    return vk::RawBufferLoad<uint>(xePush.SharedConstants + 640 + slot * 4);
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
#ifdef __spirv__
    return vk::RawBufferLoad<float4>(XE_CONSTANTS_ADDRESS + uint64_t(clamp(index, 0, 255)) * 16);
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
    if (info & (1u << 20)) value = value.bgra;
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

float4 XeTextureResult(float4 value, uint slot)
{
    #ifdef __spirv__
    return XeDecodeTexture(value, vk::RawBufferLoad<uint>(xePush.SharedConstants + 768 + slot * 4));
#else
    return XeDecodeTexture(value, xeTextureInfo[slot >> 2][slot & 3]);
#endif
}

bool XeBool(uint index)
{
    #ifdef __spirv__
    return ((vk::RawBufferLoad<uint>(xePush.SharedConstants + ((index >> 5) & 7) * 4) >> (index & 31)) & 1u) != 0u;
#else
    return ((xeBools[(index >> 7) & 1][(index >> 5) & 3] >> (index & 31)) & 1u) != 0u;
#endif
}

uint XeLoopConst(uint id)
{
    #ifdef __spirv__
    return vk::RawBufferLoad<uint>(xePush.SharedConstants + 32 + (id & 31) * 4);
#else
    return xeLoops[(id >> 2) & 7][id & 3];
#endif
}

// ---- vertex fetch ----
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

// ---- texture fetch ----
float2 XeTextureDimensions(Texture2D<float4> t, uint slot)
{
    #ifdef __spirv__
    uint packed = vk::RawBufferLoad<uint>(xePush.SharedConstants + 896 + slot * 4);
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
)HLSL";
}
