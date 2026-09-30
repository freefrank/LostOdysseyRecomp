#include <gpu/fsr_projection.h>
#include <gpu/frame_plan.h>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
using gpu::temporal::Matrix;
void Check(bool value, const char* reason) { if (!value) { std::fprintf(stderr, "FAIL: %s\n",reason); std::exit(1); } }
Matrix Decode(std::array<uint32_t,16> bits) { Matrix m{}; for(size_t i=0;i<16;++i)m[i]=std::bit_cast<float>(bits[i]);return m; }
int main() {
    // Captured guest matrices: Map2 f5997 and battle f2871, before raster jitter.
    const Matrix map=Decode({984490141,1048933738,1065276239,1065292956,1071494100,3109135293,3125290108,3125303238,0,1078221940,3182244301,3182255663,1115486952,3279883186,1157202350,1157300638});
    const Matrix battle=Decode({1065421082,1036507293,1061409510,1061422356,1067057496,3181867434,3206812392,3206823155,0,1077045441,3174739206,3174751452,1127137590,3281230865,1146691008,1146869091});
    for (const auto& m : {map,battle}) {
        auto p=gpu::fsr::DeriveProjection(m,16.0/9); Check(bool(p),"real VP accepted");
        Check(std::abs(p->nearDistance-10)<0.001,"real near distance");
        Check(std::abs(p->pole-0.001)<1e-6,"real depth pole");
        Check(std::abs(p->depthScale+p->depthBias-1)<1e-6,"near maps to one");
        Check(std::abs(p->pole*p->depthScale+p->depthBias)<1e-7,"infinite depth maps to zero");
        for (double d : {10.0,100.0,1000.0}) {
            const double raw=p->pole+(1-p->pole)*p->nearDistance/d;
            Check(std::abs(raw*p->depthScale+p->depthBias-p->nearDistance/d)<1e-6,"canonical depth preserves near/distance");
        }
    }
    Check(std::abs(gpu::fsr::DeriveProjection(map,16.0/9)->verticalFovRadians*180/3.141592653589793-35.983394)<0.001,"map FOV");
    Check(std::abs(gpu::fsr::DeriveProjection(battle,16.0/9)->verticalFovRadians*180/3.141592653589793-39.430061)<0.001,"battle FOV");
    auto invalid=map;invalid[3]=invalid[7]=invalid[11]=0;Check(!gpu::fsr::DeriveProjection(invalid,16.0/9),"orthographic rejected");
    invalid=map;invalid[2]+=0.1;Check(!gpu::fsr::DeriveProjection(invalid,16.0/9),"oblique rejected");
    invalid=map;for(int i=0;i<4;++i)invalid[i*4]+=0.1*map[i*4+3];Check(!gpu::fsr::DeriveProjection(invalid,16.0/9),"off center rejected");
    invalid=map;invalid[0]=std::numeric_limits<double>::quiet_NaN();Check(!gpu::fsr::DeriveProjection(invalid,16.0/9),"NaN rejected");
    Check(!gpu::fsr::DeriveProjection(map,4.0/3),"aspect mismatch rejected");
    using namespace gpu;
    frame_plan::PlannerState planner;
    frame_plan::PlannerInput input{}; input.output={{1920,1080},0,0,1920,1080};
    input.upscaler=upscaling::Upscaler::Fsr; input.device={backend::Backend::Vulkan,4,true,false,false,true};
    upscaling::OutputSizing sizing{};sizing.key={4,1920,1080,upscaling::Upscaler::Fsr,0,0};
    sizing.modes[0]={upscaling::SizingState::Ready,{1280,720},{1280,720},{1280,720}};input.sizing=&sizing;
    auto first=planner.Begin(input);Check(first.consumer==upscaling::TemporalConsumer::FsrSr&&first.width==1280,"FSR plan activates actual sizing");
    auto second=planner.Begin(input);Check(second.geometryEpoch==first.geometryEpoch&&second.requestSignature==first.requestSignature,"same FSR identity stable");
    Check(first.requestSignature==frame_plan::InputRequestSignature(input),"FSR incoming signature matches selected sizing");
    frame_plan::UpscalerExecutionObservation skipped{};
    skipped.plan=second; skipped.renderFrame=12; skipped.actualProvider=upscaling::Upscaler::Fsr;
    skipped.reason=frame_plan::DlssEffectReason::UnsupportedProjection;
    Check(planner.ReportUpscalerExecution(skipped),"unsupported projection is a current-frame observation");
    const auto recovered=planner.Begin(input);
    Check(recovered.consumer==upscaling::TemporalConsumer::FsrSr && !planner.Observe().persistentFailure,
        "the next valid frame retains FSR after a projection-only fallback");
    input.fsrQuality=upscaling::FsrQuality::NativeAA;sizing.modes[3]={upscaling::SizingState::Ready,{1920,1080},{1920,1080},{1920,1080}};
    auto native=planner.Begin(input);Check(native.width==1920&&native.consumer==upscaling::TemporalConsumer::FsrSr&&native.requestSignature!=first.requestSignature,"Native AA distinct identity");
    input.readback=true;Check(planner.Begin(input).consumer!=upscaling::TemporalConsumer::FsrSr,"readback bypass");
    std::puts("FSR projection and planner tests passed");
}
