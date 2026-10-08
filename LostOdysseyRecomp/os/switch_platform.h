#pragma once

// Nintendo Switch platform services (os/switch/switch_runtime.cpp). This header
// does not include <switch.h>: libnx's Mutex/Event/Thread typedefs collide with
// the runtime's own types, so only os/switch/*.cpp include it.
#include <os/platform.h>

#if LO_PLATFORM_SWITCH
#include <cstdint>
#include <filesystem>
#include <string>

namespace os::switch_platform
{
    // Creates the app folder on the SD card, sends stderr (renderer, Mesa and
    // NVK diagnostics) to <dataRoot>/stderr.log and arms the crash log. Call
    // first thing in main().
    void Initialize(const std::filesystem::path& dataRoot);

    // Shows the system error dialog (blocking) with a short text and a full
    // page of detail. Used when the game cannot start at all.
    void ShowError(const std::string& summary, const std::string& details);

    // Docked (true) or handheld (false).
    bool IsDocked();

    // Raises the CPU clock while loading (applet CPU boost), off when false.
    void SetLoadingBoost(bool enabled);
}
#endif
