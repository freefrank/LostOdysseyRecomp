#include <stdafx.h>
#include <os/logger.h>
#include "save_anywhere.h"
#include "menu_animations.h"
#include <settings/config.h>
#include <fstream>

extern "C" PPC_FUNC(__imp__sub_822E0E10);
extern "C" PPC_FUNC(__imp__sub_82876EA8);

namespace
{
    constexpr uint32_t SaveRow = 0x8326D690;
    constexpr uint32_t Visible = 0x80000000;
    constexpr uint32_t Enabled = 0x40000000;
    std::atomic<bool>& Requested()
    {
        static std::atomic<bool> requested{settings::GetConfig().saveAnywhere};
        return requested;
    }
    // Event script VM manager; script ref 0x1034 (at +0xF88) arms the RB party
    // switch during split-party sections. It is not restored when a save loads.
    constexpr uint32_t EventVmSlot = 0x831F1B60;
    constexpr uint32_t PartySwitchFlag = 0xF88;
    std::atomic<bool> partySwitchRequested{false};
    uint32_t EventVm(uint8_t* base)
    {
        const uint32_t vm = PPC_LOAD_U32(EventVmSlot);
        return vm >= 0x100000 && !(vm & 3) ? vm : 0;
    }
    // A save made while the party is split cannot restore RB switching, so
    // Save Anywhere leaves the game's own Save permission in place there.
    bool PartySplit(uint8_t* base)
    {
        const uint32_t vm = EventVm(base);
        return vm && PPC_LOAD_U32(vm + PartySwitchFlag) == 1;
    }
    // Only the guest menu thread reads/writes these fields and the menu table.
    bool known = false;
    uint32_t originalEnabled = 0, lastWritten = 0;

    void PollRequest()
    {
        static const char* path = getenv("LO_SAVE_ANYWHERE_REQUEST");
        if (!path || !*path) return;
        static auto next = std::chrono::steady_clock::time_point{};
        const auto now = std::chrono::steady_clock::now();
        if (now < next) return;
        next = now + std::chrono::milliseconds(250);
        static uint64_t previous = 0;
        uint64_t serial = 0;
        int enabled = -1;
        std::ifstream input(path);
        if ((input >> serial >> enabled) && serial && serial != previous &&
            (enabled == 0 || enabled == 1))
        {
            previous = serial;
            debug_menu::SetSaveAnywhereEnabled(enabled != 0);
        }
    }

    void Apply(uint8_t* base, bool gameUpdated = false)
    {
        if (PPC_LOAD_U32(SaveRow + 4) != 34) return;
        const uint32_t flags = PPC_LOAD_U32(SaveRow);
        if (!known || gameUpdated || flags != lastWritten)
        {
            originalEnabled = flags & Enabled;
            known = true;
        }
        const bool allow = Requested().load(std::memory_order_relaxed) && (flags & Visible) && !PartySplit(base);
        lastWritten = (flags & ~Enabled) | (allow ? Enabled : originalEnabled);
        PPC_STORE_U32(SaveRow, lastWritten);
    }
}

bool debug_menu::SaveAnywhereEnabled()
{
    return Requested().load(std::memory_order_relaxed);
}

void debug_menu::SetSaveAnywhereEnabled(bool enabled)
{
    Requested().store(enabled, std::memory_order_relaxed);
    if (!settings::SaveSaveAnywhere(enabled))
        LOG_WARNING("debug menu: failed to persist save anywhere setting");
    LOG_INFO("debug menu: save anywhere {} (reopen System menu to refresh)", enabled);
}

void debug_menu::RequestPartySwitch()
{
    partySwitchRequested.store(true, std::memory_order_relaxed);
}

// Called from the engine tick on the guest game thread.
void debug_menu::PartySwitchTick(uint8_t* base)
{
    if (!partySwitchRequested.exchange(false, std::memory_order_relaxed)) return;
    const uint32_t vm = EventVm(base);
    if (!vm)
    {
        LOG_WARNING("debug menu: party switch unavailable (event VM missing)");
        return;
    }
    const uint32_t previous = PPC_LOAD_U32(vm + PartySwitchFlag);
    PPC_STORE_U32(vm + PartySwitchFlag, 1);
    LOG_INFO("debug menu: party switch armed (was {})", previous);
}

// Preserve the latest game-authored permission, including save-point changes.
PPC_FUNC(sub_82876EA8)
{
    const bool save = ctx.r4.s32 == 34;
    __imp__sub_82876EA8(ctx, base);
    if (save) Apply(base, true);
}

// Apply UI requests on the guest thread before the native menu consumes input.
PPC_FUNC(sub_822E0E10)
{
    PollRequest();
    Apply(base);
    const debug_menu::menu_animations::ScopedCampUpdate animations(ctx.r3.u32);
    __imp__sub_822E0E10(ctx, base);
}
