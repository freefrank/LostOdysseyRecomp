#pragma once
#include "resource_scan.h"

namespace xenos::resources {
// Source locations for PM4_IM_LOAD_IMMEDIATE arrays in the supported XEX.
// Metadata only. Copy bytes from the loaded original image after exact hash
// verification; no game shader bytecode is embedded in this program.
inline size_t ExtractXexShaders(std::span<const uint8_t> image, const fs::path& source) {
    constexpr IndexEntry entries[] = {
        {0x185B10,60,0x8471352ddebb20e4ULL,false},
        {0x185B4C,36,0x63c971f5e9d59913ULL,true},
        {0x185B70,108,0x760aacf6212e632cULL,false},
        {0x185C00,96,0x61722cf30bd5fa6fULL,false}
    };
    size_t count=0;
    for (const auto& entry : entries) {
        if (entry.offset>image.size() || entry.size>image.size()-entry.offset) continue;
        const auto code=image.subspan(entry.offset,entry.size);
        if (Hash(code)!=entry.hash) continue;
        SaveSource(source,entry.pixel,code); ++count;
    }
    return count;
}
}
