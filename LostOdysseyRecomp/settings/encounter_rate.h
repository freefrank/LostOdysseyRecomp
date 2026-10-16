#pragma once
#include <cstdint>

namespace settings
{
// Stable settings.ini values; the legacy no_random_encounters=1 maps to None.
enum class EncounterRate : uint32_t { Normal, Half, None };
inline constexpr EncounterRate ValidEncounterRate(EncounterRate rate)
{
    return uint32_t(rate) <= uint32_t(EncounterRate::None) ? rate : EncounterRate::Normal;
}
}
