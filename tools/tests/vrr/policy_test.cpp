#include <gpu/vrr_policy.h>
#include <gpu/frame_pacer.h>
#include <cstdio>
#include <cstdlib>
#include <limits>
void Check(bool ok, const char* reason) {
    if (!ok) { std::fprintf(stderr,"FAIL: %s\n",reason); std::exit(1); }
}
int main() {
    namespace v = gpu::vrr;
    for (auto rate : gpu::frame_rate::kNativeRates) {
        for (auto hz : {0u,60u,90u,120u,144u,165u,240u}) {
            Check(v::PacingTarget(rate,false,hz,4)==rate,"off preserves configured rate");
            for (auto vsync : {false,true})
                Check(v::HostVsyncEnabled(rate,vsync,true,false,false)==gpu::frame_rate::HostVsyncEnabled(rate,vsync,true),"off follows the VSync setting");
            Check(!v::HostVsyncEnabled(rate,true,true,false,true),"VRR asks for asynchronous presentation");
            for (auto multiplier : {1u,2u,3u,4u,16u}) {
                const auto paced=v::PacingTarget(rate,true,hz,multiplier);
                Check(paced<=rate && paced>0,"never raises game target");
                if (hz) Check(paced*multiplier<=hz-3,"fixed FG output under display headroom");
                else Check(paced==rate,"unknown display is not guessed");
            }
        }
        for (auto flags : {0u,0x7fu,0x80000000u}) {
            Check(gpu::MapPresentInterval(flags|0x200,0x827B4A4C,rate,true)==flags,"VRR removes guest-vblank quantization including 30/60");
            Check(gpu::MapPresentInterval(flags|0x200,0x1234,rate,true)==(flags|0x200),"unknown callers untouched");
            for(auto interval : {0u,0x100u,0x300u,0x400u})
                Check(gpu::MapPresentInterval(flags|interval,0x827B4A4C,rate,true)==(flags|interval),"other intervals untouched");
        }
    }
    Check(v::PacingTarget(120,true,120)==117,"120Hz native ceiling 117");
    Check(v::PacingTarget(90,true,90)==87,"90Hz native ceiling 87");
    Check(v::PacingTarget(120,true,144)==120,"144Hz retains native 120");
    Check(v::PacingTarget(120,true,144,2)==70,"fixed 2x uses total output budget");
    Check(v::PacingTarget(120,true,144,4)==35,"fixed 4x uses total output budget");
    Check(v::PacingTarget(0,true,144,4)==0,"diagnostic uncapped mode retained");
    Check(v::OutputLimit(1)==0 && v::OutputLimit(1001)==0,"invalid modes unknown");
    Check(v::DynamicTarget(0,true,144)==141,"dynamic SDK automatic target gets headroom");
    Check(v::DynamicTarget(100,true,144)==100,"lower dynamic target retained");
    Check(v::DynamicTarget(240,true,144)==141,"dynamic target bounded");
    Check(v::DynamicTarget(0,false,144)==0 && v::DynamicTarget(0,true,0)==0,"dynamic off and unknown fallback");
    Check(v::DynamicTarget(-1,true,144)==-1,"invalid target remains invalid");
    Check(std::isnan(v::DynamicTarget(std::numeric_limits<float>::quiet_NaN(),true,144)),"NaN not silently normalized");
    Check(v::DynamicPacingTarget(120,true,144,45)==45,"base does not exceed a lower dynamic SDK output target");
    Check(v::DynamicPacingTarget(120,true,144,141)==120,"dynamic does not divide by maximum MFG ratio");
    Check(v::DynamicPacingTarget(0,true,144,141)==0,"dynamic respects uncapped diagnostic");
    using Clock=gpu::FramePacer::Clock;
    const auto start=Clock::time_point(std::chrono::seconds(1));
    gpu::FramePacer p;
    auto next=p.Schedule(start,117);
    Check(next==start+std::chrono::nanoseconds(1000000000ull/117),"VRR rate accepted by real pacer");
    Check(p.Schedule(next,87)==next+std::chrono::nanoseconds(1000000000ull/87),"monitor move resets deadline");
    std::puts("VRR policy/guest interval/pacer checks passed (no display or GPU)");
}
