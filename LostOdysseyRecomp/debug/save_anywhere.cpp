#include <stdafx.h>
#include <os/logger.h>
#include "save_anywhere.h"
#include <fstream>

extern "C" PPC_FUNC(__imp__sub_822E0E10);
extern "C" PPC_FUNC(__imp__sub_82876EA8);

namespace
{
    constexpr uint32_t SaveRow = 0x8326D690;
    constexpr uint32_t Visible = 0x80000000;
    constexpr uint32_t Enabled = 0x40000000;
    std::atomic<bool> requested{false};
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
        const bool allow = requested.load(std::memory_order_relaxed) && (flags & Visible);
        lastWritten = (flags & ~Enabled) | (allow ? Enabled : originalEnabled);
        PPC_STORE_U32(SaveRow, lastWritten);
    }
}

bool debug_menu::SaveAnywhereEnabled()
{
    return requested.load(std::memory_order_relaxed);
}

void debug_menu::SetSaveAnywhereEnabled(bool enabled)
{
    requested.store(enabled, std::memory_order_relaxed);
    LOG_INFO("debug menu: save anywhere {} (reopen System menu to refresh)", enabled);
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
    __imp__sub_822E0E10(ctx, base);
}
