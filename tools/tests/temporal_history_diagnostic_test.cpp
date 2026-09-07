#include <gpu/temporal_history.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>

using namespace gpu::temporal;
namespace {
unsigned checks=0;
void Require(bool value,const char* message) {
    ++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
void Near(double actual,double expected,const char* message) {
    Require(std::isfinite(actual)&&std::abs(actual-expected)<1e-9,message);
}
void CheckContinuity(const Camera& current,const Camera& previous,bool expected,const char* message) {
    Require(ContinuousHistoryCamera(current,previous)==expected,message);
    const HistoryReuseState valid{true,true,true,true,true,18,17,3,3,42,42};
    const auto report=InspectHistoryReuse(valid,&current,&previous);
    Require(!(report.rejected&uint32_t(HistoryReuseRejection::CameraDiscontinuity))==expected,
        "diagnosis agrees with the production camera guard");
}
void CameraDepthRangeTests() {
    const Viewport raster{0,0,1280,720,1,1./1280,-1./720};
    const Matrix identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    const Matrix finitePerspective{1,0,0,0, 0,1,0,0, 0,0,100./99,1, 0,0,-100./99,0};
    const Matrix infinitePerspective{1,0,0,0, 0,1,0,0, 0,0,1,1, 0,0,-1,0};
    const std::array<double,3> originalDepths{.001,.01,.1};
    for(const auto& vp:{identity,finitePerspective,infinitePerspective}) {
        const auto camera=Camera::Create(vp,raster);Require(bool(camera),"ordinary analytic camera is valid");
        const auto range=SelectHistoryContinuityDepths(*camera);
        Require(range.valid&&range.lowerBound==0&&range.depths==originalDepths,"ordinary finite/infinite/orthographic projection retains exact old depths");
        CheckContinuity(*camera,*camera,true,"ordinary unchanged camera remains continuous");
    }
    // Analytic projection with W(d)=d-.25. An arbitrary replacement of .001
    // with .003 would still be invalid, whereas interval selection is general.
    auto pole=infinitePerspective;pole[10]=.75;
    const auto poleCamera=Camera::Create(pole,raster);Require(bool(poleCamera),"analytic positive-W subinterval exists");
    const auto range=SelectHistoryContinuityDepths(*poleCamera);
    Require(range.valid,"pole projection has valid near endpoint");Near(range.lowerBound,.25,"analytic infinity endpoint");
    Near(range.depths[0],.25075,"first depth mapped into valid interval");
    Near(range.depths[1],.2575,"second depth mapped into valid interval");
    Near(range.depths[2],.325,"third depth mapped into valid interval");
    CheckContinuity(*poleCamera,*poleCamera,true,"valid subinterval supports static camera");
    for(double nearW:{0.,-1.,1e-13}) {
        auto invalid=infinitePerspective;invalid[10]=nearW;
        const auto camera=Camera::Create(invalid,raster);Require(bool(camera),"invalid near endpoint can have invertible VP");
        Require(!SelectHistoryContinuityDepths(*camera).valid,"zero/negative/unsafe near W rejects interval");
        CheckContinuity(*camera,*camera,false,"invalid near endpoint never authorizes reuse");
    }
    auto nearBehind=infinitePerspective;nearBehind[10]=.5;nearBehind[14]=1;
    const auto behindCamera=Camera::Create(nearBehind,raster);
    Require(bool(behindCamera)&&!SelectHistoryContinuityDepths(*behindCamera).valid,"positive far W cannot rescue invalid near endpoint");
    auto invalid=identity;invalid[0]=std::numeric_limits<double>::quiet_NaN();
    Require(!Camera::Create(invalid,raster),"nonfinite camera input remains rejected");
    Require(!Camera::Create(Matrix{},raster),"singular camera input remains rejected");

    // Nine significant digits from EXE 57ea3c3d, taa-gates-auto4k, reproduce
    // the exact guest float32 bits (float literals are promoted to Matrix doubles).
    // f1300 is bit-identical to f1299, but the old .001 probe is behind infinity.
    const Matrix stationary{
        -.155554906f,.388243675f,.986948192f,.987936139f,
        1.72504044f,.0350097343f,.0889977068f,.0890867934f,
        0,3.05441737f,-.126471624f,-.126598224f,
        -1371.8844f,-810.837891f,-704.081543f,-694.786316f};
    // f1229 <- f1228: continuous camera motion whose original first probe fails.
    const Matrix moving{
        -.0873113498f,.361231178f,.990816474f,.991808295f,
        1.72984266f,.0182326306f,.0500100553f,.0500601158f,
        0,3.05787635f,-.117345557f,-.117463015f,
        -1521.30579f,-674.316223f,-337.877258f,-328.215485f};
    const Matrix before{
        -.0871447176f,.361525714f,.990810096f,.991801858f,
        1.72985101f,.0182125829f,.0499140471f,.0499640107f,
        0,3.05784178f,-.117440663f,-.117558219f,
        -1523.58142f,-669.325867f,-326.057648f,-316.384033f};
    for(double scale:{1.,1.5,3.}) {
        auto scaled=raster;scaled.width*=scale;scaled.height*=scale;
        scaled.halfPixelNdcX=double(1.f/1280);scaled.halfPixelNdcY=double(-1.f/720);
        const auto still=Camera::Create(stationary,scaled),current=Camera::Create(moving,scaled),previous=Camera::Create(before,scaled);
        Require(still&&current&&previous,"actual float32 camera matrices are valid at 720p/1080p/4K");
        Require(Reproject({scaled.width*.5,scaled.height*.5,.001},*still,*still).rejection==Rejection::InvalidWorldW,"actual static matrix reproduces original invalid .001 sample");
        Require(Reproject({scaled.width*.5,scaled.height*.5,.001},*current,*previous).rejection==Rejection::InvalidWorldW,"actual continuous matrix reproduces original invalid .001 sample");
        CheckContinuity(*still,*still,true,"actual stationary camera remains continuous at every resolution");
        CheckContinuity(*current,*previous,true,"actual moving camera remains continuous at every resolution");
        Near(SelectHistoryContinuityDepths(*still).lowerBound,.001000010958135234,"actual static projection infinity endpoint");
        // Post-projection X offset is analytically 30% of screen width. It
        // preserves depth/W and stays in bounds, isolating the cut threshold.
        auto cut=stationary;for(unsigned row=0;row<4;++row)cut[row*4]+=.6*cut[row*4+3];
        const auto cutCamera=Camera::Create(cut,scaled);Require(bool(cutCamera),"real-matrix cut camera remains valid");
        CheckContinuity(*still,*cutCamera,false,"real-matrix quarter-screen cut remains rejected at every resolution");
        // Negating homogeneous previous VP preserves NDC but violates positive
        // previous W. Depth-range selection cannot waive the previous-W guard.
        auto negative=stationary;for(double& value:negative)value=-value;
        const auto negativeCamera=Camera::Create(negative,scaled);Require(bool(negativeCamera),"negative-W previous camera matrix is invertible");
        CheckContinuity(*still,*negativeCamera,false,"invalid previous W remains rejected");
        Require(Reproject({scaled.width*.5,scaled.height*.5,.01},*still,*negativeCamera).rejection==Rejection::InvalidPreviousW,"negative control reaches the previous-W guard");
    }
}
void ReplayCameraPairs(const char* path) {
    // Optional retained CPU manifest: row count, then per row frame, old mask,
    // x/y/w/h/ndcY/halfX/halfY and 16 current + 16 previous VP coefficients.
    std::ifstream input(path);unsigned count=0,oldRejected=0;
    Require(bool(input>>count)&&count>0,"CPU camera-pair manifest has a row count");
    for(unsigned row=0;row<count;++row) {
        unsigned frame=0,mask=0;Viewport raster{};Matrix currentVP{},previousVP{};
        input>>frame>>mask>>raster.x>>raster.y>>raster.width>>raster.height>>raster.ndcYSign>>raster.halfPixelNdcX>>raster.halfPixelNdcY;
        for(double& value:currentVP)input>>value;
        for(double& value:previousVP)input>>value;
        Require(bool(input)&&frame>0,"CPU camera-pair row is complete");
        const auto current=Camera::Create(currentVP,raster),previous=Camera::Create(previousVP,raster);
        Require(current&&previous,"retained camera pair is valid");
        const auto oldProbe=Reproject({raster.width*.5,raster.height*.5,.001},*current,*previous);
        Require(mask==0||mask==256,"retained evidence isolates camera discontinuity");
        Require((oldProbe.rejection==Rejection::InvalidWorldW)==(mask==256),"original invalid-world-W reproduces the logged refusal");
        oldRejected+=mask==256;
        CheckContinuity(*current,*previous,true,"retained camera pair passes corrected production continuity guard");
    }
    input>>std::ws;Require(input.eof(),"CPU camera-pair manifest has no trailing rows");
    std::printf("camera replay: %u pairs passed; %u original InvalidWorldW refusals reproduced\n",count,oldRejected);
}
}
int main(int argc,char** argv) {
    const Matrix identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    const Viewport raster{0,0,1280,720,1,1./1280,-1./720};
    const auto camera=Camera::Create(identity,raster);
    Require(bool(camera),"analytic reference camera is valid");
    const HistoryReuseState valid{true,true,true,true,true,18,17,3,3,42,42};
    auto inspect=[&](const HistoryReuseState& state){return InspectHistoryReuse(state,&*camera,&*camera);};
    const auto good=inspect(valid);
    Require(good.captured&&good.cameraChecksAvailable&&good.rejected==0,"unchanged consecutive history has no rejection");
    for(const auto& probe:good.probes) {
        Require(probe.rejection==Rejection::None&&probe.projectedValid&&!probe.quarterScreenRejected,"identity probes remain valid");
        Near(probe.projected.x,640,"identity probe x");Near(probe.projected.y,360,"identity probe y");
        Near(probe.projected.depth,probe.depth,"identity probe retains independent depth");
    }
    // Each supplied history state differs from the valid reference in one fact.
    // Require exact masks, so an unrelated rejection or an omitted gate fails.
    auto state=valid;state.valid=false;Require(inspect(state).rejected==1,"invalid history is independently diagnosed");
    state=valid;state.previousCompleted=false;Require(inspect(state).rejected==2,"previous completion is independently diagnosed");
    state=valid;state.previousStable=false;Require(inspect(state).rejected==4,"grid mode is independently diagnosed");
    state=valid;state.previousFrame=16;Require(inspect(state).rejected==8,"frame gap is independently diagnosed");
    state=valid;state.previousEpoch=2;Require(inspect(state).rejected==16,"epoch is independently diagnosed");
    const auto missing=InspectHistoryReuse(valid,&*camera,nullptr);
    Require(missing.rejected==32&&!missing.cameraChecksAvailable&&!missing.previousCamera,"missing camera is not misreported as projection failure");
    state=valid;state.previousAllocation=41;Require(inspect(state).rejected==64,"allocation identity is independently diagnosed");
    auto shiftedRaster=raster;shiftedRaster.halfPixelNdcX+=.001;
    const auto rasterCamera=Camera::Create(identity,shiftedRaster);
    Require(InspectHistoryReuse(valid,&*camera,&*rasterCamera).rejected==128,"half-pixel raster change is diagnosed separately from continuity");
    state=valid;state.allowHistory=false;Require(inspect(state).rejected==512,"explicit history disable is independently diagnosed");
    state=valid;state.valid=false;state.previousCompleted=false;state.previousFrame=0;state.previousEpoch=0;state.previousAllocation=0;state.previousStable=false;state.allowHistory=false;
    Require(InspectHistoryReuse(state,&*camera,nullptr).rejected==(1|2|4|8|16|32|64|512),"multiple failures retain every state bit");

    // Translation is analytically 30% of screen width, independent of pixel
    // resolution. It remains in-bounds but crosses the quarter-screen guard.
    Matrix moved=identity;moved[12]=.6;
    for(double scale:{1.,1.5,3.}) {
        auto scaled=raster;scaled.width*=scale;scaled.height*=scale;
        const auto current=Camera::Create(identity,scaled),previous=Camera::Create(moved,scaled);
        const auto report=InspectHistoryReuse(valid,&*current,&*previous);
        Require(report.rejected==256,"quarter-screen gate is resolution invariant");
        for(const auto& probe:report.probes) {
            Require(probe.rejection==Rejection::None&&probe.projectedValid&&probe.quarterScreenRejected,"in-bounds excessive motion has a separate displacement result");
            Near(probe.deltaXFraction,.3,"analytic horizontal screen displacement");Near(probe.deltaYFraction,0,"analytic vertical screen displacement");
        }
    }
    // Preserve coordinates even though the strict Reproject API rejects them.
    Matrix outside=identity;outside[12]=2;
    const auto outsideCamera=Camera::Create(outside,raster);
    const auto outsideReport=InspectHistoryReuse(valid,&*camera,&*outsideCamera);
    Require(outsideReport.rejected==256,"outside projection rejects continuity");
    Require(outsideReport.probes[0].rejection==Rejection::PreviousOutside&&outsideReport.probes[0].projectedValid,"outside rejection preserves raw projection");
    Near(outsideReport.probes[0].projected.x,1920,"raw outside coordinate is not the API default zero");
    Matrix depth=identity;depth[14]=.01;
    const auto depthCamera=Camera::Create(depth,raster);
    const auto depthReport=InspectHistoryReuse(valid,&*camera,&*depthCamera);
    Require(depthReport.rejected==256&&depthReport.probes[0].rejection==Rejection::PreviousDepth,"invalid previous depth explains rejection");
    Near(depthReport.probes[0].projected.depth,-.009,"raw rejected depth is retained");
    Require(depthReport.probes[2].rejection==Rejection::None,"later depth slices are still diagnosed after an earlier failure");
    CameraDepthRangeTests();
    Require(argc<=2,"usage: temporal_history_diagnostic_test [CPU-camera-pair-manifest]");
    if(argc==2)ReplayCameraPairs(argv[1]);
    std::printf("temporal history continuity/diagnostics: %u CPU checks passed; no GPU\n",checks);
}
