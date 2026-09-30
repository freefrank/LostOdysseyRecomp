#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <utility>

namespace notified_wait
{
    inline constexpr uint32_t Infinite = UINT32_MAX;

    // The caller owns the state mutex. Producers must publish state while
    // holding the same mutex, then notify, so a wake cannot be lost between
    // the predicate check and parking the thread.
    template<class Rep, class Period, class Predicate>
    bool For(std::condition_variable& changed, std::unique_lock<std::mutex>& lock,
        std::chrono::duration<Rep, Period> timeout, Predicate&& ready)
    {
        if (ready()) return true;
        if (timeout <= timeout.zero()) return false;
        return changed.wait_until(lock, std::chrono::steady_clock::now() + timeout,
            std::forward<Predicate>(ready));
    }

    template<class Predicate>
    bool Until(std::condition_variable& changed, std::unique_lock<std::mutex>& lock,
        uint32_t timeoutMs, Predicate&& ready)
    {
        if (timeoutMs == Infinite)
        {
            if (ready()) return true;
            changed.wait(lock, std::forward<Predicate>(ready));
            return true;
        }
        return For(changed, lock, std::chrono::milliseconds(timeoutMs),
            std::forward<Predicate>(ready));
    }
}
