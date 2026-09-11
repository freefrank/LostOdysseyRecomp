#include "../../LostOdysseyRecomp/gpu/render_batch_policy.h"

#include <cstdio>

namespace {
struct FakeClock {
    using duration = std::chrono::microseconds;
    using time_point = std::chrono::time_point<FakeClock, duration>;
    static inline time_point current{};
    static inline unsigned reads = 0;
    static time_point now() { ++reads; return current; }
};

unsigned checks = 0;
unsigned failures = 0;
void Check(bool condition, const char* name) {
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", name); }
}
}

int main() {
    using gpu::render_batch::CpuTimer;
    using gpu::render_batch::DescriptorLimit;

    Check(DescriptorLimit(false) == 1800, "D3D12 unique 2D sets fit the shared heap");
    Check(DescriptorLimit(false, "2048") == 1800, "Vulkan override cannot expand D3D12 heap");
    Check(DescriptorLimit(true) == 2048, "Vulkan default");
    Check(DescriptorLimit(true, "500") == 500, "baseline override");
    Check(DescriptorLimit(true, "1024") == 1024, "intermediate override");
    Check(DescriptorLimit(true, "2048") == 2048, "upper boundary");
    for (const auto invalid : {"499", "2049", "-500", "+500", " 500", "500 ",
                               "500x", "0x500", "4294967796", "999999999999999999999"}) {
        Check(DescriptorLimit(true, invalid) == 2048, invalid);
    }
    const char embeddedNull[] = {'5', '0', '0', '\0', '1'};
    Check(DescriptorLimit(true, std::string_view(embeddedNull, sizeof(embeddedNull))) == 2048,
          "embedded null cannot truncate override");

    double disabledTotal = 7.0;
    {
        CpuTimer<FakeClock> disabled(false);
        FakeClock::current += FakeClock::duration(1000);
        disabled.AddTo(disabledTotal);
        disabled.AddTo(disabledTotal);
    }
    Check(FakeClock::reads == 0, "disabled timer never reads clock, including destruction");
    Check(disabledTotal == 7.0, "disabled timer preserves accumulator");

    FakeClock::current = FakeClock::time_point{};
    double outerTotal = 2.0;
    double innerTotal = 0.0;
    {
        CpuTimer<FakeClock> outer(true);
        FakeClock::current += FakeClock::duration(1250);
        {
            CpuTimer<FakeClock> inner(true);
            FakeClock::current += FakeClock::duration(2500);
            inner.AddTo(innerTotal);
        }
        FakeClock::current += FakeClock::duration(1250);
        outer.AddTo(outerTotal);
    }
    Check(innerTotal == 2.5, "nested timer accumulates exact milliseconds");
    Check(outerTotal == 7.0, "outer timer includes nested work and adds to existing total");
    Check(FakeClock::reads == 4, "enabled timers read only at construction and AddTo");

    std::printf("render_batch_policy: %u/%u checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
