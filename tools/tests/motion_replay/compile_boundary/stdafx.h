#pragma once

// Precompiled header for the LostOdysseyRecomp runtime.
// Layout follows UnleashedRecomp (GPLv3), adapted for Lost Odyssey.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#if defined(_WIN32)
#include <windows.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#elif defined(__linux__)
#include <unistd.h>
#include <sys/mman.h>
#include <strings.h>
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _fseeki64 fseeko
#define _ftelli64 ftello
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <list>
#include <memory>
#include <mutex>
#include <numeric>
#include <semaphore>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

// No guest program is linked by this compile-only boundary fixture.
extern "C" unsigned long long XXH3_64bits(const void*, size_t);
#include <ankerl/unordered_dense.h>
// Only the unreferenced inline Memory helpers need generated guest ABI names.
// This does NOT test the generated PPC program or constitute a full-game build.
inline constexpr uint64_t PPC_MEMORY_SIZE = 0x100000000ull;
using PPCFunc = void();
#define PPC_LOOKUP_FUNC(base, address) (*reinterpret_cast<PPCFunc**>((base) + (address)))

#include <fmt/core.h>
#include <fmt/format.h>

#include "framework.h"
#include "mutex.h"
inline uint32_t ByteSwap(uint32_t x) { return (x>>24)|((x>>8)&0xff00u)|((x<<8)&0xff0000u)|(x<<24); }

// Pointer-only HID declarations used by video.cpp; no guest layout is modeled.
struct XAMINPUT_STATE;
struct XAMINPUT_VIBRATION;
struct XAMINPUT_CAPABILITIES;
