#pragma once
// FSR 1 for the final pass (PresentationOptions::spatialUpscaler; the Switch's LO_SWITCH_UPSCALER):
// AMD FidelityFX Super Resolution 1.0 (MIT; ffx_fsr1.h FsrEasuF and FsrRcasF), the port SwitchWakerHD's deko3d
// present shader (present_fsh.glsl) uses, in HLSL. Presentation appends it to its own shader source, whose
// parameters, finishFrameAs() and sourcePixel() it uses; tools/switch/gen-builtin-spirv.py precompiles both
// entries for the Switch.
//
// The source is the game's display-encoded picture, so the taps are perceptual as FSR expects and need no
// gamma 2.0 round trip. Taps are integer loads clamped to imageSize, the picture's valid extent: the image may
// be padded (960x552 for 540 rows), and the clamp repeats the edge texel as a clamp-to-edge sampler would at the
// image's own edge. Twelve loads cost the same texture instructions as FSR's twelve gathers.
namespace gpu::fsr1
{
inline constexpr const char* Shader = R"(
// The RCAS sharpness, exp2(-stops), in the otherwise unused calibration rectangle (Presentation::Draw).
#define rcasSharpness calibrationRect.x
float3 fsrTexel(int x, int y) { return frame.Load(int3(x,y,0)).rgb; }
float fsrLuma(float3 c) { return c.b*0.5+(c.r*0.5+c.g); }
// ---- EASU: an edge-adaptive upscale from 12 taps around the output pixel's source position.
void easuSet(inout float2 dir, inout float len, float w, float lA, float lB, float lC, float lD, float lE) {
    // (AMD's approximate reciprocal gives a large finite number for a flat neighbourhood, 0/0 here: the epsilon
    // keeps it 0, as SwitchWakerHD does)
    float lenX=max(abs(lD-lC),abs(lC-lB)), dirX=lD-lB;
    dir.x+=dirX*w;
    lenX=saturate(abs(dirX)/(lenX+1.0e-5));
    len+=lenX*lenX*w;
    float lenY=max(abs(lE-lC),abs(lC-lA)), dirY=lE-lA;
    dir.y+=dirY*w;
    lenY=saturate(abs(dirY)/(lenY+1.0e-5));
    len+=lenY*lenY*w;
}
void easuTap(inout float3 aC, inout float aW, float2 off, float2 dir, float2 len, float lob, float clp, float3 c) {
    float2 v=float2(off.x*dir.x+off.y*dir.y,off.x*-dir.y+off.y*dir.x)*len;
    float d2=min(dot(v,v),clp);
    // Lanczos 2 without sin, rcp or sqrt
    float wB=2.0/5.0*d2-1.0, wA=lob*d2-1.0;
    wB*=wB;
    wA*=wA;
    wB=25.0/16.0*wB-(25.0/16.0-1.0);
    float w=wB*wA;
    aC+=c*w;
    aW+=w;
}
float3 easu(float2 position) {
    float2 pp=position-0.5;  // in texels, texel centres on integers
    float2 fp=floor(pp);
    pp-=fp;
    int2 f=int2(fp), last=int2(imageSize)-1;
    int x0=clamp(f.x-1,0,last.x), x1=clamp(f.x,0,last.x), x2=clamp(f.x+1,0,last.x), x3=clamp(f.x+2,0,last.x);
    int y0=clamp(f.y-1,0,last.y), y1=clamp(f.y,0,last.y), y2=clamp(f.y+1,0,last.y), y3=clamp(f.y+2,0,last.y);
    //    b c
    //  e f g h
    //  i j k l
    //    n o
    float3 bC=fsrTexel(x1,y0), cC=fsrTexel(x2,y0);
    float3 eC=fsrTexel(x0,y1), fC=fsrTexel(x1,y1), gC=fsrTexel(x2,y1), hC=fsrTexel(x3,y1);
    float3 iC=fsrTexel(x0,y2), jC=fsrTexel(x1,y2), kC=fsrTexel(x2,y2), lC=fsrTexel(x3,y2);
    float3 nC=fsrTexel(x1,y3), oC=fsrTexel(x2,y3);
    float bL=fsrLuma(bC), cL=fsrLuma(cC), eL=fsrLuma(eC), fL=fsrLuma(fC), gL=fsrLuma(gC), hL=fsrLuma(hC);
    float iL=fsrLuma(iC), jL=fsrLuma(jC), kL=fsrLuma(kC), lL=fsrLuma(lC), nL=fsrLuma(nC), oL=fsrLuma(oC);
    // The direction and length of the edge from the four bilinear neighbours, weighted as bilinear would.
    float2 dir=0;
    float len=0;
    easuSet(dir,len,(1.0-pp.x)*(1.0-pp.y),bL,eL,fL,gL,jL);
    easuSet(dir,len,pp.x*(1.0-pp.y),cL,fL,gL,hL,kL);
    easuSet(dir,len,(1.0-pp.x)*pp.y,fL,iL,jL,kL,nL);
    easuSet(dir,len,pp.x*pp.y,gL,jL,kL,lL,oL);
    float dirR=dot(dir,dir);
    bool zro=dirR<1.0/32768.0;
    dirR=zro ? 1.0 : rsqrt(dirR);
    dir.x=zro ? 1.0 : dir.x;
    dir*=dirR;
    len=len*0.5;
    len*=len;
    // The kernel stretches along the edge (to sqrt(2) on a diagonal) and its window widens with the edge.
    float stretch=dot(dir,dir)/max(abs(dir.x),abs(dir.y));
    float2 len2=float2(1.0+(stretch-1.0)*len,1.0-0.5*len);
    float lob=0.5+((1.0/4.0-0.04)-0.5)*len, clp=1.0/lob;
    float3 aC=0;
    float aW=0;
    easuTap(aC,aW,float2(0.0,-1.0)-pp,dir,len2,lob,clp,bC);
    easuTap(aC,aW,float2(1.0,-1.0)-pp,dir,len2,lob,clp,cC);
    easuTap(aC,aW,float2(-1.0,1.0)-pp,dir,len2,lob,clp,iC);
    easuTap(aC,aW,float2(0.0,1.0)-pp,dir,len2,lob,clp,jC);
    easuTap(aC,aW,float2(0.0,0.0)-pp,dir,len2,lob,clp,fC);
    easuTap(aC,aW,float2(-1.0,0.0)-pp,dir,len2,lob,clp,eC);
    easuTap(aC,aW,float2(1.0,1.0)-pp,dir,len2,lob,clp,kC);
    easuTap(aC,aW,float2(2.0,1.0)-pp,dir,len2,lob,clp,lC);
    easuTap(aC,aW,float2(2.0,0.0)-pp,dir,len2,lob,clp,hC);
    easuTap(aC,aW,float2(1.0,0.0)-pp,dir,len2,lob,clp,gC);
    easuTap(aC,aW,float2(1.0,2.0)-pp,dir,len2,lob,clp,oC);
    easuTap(aC,aW,float2(0.0,2.0)-pp,dir,len2,lob,clp,nC);
    // Within the range of the 2x2 around the position: no ringing
    float3 min4=min(min(fC,gC),min(jC,kC)), max4=max(max(fC,gC),max(jC,kC));
    return min(max4,max(min4,aC/aW));
}
// ---- RCAS: contrast-adaptive sharpening of a texel from its four neighbours.
float3 rcas(int2 p) {
    int2 last=int2(imageSize)-1;
    p=min(p,last);
    float3 b=fsrTexel(p.x,max(p.y-1,0)), d=fsrTexel(max(p.x-1,0),p.y), e=fsrTexel(p.x,p.y);
    float3 f=fsrTexel(min(p.x+1,last.x),p.y), h=fsrTexel(p.x,min(p.y+1,last.y));
    float3 mn4=min(min(b,d),min(f,h)), mx4=max(max(b,d),max(f,h));
    // (the epsilons keep black and white neighbourhoods from dividing by zero)
    float3 hitMin=min(mn4,e)/(4.0*mx4+1.0e-5);
    float3 hitMax=(1.0-max(mx4,e))/(4.0*mn4-4.0-1.0e-5);
    float3 lobeC=max(-hitMin,hitMax);
    float lobe=max(-(0.25-1.0/16.0),min(max(lobeC.r,max(lobeC.g,lobeC.b)),0.0))*rcasSharpness;
    return (lobe*(b+d+f+h)+e)/(4.0*lobe+1.0);
}
// EASU into the output viewport: the final pass (easu), or FSR's first pass into an SDR intermediate of the
// viewport's size (fsr), where finishFrameAs has nothing to do (no final-pass flags).
float4 easuPixel(float4 position : SV_Position) : SV_Target {
    return finishFrameAs(easu(sourcePixel(position.xy)),true);
}
// RCAS of the texel under the output pixel, at a 1:1 mapping: FSR's second pass, the final one (fsr), or
// sharpening at the scene's size before the bilinear final pass (rcas).
float4 rcasPixel(float4 position : SV_Position) : SV_Target {
    return finishFrameAs(rcas(int2(sourcePixel(position.xy))),true);
}
)";
} // namespace gpu::fsr1
