#pragma once

#include <cstdint>

namespace debug_menu::soft_reset
{
enum class State { Unavailable, Ready, Loading, Saving, StorageMenu, Title, Pending, Returning, Battle };

struct Snapshot
{
    uint32_t engine = 0, world = 0, title = 0, reset = 0;
    uint32_t storageStage = 0, pendingMap = 0;
    uint32_t worldMode = 0, activeWorldMode = 0;
    bool battleQueued = false, storageIoBusy = false, loading = false, settingsOpen = false, debugPending = false;
};

inline State Availability(const Snapshot& s)
{
    if (s.reset) return State::Returning;
    // Native sub_829E54F0's scene predicate, plus the existing Loader battle
    // slot for requests that have not committed a scene mode yet.
    if (s.battleQueued || s.worldMode == 3 || s.activeWorldMode == 3) return State::Battle;
    if (s.storageIoBusy) return State::Saving;
    if (s.storageStage > 1) return State::StorageMenu;
    if (s.loading || s.pendingMap) return State::Loading;
    if (s.title) return State::Title;
    if (s.settingsOpen || s.debugPending || !s.engine || !s.world) return State::Unavailable;
    return State::Ready;
}

// The request must still belong to the engine/world that the overlay observed.
inline bool SameScene(const Snapshot& seen, const Snapshot& current)
{
    return seen.engine == current.engine && seen.world == current.world;
}
}
