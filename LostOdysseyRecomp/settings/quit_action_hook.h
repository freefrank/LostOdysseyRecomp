#pragma once

#include <cstdint>

namespace settings::quit_action
{
inline constexpr uint32_t SettingsMenu = 0x832AD2B8;
inline constexpr uint32_t SystemMenu = 0x8326DAA8;
// System's completed exit task calls at 822E2560 (observed in-game); the
// separate state-16 confirmation modal also has a Yes call at 822E26B4.
inline constexpr uint32_t SystemTaskYesCaller = 0x822E256C;
inline constexpr uint32_t ModalYesCaller = 0x822E26B8;

inline constexpr bool IsSystemYesCaller(uint32_t lr)
{
    return lr == SystemTaskYesCaller || lr == ModalYesCaller;
}

// Called only after the retail Settings close task has ticked. It is the
// original guest title transition, deliberately independent of SDL_QUIT.
template<class Context, class State, class Original>
bool RequestTitle(Context& ctx, uint32_t settingsMenu, State&& state, Original&& original)
{
    if (settingsMenu != SettingsMenu || state(settingsMenu) > 2)
        return false;
    ctx.r3.s64 = int32_t(SystemMenu);
    ctx.r4.s64 = 1;
    original(ctx);
    return true;
}

// Both known System Yes paths are restricted to the native System menu and
// code 1. Every other caller, including Settings, retains retail behavior.
template<class Context, class PushQuit, class Recover, class Original>
void Dispatch(Context& ctx, PushQuit&& pushQuit, Recover&& recover, Original&& original)
{
    if (!IsSystemYesCaller(ctx.lr) || ctx.r3.u32 != SystemMenu || ctx.r4.u32 != 1)
    {
        original(ctx);
        return;
    }
    // The System task's 822E256C continuation performs its own UI close and
    // resets its state to zero. Only modal-16 needs the explicit cancel branch.
    if (!pushQuit() && ctx.lr == ModalYesCaller)
        recover(ctx);
}
} // namespace settings::quit_action
