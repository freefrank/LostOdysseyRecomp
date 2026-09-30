#pragma once

// Thread names are diagnostic only. Failure to set one must not affect runtime.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__)
#include <pthread.h>
#endif

namespace os {

inline void SetCurrentThreadName(const char* name) noexcept
{
    if (!name || !*name) return;
#ifdef _WIN32
    wchar_t wide[64]{};
    for (unsigned i = 0; i < 63 && name[i]; ++i)
        wide[i] = static_cast<unsigned char>(name[i]);
    (void)::SetThreadDescription(::GetCurrentThread(), wide);
#elif defined(__linux__)
    char truncated[16]{};
    for (unsigned i = 0; i < 15 && name[i]; ++i)
        truncated[i] = name[i];
    (void)pthread_setname_np(pthread_self(), truncated);
#else
    (void)name;
#endif
}

} // namespace os
