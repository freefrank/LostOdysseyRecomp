#pragma once

// Precompiled header for the LostOdysseyRecomp runtime.
// Layout follows UnleashedRecomp (GPLv3), adapted for Lost Odyssey.

#define NOMINMAX

#if defined(_WIN32)
#include <windows.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#elif defined(__linux__)
#include <unistd.h>
#include <sys/mman.h>
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
