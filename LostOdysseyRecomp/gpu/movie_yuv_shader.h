#pragma once

namespace gpu::movie_yuv
{
// Cutscene replacement frame: NV12 (H.264, BT.709 limited range) to RGB over
// the guest movie rectangle. Drawn with the blit vertex shader and the viewport
// on the rectangle; xeRect (SharedConstants::transfer) is that rectangle in
// target pixels: x, y, width, height as floats.
inline constexpr const char* PixelShader = R"hlsl(
Texture2D<float> luma : register(t0, space1);
Texture2D<float2> chroma : register(t1, space1);
#ifdef __spirv__
SamplerState linearSampler : register(s0, space4);
struct XePushConstants { uint64_t vs; uint64_t sharedAddress; uint64_t ps; };
[[vk::push_constant]] ConstantBuffer<XePushConstants> xePush;
#define xeRect asfloat(vk::RawBufferLoad<uint4>(xePush.sharedAddress + 240))
#else
SamplerState linearSampler : register(s0, space0);
cbuffer XeShared : register(b1, space0) {
    uint4 pad0[2]; uint4 pad1[8]; float4 pad2; float4 pad3;
    float4 pad4; float4 pad5; float4 xeColorMax; uint4 xeRectBits;
};
#define xeRect asfloat(xeRectBits)
#endif

// The default sampler wraps: keep the taps half a texel inside each plane.
float3 Yuv(float2 uv, float2 lumaEdge, float2 chromaEdge)
{
    float y = luma.SampleLevel(linearSampler, clamp(uv, lumaEdge, 1.0 - lumaEdge), 0.0);
    float2 c = chroma.SampleLevel(linearSampler, clamp(uv, chromaEdge, 1.0 - chromaEdge), 0.0);
    return float3(y, c);
}

float4 main(float4 pos : SV_Position) : SV_Target
{
    float4 rect = xeRect;
    float2 uv = (pos.xy - rect.xy) / rect.zw;
    uint lumaWidth, lumaHeight, chromaWidth, chromaHeight;
    luma.GetDimensions(lumaWidth, lumaHeight);
    chroma.GetDimensions(chromaWidth, chromaHeight);
    float2 lumaEdge = 0.5 / float2(lumaWidth, lumaHeight);
    float2 chromaEdge = 0.5 / float2(chromaWidth, chromaHeight);
    float3 yuv;
    // A video much larger than its rectangle (4K into 720p): four bilinear
    // taps spread over the pixel's footprint instead of one.
    if (float(lumaWidth) > rect.z * 1.5 || float(lumaHeight) > rect.w * 1.5)
    {
        float2 d = 0.25 / rect.zw;
        yuv = 0.25 * (Yuv(uv + float2(-d.x, -d.y), lumaEdge, chromaEdge) + Yuv(uv + float2(d.x, -d.y), lumaEdge, chromaEdge) +
                      Yuv(uv + float2(-d.x, d.y), lumaEdge, chromaEdge) + Yuv(uv + float2(d.x, d.y), lumaEdge, chromaEdge));
    }
    else
        yuv = Yuv(uv, lumaEdge, chromaEdge);
    float y = (yuv.x - 16.0 / 255.0) * (255.0 / 219.0);
    float2 c = (yuv.yz - 128.0 / 255.0) * (255.0 / 224.0);
    float3 rgb = float3(y + 1.5748 * c.y, y - 0.187324 * c.x - 0.468124 * c.y, y + 1.8556 * c.x);
    return float4(saturate(rgb), 1.0);
}
)hlsl";
}
