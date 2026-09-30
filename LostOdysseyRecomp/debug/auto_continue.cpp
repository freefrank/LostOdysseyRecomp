#include <stdafx.h>
#include <os/logger.h>
#include "auto_continue.h"
#include <chrono>
#include <cstdlib>
#include <cstring>

extern "C" PPC_FUNC(__imp__sub_8234B9B8);
extern "C" PPC_FUNC(__imp__sub_8282B540);

bool debug_menu::AutoContinueEnabled()
{
    static const bool enabled = [] {
        const char* value = std::getenv("LO_DEBUG_AUTO_CONTINUE");
        return value && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

namespace
{
    // All mutable state belongs to the native title/menu guest thread.
    enum class Step { Start, Menu, Saves, Loading, Done };
    Step step = Step::Start;
    uint32_t owner = 0;
    uint32_t previousState = ~0u;
    bool loadStarted = false;
    std::chrono::steady_clock::time_point began;
    constexpr uint32_t SaveManager = 0x832688E8;

    bool GuestObject(uint32_t address)
    {
        return address >= 0x100000 && address < 0x7C000000 && (address & 3) == 0;
    }

    void Action(PPCContext& ctx, uint8_t* base, uint32_t boot, uint32_t action)
    {
        // Invoke the same native menu action as a selection, on its own guest
        // thread/stack. Restore the caller's complete register context afterwards.
        const PPCContext saved = ctx;
        ctx.r3.u64 = boot;
        ctx.r4.u64 = action;
        __imp__sub_8282B540(ctx, base);
        ctx = saved;
    }

    void Stop(uint32_t state, const char* reason)
    {
        LOG_INFO("debug auto-continue: stopped at title state {:#x}: {}; native menu remains in control", state, reason);
        step = Step::Done;
    }

    void Advance(PPCContext& ctx, uint8_t* base)
    {
        if (step == Step::Done) return;
        const uint32_t boot = ctx.r3.u32;
        if (!GuestObject(boot)) return;
        const uint32_t state = PPC_LOAD_U32(boot + 0x14);
        if (!owner) {
            owner = boot;
            began = std::chrono::steady_clock::now();
            LOG_INFO("debug auto-continue: enabled, waiting for native title initialization (object {:#x})", boot);
        }
        if (owner != boot) {
            Stop(state, "title object changed");
            return;
        }
        if (state != previousState) {
            LOG_INFO("debug auto-continue: title state {:#x} -> {:#x}", previousState, state);
            previousState = state;
        }
        if (std::chrono::steady_clock::now() - began > std::chrono::seconds(120)) {
            Stop(state, "native startup/load did not complete within 120 seconds");
            return;
        }

        if (step == Step::Start && state == 6) {
            // A real Start input also selects its local player. The request byte
            // alone leaves player+0x60 == -1, so the native profile initialization
            // asks to sign in even though XAM already exposes offline user 0.
            // Bind that existing local user before state 6 passes it to 82DCF580.
            const uint32_t play = PPC_LOAD_U32(0x83315FB4);
            if (!GuestObject(play)) return;
            const uint32_t players = PPC_LOAD_U32(play + 0x2B8);
            if (!GuestObject(players)) return;
            const uint32_t player = PPC_LOAD_U32(players);
            if (!GuestObject(player)) return;
            if (PPC_LOAD_U32(player + 0x60) == ~0u) {
                PPC_STORE_U32(player + 0x60, 0);
                LOG_INFO("debug auto-continue: bound existing offline user 0 to the native start request");
            }
            // This is the existing non-input start request. The original branch
            // still initializes profile/storage through states 0x54..0x57.
            PPC_STORE_U8(boot + 0xBC, 1);
            step = Step::Menu;
            LOG_INFO("debug auto-continue: requested native start initialization");
        }
        if ((step == Step::Start || step == Step::Menu) && state == 8) {
            // Action 8 performs native device selection and save enumeration.
            // Do not jump directly to the load manager before those complete.
            Action(ctx, base, boot, 8);
            step = Step::Saves;
            LOG_INFO("debug auto-continue: requested native save enumeration");
            return;
        }
        if (step == Step::Saves && state == 0x19) {
            const uint32_t device = PPC_LOAD_U32(boot + 0xCC);
            const int32_t newest = int32_t(PPC_LOAD_U32(boot + 0xD0));
            if (!device || newest < 0) {
                Stop(state, "no available device or newest save");
                return;
            }
            if (PPC_LOAD_U32(SaveManager + 0x14) != 1) return;
            // Action 0xC preserves fade, mode 3 Continue, async load, result
            // handling and scene entry. Modes 0/1/4/6 are SAVE paths, never used.
            Action(ctx, base, boot, 0xC);
            step = Step::Loading;
            LOG_INFO("debug auto-continue: requested native newest-save Continue (device {}, enumerated index {})", device, newest);
            return;
        }
        if (step == Step::Saves && (state == 5 || state == 6 || state == 8 ||
            state == 0x1C || state == 0x1E || state == 0x20 || state == 0x21)) {
            Stop(state, "save enumeration returned to the menu or reported unavailable content");
            return;
        }
        if (step == Step::Loading) {
            // Action 0xC first fades the title. Only inspect the result after
            // state 0x15 has actually submitted mode 3 and reset its result.
            if (state == 0x17 && PPC_LOAD_U32(SaveManager + 0xC) == 3)
                loadStarted = true;
            if (!loadStarted) return;
            const uint32_t result = PPC_LOAD_U32(SaveManager + 0x30);
            if (result == 1) {
                LOG_INFO("debug auto-continue: native save load succeeded; native scene transition continues");
                step = Step::Done;
            }
            else if (result >= 2 && result <= 4) {
                Stop(state, "native Continue failed or was cancelled");
            }
        }
    }
}

PPC_FUNC(sub_8234B9B8)
{
    if (debug_menu::AutoContinueEnabled()) Advance(ctx, base);
    __imp__sub_8234B9B8(ctx, base);
}
