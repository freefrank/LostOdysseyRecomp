Texture2D<float4> sourceColor : register(t0);
Texture2D<float> sourceDepth : register(t1);
Texture2D<float> sceneContribution : register(t2);
RWTexture2D<float4> linearColor : register(u0);
RWTexture2D<float> canonicalDepth : register(u1);
RWTexture2D<float> reactiveMask : register(u2);

cbuffer Parameters : register(b0) {
    int2 colorOrigin;
    int2 depthOrigin;
    int2 size;
    float depthScale;
    float depthBias;
    int2 maskOrigin;
    uint maskEnabled;
    float reactiveMax;
};

[numthreads(8, 8, 1)]
void main(uint3 globalId : SV_DispatchThreadID) {
    int2 p = int2(globalId.xy);
    if (any(p >= size)) return;
    float4 encoded = sourceColor.Load(int3(p + colorOrigin, 0));
    float rawDepth = sourceDepth.Load(int3(p + depthOrigin, 0));
    linearColor[p] = float4(pow(max(encoded.rgb, 0.0.xxx), 2.2.xxx), encoded.a);
    canonicalDepth[p] = (isnan(rawDepth) || isinf(rawDepth)) ? 0.0 :
        saturate(rawDepth * depthScale + depthBias);
    if (maskEnabled != 0) {
        float m = sceneContribution.Load(int3(p + maskOrigin, 0));
        reactiveMask[p] = min(reactiveMax, saturate(m));
    }
}
