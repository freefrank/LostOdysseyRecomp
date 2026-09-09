#include "../../LostOdysseyRecomp/gpu/deadline_wait.h"
#include <cstdio>
#include <cstdlib>

int main()
{
    using Clock = std::chrono::steady_clock;
    for (bool high : {true, false})
    {
        gpu::DeadlineWait wait(high);
        const auto start = Clock::now();
        wait.Until(start - std::chrono::seconds(1));
        for (int i = 0; i < 32; ++i)
        {
            auto deadline = Clock::now() + std::chrono::microseconds(250 + i * 50);
            wait.Until(deadline);
            if (Clock::now() < deadline) std::abort();
        }
        std::printf("deadline wait: requested_high=%d actual_high=%d, 32 future deadlines passed\n", high, wait.HighResolution());
    }
}
