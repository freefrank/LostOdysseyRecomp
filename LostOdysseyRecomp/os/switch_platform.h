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

    // Process memory and thread usage, for failure logs ("used/total MiB ...").
    std::string MemorySummary();

    // Handheld only: asks apm for GPU 460.8 MHz with the memory clock kept at
    // 1331.2 MHz (configuration 0x92220008, what commercial games use; from
    // UnleashedRecomp-NX, ChanseyIsTheBest's fork). Reverts if the memory clock
    // moves. Runs on its own thread; logs the result to stderr.
    void StartHandheldGpuBoost();

    // Horizon thread priority of the calling thread (0x2C is above the 0x3B
    // every pthread gets; lower numbers run first).
    void SetCurrentThreadPriority(int priority);

    // Raises the CPU clock while loading (applet CPU boost), off when false.
    void SetLoadingBoost(bool enabled);
}
#endif
