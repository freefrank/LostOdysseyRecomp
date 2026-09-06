#include <stdafx.h>
#include "disc_set.h"

namespace DiscSet
{
Identity ReadIdentity(const std::filesystem::path& directory)
{
    std::array<uint8_t, 65536> bytes{};
    std::ifstream input(directory / "default.xex", std::ios::binary);
    input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    const size_t size = size_t(input.gcount());
    auto u32 = [&](size_t p) { return uint32_t(bytes[p]) << 24 | uint32_t(bytes[p+1]) << 16 |
        uint32_t(bytes[p+2]) << 8 | bytes[p+3]; };
    if (size < 24 || memcmp(bytes.data(), "XEX2", 4)) return {};
    const auto count = u32(20);
    if (count > 1024 || 24 + size_t(count)*8 > size) return {};
    for (uint32_t i = 0; i < count; ++i)
    {
        if (u32(24+i*8) != 0x40006) continue;
        const size_t p = u32(28+i*8);
        if (p + 24 > size || u32(p+12) != 0x4D5307FA || bytes[p+19] != 4) return {};
        const uint32_t disc = bytes[p+18], edition = u32(p+4);
        if (disc < 1 || disc > 4) return {};
        constexpr uint32_t asia[] = {0x39F7D748, 0x0EF8CEA8, 0x309E3386, 0x7B21A91D};
        constexpr uint32_t europe[] = {0x368DE6DD, 0x1888BE4E, 0x6DD59D08, 0x0C0E80B5};
        if ((edition == 4 && u32(p) == asia[disc-1]) || (edition == 3 && u32(p) == europe[disc-1]))
            return {edition, disc};
        return {};
    }
    return {};
}

bool Validate(const std::filesystem::path& directory, Identity expected)
{
    const auto actual = ReadIdentity(directory);
    if (!expected.edition || actual.edition != expected.edition || actual.disc != expected.disc) return false;
    std::ifstream input(directory / "LO.fpi", std::ios::binary | std::ios::ate);
    const auto length = input.tellg();
    // Original loader accepts at most 512 sectors of index data.
    if (length < 64 || length > 1024*1024) return false;
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) return false;
    auto u16 = [&](size_t p) { return uint32_t(bytes[p]) | uint32_t(bytes[p+1]) << 8; };
    auto u32 = [&](size_t p) { return u16(p) | u16(p+2) << 16; };
    if (bytes[20] != expected.disc || bytes[21] != 4 || u16(24) != 1 || u16(26) != 13 ||
        !u16(12) || u16(12)*2048u > bytes.size()) return false;
    const uint64_t archives = u32(32), entries = u32(36), strings = u32(40);
    if (archives < 64 || archives+13*48 > entries || entries > strings || strings > u16(12)*2048u ||
        entries+uint64_t(u32(28))*24 != strings) return false;
    constexpr const char* names[] = {"LO.fpd", "xenon_chr.fpd", "xenon_event.fpd", "xenon_field.fpd",
        "xenon_obj.fpd", "xenon_scr.fpd", "xenon_sys.fpd", "xenon_vfx.fpd", "xenon_world.fpd",
        "xenon_battle.fpd", "xenon_loc.fpd", "xenon_mov.fpd", "xenon_snd.fpd"};
    for (size_t i = 0; i < 13; ++i)
    {
        std::error_code ec;
        const auto size = std::filesystem::file_size(directory / names[i], ec);
        if (ec || !size) return false;
        const uint64_t record = archives+i*48;
        const uint64_t begin = record+u32(record+4);
        const uint64_t end = i == 12 ? strings : record+48+u32(record+52);
        if (begin < entries || end < begin || end > strings || (end-begin)%24) return false;
        for (uint64_t p = begin; p < end; p += 24)
        {
            const uint64_t length = u32(p+16);
            const uint64_t offset = uint64_t(u32(p+8) & 0xFFFFFF) * 2048;
            if (length && (offset > size || length > size-offset)) return false;
        }
    }
    return true;
}
}
