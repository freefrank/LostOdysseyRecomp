#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
template<class T> T ByteSwap(T n) {
    T result=0;
    for(unsigned i=0;i<sizeof(T);++i) { result=T((result<<8)|(n&255)); n=T(n>>8); }
    return result;
}
namespace fmt { template<class... T> std::string format(const char* text,T&&...) { return text; } }
inline std::vector<std::vector<uint8_t>> sentFrames;
