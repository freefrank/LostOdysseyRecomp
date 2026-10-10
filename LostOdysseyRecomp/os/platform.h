#pragma once

// Host platform identification for the runtime. Every macro is always defined
// as 0 or 1, so platform code tests `#if LO_PLATFORM_POSIX` rather than raw
// compiler macros. Use LO_PLATFORM_POSIX for code that relies only on POSIX
// APIs; use a specific platform only for APIs unique to it (memfd, Mach VM).
#if defined(__SWITCH__)
// Nintendo Switch homebrew (devkitA64 + libnx, Horizon OS). newlib provides a
// POSIX-like C library (files, pthreads, clocks) but no mmap, dlopen, signals
// or /proc, so it is not LO_PLATFORM_POSIX; code tests LO_PLATFORM_SWITCH.
#define LO_PLATFORM_WINDOWS 0
#define LO_PLATFORM_LINUX 0
#define LO_PLATFORM_MACOS 0
#elif defined(_WIN32)
#define LO_PLATFORM_WINDOWS 1
#define LO_PLATFORM_LINUX 0
#define LO_PLATFORM_MACOS 0
#elif defined(__linux__)
#define LO_PLATFORM_WINDOWS 0
#define LO_PLATFORM_LINUX 1
#define LO_PLATFORM_MACOS 0
#elif defined(__APPLE__)
#define LO_PLATFORM_WINDOWS 0
#define LO_PLATFORM_LINUX 0
#define LO_PLATFORM_MACOS 1
#else
#error "Unsupported host platform"
#endif

#if defined(__SWITCH__)
#define LO_PLATFORM_SWITCH 1
#else
#define LO_PLATFORM_SWITCH 0
#endif

#if defined(__ANDROID__)
#define LO_PLATFORM_ANDROID 1
#else
#define LO_PLATFORM_ANDROID 0
#endif

// Android also uses the Linux/POSIX memory and scheduling implementations.
// Desktop-only process, updater and storage paths test Android separately.
#define LO_PLATFORM_POSIX (LO_PLATFORM_LINUX || LO_PLATFORM_MACOS)
