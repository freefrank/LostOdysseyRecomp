#include <stdafx.h>
#include "mod_hot_reload.h"
#include <host_ui/host_ui.h>
#include <kernel/io/file_system.h>
#include <modding/mod_api.h>
#include <modding/text_overlay.h>
#include <os/logger.h>

#include <chrono>
#include <mutex>

// The game's disc change (see docs/notes/disc-selection.md):
// - sub_82821E90(manager, disc) asks for a disc. For the disc the manager
//   already has (+8) it does nothing unless +20 is set: the retry flag of a
//   change whose mount failed. Then +15 is set too, which skips the music fade
//   and the waits of the change.
// - The manager's update (sub_82320428, every frame from sub_8231EE68) closes
//   every open member file and archive (sub_82854C10), calls XamSwapDisc (the
//   host selects the same folder and signals at once), reads LO.FPI again
//   (sub_82854CF8 -> sub_82854400), reopens the archives and clears the package
//   cache (*0x83302A18 vtable[0]). Then +20 and +24 are 0 again.
// text_overlay switches to the prepared files when LO.FPI is opened again, so
// the new index meets only new archive handles.
PPC_FUNC(sub_82821E90);

namespace mod_hot_reload
{
namespace
{
constexpr uint32_t kDiscManager = 0x832640B0; // +4 change event, +8 disc, +20 retry, +24 state
constexpr uint32_t kArchives = 0x83264D90;    // +0 mounted
constexpr uint32_t kPendingMap = 0x83263E50;  // a map change is pending
// The game normally finishes within a second; past this the request is retried.
constexpr auto kReloadTimeout = std::chrono::seconds(20);

using State = modding::text_overlay::ReloadState;
std::mutex mutex;
Phase phase = Phase::Idle;
std::filesystem::path folder; // the disc being prepared
std::chrono::steady_clock::time_point asked;
uint32_t attempts = 0;

void Set(Phase next)
{
    std::lock_guard lock(mutex);
    phase = next;
}

bool ManagerIdle(uint8_t *base)
{
    return !PPC_LOAD_U32(kDiscManager + 4) && !PPC_LOAD_U32(kDiscManager + 20) && !PPC_LOAD_U32(kDiscManager + 24);
}

bool Safe(uint8_t *base, bool playerInField)
{
    return playerInField && !host_ui::IsGamePaused() && !PPC_LOAD_U32(kPendingMap) && PPC_LOAD_U8(kArchives) && ManagerIdle(base);
}

void AskForSameDisc(PPCContext &ctx, uint8_t *base)
{
    const uint32_t disc = PPC_LOAD_U16(kDiscManager + 8);
    PPC_STORE_U32(kDiscManager + 20, 1);
    const PPCContext saved = ctx;
    ctx.r3.u64 = kDiscManager;
    ctx.r4.u64 = disc;
    sub_82821E90(ctx, base);
    const int32_t result = ctx.r3.s32;
    ctx = saved;
    LOG_NOTICE("[mods] live reload: the game reloads the index of disc {} (request {})", disc, result);
}
} // namespace

bool Request()
{
    if (!modding::Enabled()) return false;
    const auto disc = FileSystem::GetDiscRoot();
    modding::Reload();
    modding::text_overlay::PrepareReload(disc);
    std::lock_guard lock(mutex);
    folder = (disc / "LO.fpi").parent_path().lexically_normal();
    phase = Phase::Preparing;
    attempts = 0;
    return true;
}

Status GetStatus()
{
    Status status;
    {
        std::lock_guard lock(mutex);
        status.phase = phase;
    }
    if (status.phase == Phase::Preparing)
    {
        const auto prepared = modding::text_overlay::GetReloadStatus();
        status.done = prepared.done;
        status.total = prepared.total;
    }
    return status;
}

void Tick(PPCContext &ctx, uint8_t *base, bool playerInField)
{
    Phase current;
    std::filesystem::path prepared;
    {
        std::lock_guard lock(mutex);
        current = phase;
        prepared = folder;
    }
    if (current == Phase::Idle || current == Phase::Done || current == Phase::Unchanged || current == Phase::Failed) return;
    const auto status = modding::text_overlay::GetReloadStatus();
    switch (status.state)
    {
    case State::Preparing:
        return;
    case State::Unchanged:
        modding::text_overlay::EndReload();
        Set(Phase::Unchanged);
        return;
    case State::Failed:
        LOG_WARNING("[mods] live reload failed: {}", status.error);
        modding::text_overlay::EndReload();
        Set(Phase::Failed);
        return;
    case State::Ready:
        if (current == Phase::Preparing)
        {
            Set(Phase::Waiting);
            LOG_NOTICE("[mods] live reload: waiting for the player to walk a field map with nothing loading");
        }
        // A disc change since the request: prepare the disc the game has now.
        if ((FileSystem::GetDiscRoot() / "LO.fpi").parent_path().lexically_normal() != prepared)
        {
            LOG_NOTICE("[mods] live reload: the disc changed; preparing the new one");
            Request();
            return;
        }
        if (!Safe(base, playerInField)) return;
        if (++attempts > 3)
        {
            LOG_WARNING("[mods] live reload: the game did not reload its index; the mods apply at the next start");
            modding::text_overlay::EndReload();
            Set(Phase::Failed);
            return;
        }
        modding::text_overlay::ArmReload();
        asked = std::chrono::steady_clock::now();
        Set(Phase::Reloading);
        AskForSameDisc(ctx, base);
        return;
    case State::Armed:
        if (std::chrono::steady_clock::now() - asked > kReloadTimeout && ManagerIdle(base))
        {
            LOG_WARNING("[mods] live reload: the game did not open the index of {} again within {} s", FileSystem::PathUtf8(prepared),
                        kReloadTimeout.count());
            modding::text_overlay::DisarmReload();
            Set(Phase::Waiting);
        }
        return;
    case State::Committed:
        if (!ManagerIdle(base)) return;
        LOG_NOTICE("[mods] live reload done in {:.1f} s; areas loaded from now on use the new mods",
                   std::chrono::duration<double>(std::chrono::steady_clock::now() - asked).count());
        modding::text_overlay::EndReload();
        Set(Phase::Done);
        return;
    case State::Idle:
        Set(Phase::Idle);
        return;
    }
}
} // namespace mod_hot_reload
