#pragma once

#include <os/platform.h>
#include <cstddef>
#include <functional>
#include <thread>
#include <utility>

namespace os
{
// Host stack for threads that execute recompiled guest code. Guest call depth
// becomes host call depth; the runtime is validated with the Windows 1 MiB
// thread default, while macOS gives secondary threads only 512 KiB.
#if LO_PLATFORM_SWITCH
// libnx gives threads 128 KiB unless told otherwise, and GCC lays out the
// recompiled code's frames differently from Clang: use UnleashedRecomp-NX's
// generous margin (it runs guest threads with 8 MiB).
inline constexpr size_t kGuestCodeStackSize = 4 * 1024 * 1024;
#else
inline constexpr size_t kGuestCodeStackSize = 1024 * 1024;
#endif

#if LO_PLATFORM_MACOS || LO_PLATFORM_SWITCH
// The std::thread subset the runtime uses, created with kGuestCodeStackSize
// (std::thread cannot set a stack size).
class GuestCodeThread
{
public:
    GuestCodeThread() noexcept = default;
    template <typename Function, typename... Args>
    explicit GuestCodeThread(Function&& function, Args&&... args)
    {
        Start(std::function<void()>(std::bind_front(std::forward<Function>(function), std::forward<Args>(args)...)));
    }
    GuestCodeThread(GuestCodeThread&& other) noexcept { *this = std::move(other); }
    GuestCodeThread& operator=(GuestCodeThread&& other) noexcept;
    GuestCodeThread(const GuestCodeThread&) = delete;
    GuestCodeThread& operator=(const GuestCodeThread&) = delete;
    // Like std::thread, destroying a joinable thread terminates the process.
    ~GuestCodeThread();

    bool joinable() const noexcept { return m_joinable; }
    std::thread::id get_id() const noexcept { return m_id; }
    void join();
    void detach();

private:
    void Start(std::function<void()> body);

    void* m_handle = nullptr; // pthread_t
    std::thread::id m_id{};
    bool m_joinable = false;
};
#else
// Windows (1 MiB) and Linux (8 MiB) default thread stacks already suffice.
using GuestCodeThread = std::thread;
#endif
}
