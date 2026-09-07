#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>

namespace xenos::cache {
// Shared by the runtime and offline compiler. Bump for translation changes.
inline constexpr unsigned Version = 21;
inline std::string FileName(bool pixel, uint64_t hash) {
    char name[64];
    std::snprintf(name, sizeof(name), "%s_%016llx_v%u.dxil",
                  pixel ? "ps" : "vs", static_cast<unsigned long long>(hash), Version);
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
}
