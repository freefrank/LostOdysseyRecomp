#pragma once
#include <filesystem>
#include <string>

// iCloud save sync between the iOS and macOS builds (LO_ICLOUD_CONTAINER).
//
// The local save folder stays the game's working copy. Each save slot (a
// folder such as save/user00) is mirrored to <container>/Documents/Saves:
// at startup the newer copy of every slot wins in both directions, and after
// each save (XamContentClose) the slot is copied to iCloud in the background.
// A local slot replaced by a newer iCloud copy is first moved to
// save/.cloud-backup/. Builds without a container, and other platforms,
// compile to no-ops.
namespace os::cloud_saves
{
struct Status
{
    bool supported = false;  // built with an iCloud container
    bool enabled = false;    // Settings: icloud_saves
    bool available = false;  // iCloud signed in and the container reachable
    std::string lastSync;    // human-readable summary of the last sync
};

// Blocking: resolves the container (on iOS this can take a few seconds on the
// first call) and reconciles every slot. Call before the guest lists saves.
void Start(const std::filesystem::path& localSaves);
// Copies one slot folder to iCloud on a worker thread.
void PushSlot(const std::filesystem::path& slot);
Status GetStatus();
}
