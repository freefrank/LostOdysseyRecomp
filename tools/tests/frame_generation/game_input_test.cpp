#include "LostOdysseyRecomp/gpu/frame_generation_composite.h"
#include "LostOdysseyRecomp/gpu/temporal_lifecycle.h"
#include "shared/frame_generation/environment.h"
#include <cstdio>
#include <cstdlib>
using namespace gpu;
unsigned checks=0;
void Check(bool value) { ++checks; if (!value) { std::fprintf(stderr,"game FG check %u failed\n",checks); std::exit(1); } }
int main() {
    auto native=frame_plan::Choose(1,1,0,2560,1440);
    native.requestedUpscaler=upscaling::Upscaler::Off;
    native.consumer=upscaling::TemporalConsumer::None;
    native.width=1280; native.height=720;
    Check(frame_generation::NativeCompositePlan(native));
    Check(!dlss_fg::CompositePlanSupported(native)); // original P4 SR contract unchanged
    Check(frame_generation::CompositePlanSupported(native));
    Check(frame_generation::CompositeSourceExtent(native).width==1280);
    Check(frame_generation::CompositeResolveGeometry{{1280,736},{1280,720},0,0,1280,720}.Matches(native));
    Check(!frame_generation::CompositeResolveGeometry{{1280,736},{1280,720},1,0,1279,720}.Matches(native));
    auto bad=native; bad.requiresReadback=true; Check(!frame_generation::NativeCompositePlan(bad));
    bad=native; bad.failed=true; Check(!frame_generation::NativeCompositePlan(bad));
    bad=native; bad.inputProbe=true; Check(!frame_generation::NativeCompositePlan(bad));
    bad=native; bad.consumer=upscaling::TemporalConsumer::LegacyTaa; Check(!frame_generation::NativeCompositePlan(bad));
    auto sr=native; sr.requestedUpscaler=upscaling::Upscaler::Dlss; sr.consumer=upscaling::TemporalConsumer::DlssSr;
    Check(frame_generation::CompositePlanSupported(sr));
    Check(frame_generation::CompositeSourceExtent(sr).width==2560);
    Check(temporal::TemporalConsumerActive(false,false,false,true));
    Check(!temporal::TemporalConsumerActive(false,false,false,false));
    Check(!temporal::JitterAfterLongInterval(false,false,-1,false));
    Check(framegen::ParseEnvironment("fsr",nullptr,nullptr,nullptr).Enabled());
    Check(framegen::ParseEnvironment("dlss","dynamic",nullptr,"144").Enabled());
    Check(!framegen::ParseEnvironment("off",nullptr,nullptr,nullptr,"1").Enabled());
    Check(!framegen::ParseEnvironment("fsr","dynamic",nullptr,nullptr).Enabled());
    std::printf("game FG input contract: %u checks passed\n",checks);
}
