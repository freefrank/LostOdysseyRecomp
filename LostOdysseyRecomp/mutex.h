#pragma once

#ifdef _WIN32

struct Mutex : CRITICAL_SECTION
{
    Mutex() { InitializeCriticalSection(this); }
    ~Mutex() { DeleteCriticalSection(this); }
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    void lock() { EnterCriticalSection(this); }
    void unlock() { LeaveCriticalSection(this); }
};

#else

using Mutex = std::mutex;

#endif
