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

    // The libnx operation-mode hook only records a notification. Consume it
    // from the SDL event pump after appletMainLoop has processed the message.
    bool TakeOperationModeChanged();

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
    int CurrentThreadPriority(); // -1 if the priority cannot be read
    uint32_t SetThreadPriority(uint32_t handle, int priority);

    // Kernel handle of the calling thread, and its ideal core (0-2; allowed
    // cores stay 0-2). handle 0 means the calling thread.
    uint32_t CurrentThreadHandle();
    uint32_t SetThreadIdealCore(uint32_t handle, int core);

    // Appends to a file in the app folder through a file system session of this module's own, opened
    // in Initialize: no newlib, no allocation and none of libnx's shared file system sessions, so it
    // still works when the rest of the process's file access is stuck (crash handler, hang watchdog).
    // False if the session is not available or a request failed.
    bool RawAppend(const char* fileName, const char* data, size_t size);

    // Kernel handle of the calling thread, and its ideal core (0-2; allowed
    // cores stay 0-2). handle 0 means the calling thread.
    uint32_t CurrentThreadHandle();
    uint32_t SetThreadIdealCore(uint32_t handle, int core);

    // Raises the CPU clock while loading (applet CPU boost), off when false.
    void SetLoadingBoost(bool enabled);

    // Closing the game from HOME asks it to quit (SDL_EVENT_QUIT, the normal
    // shutdown that tears down the GPU) instead of killing it: a killed game
    // left the console unable to start it again until a reboot. Initialize
    // holds the exit; call this right before the process ends. Horizon kills
    // the game anyway 15 s after the request.
    void AllowExit();
    // Ends the process at once after writing the logs (no newlib/libnx exit
    // teardown): quitting through std::_Exit unmounted the SD card while game
    // threads were still reading it, and one faulted in lseek.
    [[noreturn]] void ExitNow();
}
#endif
