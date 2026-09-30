Texture2D<float4> linearColor : register(t0);
Texture2D<float4> sourceColor : register(t1);
RWTexture2D<float4> encodedColor : register(u0);

cbuffer Parameters : register(b0) {
    int2 size;
    int2 renderSize;
    int2 colorOrigin;
};

[numthreads(8, 8, 1)]
void main(uint3 globalId : SV_DispatchThreadID) {
    int2 p = int2(globalId.xy);
    if (any(p >= size)) return;
    float3 sceneLinear = linearColor.Load(int3(p, 0)).rgb;
    int2 sourceP = min(int2((float2(p) + 0.5) * float2(renderSize) / float2(size)),
        renderSize - 1) + colorOrigin;
    float alpha = sourceColor.Load(int3(sourceP, 0)).a;
    encodedColor[p] = float4(pow(saturate(sceneLinear), (1.0 / 2.2).xxx), alpha);
}
