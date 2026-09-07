#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <span>
#include <stdexcept>
#include <vector>

namespace xenos::resources {
struct ResourceExtent { uint64_t offset; uint32_t size; };
using ResourceExtents = std::map<std::filesystem::path, std::vector<ResourceExtent>>;
inline uint32_t ReadLE(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
// Same thirteen archive slots consumed by the original loader. Never interpret
// untrusted packed filenames as host paths. No resource is written or unpacked
// here: this table only supplies bounded reads within its adjacent FPD files.
inline ResourceExtents ReadResourceExtents(const std::filesystem::path& file, uint64_t* bytesRead = nullptr) {
    std::ifstream input(file, std::ios::binary | std::ios::ate);
    const auto length = input.tellg();
    if (length < 64 || length > 1024 * 1024) throw std::runtime_error("invalid FPI size");
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), length)) throw std::runtime_error("short FPI read");
    if (bytesRead) *bytesRead += bytes.size();
    auto u16 = [&](size_t p) { return uint32_t(bytes[p]) | uint32_t(bytes[p+1]) << 8; };
    auto u32 = [&](size_t p) { return ReadLE(bytes.data()+p); };
    const uint64_t used = u16(12) * 2048u;
    const uint64_t archives = u32(32), entries = u32(36), strings = u32(40);
    if (bytes[20] < 1 || bytes[20] > 4 || bytes[21] != 4 || u16(24) != 1 || u16(26) != 13 ||
        !used || used > bytes.size() || archives < 64 || archives+13*48 > entries ||
        entries > strings || strings > used || entries+uint64_t(u32(28))*24 != strings)
        throw std::runtime_error("invalid FPI tables");
    constexpr const char* names[] = {"LO.fpd", "xenon_chr.fpd", "xenon_event.fpd", "xenon_field.fpd",
        "xenon_obj.fpd", "xenon_scr.fpd", "xenon_sys.fpd", "xenon_vfx.fpd", "xenon_world.fpd",
        "xenon_battle.fpd", "xenon_loc.fpd", "xenon_mov.fpd", "xenon_snd.fpd"};
    ResourceExtents result;
    for (size_t i = 0; i < 13; ++i) {
        const uint64_t record = archives+i*48;
        const uint64_t begin = record+u32(record+4);
        const uint64_t end = i == 12 ? strings : record+48+u32(record+52);
        if (begin < entries || end < begin || end > strings || (begin-entries)%24 || (end-begin)%24)
            throw std::runtime_error("invalid FPI archive range");
        const auto archive = file.parent_path()/names[i];
        const auto size = std::filesystem::file_size(archive);
        auto& extents = result[archive];
        for (uint64_t p = begin; p < end; p += 24) {
            const auto amount = u32(p+16);
            if (!amount || (u32(p) & 0x10000000)) continue; // directories have no shader payload
            const uint64_t offset = uint64_t(u32(p+8) & 0xFFFFFF) * 2048;
            if (offset > size || amount > size-offset) throw std::runtime_error("FPI extent outside archive");
            extents.push_back({offset, amount});
        }
    }
    return result;
}
}
