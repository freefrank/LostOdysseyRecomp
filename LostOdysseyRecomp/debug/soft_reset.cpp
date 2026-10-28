#include <stdafx.h>
#include <os/logger.h>
#include <host_ui/host_ui.h>
#include <settings/menu.h>
#include <settings/config.h>
#include "soft_reset.h"
#include "soft_reset_policy.h"
#include "fast_forward.h"
#include "battle_menu.h"
#include "teleport.h"
#include "cheats.h"
#include "translations.h"
#include <mutex>

extern "C" PPC_FUNC(__imp__sub_82820A10);

namespace debug_menu::soft_reset
{
namespace
{
constexpr uint32_t Engine = 0x83315FB4, World = 0x83318744;
constexpr uint32_t Title = 0x832644EC, Reset = 0x832631D0;
constexpr uint32_t Storage = 0x832688E8, Loader = 0x832631F0;
constexpr uint32_t StorageManager = 0x832EC774;
constexpr uint32_t PendingMap = 0x83263E50;
constexpr uint32_t AsyncTasks = 0x832640B0;
std::mutex mutex;
Snapshot observed;
State state = State::Unavailable;
uint64_t lastTick = 0;
uint32_t previousReset = ~0u;
bool returning = false;
State refusal = State::Ready;

const wchar_t* Message(State value)
{
    const wchar_t* key;
    switch (value)
    {
    case State::Battle: key = L"Not available during battle"; break;
    case State::Loading: key = L"Please wait for loading to finish"; break;
    case State::Saving: key = L"Please wait for saving to finish"; break;
    case State::StorageMenu: key = L"Close the save/load screen first"; break;
    case State::Title: key = L"Already at the title screen"; break;
    case State::Pending:
    case State::Returning: key = L"Returning to title"; break;
    default: key = L"Return to title unavailable"; break;
    }
    return translations::Text(key, settings::GetConfig().debugLanguage == 1);
}

bool Object(uint32_t address)
{
    return address >= 0x100000 && address < 0x7C000000 && !(address & 3);
}

Snapshot Read(uint8_t* base)
{
    Snapshot s;
    const uint32_t engine = PPC_LOAD_U32(Engine), world = PPC_LOAD_U32(World);
    s.engine = Object(engine) ? engine : 0;
    s.world = Object(world) ? world : 0;
    if (s.world)
    {
        // Native sub_829E54F0: both scene modes remain battle through the
        // victory/results UI and the return transition (not just core phase 1..10).
        s.worldMode = PPC_LOAD_U32(s.world + 472);
        s.activeWorldMode = PPC_LOAD_U32(s.world + 476);
    }
    s.battleQueued = int32_t(PPC_LOAD_U32(Loader + 3168 + 88)) > 0;
    s.title = PPC_LOAD_U32(Title);
    s.reset = PPC_LOAD_U32(Reset);
    // Match native sub_8229E4F0: stages 0/1 are inactive. Refuse the whole
    // save/load UI, including confirmations, rather than just its file write.
    s.storageStage = PPC_LOAD_U32(Storage + 20);
    // Match native sub_82DCB858 as well, so an asynchronous I/O operation
    // remains protected even if the screen has already started closing.
    const uint32_t storageManager = PPC_LOAD_U32(StorageManager);
    s.storageIoBusy = storageManager &&
        (PPC_LOAD_U8(storageManager + 197) == 2 || PPC_LOAD_U32(storageManager + 1632) == 2);
    s.pendingMap = PPC_LOAD_U32(PendingMap);
    // Reject queued travel too: stage 4/5 can still be activating a loaded
    // scene, beyond the narrower fast-loading pacing window.
    s.loading = PPC_LOAD_U8(Loader) != 0;
    for (uint32_t slot = 0; slot < 6; ++slot)
        s.loading |= int32_t(PPC_LOAD_U32(Loader + 3168 + 88 * slot)) > 0;
    s.loading |= PPC_LOAD_U32(AsyncTasks + 4) != 0 || PPC_LOAD_U32(AsyncTasks + 20) != 0;
    s.settingsOpen = settings::IsOpen();
    s.debugPending = TeleportRequestPending() || cheats::session.HasPending();
    return s;
}
}

bool Request()
{
    std::lock_guard lock(mutex);
    const uint64_t now = host_ui::GetActiveGameTimeMs();
    if (!host_ui::IsGamePaused() && (!lastTick || now - lastTick > 1000))
        state = State::Unavailable;
    // Other overlay pages can queue work while the guest is paused, after
    // the last snapshot. Do not race that work with a return-to-title request.
    if (TeleportRequestPending() || cheats::session.HasPending())
        state = State::Unavailable;
    if (state != State::Ready)
    {
        LOG_INFO("debug reset: unavailable state={} world_modes={}/{} battle_queued={} storage_stage={} storage_io={} loading={} pending_map={} title={:#x}",
            unsigned(state), observed.worldMode, observed.activeWorldMode, observed.battleQueued,
            observed.storageStage, observed.storageIoBusy, observed.loading, observed.pendingMap, observed.title);
        return false;
    }
    state = State::Pending;
    LOG_INFO("debug reset: requested return to title");
    return true;
}

const wchar_t* Status()
{
    std::lock_guard lock(mutex);
    return Message(state);
}

const wchar_t* TakeRefusal()
{
    std::lock_guard lock(mutex);
    if (refusal == State::Ready) return nullptr;
    const auto message = Message(refusal);
    refusal = State::Ready;
    return message;
}

void Tick(PPCContext& ctx, uint8_t* base)
{
    const Snapshot current = Read(base);
    const State available = Availability(current);
    bool apply = false;
    {
        std::lock_guard lock(mutex);
        if (returning && current.reset != previousReset)
        {
            LOG_INFO("debug reset: native state {} -> {} title={:#x}", previousReset, current.reset, current.title);
            previousReset = current.reset;
        }
        if (returning && !current.reset && current.title)
        {
            returning = false;
            LOG_INFO("debug reset: native title reached object={:#x}", current.title);
        }
        if (state == State::Pending)
        {
            apply = available == State::Ready && SameScene(observed, current);
            if (!apply)
            {
                refusal = available == State::Ready ? State::Unavailable : available;
                LOG_INFO("debug reset: refused on guest thread state={} world_modes={}/{} battle_queued={} storage_stage={} storage_io={} loading={} pending_map={} title={:#x}",
                    unsigned(available), current.worldMode, current.activeWorldMode, current.battleQueued,
                    current.storageStage, current.storageIoBusy, current.loading, current.pendingMap, current.title);
            }
        }
        state = returning ? State::Returning : available;
        observed = current;
        lastTick = host_ui::GetActiveGameTimeMs();
        if (apply)
        {
            returning = true;
            previousReset = 16;
            state = State::Returning;
        }
    }
    if (!apply) return;

    // Use the native global reset request, not a forced title/map pointer.
    // Engine sub_8231EE68 -> sub_82321058 advances states 16..21:
    // cancel scene/event tasks, stop sound classes 4/1/3, notify the current
    // game, finish asynchronous work and release audio, then sub_828295C8.
    // Keep the full context, including the owning engine's delta and stack.
    fast_forward::Release();
    CancelVictory();
    const PPCContext saved = ctx;
    ctx.r3.u64 = 16;
    __imp__sub_82820A10(ctx, base);
    ctx = saved;
    LOG_INFO("debug reset: entered native return-to-title request 16");
}
}
