#include <gpu/frame_pacer.h>
#include <cstdio>
#include <cstdlib>

static void Require(bool value, const char* message)
{
    if (!value) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
int main()
{
    using namespace std::chrono;
    using gpu::FramePacer;
    namespace rate = gpu::frame_rate;
    const auto origin = FramePacer::Clock::time_point(seconds(100));
    static_assert(rate::kCount == 4);
    for (uint32_t i = 0; i < rate::kCount; ++i)
    {
        const auto fps = rate::kNativeRates[i];
        Require(rate::Supported(fps) && rate::Normalize(fps) == fps, "native rate accepted");
        Require(rate::MenuIndex(fps) == i && rate::FromMenuIndex(i) == fps, "menu round trip");
        const auto period = nanoseconds(1000000000ull / fps);
        FramePacer pacer;
        auto next = pacer.Schedule(origin, fps);
        Require(next == origin + period, "first deadline");
        next = pacer.Schedule(next + milliseconds(2), fps);
        Require(next == origin + period * 2, "render cost must not accumulate into period");
        const auto stalled = origin + seconds(3);
        Require(pacer.Schedule(stalled, fps) == stalled, "late frame released without extra delay");
        next = pacer.Schedule(stalled, fps);
        Require(next == stalled + period, "no catch-up burst after a stall");
        for (int n = 0; n < 5; ++n)
        {
            const auto previous = next;
            next = pacer.Schedule(previous + milliseconds(2), fps);
            Require(next == previous + period, "normal cadence resumes after stall");
        }
        FramePacer overloaded;
        auto presented = overloaded.Schedule(origin, fps);
        for (int n = 0; n < 120; ++n)
        {
            const auto ready = presented + period + milliseconds(1);
            presented = overloaded.Schedule(ready, fps);
            Require(presented == ready, "overload must not add another frame of sleep");
        }
        Require(overloaded.Schedule(presented + milliseconds(2), fps) == presented + period,
            "overload recovery must resume pacing without a catch-up burst");
        // Every runtime rate pair, including high -> low and leaving uncapped.
        for (const auto target : rate::kNativeRates)
        {
            FramePacer switching;
            const auto previous = switching.Schedule(origin, fps);
            const auto targetPeriod = nanoseconds(1000000000ull / target);
            Require(switching.Schedule(previous, target) == previous + targetPeriod, "rate switch deadline");
            Require(switching.Schedule(previous, 0) == previous, "uncapped does not sleep");
            Require(switching.Schedule(previous, target) == previous + targetPeriod, "uncapped to capped resets");
        }
    }
    for (const auto invalid : {0u, 29u, 59u, 61u, 119u, 144u, 1000u, 0xFFFFFFFFu})
        Require(!rate::Supported(invalid) && rate::Normalize(invalid) == 30 && rate::MenuIndex(invalid) == 0,
            "invalid persisted rate defaults to 30; diagnostics are separate");
    {
        gpu::OutputFramePacer output;
        const auto period = nanoseconds(1000000000ull / 141);
        Require(output.Schedule(origin, 141, 100) == origin, "first FG output observation starts a new cadence");
        const auto twoFrames = output.Schedule(origin + milliseconds(1), 141, 102);
        Require(twoFrames == origin + period * 2, "two SDK presents reserve two output periods");
        Require(output.Schedule(twoFrames + milliseconds(1), 141, 104) == twoFrames + period * 2,
            "FG output pacing follows cumulative SDK presents");
        Require(output.Schedule(origin + seconds(3), 141, 106) == origin + seconds(3),
            "a long stall does not accumulate output debt");
        Require(output.Schedule(origin + seconds(3), 0, 108) == origin + seconds(3),
            "FG output pacing turns off with the VRR budget");
        Require(output.Schedule(origin + seconds(4), 141, 110) == origin + seconds(4),
            "FG output pacing restarts after an off-to-on transition");
    }
    Require(rate::FromMenuIndex(rate::kCount) == 30, "invalid menu index is bounded");
    for (const auto fps : {30u, 60u, 90u, 120u, 0u, 144u})
    {
        const bool high = fps == 0 || fps > 60;
        Require(rate::NeedsImmediate(fps) == high, "guest immediate policy");
        Require(rate::HostVsyncEnabled(fps, true, true) == (fps != 0), "VSync On syncs every capped rate");
        Require(!rate::HostVsyncEnabled(fps, false, true), "VSync Off presents immediately");
        Require(!rate::HostVsyncEnabled(fps, true, false), "a backend's disabled-vsync baseline is kept");
        Require(!rate::HostVsyncEnabled(fps, true, true, true), "FG immediate requirement survives every native cap");
    }
    using gpu::MapPresentInterval;
    for (const auto flags : {0u, 0x7Fu, 0x80000000u, 0xFFFF007Fu})
    {
        Require(MapPresentInterval(flags | 0x200, 0x827B4A4C, 30) == (flags | 0x200), "30 baseline");
        Require(MapPresentInterval(flags | 0x200, 0x827B4A4C, 30, true) == flags, "VSync Off or VRR: 30 is immediate");
        for (const auto fps : {60u, 90u, 120u, 0u, 144u})
        {
            Require(MapPresentInterval(flags | 0x200, 0x827B4A4C, fps) == flags, "60 and above are immediate without an environment gate");
            Require(MapPresentInterval(flags | 0x200, 0x1234, fps) == (flags | 0x200), "other callers untouched");
            for (const auto interval : {0u, 0x100u, 0x300u, 0x400u, 0xFF00u})
                Require(MapPresentInterval(flags | interval, 0x827B4A4C, fps) == (flags | interval), "other intervals untouched");
        }
        Require(MapPresentInterval(flags | 0x200, 0x827B4A4C, 59) == (flags | 0x200), "sub-60 diagnostic preserves baseline");
    }
    std::puts("native 30/60/90/120 policy, frame pacer and guest interval checks passed");
}
