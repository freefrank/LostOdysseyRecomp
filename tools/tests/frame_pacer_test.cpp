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
    const auto origin = FramePacer::Clock::time_point(seconds(100));
    const auto p30 = nanoseconds(1000000000ull / 30);
    const auto p60 = nanoseconds(1000000000ull / 60);
    const auto p120 = nanoseconds(1000000000ull / 120);
    FramePacer pacer;
    auto next = pacer.Schedule(origin, 30);
    Require(next == origin + p30, "first deadline");
    Require(pacer.Schedule(next + milliseconds(2), 30) == next + p30, "normal render cost must not accumulate into period");
    const auto stalled = origin + seconds(3);
    next = pacer.Schedule(stalled, 30);
    Require(next == stalled, "late frame must present immediately without another period of delay");
    next = pacer.Schedule(stalled, 30);
    Require(next == stalled + p30, "after a stall a second frame cannot catch up immediately");
    for (int i = 0; i < 5; ++i)
    {
        const auto previous = next;
        next = pacer.Schedule(previous + milliseconds(2), 30);
        Require(next == previous + p30, "after a stall normal cadence resumes without catch-up bursts");
    }
    const auto beforeChange = next;
    next = pacer.Schedule(next, 60);
    Require(next == beforeChange + p60, "30 to 60 resets deadline");
    next = pacer.Schedule(next, 120);
    Require(next == beforeChange + p60 + p120, "60 to 120 resets deadline");
    Require(pacer.Schedule(next, 0) == next, "uncapped does not sleep");
    Require(pacer.Schedule(next, 30) == next + p30, "uncapped to 30 resets");
    FramePacer overloaded;
    auto presented = overloaded.Schedule(origin, 60);
    for (int i = 0; i < 120; ++i)
    {
        const auto ready = presented + milliseconds(18);
        presented = overloaded.Schedule(ready, 60);
        Require(presented == ready, "sustained 18ms work at 60 must not incur an extra 16.67ms sleep");
    }
    Require(presented == origin + p60 + milliseconds(2160), "overload throughput must follow work time, not work plus period");
    Require(overloaded.Schedule(presented + milliseconds(2), 60) == presented + p60,
        "recovery from sustained overload resumes pacing without catch-up burst");
    using gpu::MapPresentInterval;
    for (uint32_t flags : {0u, 0x7Fu, 0x80000000u})
    {
        Require(MapPresentInterval(flags | 0x200, 0x827B4A4C, 30, true) == (flags | 0x200), "30 baseline");
        Require(MapPresentInterval(flags | 0x200, 0x827B4A4C, 60, false) == (flags | 0x100), "60 interval and flags");
        Require(MapPresentInterval(flags | 0x200, 0x827B4A4C, 120, false) == (flags | 0x100), "120 experimental gate");
        Require(MapPresentInterval(flags | 0x200, 0x827B4A4C, 120, true) == flags, "120 immediate preserves flags");
        Require(MapPresentInterval(flags | 0x200, 0x1234, 120, true) == (flags | 0x200), "other callers untouched");
        for (uint32_t interval : {0u, 0x100u, 0x300u, 0x400u})
            Require(MapPresentInterval(flags | interval, 0x827B4A4C, 120, true) == (flags | interval), "other intervals untouched");
    }
    std::puts("frame pacer and guest interval checks passed");
}
