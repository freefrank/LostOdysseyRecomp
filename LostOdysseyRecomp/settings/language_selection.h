#pragma once

#include <cstdint>

namespace settings::language
{
inline constexpr uint32_t Registry = 0x8336A5F0;
inline constexpr uint32_t Records = 0x832455F0;
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
}
