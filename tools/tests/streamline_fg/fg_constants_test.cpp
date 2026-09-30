#include <gpu/dlss_fg_constants.h>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

using gpu::temporal::Matrix;
using gpu::temporal::Vector;
void Check(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
void Close(double actual, double expected, double tolerance, const char* message) {
    Check(std::abs(actual-expected) <= tolerance,message);
}
Matrix SlMatrix(const sl::float4x4& m) {
    Matrix result{};
    for (unsigned i=0;i<4;++i) {
        const auto& r=m[i]; result[4*i]=r.x; result[4*i+1]=r.y;
        result[4*i+2]=r.z; result[4*i+3]=r.w;
    }
    return result;
}
Matrix RawVP(double yaw, const gpu::temporal::Vector& eye) {
    const double c=std::cos(yaw), s=std::sin(yaw);
    // Right=(c,0,-s), up=(0,1,0), forward=(s,0,c).
    Matrix view={c,0,s,0, 0,1,0,0, -s,0,c,0,
        -(eye[0]*c-eye[2]*s),-eye[1],-(eye[0]*s+eye[2]*c),1};
    const double fx=1.25, fy=fx*16.0/9.0, a=.999, q=a*10;
    Matrix projection={fx,0,0,0, 0,fy,0,0, 0,0,a,1, 0,0,-q,0};
    return gpu::dlss_fg::detail::Multiply(view,projection);
}
gpu::temporal::TemporalFrameInputs Inputs(const Matrix& vp) {
    gpu::temporal::TemporalFrameInputs in{};
    in.cameraValid=true; in.cameraViewProjection=vp;
    in.cameraRaster={0,0,1280,720,-1,1.0/1280,-1.0/720};
    in.depthConvention=gpu::temporal::DepthConvention::Reversed;
    in.depth={reinterpret_cast<plume::RenderTexture*>(uintptr_t(1)),{1280,736},0,0,1280,720};
    in.motion={reinterpret_cast<plume::RenderTexture*>(uintptr_t(2)),{1280,736},0,0,1280,720};
    in.jitter.pixelX=.25; in.jitter.pixelY=-.125;
    return in;
}
Matrix Decode(std::array<uint32_t,16> bits) {
    Matrix m{}; for(size_t i=0;i<16;++i)m[i]=std::bit_cast<float>(bits[i]); return m;
}
int main() {
    const Vector eye{100,20,30,1}, previousEye{102,21,28,1};
    const auto current=Inputs(RawVP(.31,eye));
    const auto previous=RawVP(.37,previousEye);
    sl::Constants c{}; gpu::dlss_fg::DepthRemap d{};
    Check(gpu::dlss_fg::BuildConstants(current,&previous,c,&d,&current.cameraRaster),"moving camera accepted");
    Close(c.cameraPos.x,eye[0],1e-4,"camera x");
    Close(c.cameraPos.y,eye[1],1e-4,"camera y");
    Close(c.cameraPos.z,eye[2],1e-4,"camera z");
    Close(c.cameraNear,10,1e-4,"near");
    Check(c.cameraFar>1e6 && c.cameraFar<sl::INVALID_FLOAT,"finite canonical far");
    Check(c.depthInverted==sl::Boolean::eTrue,"reversed depth declaration");
    Check(c.reset==sl::Boolean::eFalse,"history attached");
    Close(c.mvecScale.x,1.0/1280,1e-9,"motion x pixels");
    Close(c.mvecScale.y,1.0/720,1e-9,"motion y pixels");
    Close(c.jitterOffset.x,.25,1e-8,"jitter x separate");
    Close(c.jitterOffset.y,-.125,1e-8,"jitter y separate");
    const auto projection=SlMatrix(c.cameraViewToClip), inverse=SlMatrix(c.clipToCameraView);
    const auto identity=gpu::dlss_fg::detail::Multiply(projection,inverse);
    for(unsigned i=0;i<16;++i) Close(identity[i],i%5==0?1:0,1e-5,"projection inverse");
    const Vector point{110,22,120,1};
    const auto cur=gpu::dlss_fg::detail::Decompose(current.cameraViewProjection,current.cameraRaster);
    const auto prev=gpu::dlss_fg::detail::Decompose(previous,current.cameraRaster);
    Check(bool(cur)&&bool(prev),"both camera decompositions");
    const auto currentClip=gpu::temporal::Transform(point,cur->hostVP);
    const auto actualPrevious=gpu::temporal::Transform(currentClip,SlMatrix(c.clipToPrevClip));
    const auto expectedPrevious=gpu::temporal::Transform(point,prev->hostVP);
    for(unsigned i=0;i<4;++i) Close(actualPrevious[i],expectedPrevious[i],.002,"current-to-previous clip");
    const auto back=gpu::temporal::Transform(actualPrevious,SlMatrix(c.prevClipToClip));
    for(unsigned i=0;i<4;++i) Close(back[i],currentClip[i],.002,"previous-to-current clip");
    const auto host=gpu::temporal::Transform(point,cur->hostVP);
    const auto guest=gpu::temporal::Transform(point,current.cameraViewProjection);
    Close(host[0]/host[3],guest[0]/guest[3]+current.cameraRaster.halfPixelNdcX,1e-6,"host x transform");
    Close(host[1]/host[3],-guest[1]/guest[3]+current.cameraRaster.halfPixelNdcY,1e-6,"host y transform");
    for (double z : {10.0,100.0,1000.0}) {
        const double raw=.001+.999*10/z;
        const double normalized=raw*d.scale+d.bias;
        const double expected=(d.nearDistance*d.farDistance/(d.farDistance-d.nearDistance))/z-
            d.nearDistance/(d.farDistance-d.nearDistance);
        Close(normalized,expected,1e-6,"finite depth reparameterization");
    }
    const Matrix map=Decode({984490141,1048933738,1065276239,1065292956,1071494100,3109135293,3125290108,3125303238,0,1078221940,3182244301,3182255663,1115486952,3279883186,1157202350,1157300638});
    const Matrix battle=Decode({1065421082,1036507293,1061409510,1061422356,1067057496,3181867434,3206812392,3206823155,0,1077045441,3174739206,3174751452,1127137590,3281230865,1146691008,1146869091});
    for (const auto& m : {map,battle}) {
        auto input=Inputs(m); input.cameraRaster.halfPixelNdcX=input.cameraRaster.halfPixelNdcY=0;
        Check(gpu::dlss_fg::BuildConstants(input,nullptr,c,&d),"captured guest VP accepted");
        Close(c.cameraNear,10,.002,"captured near");
        Check(c.reset==sl::Boolean::eTrue,"reset without previous");
    }
    auto invalid=current; invalid.cameraViewProjection[0]=std::numeric_limits<double>::quiet_NaN();
    Check(!gpu::dlss_fg::BuildConstants(invalid,nullptr,c,&d),"NaN rejected");
    invalid=current; invalid.cameraViewProjection[3]=invalid.cameraViewProjection[7]=invalid.cameraViewProjection[11]=0;
    Check(!gpu::dlss_fg::BuildConstants(invalid,nullptr,c,&d),"nonprojective rejected");
    invalid=current; invalid.cameraViewProjection[2]+=.1;
    Check(!gpu::dlss_fg::BuildConstants(invalid,nullptr,c,&d),"oblique rejected");
    invalid=current; invalid.cameraRaster.x=1;
    Check(!gpu::dlss_fg::BuildConstants(invalid,nullptr,c,&d),"partial raster rejected");
    std::puts("FG constants math: passed");
}
