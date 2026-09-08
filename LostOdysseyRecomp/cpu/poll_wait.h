#pragma once

#include <chrono>
#include <cstdint>
#include <thread>
#include <utility>
#ifdef _WIN32
#include <Windows.h>
#endif

// Only the two known guest polling loops opt in. No guest result, timeout or
// memory access is replaced: every pause returns to the original guest code.
namespace poll_wait
{
    enum class Kind { Query, SharedValue };

    struct State
    {
        bool active = false;
        uint32_t polls = 0;

        uint32_t NextDelayUs()
        {
            if (polls < 65) ++polls;
            if (polls <= 32) return 0;
            if (polls <= 48) return 50;
            if (polls <= 64) return 100;
            return 200;
        }
    };

    inline thread_local State query;
    inline thread_local State sharedValue;

    inline void ResetThread() { query = {}; sharedValue = {}; }

    // Guest ExTerminateThread uses longjmp. Keep the production wrapper free of
    // automatic objects requiring destruction; the guest thread boundary resets
    // the TLS state after that jump. Ordinary C++ exceptions still restore nesting.
    template<class Run>
    void RunScoped(Kind kind, Run&& run)
    {
        State& state = kind == Kind::Query ? query : sharedValue;
        const State previous = state;
        state = {true, 0};
        try { run(); }
        catch (...) { state = previous; throw; }
        state = previous;
    }

    class Scope
    {
        State& state;
        State previous;
    public:
        explicit Scope(Kind kind) : state(kind == Kind::Query ? query : sharedValue), previous(state)
        { state = {true, 0}; }
        ~Scope() { state = previous; }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    };

    // One attempt, including failed timer setup or timeout. In particular this
    // never loops waiting for a timer or a guest notification that may not arrive.
    template<class Arm, class Wait, class Cancel, class Yield>
    void WaitOnce(uint32_t delayUs, Arm&& arm, Wait&& wait, Cancel&& cancel, Yield&& yield)
    {
        if (!arm(delayUs)) { yield(); return; }
        if (!wait()) { cancel(); yield(); }
    }

    inline void Pause(uint32_t delayUs)
    {
        if (!delayUs) return;
#ifdef _WIN32
        struct Timer
        {
            HANDLE handle = CreateWaitableTimerExW(nullptr, nullptr,
                CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_MODIFY_STATE | SYNCHRONIZE);
            ~Timer() { if (handle) CloseHandle(handle); }
        };
        static thread_local Timer timer;
        WaitOnce(delayUs,
            [&](uint32_t us) {
                LARGE_INTEGER due;
                due.QuadPart = -int64_t(us) * 10;
                return timer.handle && SetWaitableTimerEx(timer.handle, &due, 0, nullptr, nullptr, nullptr, 0);
            },
            [&] { return WaitForSingleObject(timer.handle, 1) == WAIT_OBJECT_0; },
            [&] { CancelWaitableTimer(timer.handle); },
            [] { std::this_thread::yield(); });
#else
        std::this_thread::sleep_for(std::chrono::microseconds(delayUs));
#endif
    }

    template<class Wait = decltype(&Pause)>
    void QueryResult(int32_t result, Wait wait = &Pause)
    {
        if (!query.active) return;
        // This is exactly the retry predicate of guest sub_823CF390.
        if (result == 1 || result < 0)
        {
            if (const auto us = query.NextDelayUs()) wait(us);
        }
        else query.polls = 0;
    }

    template<class Wait = decltype(&Pause)>
    bool ZeroDelay(Wait wait = &Pause)
    {
        if (!sharedValue.active) return false;
        if (const auto us = sharedValue.NextDelayUs()) wait(us);
        return true;
    }
}
