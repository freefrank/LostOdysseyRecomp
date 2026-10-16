#pragma once

#include <cstdint>

namespace debug_menu::soft_reset
{
enum class State { Unavailable, Ready, Loading, Saving, StorageMenu, Title, Pending, Returning };

struct Snapshot
{
    uint32_t engine = 0, world = 0, title = 0, reset = 0;
    uint32_t storageStage = 0, pendingMap = 0;
    bool storageIoBusy = false, loading = false, settingsOpen = false, debugPending = false;
};

inline State Availability(const Snapshot& s)
{
    if (s.reset) return State::Returning;
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
