#pragma once

// Precompiled header for the LostOdysseyRecomp runtime.
// Layout follows UnleashedRecomp (GPLv3), adapted for Lost Odyssey.

#define NOMINMAX

#include <os/platform.h>

#if LO_PLATFORM_WINDOWS
#include <windows.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#elif LO_PLATFORM_POSIX
#include <unistd.h>
#include <sys/mman.h>
#include <strings.h>
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _fseeki64 fseeko
#define _ftelli64 ftello
#elif LO_PLATFORM_SWITCH
// newlib: POSIX strings and 64-bit file offsets, but no mmap.
#include <unistd.h>
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

#include <xbox.h>
#include <xxhash.h>
#include <ankerl/unordered_dense.h>
#include <ppc/ppc_recomp_shared.h>
#include <o1heap.h>
#include <fmt/core.h>
#include <fmt/format.h>

#include "framework.h"
#include "mutex.h"
