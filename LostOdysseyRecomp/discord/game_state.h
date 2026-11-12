#pragma once

#include "presence.h"

#include <cstdint>
#include <string_view>

namespace discord_rpc
{
// All guest memory reads stay on the existing engine tick. The worker receives
// only Snapshot's owned, player-facing strings; it never accesses guest memory.
#if defined(__ANDROID__) || defined(__SWITCH__) || defined(__NX__)
inline void Tick(uint8_t*) {}
#else
void Tick(uint8_t* base);
#endif

namespace game_state
{
struct Observed
{
    uint32_t worldMode = 0, activeWorldMode = 0;
    bool title = false, movie = false, menu = false, fieldEvent = false;
};

constexpr Mode Classify(const Observed& s)
{
    if (s.movie) return Mode::Movie;
    if (s.menu) return Mode::Menu;
    if (s.title) return Mode::Title;
    // Like soft_reset_policy.h: include battle results and the return transition.
    if (s.worldMode == 3 || s.activeWorldMode == 3) return Mode::Battle;
    if (s.fieldEvent) return Mode::Cutscene;
    return Mode::Exploring;
}

#if !defined(__ANDROID__) && !defined(__SWITCH__) && !defined(__NX__)
// Engine thread only. Also exercised directly by the CPU fixture so its timing
// includes the real guest reads, location conversion and snapshot allocation.
Snapshot Sample(uint8_t* base, std::string_view previousLocation = {});
#endif
}
}
