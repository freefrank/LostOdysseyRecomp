#pragma once
#include <bit>
// SDK v1.1.4 uses MSVC array helpers and omits these includes in POSIX branches.
// Force-included only for this third-party target; no SDK source is modified.
#include <cwchar>
#include <cerrno>
#include <cstring>
#include <cmath>
#include <locale>
#include <cstdio>
#include <FidelityFX/host/ffx_util.h>

using std::abs; // Match MSVC's float overload for the SDK's unqualified abs(value).

#ifndef _countof
#define _countof(array) (sizeof(array) / sizeof((array)[0]))
#endif

template <size_t N>
inline int wcscpy_s(wchar_t (&destination)[N], const wchar_t* source)
{
    const size_t length = std::wcslen(source);
    if (length >= N) {
        destination[0] = L'\0';
        return ERANGE;
    }
    std::wmemcpy(destination, source, length + 1);
    return 0;
}

inline int wcscpy_s(wchar_t* destination, size_t capacity, const wchar_t* source)
{
    const size_t length = std::wcslen(source);
    if (length >= capacity) {
        if (capacity) destination[0] = L'\0';
        return ERANGE;
    }
    std::wmemcpy(destination, source, length + 1);
    return 0;
}

inline int strcpy_s(char* destination, size_t capacity, const char* source)
{
    const size_t length = std::strlen(source);
    if (length >= capacity) {
        if (capacity) destination[0] = '\0';
        return ERANGE;
    }
    std::memcpy(destination, source, length + 1);
    return 0;
}

template <typename... Args>
inline int sprintf_s(char* destination, size_t capacity, const char* format, Args... args)
{
    const int result = std::snprintf(destination, capacity, format, args...);
    if (result < 0 || static_cast<size_t>(result) >= capacity) {
        if (capacity) destination[0] = '\0';
        return -1;
    }
    return result;
}
