#pragma once

#include <cstdint>
#include <iterator>
#include <string_view>

namespace settings::language
{
inline constexpr uint32_t Registry = 0x8336A5F0;
inline constexpr uint32_t Records = 0x832455F0;
// The folder and file code of each language ID 1-9 (loc/<code>/, *_<code>.xxx).
inline constexpr const char *Codes[] = {"", "int", "jpn", "deu", "fra", "spa", "ita", "kor", "chi", "sch"};
constexpr uint32_t IdFromCode(std::string_view code)
{
    for (uint32_t id = 1; id < std::size(Codes); ++id)
        if (code == Codes[id]) return id;
    return 0;
}
// Native parser caps this U16 table at 16 entries (824822F8, +288..319).
// A missing/invalid list is not a one-entry English list.
inline constexpr uint32_t VoiceCapacity = 16;
constexpr uint32_t VoiceCount(uint32_t raw)
{
    return raw >= 1 && raw <= VoiceCapacity ? raw : 0;
}
constexpr bool ValidVoiceIndex(uint32_t raw, uint32_t index)
{
    return index < VoiceCount(raw);
}
constexpr bool ResourceOverride(uint32_t host, uint32_t object, uint32_t request)
{
    return host >= 1 && host <= 9 && object == Registry && (request == 0 || request == host);
}
// An overridden lookup keeps the registry's own record when it carries the host
// language. Callers compare the returned code with the registry's lower-case
// codes ("int", "ita") case-sensitively; the static table's "INT" never matches,
// and the FMV player then picks FMVInfo.dat's ID-0 (Japanese) audio track.
constexpr bool KeepRegistryRecord(uint32_t record, uint32_t recordId, uint32_t host)
{
    return record != 0 && recordId == host;
}
}
