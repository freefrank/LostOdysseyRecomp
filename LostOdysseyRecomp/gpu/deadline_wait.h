#pragma once

#include <chrono>
#include <thread>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace gpu
{
    // One reusable, auto-reset timer per pacing thread. Keep FramePacer's
    // deadlines unchanged; avoid millisecond sleep rounding without spinning.
    class DeadlineWait
    {
#ifdef _WIN32
        HANDLE timer = nullptr;
        bool highResolution = false;
#endif
    public:
        explicit DeadlineWait(bool preferHighResolution = true)
        {
#ifdef _WIN32
            if (preferHighResolution)
                timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                    TIMER_MODIFY_STATE | SYNCHRONIZE);
            highResolution = timer != nullptr;
            if (!timer) timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_MODIFY_STATE | SYNCHRONIZE);
#endif
        }
        ~DeadlineWait()
        {
#ifdef _WIN32
            if (timer) CloseHandle(timer);
#endif
        }
        DeadlineWait(const DeadlineWait&) = delete;
        DeadlineWait& operator=(const DeadlineWait&) = delete;
        bool HighResolution() const
        {
#ifdef _WIN32
            return highResolution;
#else
            return false;
#endif
        }
        void Until(std::chrono::steady_clock::time_point deadline)
        {
            const auto now = std::chrono::steady_clock::now();
            if (deadline <= now) return;
#ifdef _WIN32
            if (timer)
            {
                using Ticks = std::chrono::duration<int64_t, std::ratio<1, 10000000>>;
                LARGE_INTEGER due;
                due.QuadPart = -std::chrono::ceil<Ticks>(deadline - now).count();
                if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE) &&
                    WaitForSingleObject(timer, INFINITE) == WAIT_OBJECT_0)
                    return;
            }
#endif
            std::this_thread::sleep_until(deadline);
        }
    };
}
