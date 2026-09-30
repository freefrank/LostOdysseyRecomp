#include "temporal_aa.h"
#include "shader/dxc_compiler.h"
#include <array>
#include <vector>
#include <limits>
#include <cstring>
#ifdef LO_GPU_PLUME
#include <plume_render_interface.h>
#include <plume_render_interface_builders.h>
namespace gpu
{
using namespace plume;
struct TemporalAA::Impl
{
    RenderDevice* device=nullptr;
    bool vulkan=false;
    std::string error;
    std::unique_ptr<RenderPipelineLayout> layout;
    std::unique_ptr<RenderShader> vs, ps, displayPs;
    std::array<std::unique_ptr<RenderPipeline>,2> pipeline,displayPipeline;
    bool defaultFp16=false;
    std::unique_ptr<RenderSampler> sampler;
    struct Pending { uint64_t serial=0; std::unique_ptr<RenderBuffer> constants; std::unique_ptr<RenderDescriptorSet> set; std::unique_ptr<RenderFramebuffer> framebuffer; };
    std::vector<Pending> pending;
    uint64_t recordedSerial=0;
    temporal::GpuPassTimer<> resolveTimer, displayTimer;
    RenderPipeline* Pipeline(bool display,TemporalColorStorage storage) {
        if(storage!=TemporalColorStorage::Default&&storage!=TemporalColorStorage::Rgba8&&storage!=TemporalColorStorage::Rgba16Float) {
            error="Invalid temporal output storage";return nullptr;
        }
        if(!device||!layout||!vs||!ps||!displayPs) {error="Temporal component is not initialized";return nullptr;}
        const bool fp16=storage==TemporalColorStorage::Default?defaultFp16:storage==TemporalColorStorage::Rgba16Float;
        auto& result=(display?displayPipeline:pipeline)[fp16?1:0];
        if(!result) {
            RenderGraphicsPipelineDesc desc;desc.pipelineLayout=layout.get();desc.vertexShader=vs.get();desc.pixelShader=display?displayPs.get():ps.get();
            desc.renderTargetCount=1;desc.renderTargetFormat[0]=fp16?RenderFormat::R16G16B16A16_FLOAT:RenderFormat::R8G8B8A8_UNORM;
            desc.renderTargetBlend[0]=RenderBlendDesc::Copy();desc.cullMode=RenderCullMode::NONE;
            result=device->createGraphicsPipeline(desc);
            if(!result)error="Temporal pipeline creation failed";
        }
        return result.get();
    }
};
namespace
{
struct Constants
{
    float transform[16]{}; // Explicit HLSL row_major matrix.
    float previousScaleBias[4]{};
    float size[4]{}; // current xy / previous xy
    float policy[4]{}; // weight, abs depth, relative depth, unused
    uint32_t active=0, reactive=0, pad0=0, pad1=0;
    float jitter[4]{}; // current xy, previous xy; raster pixel displacement.
    float stationaryPolicy[4]{}; // stationary weight, motion min/max, coverage enabled.
};
static_assert(sizeof(Constants)==160);
void DefineSet(RenderDescriptorSetBuilder& set, bool vulkan)
{
    set.begin(); for(uint32_t i=0;i<7;++i) set.addTexture(i); set.addSampler(vulkan?7:0); if(vulkan) set.addConstantBuffer(8); set.end();
}
temporal::Matrix Multiply(const temporal::Matrix& a,const temporal::Matrix& b)
{
    temporal::Matrix out{};
    for(int i=0;i<4;++i) for(int j=0;j<4;++j) for(int k=0;k<4;++k) out[4*i+j]+=a[4*i+k]*b[4*k+j];
    return out;
}
bool FullViewport(const temporal::Camera& camera,uint32_t w,uint32_t h)
{
    const auto& v=camera.Raster();return v.x==0 && v.y==0 && v.width==w && v.height==h;
}
const char* source=R"(
Texture2D<float4> currentColor:register(t0);
Texture2D<float> currentDepth:register(t1);
Texture2D<float4> historyColor:register(t2);
Texture2D<float> historyDepth:register(t3);
Texture2D<float> reactiveMask:register(t4);
Texture2D<float2> motionVector:register(t5);
Texture2D<float2> motionDepths:register(t6);
#ifdef __spirv__
[[vk::binding(7,0)]]
#endif
SamplerState linearClamp:register(s0);
#ifdef __spirv__
[[vk::binding(8,0)]]
#endif
cbuffer Parameters:register(b0) {
 row_major float4x4 transform;
 float4 previousScaleBias;
 float4 imageSize;
 float4 policy;
 uint active,reactive,pad0,pad1;
 float4 jitter;
 float4 stationaryPolicy;
};
float4 vertex(uint id:SV_VertexID):SV_Position {
 float2 uv=float2((id<<1)&2,id&2);
 return float4(uv*float2(2,-2)+float2(-1,1),0,1);
}
bool depthAgrees(float value,float predicted) {
 return isfinite(value) && value>0 && value<=1 && abs(value-predicted)<=policy.y+policy.z*max(value,predicted);
}
// Catmull-Rom retains subpixel detail across repeated history resampling. All
// sixteen source depths are validated; signed lobes are constrained below by
// the current neighborhood, never allowed to import unvalidated geometry.
float4 cubicWeights(float t) {
 return float4(-.5*t+t*t-.5*t*t*t,1-2.5*t*t+1.5*t*t*t,
              .5*t+2*t*t-1.5*t*t*t,-.5*t*t+.5*t*t*t);
}
float4 rejected(float4 center) { return (pad1&2)?float4(0,0,0,1):center; }
float4 accumulate(float4 center,float3 history,float3 lo,float3 hi,float historyWeight,bool stationaryClip=false) {
 bool colorReject=(pad1&1) && (any(history<lo-1.0/255.0)||any(history>hi+1.0/255.0));
 if(pad1&2)return colorReject?(stationaryClip?float4(1,.5,0,1):float4(0,1,0,1)):float4(1,0,0,1);
 if(colorReject&&!stationaryClip)return center;
 return float4(lerp(center.rgb,clamp(history,lo,hi),historyWeight),center.a);
}
float4 stablePixel(float4 position) {
 int2 p=int2(position.xy);float4 center=currentColor.Load(int3(p,0));
 if(!active)return rejected(center);
 if(reactive){float mask=reactiveMask.Load(int3(p,0));if(!isfinite(mask)||mask>0)return rejected(center);}
 float d=currentDepth.Load(int3(p,0));if(!isfinite(d)||d<=0||d>1)return rejected(center);
 float2 raster=position.xy+jitter.xy;
 float4 clip=mul(float4(raster,1-d,1),transform);
 if(!all(isfinite(clip))||clip.w<=1e-6*max(1,max(max(abs(clip.x),abs(clip.y)),abs(clip.z))))return rejected(center);
 float2 raw=(clip.xy/clip.w)*previousScaleBias.xy+previousScaleBias.zw;
 float2 q=raw-jitter.zw;
 if(!all(isfinite(raw))||any(raw<.5)||any(raw>imageSize.zw-.5)||any(q<.5)||any(q>imageSize.zw-.5))return rejected(center);
 // Fixed radius-one support, paired NEAR/FAR endpoints, not a set-intersection
 // test. A silhouette pixel represents coverage of two surfaces. Requiring every
 // history tap to have the center depth would discard all temporal edge coverage.
 float cmin=1,cmax=0,pmin=1,pmax=0;float3 lo=center.rgb,hi=center.rgb;
 int2 hp=int2(raw);
 [unroll]for(int y=-1;y<=1;++y) [unroll]for(int x=-1;x<=1;++x) {
  int2 cp=clamp(p+int2(x,y),int2(0,0),int2(imageSize.xy)-1);
  int2 pp=clamp(hp+int2(x,y),int2(0,0),int2(imageSize.zw)-1);
  float cd=currentDepth.Load(int3(cp,0)),pd=historyDepth.Load(int3(pp,0));
  if(!isfinite(cd)||cd<=0||cd>1||!isfinite(pd)||pd<=0||pd>1)return rejected(center);
  cmin=min(cmin,cd);cmax=max(cmax,cd);pmin=min(pmin,pd);pmax=max(pmax,pd);
  float3 color=currentColor.Load(int3(cp,0)).rgb;lo=min(lo,color);hi=max(hi,color);
 }
 float4 farClip=mul(float4(raster,1-cmin,1),transform),nearClip=mul(float4(raster,1-cmax,1),transform);
 if(!all(isfinite(farClip))||!all(isfinite(nearClip))||farClip.w<=1e-6||nearClip.w<=1e-6)return rejected(center);
 float farDepth=1-farClip.z/farClip.w,nearDepth=1-nearClip.z/nearClip.w;
 if(!isfinite(farDepth)||!isfinite(nearDepth)||farDepth<=0||nearDepth<=0||farDepth>1||nearDepth>1)return rejected(center);
 if(!depthAgrees(pmin,min(farDepth,nearDepth))||!depthAgrees(pmax,max(farDepth,nearDepth)))return rejected(center);
 // Both coverage surfaces must reproject inside the same fixed-radius support.
 // Large differential parallax cannot be justified by matching depth extrema.
 float2 farRaw=farClip.xy/farClip.w*previousScaleBias.xy+previousScaleBias.zw;
 float2 nearRaw=nearClip.xy/nearClip.w*previousScaleBias.xy+previousScaleBias.zw;
 if(any(abs(farRaw-raw)>1)||any(abs(nearRaw-raw)>1))return rejected(center);
 int2 first=int2(floor(q-.5));float2 fraction=frac(q-.5);
 float4 wx=cubicWeights(fraction.x),wy=cubicWeights(fraction.y);float3 history=0;
 [unroll]for(int yy=0;yy<4;++yy) [unroll]for(int xx=0;xx<4;++xx) {
  int2 tap=clamp(first+int2(xx-1,yy-1),int2(0,0),int2(imageSize.zw)-1);
  float weight=wx[xx]*wy[yy];if(weight==0)continue;
  // A stable color texel refers to its separately jittered raster-depth location.
  // Validate the actual signed-cubic footprint, including taps beyond the 3x3
  // endpoint search, so an unobserved third layer cannot enter via a negative lobe.
  int2 rawTap=clamp(int2(floor(float2(tap)+.5+jitter.zw)),int2(0,0),int2(imageSize.zw)-1);
  float tapDepth=historyDepth.Load(int3(rawTap,0));
  if(!depthAgrees(tapDepth,pmin)&&!depthAgrees(tapDepth,pmax))return rejected(center);
  history+=historyColor.Load(int3(tap,0)).rgb*weight;
 }
 if(!all(isfinite(history)))return rejected(center);
 return accumulate(center,history,lo,hi,policy.x);
}
// A stationary silhouette can exchange its two raster-depth owners as jitter
// changes while stable color history still represents their accumulated coverage.
// This proves only local two-surface support, not persistent geometry identity.
bool stationaryCoverageSupport(int2 p,float2 centerDepths,float footprintDepth) {
 if(any(imageSize.xy!=imageSize.zw))return false;
 float cmin=1,cmax=0,pmin=1,pmax=0;
 [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x) {
  int2 t=clamp(p+int2(x,y),int2(0,0),int2(imageSize.xy)-1);
  int2 pt=clamp(int2(floor(float2(t)+.5+jitter.zw)),int2(0,0),int2(imageSize.zw)-1);
  float mask=reactiveMask.Load(int3(t,0));float2 mv=motionVector.Load(int3(t,0)),md=motionDepths.Load(int3(t,0));
  float cd=currentDepth.Load(int3(t,0)),pd=historyDepth.Load(int3(pt,0));
  if(!isfinite(mask)||mask!=0||!all(isfinite(mv))||length(mv)>stationaryPolicy.y||!all(isfinite(md))
    ||!depthAgrees(cd,md.x)||!depthAgrees(md.y,md.x)||!isfinite(pd)||pd<=0||pd>1)return false;
  cmin=min(cmin,cd);cmax=max(cmax,cd);pmin=min(pmin,pd);pmax=max(pmax,pd);
 }
 if(depthAgrees(cmin,cmax)||!depthAgrees(pmin,cmin)||!depthAgrees(pmax,cmax))return false;
 if(!depthAgrees(centerDepths.x,centerDepths.y)
   ||(!depthAgrees(centerDepths.y,cmin)&&!depthAgrees(centerDepths.y,cmax))
   ||(!depthAgrees(footprintDepth,pmin)&&!depthAgrees(footprintDepth,pmax)))return false;
 // Matching extrema alone would admit a third surface between the endpoints.
 [unroll]for(int yy=-1;yy<=1;++yy)[unroll]for(int xx=-1;xx<=1;++xx) {
  int2 t=clamp(p+int2(xx,yy),int2(0,0),int2(imageSize.xy)-1);
  int2 pt=clamp(int2(floor(float2(t)+.5+jitter.zw)),int2(0,0),int2(imageSize.zw)-1);
  float cd=currentDepth.Load(int3(t,0)),pd=historyDepth.Load(int3(pt,0));
  if((!depthAgrees(cd,cmin)&&!depthAgrees(cd,cmax))||(!depthAgrees(pd,pmin)&&!depthAgrees(pd,pmax)))return false;
 }
 return true;
}
bool stationaryMultiSurfaceSupport(int2 p,float2 centerDepths,float footprintDepth) {
 if(any(imageSize.xy!=imageSize.zw)||!depthAgrees(centerDepths.x,centerDepths.y))return false;
 float currentSupport[9],previousSupport[9];float cmin=1,cmax=0,pmin=1,pmax=0;
 bool allZero=true;
 [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x) {
  int2 t=clamp(p+int2(x,y),int2(0,0),int2(imageSize.xy)-1);
  int2 pt=clamp(int2(floor(float2(t)+.5+jitter.zw)),int2(0,0),int2(imageSize.zw)-1);
  float mask=reactiveMask.Load(int3(t,0));float2 mv=motionVector.Load(int3(t,0)),md=motionDepths.Load(int3(t,0));
  float cd=currentDepth.Load(int3(t,0)),pd=historyDepth.Load(int3(pt,0));
  if(!isfinite(mask)||mask!=0||!all(isfinite(mv))||length(mv)>stationaryPolicy.y||!all(isfinite(md))
    ||!depthAgrees(cd,md.x)||!depthAgrees(md.y,md.x)||!isfinite(pd)||pd<=0||pd>1)return false;
  currentSupport[(y+1)*3+x+1]=cd;previousSupport[(y+1)*3+x+1]=pd;
  cmin=min(cmin,cd);cmax=max(cmax,cd);pmin=min(pmin,pd);pmax=max(pmax,pd);
  allZero=allZero&&all(mv==0);
 }
 if(depthAgrees(cmin,cmax)||!depthAgrees(cmin,pmin)||!depthAgrees(cmax,pmax))return false;
 // A radius-one silhouette may contain a third surface. Compare the near/far
 // endpoints and at most one middle cluster on each side; every tap must fit.
 // This bounds the fallback to two short loops instead of 81 cross comparisons.
 float cmid=0,pmid=0;bool cthird=false,pthird=false;
 [loop]for(int i=0;i<9;++i) {
  float cd=currentSupport[i],pd=previousSupport[i];
  if(!depthAgrees(cd,cmin)&&!depthAgrees(cd,cmax)) {
   if(!cthird){cmid=cd;cthird=true;}else if(!depthAgrees(cd,cmid))return false;
  }
  if(!depthAgrees(pd,pmin)&&!depthAgrees(pd,pmax)) {
   if(!pthird){pmid=pd;pthird=true;}else if(!depthAgrees(pd,pmid))return false;
  }
 }
 if(cthird!=pthird||(cthird&&(!allZero||!depthAgrees(cmid,pmid))))return false;
 if(!depthAgrees(centerDepths.y,cmin)&&!depthAgrees(centerDepths.y,cmax)
    &&(!cthird||!depthAgrees(centerDepths.y,cmid)))return false;
 return depthAgrees(footprintDepth,pmin)||depthAgrees(footprintDepth,pmax)
   ||(pthird&&depthAgrees(footprintDepth,pmid));
}
float4 motionRejected(float4 center,float3 reason) {
 return ((pad1&2) && (pad1&16))?float4(reason,1):rejected(center);
}
float4 motionPixel(float4 position) {
 int2 p=int2(position.xy);float4 center=currentColor.Load(int3(p,0));
 if(!active)return rejected(center);
 float mask=reactiveMask.Load(int3(p,0));
 if(!isfinite(mask)||mask>0)return (pad1&4)?float4(1,0,1,1):motionRejected(center,float3(0,0,1));
 float2 mv=motionVector.Load(int3(p,0)),md=motionDepths.Load(int3(p,0));
 float d=currentDepth.Load(int3(p,0));
 if(!all(isfinite(mv))||!all(isfinite(md))||md.y<=0||md.y>1)return motionRejected(center,float3(0,1,1));
 if(!depthAgrees(d,md.x))return motionRejected(center,float3(1,1,0));
 if(pad1&4)return float4(.5+clamp(mv/32,-.5,.5),0,1);
 bool stable=(pad0&1)!=0;
 // MV is unjittered. Convert to the history color/depth grids once.
 float2 historyMotion=mv;
 if(stable && (pad1&8) && (pad1&32) && length(mv)<=stationaryPolicy.y)historyMotion=0;
 float2 q=position.xy+historyMotion+(stable?float2(0,0):jitter.zw-jitter.xy);
 float2 raw=q+(stable?jitter.zw:float2(0,0));
 if(any(q<.5)||any(q>imageSize.zw-.5)||any(raw<.5)||any(raw>imageSize.zw-.5))return motionRejected(center,float3(0,1,1));
  int2 first=int2(floor(q-.5));float2 f=frac(q-.5);
  float4 wx=cubicWeights(f.x),wy=cubicWeights(f.y);float3 history=0;
  float secondaryDepth=0;bool primaryDepthFound=false,secondaryDepthFound=false,thirdDepthFound=false;
  float coreSecondary=0;bool corePrimary=false,coreSecondaryFound=false,coreThird=false;
  float3 bilinearHistory=0;
  const bool movingFallback=(pad1&256) && length(mv)>stationaryPolicy.y;
  [unroll]for(int y=0;y<4;++y)[unroll]for(int x=0;x<4;++x) {
   float weight=wx[x]*wy[y];if(weight==0)continue;
   int2 tap=clamp(first+int2(x-1,y-1),int2(0,0),int2(imageSize.zw)-1);
   int2 dtap=stable?int2(floor(float2(tap)+.5+jitter.zw)):tap;
   dtap=clamp(dtap,int2(0,0),int2(imageSize.zw)-1);
   float tapDepth=historyDepth.Load(int3(dtap,0));
   if(depthAgrees(tapDepth,md.y))primaryDepthFound=true;
   else if(!secondaryDepthFound){if(!isfinite(tapDepth)||tapDepth<=0||tapDepth>1)return motionRejected(center,float3(0,1,1));secondaryDepth=tapDepth;secondaryDepthFound=true;}
   else if(!depthAgrees(tapDepth,secondaryDepth)) {
    if(!movingFallback)return motionRejected(center,float3(1,0,1));
    thirdDepthFound=true;
   }
   float3 tapColor=historyColor.Load(int3(tap,0)).rgb;
   history+=tapColor*weight;
   if(movingFallback && x>=1 && x<=2 && y>=1 && y<=2) {
    if(depthAgrees(tapDepth,md.y))corePrimary=true;
    else if(!coreSecondaryFound){coreSecondary=tapDepth;coreSecondaryFound=true;}
    else if(!depthAgrees(tapDepth,coreSecondary))coreThird=true;
    bilinearHistory+=tapColor*(x==1?1-f.x:f.x)*(y==1?1-f.y:f.y);
   }
  }
  if(thirdDepthFound) {
   // Reuse the middle four samples already fetched by the cubic footprint.
   // A third layer in its outer ring cannot enter the smaller bilinear core.
   if(coreThird)return motionRejected(center,float3(1,0,1));
   if(!corePrimary)return motionRejected(center,float3(0,0,0));
   history=bilinearHistory;
  }
  [branch]if(!primaryDepthFound) {
   if(!stable || !(pad1&8) || stationaryPolicy.w==0 || length(mv)>stationaryPolicy.y)return motionRejected(center,float3(0,0,0));
   if(((pad1&128)?!stationaryMultiSurfaceSupport(p,md,secondaryDepth)
                 :!stationaryCoverageSupport(p,md,secondaryDepth)))
    return motionRejected(center,float3(0,0,0));
  }
 if(!all(isfinite(history)))return motionRejected(center,float3(0,1,1));
 float3 lo=center.rgb,hi=center.rgb;
 [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x) {
  int2 t=clamp(p+int2(x,y),int2(0,0),int2(imageSize.xy)-1);
  float3 c=currentColor.Load(int3(t,0)).rgb;lo=min(lo,c);hi=max(hi,c);
 }
 // Default 31/33 gives an effective size of 32 independent samples, not a sliding
 // window. Geometric displacement excludes jitter; fade over the configured range.
 float historyWeight=policy.x;
 if(stable && (pad1&8))
  historyWeight=lerp(max(policy.x,stationaryPolicy.x),policy.x,smoothstep(stationaryPolicy.y,stationaryPolicy.z,length(mv)));
  const bool stationaryClip=stable && (pad1&8) && (pad1&64) && all(mv==0);
  return accumulate(center,history,lo,hi,historyWeight,stationaryClip);
}
float4 pixel(float4 position:SV_Position):SV_Target {
 if(pad0&4)return motionPixel(position);
 if(pad0&1)return stablePixel(position);
 int2 p=int2(position.xy);float4 center=currentColor.Load(int3(p,0));
 if(!active) return rejected(center);
 if(reactive) { float mask=reactiveMask.Load(int3(p,0));if(!isfinite(mask)||mask>0) return rejected(center); }
 float d=currentDepth.Load(int3(p,0));
 if(!isfinite(d)||d<=0||d>1) return rejected(center);
 float4 clip=mul(float4(position.xy,1-d,1),transform);
 if(!all(isfinite(clip))||clip.w<=1e-6*max(1,max(max(abs(clip.x),abs(clip.y)),abs(clip.z))))return rejected(center);
 float2 q=(clip.xy/clip.w)*previousScaleBias.xy+previousScaleBias.zw;
 float predicted=1-clip.z/clip.w;
 if(!all(isfinite(q))||!isfinite(predicted)||predicted<=0||predicted>1||any(q<.5)||any(q>imageSize.zw-.5))return rejected(center);
 int2 first=int2(floor(q-.5)),last=min(first+1,int2(imageSize.zw)-1);
 if(!depthAgrees(historyDepth.Load(int3(first,0)),predicted)
  ||!depthAgrees(historyDepth.Load(int3(last.x,first.y,0)),predicted)
  ||!depthAgrees(historyDepth.Load(int3(first.x,last.y,0)),predicted)
  ||!depthAgrees(historyDepth.Load(int3(last,0)),predicted))return rejected(center);
 float2 fraction=frac(q-.5);
 float4 wx=cubicWeights(fraction.x),wy=cubicWeights(fraction.y);
 float3 history=0;
 [unroll]for(int cy=0;cy<4;++cy) [unroll]for(int cx=0;cx<4;++cx) {
  int2 tap=clamp(first+int2(cx-1,cy-1),int2(0,0),int2(imageSize.zw)-1);
  float tapD=historyDepth.Load(int3(tap,0));
  if(!depthAgrees(tapD,predicted))return rejected(center);
  history+=historyColor.Load(int3(tap,0)).rgb*wx[cx]*wy[cy];
 }
 if(!all(isfinite(history))) return rejected(center);
 float3 lo=center.rgb,hi=center.rgb;
 [unroll]for(int y=-1;y<=1;++y) [unroll]for(int x=-1;x<=1;++x) {
  int2 tap=clamp(p+int2(x,y),int2(0,0),int2(imageSize.xy)-1);
  float3 c=currentColor.Load(int3(tap,0)).rgb;lo=min(lo,c);hi=max(hi,c);
 }
 return accumulate(center,history,lo,hi,policy.x);
}
float4 displayPixel(float4 position:SV_Position):SV_Target {
 if(all(policy.xy==0)) return currentColor.Load(int3(int2(position.xy),0));
 float2 p=clamp(position.xy+policy.xy,.5,imageSize.xy-.5);
 int2 first=int2(floor(p-.5));float2 fraction=frac(p-.5);
 float4 wx=cubicWeights(fraction.x),wy=cubicWeights(fraction.y);
 float4 value=0;
 float4 lo=currentColor.Load(int3(clamp(first-int2(1,1),int2(0,0),int2(imageSize.xy)-1),0)),hi=lo;
 [unroll]for(int y=0;y<4;++y) [unroll]for(int x=0;x<4;++x) {
  int2 tap=clamp(first+int2(x-1,y-1),int2(0,0),int2(imageSize.xy)-1);
  float4 c=currentColor.Load(int3(tap,0));value+=c*wx[x]*wy[y];lo=min(lo,c);hi=max(hi,c);
 }
 return clamp(value,lo,hi);
})";
}
TemporalAA::TemporalAA():impl(std::make_unique<Impl>()) {}
TemporalAA::~TemporalAA()=default;
const std::string& TemporalAA::LastError() const { return impl->error; }
void TemporalAA::EnableGpuTiming(bool enabled) {impl->resolveTimer.Enable(enabled);impl->displayTimer.Enable(enabled);}
const temporal::GpuPassTimingStats& TemporalAA::ResolveTiming() const {return impl->resolveTimer.Stats();}
const temporal::GpuPassTimingStats& TemporalAA::DisplayTiming() const {return impl->displayTimer.Stats();}
void TemporalAA::ReleaseCompleted() { ReleaseCompletedThrough(impl->recordedSerial); }
uint64_t TemporalAA::RecordedSerial() const { return impl->recordedSerial; }
void TemporalAA::RecordExternalUse() { ++impl->recordedSerial; }
void TemporalAA::ReleaseCompletedThrough(uint64_t serial) {
    impl->resolveTimer.ReleaseCompletedThrough(serial);impl->displayTimer.ReleaseCompletedThrough(serial);
    std::erase_if(impl->pending,[serial](const Impl::Pending& pending){return pending.serial<=serial;});
}
bool TemporalAA::Init(RenderDevice* device)
{
    return Init(device,false);
}
bool TemporalAA::Init(RenderDevice* device,bool hdrColor)
{
    if(!device || impl->device) { impl->error="Init requires a non-null device and a fresh component";return false; }
    auto& p=*impl;p.device=device;p.defaultFp16=hdrColor;
    p.vulkan=device->getCapabilities().shaderFormat==RenderShaderFormat::SPIRV;
    const auto binaryFormat=p.vulkan?xenos::ShaderBinaryFormat::Spirv:xenos::ShaderBinaryFormat::Dxil;
    const auto renderFormat=p.vulkan?RenderShaderFormat::SPIRV:RenderShaderFormat::DXIL;
    auto vs=xenos::CompileCachedHlsl(source,"vertex","vs_6_0",binaryFormat),ps=xenos::CompileCachedHlsl(source,"pixel","ps_6_0",binaryFormat),displayPs=xenos::CompileCachedHlsl(source,"displayPixel","ps_6_0",binaryFormat);
    if(!vs.ok||!ps.ok||!displayPs.ok) { p.error=vs.errors+ps.errors+displayPs.errors;return false; }
    p.vs=device->createShader(vs.bytecode.data(),vs.bytecode.size(),"vertex",renderFormat);
    p.ps=device->createShader(ps.bytecode.data(),ps.bytecode.size(),"pixel",renderFormat);
    p.displayPs=device->createShader(displayPs.bytecode.data(),displayPs.bytecode.size(),"displayPixel",renderFormat);
    RenderDescriptorSetBuilder set;DefineSet(set,p.vulkan);
    RenderPipelineLayoutBuilder layout;layout.begin(false,false);
    if(!p.vulkan) layout.addPushConstant(0,0,sizeof(Constants),RenderShaderStageFlag::PIXEL);layout.addDescriptorSet(set);layout.end();
    p.layout=layout.create(device);
    RenderSamplerDesc sampler;sampler.addressU=sampler.addressV=sampler.addressW=RenderTextureAddressMode::CLAMP;
    p.sampler=device->createSampler(sampler);
    if(!p.vs||!p.ps||!p.displayPs||!p.layout||!p.sampler) {p.error="Temporal shader/layout/sampler creation failed";return false;}
    return p.Pipeline(false,TemporalColorStorage::Default)&&p.Pipeline(true,TemporalColorStorage::Default);
}
bool TemporalAA::Resolve(RenderCommandList* commands,const TemporalAAInputs& in)
{
    auto& p=*impl;p.error.clear();
    auto fail=[&](const char* error){p.error=error;return false;};
    if(!commands||!in.currentColor||!in.output||!in.width||!in.height||in.width>16384||in.height>16384)
        return fail("Invalid command list, initialization, color/output, or extent");
    auto* pipeline=p.Pipeline(false,in.outputStorage);if(!pipeline)return false;
    for(auto texture:{in.currentColor,in.currentDepth,in.historyColor,in.historyDepth,in.reactiveMask,in.motionVector,in.motionDepths})
        if(texture && texture==in.output)return fail("Output must not alias an input");
    if(!std::isfinite(in.historyWeight)||in.historyWeight<0||in.historyWeight>.95f
       ||!std::isfinite(in.depthAbsoluteThreshold)||in.depthAbsoluteThreshold<0||in.depthAbsoluteThreshold>1
       ||!std::isfinite(in.depthRelativeThreshold)||in.depthRelativeThreshold<0||in.depthRelativeThreshold>1)
        return fail("Invalid temporal weight/depth policy");
    if(!std::isfinite(in.stationaryHistoryWeight)||in.stationaryHistoryWeight<0||in.stationaryHistoryWeight>.995f
       ||!std::isfinite(in.stationaryMotionMin)||in.stationaryMotionMin<0
       ||!std::isfinite(in.stationaryMotionMax)||in.stationaryMotionMax<=in.stationaryMotionMin||in.stationaryMotionMax>16)
        return fail("Invalid stationary temporal policy");
    Constants c;c.size[0]=float(in.width);c.size[1]=float(in.height);
    c.pad0=in.stableGrid?1u:0u;
    c.pad1=(in.rejectOutOfNeighborhoodHistory?1u:0u)|(in.diagnosticAcceptance?2u:0u)|(in.motionVectorDebug?4u:0u)|(in.stabilizeStationaryGeometry?8u:0u)|(in.diagnosticRejectionReasons?16u:0u)|(in.snapStationaryMotion?32u:0u)|(in.stationaryColorClip?64u:0u)|(in.stationaryMultiSurface?128u:0u)|(in.movingBilinearFallback?256u:0u);
    if(in.stableGrid)for(double j:{in.currentJitterX,in.currentJitterY,in.previousJitterX,in.previousJitterY})
        if(!std::isfinite(j)||std::abs(j)>=.5)return fail("Stable coverage mode requires jitter strictly within half a raster pixel");
    c.jitter[0]=float(in.currentJitterX);c.jitter[1]=float(in.currentJitterY);c.jitter[2]=float(in.previousJitterX);c.jitter[3]=float(in.previousJitterY);
    c.policy[0]=in.historyWeight;c.policy[1]=in.depthAbsoluteThreshold;c.policy[2]=in.depthRelativeThreshold;
    c.stationaryPolicy[0]=in.stationaryHistoryWeight;c.stationaryPolicy[1]=in.stationaryMotionMin;
    c.stationaryPolicy[2]=in.stationaryMotionMax;c.stationaryPolicy[3]=in.stationaryCoverage?1.f:0.f;
    const bool active=in.historyValid&&!in.rejectAllHistory&&in.historyWeight>0;
    if(active)
    {
        if(!in.currentDepth||!in.historyColor||!in.historyDepth||!in.currentCamera||!in.previousCamera
           ||!in.historyWidth||!in.historyHeight||in.historyWidth>16384||in.historyHeight>16384
           ||!FullViewport(*in.currentCamera,in.width,in.height)||!FullViewport(*in.previousCamera,in.historyWidth,in.historyHeight))
            return fail("History requires valid full-extent cameras and color/depth inputs");
        for(double jitter:{in.currentJitterX,in.currentJitterY,in.previousJitterX,in.previousJitterY})
            if(!std::isfinite(jitter)||std::abs(jitter)>16)return fail("Jitter must be finite raster displacement within 16 pixels");
        const auto& current=in.currentCamera->Raster();const auto& previous=in.previousCamera->Raster();
        temporal::Matrix rasterToNdc{};rasterToNdc[0]=2/current.width;rasterToNdc[5]=-2/(current.height*current.ndcYSign);rasterToNdc[10]=1;rasterToNdc[15]=1;
        rasterToNdc[12]=-1-current.halfPixelNdcX-2*in.currentJitterX/current.width;
        rasterToNdc[13]=(1-current.halfPixelNdcY+2*in.currentJitterY/current.height)/current.ndcYSign;
        const auto combined=Multiply(rasterToNdc,Multiply(in.currentCamera->InverseVP(),in.previousCamera->VP()));
        for(int i=0;i<16;++i) {c.transform[i]=float(combined[i]);if(!std::isfinite(c.transform[i]))return fail("Combined reprojection matrix is not finite float32");}
        c.previousScaleBias[0]=float(previous.width*.5);c.previousScaleBias[1]=float(-previous.height*.5*previous.ndcYSign);
        c.previousScaleBias[2]=float(previous.width*.5*(1+previous.halfPixelNdcX)+in.previousJitterX);
        c.previousScaleBias[3]=float(previous.height*.5*(1-previous.halfPixelNdcY)+in.previousJitterY);
        c.size[2]=float(in.historyWidth);c.size[3]=float(in.historyHeight);c.active=1;c.reactive=in.reactiveMask?1u:0u;
        // Enable motion vector sampling in shader if caller provides motionVector texture AND marks it valid
        if (in.motionVector && in.motionDepths && in.reactiveMask && in.motionVectorValid) {
            c.pad0 |= 4u;
        }
    }
    Impl::Pending pending;RenderDescriptorSetBuilder set;DefineSet(set,p.vulkan);pending.set=set.create(p.device);
    const RenderTexture* attachments[]={in.output};pending.framebuffer=p.device->createFramebuffer(RenderFramebufferDesc(attachments,1));
    if(!pending.set||!pending.framebuffer)return fail("Temporal descriptor/framebuffer allocation failed");
    // Inactive shader returns before accessing fallback descriptors.
    std::array<RenderTexture*,7> inputs={in.currentColor,active?in.currentDepth:in.currentColor,active?in.historyColor:in.currentColor,active?in.historyDepth:in.currentColor,active&&in.reactiveMask?in.reactiveMask:in.currentColor,active&&in.motionVector?in.motionVector:in.currentColor,active&&in.motionDepths?in.motionDepths:in.currentColor};
    for(uint32_t i=0;i<7;++i)pending.set->setTexture(i,inputs[i],RenderTextureLayout::SHADER_READ);
    pending.set->setSampler(7,p.sampler.get());
    if(p.vulkan) {
        pending.constants=p.device->createBuffer(RenderBufferDesc::UploadBuffer(sizeof(Constants),RenderBufferFlag::CONSTANT));
        if(!pending.constants){p.error="Temporal constants allocation failed";return false;}
        auto* mapped=pending.constants->map();if(!mapped){p.error="Temporal constants map failed";return false;}memcpy(mapped,&c,sizeof(c));pending.constants->unmap();
        pending.set->setBuffer(8,pending.constants.get(),sizeof(c));
    }

    pending.serial=p.recordedSerial+1;p.pending.push_back(std::move(pending));++p.recordedSerial;auto& resources=p.pending.back();
    const bool timed=p.resolveTimer.Begin(p.device,commands);
    commands->setFramebuffer(resources.framebuffer.get());RenderViewport viewport(0,0,float(in.width),float(in.height));RenderRect scissor(0,0,in.width,in.height);
    commands->setViewports(&viewport,1);commands->setScissors(&scissor,1);
    commands->setGraphicsPipelineLayout(p.layout.get());commands->setPipeline(pipeline);
    if(!p.vulkan) commands->setGraphicsPushConstants(0,&c);commands->setGraphicsDescriptorSet(resources.set.get(),0);commands->drawInstanced(3,1,0,0);
    if(timed)p.resolveTimer.End(commands,p.recordedSerial);
    return true;
}
}
// Deliberately separate from Resolve: only the caller can transition output from
// COLOR_WRITE to SHADER_READ at the correct point and preserve history ownership.
namespace gpu {
bool TemporalAA::ReconstructDisplay(RenderCommandList* commands,const TemporalDisplayInputs& in)
{
    auto& p=*impl;p.error.clear();
    if(!commands||!in.jitteredColor||!in.output||in.jitteredColor==in.output
       ||!in.width||!in.height||in.width>16384||in.height>16384
       ||!std::isfinite(in.jitterX)||!std::isfinite(in.jitterY)||std::abs(in.jitterX)>16||std::abs(in.jitterY)>16)
    {p.error="Invalid display reconstruction inputs, extent, alias, or jitter";return false;}
    auto* pipeline=p.Pipeline(true,in.outputStorage);if(!pipeline)return false;
    Constants c;c.size[0]=float(in.width);c.size[1]=float(in.height);c.policy[0]=float(in.jitterX);c.policy[1]=float(in.jitterY);
    Impl::Pending pending;RenderDescriptorSetBuilder set;DefineSet(set,p.vulkan);pending.set=set.create(p.device);
    const RenderTexture* attachments[]={in.output};pending.framebuffer=p.device->createFramebuffer(RenderFramebufferDesc(attachments,1));
    if(!pending.set||!pending.framebuffer){p.error="Display descriptor/framebuffer allocation failed";return false;}
    for(uint32_t i=0;i<7;++i)pending.set->setTexture(i,in.jitteredColor,RenderTextureLayout::SHADER_READ);
    pending.set->setSampler(7,p.sampler.get());
    if(p.vulkan) {
        pending.constants=p.device->createBuffer(RenderBufferDesc::UploadBuffer(sizeof(Constants),RenderBufferFlag::CONSTANT));
        if(!pending.constants){p.error="Temporal constants allocation failed";return false;}
        auto* mapped=pending.constants->map();if(!mapped){p.error="Temporal constants map failed";return false;}memcpy(mapped,&c,sizeof(c));pending.constants->unmap();
        pending.set->setBuffer(8,pending.constants.get(),sizeof(c));
    }
    pending.serial=p.recordedSerial+1;p.pending.push_back(std::move(pending));++p.recordedSerial;auto& resources=p.pending.back();
    const bool timed=p.displayTimer.Begin(p.device,commands);
    commands->setFramebuffer(resources.framebuffer.get());RenderViewport viewport(0,0,float(in.width),float(in.height));RenderRect scissor(0,0,in.width,in.height);
    commands->setViewports(&viewport,1);commands->setScissors(&scissor,1);
    commands->setGraphicsPipelineLayout(p.layout.get());commands->setPipeline(pipeline);
    if(!p.vulkan) commands->setGraphicsPushConstants(0,&c);commands->setGraphicsDescriptorSet(resources.set.get(),0);commands->drawInstanced(3,1,0,0);
    if(timed)p.displayTimer.End(commands,p.recordedSerial);
    return true;
}
}
#else
namespace gpu {
struct TemporalAA::Impl {std::string error="Temporal AA requires plume";temporal::GpuPassTimingStats timingStats;};
void TemporalAA::EnableGpuTiming(bool){}
const temporal::GpuPassTimingStats& TemporalAA::ResolveTiming()const{return impl->timingStats;}
const temporal::GpuPassTimingStats& TemporalAA::DisplayTiming()const{return impl->timingStats;}
TemporalAA::TemporalAA():impl(std::make_unique<Impl>()){} TemporalAA::~TemporalAA()=default;
bool TemporalAA::Init(plume::RenderDevice*){return false;}
bool TemporalAA::Init(plume::RenderDevice*,bool){return false;}
bool TemporalAA::Resolve(plume::RenderCommandList*,const TemporalAAInputs&){return false;}
bool TemporalAA::ReconstructDisplay(plume::RenderCommandList*,const TemporalDisplayInputs&){return false;}
void TemporalAA::ReleaseCompleted(){}
uint64_t TemporalAA::RecordedSerial()const{return 0;}
void TemporalAA::RecordExternalUse(){}
void TemporalAA::ReleaseCompletedThrough(uint64_t){}
const std::string& TemporalAA::LastError()const{return impl->error;}
}
#endif
