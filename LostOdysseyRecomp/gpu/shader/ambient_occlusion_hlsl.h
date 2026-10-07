#pragma once
namespace gpu::ao {
inline constexpr char Shader[] = R"HLSL(
Texture2D<float4> sceneColor : register(t0);
Texture2D<float> sceneDepth : register(t1);
// x: visibility, y: view depth of the full-resolution pixel it was computed at (0 = none).
Texture2D<float2> visibilityImage : register(t2);
Texture2D<float4> hdrSceneColor : register(t4);
#ifdef __spirv__
[[vk::binding(3,0)]]
#endif
cbuffer Parameters : register(b3) {
    float4 projection; // q, pole, tan(fovY/2)*aspect, tan(fovY/2)
    float4 extent;     // full width/height, AO width/height
    float4 raster;     // NDC X/Y additive correction, NDC Y sign, unused
    float radius;
    float strength;
    uint mode;
    uint debugView;
};
static const float PI = 3.14159265358979323846;
float4 vertex(uint id : SV_VertexID) : SV_Position {
    float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2,-2) + float2(-1,1),0,1);
}
bool positionAt(int2 pixel, out float3 p) {
    p = 0;
    if (any(pixel < 0) || any(pixel >= int2(extent.xy))) return false;
    float d = sceneDepth.Load(int3(pixel,0));
    if (!isfinite(d) || d <= 0 || d > 1 || d <= projection.y) return false;
    float z = projection.x / (d - projection.y);
    float2 ndc = (float2(pixel)+.5) / extent.xy * float2(2,-2) + float2(-1,1) + raster.xy;
    p = float3(ndc * projection.zw * float2(1,raster.z),1) * z;
    return all(isfinite(p)) && z > 0;
}
float3 normalAt(int2 pixel, float3 p) {
    float3 l,r,t,b;
    bool vl=positionAt(pixel+int2(-1,0),l), vr=positionAt(pixel+int2(1,0),r);
    bool vt=positionAt(pixel+int2(0,-1),t), vb=positionAt(pixel+int2(0,1),b);
    float3 dx = vr && (!vl || abs(r.z-p.z)<abs(p.z-l.z)) ? r-p : p-l;
    float3 dy = vb && (!vt || abs(b.z-p.z)<abs(p.z-t.z)) ? b-p : p-t;
    float3 n = cross(dx,dy);
    if ((!vl&&!vr) || (!vt&&!vb) || dot(n,n)<1e-12) return normalize(-p);
    n=normalize(n);
    return dot(n,-p)<0 ? -n : n;
}
float noise(int2 p) { return frac(52.9829189 * frac(dot(float2(p),float2(.06711056,.00583715)))); }
float ssao(int2 pixel, float3 p, float3 n) {
    float3 axis = abs(n.z)<.9 ? float3(0,0,1) : float3(0,1,0);
    float3 tangent=normalize(cross(axis,n)), bitangent=cross(n,tangent);
    float blocked=0;
    float rotation=noise(pixel)*2*PI;
    [unroll] for (uint i=0;i<12;++i) {
        float u=(float(i)+.5)/12;
        float angle=rotation+float(i)*2.39996323;
        float3 direction=tangent*(cos(angle)*sqrt(u))+bitangent*(sin(angle)*sqrt(u))+n*sqrt(1-u);
        float3 samplePos=p+direction*radius*lerp(.15,1,u*u);
        if(samplePos.z<=0) continue;
        float2 ndc=samplePos.xy/samplePos.z/projection.zw/float2(1,raster.z)-raster.xy;
        int2 q=int2(floor((ndc*float2(.5,-.5)+.5)*extent.xy));
        float3 scene;
        if(!positionAt(q,scene)) continue;
        float dist=length(scene-p);
        float bias=max(radius*.025,p.z*1e-5);
        if(scene.z<samplePos.z-bias) blocked+=saturate(2-2*dist/radius);
    }
    return saturate(1-blocked/12);
}
// GTAO horizon integration, adapted from Intel XeGTAO (MIT),
// Copyright (C) 2016-2021 Intel Corporation. See thirdparty/licenses/XeGTAO.txt.
// https://github.com/GameTechDev/XeGTAO (XeGTAO.hlsli, MainPass).
// Uses full-precision depth, three slices and four samples per side. No depth
// mip approximation or temporal noise; spatial filtering is in composite().
float gtao(int2 pixel, float3 p, float3 n) {
    float3 view=normalize(-p);
    float pixelRadius=min(128.0,radius*extent.y/(2*projection.w*p.z));
    if(pixelRadius<1) return 1;
    float visibility=0;
    [unroll] for(uint slice=0;slice<3;++slice) {
        float phi=(float(slice)+noise(pixel))/3*PI;
        float2 dir=float2(cos(phi),sin(phi));
        float3 screenDirection=float3(dir,0);
        float3 ortho=screenDirection-dot(screenDirection,view)*view;
        float3 axis=normalize(cross(ortho,view));
        float3 projected=n-axis*dot(n,axis);
        float len=length(projected);
        if(len<1e-5) continue;
        float cosNormal=saturate(dot(projected,view)/len);
        float angle=(dot(ortho,projected)<0?-1:1)*acos(cosNormal);
        float2 low=cos(angle+float2(PI*.5,-PI*.5));
        float2 horizon=low;
        [unroll] for(uint step=0;step<4;++step) {
            float s=(float(step)+.5)/4;
            int2 delta=int2(round(dir*float2(1,-raster.z)*max(1.0,s*s*pixelRadius)));
            [unroll] for(uint side=0;side<2;++side) {
                float3 samplePos;
                if(!positionAt(pixel+(side==0?delta:-delta),samplePos)) continue;
                float3 d=samplePos-p;
                float distance=length(d);
                if(distance<1e-5) continue;
                float weight=saturate(2-2*distance/radius);
                float h=dot(d/distance,view);
                horizon[side]=max(horizon[side],lerp(low[side],h,weight));
            }
        }
        float h0=-acos(clamp(horizon.y,-1,1)), h1=acos(clamp(horizon.x,-1,1));
        h0=angle+clamp(h0-angle,-PI*.5,PI*.5);
        h1=angle+clamp(h1-angle,-PI*.5,PI*.5);
        float arc0=(cosNormal+2*h0*sin(angle)-cos(2*h0-angle))*.25;
        float arc1=(cosNormal+2*h1*sin(angle)-cos(2*h1-angle))*.25;
        visibility+=len*(arc0+arc1);
    }
    return saturate(visibility/3);
}
float2 visibility(float4 pos:SV_Position):SV_Target0 {
    int2 pixel=min(int2(pos.xy*extent.xy/extent.zw),int2(extent.xy)-1);
    float3 p;
    if(!positionAt(pixel,p)) return float2(1,0);
    float3 n=normalAt(pixel,p);
    // FP32 bias toward the eye suppresses self-occlusion on reconstructed slopes.
    // The view depth lets composite() skip a depth load and division per tap.
    return float2(mode==1 ? ssao(pixel,p,n) : gtao(pixel,p*.99999,n), p.z);
}
// Filtered visibility at a full-resolution pixel; false where depth has no surface.
bool filteredVisibility(float4 pos, out float3 p, out float3 n, out float ao) {
    int2 pixel=int2(pos.xy);
    n=0; ao=1;
    if(!positionAt(pixel,p)) return false;
    n=normalAt(pixel,p);
    int2 center=int2(pos.xy*extent.zw/extent.xy);
    float total=0, weights=0;
    [unroll] for(int y=-2;y<=2;++y) [unroll] for(int x=-2;x<=2;++x) {
        int2 q=center+int2(x,y);
        if(any(q<0)||any(q>=int2(extent.zw))) continue;
        // visibility() resolved this tap's pixel (same mapping); rebuild its
        // position from the stored view depth exactly as positionAt() does.
        float2 tap=visibilityImage.Load(int3(q,0));
        if(!(tap.y>0)) continue;
        int2 full=min(int2((float2(q)+.5)*extent.xy/extent.zw),int2(extent.xy)-1);
        float2 ndc=(float2(full)+.5)/extent.xy*float2(2,-2)+float2(-1,1)+raster.xy;
        float3 other=float3(ndc*projection.zw*float2(1,raster.z),1)*tap.y;
        float plane=abs(dot(other-p,n));
        float tolerance=max(radius*.06,p.z*.001);
        float weight=exp2(-float(x*x+y*y)*.5-plane/tolerance*8);
        total+=tap.x*weight;
        weights+=weight;
    }
    ao=weights>1e-5 ? saturate(total/weights) : 1;
    return true;
}
float4 applyVisibility(float4 color, bool surface, float3 p, float3 n, float ao) {
    if(!surface) return debugView!=0 ? float4(1,1,1,color.a) : color;
    if(debugView==2) return float4(n*.5+.5,color.a);
    if(debugView==3) return float4((p.z/(p.z+radius*10)).xxx,color.a);
    if(debugView==1) return float4(ao.xxx,color.a);
    // Scene-copy and HDR sidecar are gamma encoded. Apply a bounded visibility
    // in approximate linear light, preserve alpha and extended HDR values.
    color.rgb*=pow(lerp(1,ao,strength),1.0/2.2);
    return color;
}
float4 composite(float4 pos:SV_Position):SV_Target0 {
    float3 p,n; float ao;
    const bool surface=filteredVisibility(pos,p,n,ao);
    return applyVisibility(sceneColor.Load(int3(int2(pos.xy),0)),surface,p,n,ao);
}
// The RGBA8 scene and its HDR companion share one filtered visibility.
struct CompositePair { float4 color:SV_Target0; float4 hdr:SV_Target1; };
CompositePair compositePair(float4 pos:SV_Position) {
    float3 p,n; float ao;
    const bool surface=filteredVisibility(pos,p,n,ao);
    const int3 pixel=int3(int2(pos.xy),0);
    CompositePair o;
    o.color=applyVisibility(sceneColor.Load(pixel),surface,p,n,ao);
    o.hdr=applyVisibility(hdrSceneColor.Load(pixel),surface,p,n,ao);
    return o;
}
)HLSL";
}
