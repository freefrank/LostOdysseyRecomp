#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>

namespace xenos::cache {
// Shared by the runtime and offline compiler. Bump for translation changes.
inline constexpr unsigned Version = 21;
inline std::string FileName(bool pixel, uint64_t hash, bool spirv = false) {
    char name[64];
    std::snprintf(name, sizeof(name), "%s_%016llx_v%u%s",
                  pixel ? "ps" : "vs", static_cast<unsigned long long>(hash), Version,
                  spirv ? "_vk12_1.spv" : ".dxil");
    return name;
}
// Reject incomplete/zero-filled cache writes before handing them to the driver.
// This is container framing validation, not cryptographic validation of DXIL.
inline bool CompleteContainer(std::span<const uint8_t> data) {
    if (data.size() < 32 || std::memcmp(data.data(), "DXBC", 4)) return false;
    auto word = [&](size_t offset) { uint32_t v; std::memcpy(&v, data.data()+offset, 4); return v; };
    if (word(24) != data.size()) return false;
    const uint32_t chunks = word(28);
    if (chunks > (data.size()-32)/4) return false;
    for (uint32_t i=0; i<chunks; ++i) {
        const size_t offset = word(32+i*4);
        if (offset > data.size()-8 || word(offset+4) > data.size()-offset-8) return false;
    }
    return chunks != 0;
}
// Validate complete SPIR-V instruction framing before handing a module to the driver.
inline bool CompleteSpirv(std::span<const uint8_t> data) {
    if (data.size() < 20 || data.size() % 4) return false;
    auto word = [&](size_t index) { uint32_t value; std::memcpy(&value,data.data()+index*4,4); return value; };
    if (word(0) != 0x07230203 || word(1) < 0x00010000 || word(1) > 0x00010600 || !word(3) || word(4)) return false;
    bool memoryModel=false, entryPoint=false;
    for (size_t index=5; index<data.size()/4;) {
        const uint32_t op=word(index), count=op>>16;
        if (!count || count>data.size()/4-index) return false;
        memoryModel |= (op&0xffff)==14 && count==3;
        entryPoint |= (op&0xffff)==15 && count>=4;
        index+=count;
    }
    return memoryModel && entryPoint;
}
inline bool CompleteBinary(std::span<const uint8_t> data, bool spirv) {
    return spirv ? CompleteSpirv(data) : CompleteContainer(data);
}
}
