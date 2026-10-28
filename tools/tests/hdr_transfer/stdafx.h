#pragma once
// Standard-library-only replacement for the game's generated PPC precompiled
// header. Production presentation.cpp and its graphics interfaces are unchanged.
#include <os/platform.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
