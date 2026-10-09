#define XE_SAMPLE(t, s, uv) t.SampleLevel(s, uv, 0.0)

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
// Some Android drivers misread the middle 64-bit push member. Preserve
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
#ifdef XE_PIXEL_SHADER
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
#ifdef __spirv__
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
#ifdef __spirv__
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
    return XeDecodeTexture(value, vk::RawBufferLoad<uint>(xeShared(768 + slot * 4)));
#else
    return XeDecodeTexture(value, xeTextureInfo[slot >> 2][slot & 3]);
#endif
}

bool XeBool(uint index)
{
    #ifdef __spirv__
    return ((vk::RawBufferLoad<uint>(xeShared(((index >> 5) & 7) * 4)) >> (index & 31)) & 1u) != 0u;
#else
    return ((xeBools[(index >> 7) & 1][(index >> 5) & 3] >> (index & 31)) & 1u) != 0u;
#endif
}

uint XeLoopConst(uint id)
{
    #ifdef __spirv__
    return vk::RawBufferLoad<uint>(xeShared(32 + (id & 31) * 4));
#else
    return xeLoops[(id >> 2) & 7][id & 3];
#endif
}


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
        return vk::RawBufferLoad<uint64_t>(xeShared(1024), 8) + uint64_t(min(a, limit));
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


#ifdef __spirv__
#undef ByteAddressBuffer
#endif
// ---- texture fetch ----
float2 XeTextureDimensions(Texture2D<float4> t, uint slot)
{
    #ifdef __spirv__
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

#ifdef __spirv__
#define vfetch0 xeVertexArena
#else
ByteAddressBuffer vfetch0 : register(t0, space0);
#endif
void XeNoKill(float x) {}

struct XePointSizeOutput { [[vk::builtin("PointSize")]] float size : PSIZE; };
void XeRectListVertex(
	in uint xeVertexId : SV_VertexID,
	out precise float4 oPos : SV_Position,
	out float4 o0 : TEXCOORD0,
	out float4 o1 : TEXCOORD1,
	out float4 o2 : TEXCOORD2,
	out float4 o3 : TEXCOORD3,
	out float4 o4 : TEXCOORD4,
	out float4 o5 : TEXCOORD5,
	out float4 o6 : TEXCOORD6,
	out float4 o7 : TEXCOORD7,
	out float4 o8 : TEXCOORD8,
	out float4 o9 : TEXCOORD9,
	out float4 o10 : TEXCOORD10,
	out float4 o11 : TEXCOORD11,
	out float4 o12 : TEXCOORD12,
	out float4 o13 : TEXCOORD13,
	out float4 o14 : TEXCOORD14,
	out float4 o15 : TEXCOORD15,
	out XePointSizeOutput xePointSizeOut)
{
	float4 r0 = 0.0;
	float4 r1 = 0.0;
	float4 xeDiscard = 0.0;
	float4 xeDbgTex = float4(0.0, 0.0, 0.0, 1.0);
	float4 xePV = 0.0;
	int a0 = 0;
	int aL = 0;
	bool p0 = false;
	float ps = 0.0;
	bool xeEnd = false;
	uint xeVfetchBase = 0u;
	CubeMapData cubeMapData = (CubeMapData)0;
	r0.x = float(xeVertexId);
	oPos = float4(0.0, 0.0, 0.0, 1.0);
	o0 = 0.0;
	o1 = 0.0;
	o2 = 0.0;
	o3 = 0.0;
	o4 = 0.0;
	o5 = 0.0;
	o6 = 0.0;
	o7 = 0.0;
	o8 = 0.0;
	o9 = 0.0;
	o10 = 0.0;
	o11 = 0.0;
	o12 = 0.0;
	o13 = 0.0;
	o14 = 0.0;
	o15 = 0.0;
	float4 oPointSize = float4(XeDefaultPointSize(), 0.0, 0.0, 0.0);

	xeVfetchBase = uint(int(floor(r0.x)) * 7) * 4u;
	r1.xyzw = XeVF_32_32_32_FLOAT(vfetch0, XeVfetchOffset(0u) + xeVfetchBase + 0u, true, false).xyzx;
	r1.w = 1.0;
	r0.xyzw = XeVF_32_32_32_32_FLOAT(vfetch0, XeVfetchOffset(0u) + xeVfetchBase + 12u, true, false).xyzw;
	xePV.xyzw = max(r0.xyzw, r0.xyzw);
	o0.xyzw = xePV.xyzw;
	xePV.xyzw = max(r1.xyzw, r1.xyzw);
	oPos.xyzw = xePV.xyzw;
	if ((xeFlags & 8u) == 0u) {
	if (xeVtxFmt & 1u) oPos.xy *= oPos.w;
	if (xeVtxFmt & 2u) oPos.z *= oPos.w;
	if ((xeVtxFmt & 4u) == 0u) oPos.w = rcp(oPos.w);
	oPos.xyz = oPos.xyz * xeNdcScale.xyz + xeNdcOffset.xyz * oPos.w;
	oPos.xy += xeHalfPixelOffset * oPos.w;
	}
	if (xeFlags & 4u) { o15 = oPos; uint k = xeVertexId % 3u; oPos = float4(k == 0u ? -0.6 : (k == 1u ? 0.6 : 0.0), k == 2u ? 0.6 : -0.6, 0.5, 1.0); }
	xePointSizeOut.size = XeClampPointSize(oPointSize.x);
}

// The right-angle corner is the vertex opposite the longest edge. It is
// rotated to the front (keeps the winding), and the fourth vertex is the sum
// of its neighbours minus the corner. Corners 0-5 form the triangles
// (a, b, d) and (d, b, a'), the same as a strip a, b, d, a'.
uint XeRectListCorner(float4 v0, float4 v1, float4 v2)
{
    float2 p0 = v0.xy / v0.w, p1 = v1.xy / v1.w, p2 = v2.xy / v2.w;
    float e0 = dot(p1 - p2, p1 - p2);
    float e1 = dot(p2 - p0, p2 - p0);
    float e2 = dot(p0 - p1, p0 - p1);
    return (e0 >= e1 && e0 >= e2) ? 0u : (e1 >= e2 ? 1u : 2u);
}
float4 XeRectListPick(float4 v0, float4 v1, float4 v2, uint c, uint corner)
{
    float4 a = c == 0u ? v0 : (c == 1u ? v1 : v2);
    float4 b = c == 0u ? v1 : (c == 1u ? v2 : v0);
    float4 d = c == 0u ? v2 : (c == 1u ? v0 : v1);
    if (corner == 0u) return a;
    if (corner == 1u || corner == 4u) return b;
    if (corner == 2u || corner == 3u) return d;
    return b + d - a;
}

void main(in uint xeRectVertexId : SV_VertexID, out precise float4 oPos : SV_Position, out float4 o0 : TEXCOORD0, out float4 o1 : TEXCOORD1, out float4 o2 : TEXCOORD2, out float4 o3 : TEXCOORD3, out float4 o4 : TEXCOORD4, out float4 o5 : TEXCOORD5, out float4 o6 : TEXCOORD6, out float4 o7 : TEXCOORD7, out float4 o8 : TEXCOORD8, out float4 o9 : TEXCOORD9, out float4 o10 : TEXCOORD10, out float4 o11 : TEXCOORD11, out float4 o12 : TEXCOORD12, out float4 o13 : TEXCOORD13, out float4 o14 : TEXCOORD14, out float4 o15 : TEXCOORD15, out XePointSizeOutput xePointSizeOut) {
    uint first = xeRectVertexId >> 3u;
    uint corner = xeRectVertexId & 7u;
    float4 p0;
    float4 t0_0;
    float4 t0_1;
    float4 t0_2;
    float4 t0_3;
    float4 t0_4;
    float4 t0_5;
    float4 t0_6;
    float4 t0_7;
    float4 t0_8;
    float4 t0_9;
    float4 t0_10;
    float4 t0_11;
    float4 t0_12;
    float4 t0_13;
    float4 t0_14;
    float4 t0_15;
    XePointSizeOutput s0;
    XeRectListVertex(first + 0u, p0, t0_0, t0_1, t0_2, t0_3, t0_4, t0_5, t0_6, t0_7, t0_8, t0_9, t0_10, t0_11, t0_12, t0_13, t0_14, t0_15, s0);
    float4 p1;
    float4 t1_0;
    float4 t1_1;
    float4 t1_2;
    float4 t1_3;
    float4 t1_4;
    float4 t1_5;
    float4 t1_6;
    float4 t1_7;
    float4 t1_8;
    float4 t1_9;
    float4 t1_10;
    float4 t1_11;
    float4 t1_12;
    float4 t1_13;
    float4 t1_14;
    float4 t1_15;
    XePointSizeOutput s1;
    XeRectListVertex(first + 1u, p1, t1_0, t1_1, t1_2, t1_3, t1_4, t1_5, t1_6, t1_7, t1_8, t1_9, t1_10, t1_11, t1_12, t1_13, t1_14, t1_15, s1);
    float4 p2;
    float4 t2_0;
    float4 t2_1;
    float4 t2_2;
    float4 t2_3;
    float4 t2_4;
    float4 t2_5;
    float4 t2_6;
    float4 t2_7;
    float4 t2_8;
    float4 t2_9;
    float4 t2_10;
    float4 t2_11;
    float4 t2_12;
    float4 t2_13;
    float4 t2_14;
    float4 t2_15;
    XePointSizeOutput s2;
    XeRectListVertex(first + 2u, p2, t2_0, t2_1, t2_2, t2_3, t2_4, t2_5, t2_6, t2_7, t2_8, t2_9, t2_10, t2_11, t2_12, t2_13, t2_14, t2_15, s2);
    uint c = XeRectListCorner(p0, p1, p2);
    oPos = XeRectListPick(p0, p1, p2, c, corner);
    o0 = XeRectListPick(t0_0, t1_0, t2_0, c, corner);
    o1 = XeRectListPick(t0_1, t1_1, t2_1, c, corner);
    o2 = XeRectListPick(t0_2, t1_2, t2_2, c, corner);
    o3 = XeRectListPick(t0_3, t1_3, t2_3, c, corner);
    o4 = XeRectListPick(t0_4, t1_4, t2_4, c, corner);
    o5 = XeRectListPick(t0_5, t1_5, t2_5, c, corner);
    o6 = XeRectListPick(t0_6, t1_6, t2_6, c, corner);
    o7 = XeRectListPick(t0_7, t1_7, t2_7, c, corner);
    o8 = XeRectListPick(t0_8, t1_8, t2_8, c, corner);
    o9 = XeRectListPick(t0_9, t1_9, t2_9, c, corner);
    o10 = XeRectListPick(t0_10, t1_10, t2_10, c, corner);
    o11 = XeRectListPick(t0_11, t1_11, t2_11, c, corner);
    o12 = XeRectListPick(t0_12, t1_12, t2_12, c, corner);
    o13 = XeRectListPick(t0_13, t1_13, t2_13, c, corner);
    o14 = XeRectListPick(t0_14, t1_14, t2_14, c, corner);
    o15 = XeRectListPick(t0_15, t1_15, t2_15, c, corner);
    xePointSizeOut = s0;
}
