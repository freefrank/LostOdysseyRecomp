#pragma once

#include <os/platform.h>
#include <thread>
#include <utility>

namespace os
{
// Lets a std::thread run on its own. libnx has no pthread_detach, so on the
// Switch std::thread::detach() throws and the joinable thread being destroyed
// during unwinding ends the process (std::terminate). There the thread object
// is kept alive for the rest of the run instead; everywhere else this is detach().
inline void DetachThread(std::thread&& thread)
{
#if LO_PLATFORM_SWITCH
    (void)new std::thread(std::move(thread));
#else
    thread.detach();
#endif
}

inline void DetachThread(std::thread& thread)
{
    DetachThread(std::move(thread));
}
}
