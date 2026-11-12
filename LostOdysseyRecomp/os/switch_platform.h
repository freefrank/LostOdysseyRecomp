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

    // The console's system language is Spanish (Spain or Latin America). False if set is not available.
    bool SystemLanguageIsSpanish();

    // The libnx operation-mode hook only records a notification. Consume it
    // from the SDL event pump after appletMainLoop has processed the message.
    bool TakeOperationModeChanged();

    // Process memory and thread usage, for failure logs ("used/total MiB ...").
    std::string MemorySummary();
    // Appended to MemorySummary when set (LO_DEKO3D_HEAP_STATS: the deko3d back end's heap blocks, issue #3).
    void SetMemorySummaryExtra(std::string (*extra)());
    // Bytes of newlib's heap in use and the most it can grow to (MemorySummary's "heap in use").
    void HeapUsage(uint64_t& used, uint64_t& limit);

    // The CPU, GPU and memory clocks right now, in Hz (clkrst). A few IPC calls each time: not for every frame.
    // False (the ones not read left at 0) when clkrst is not available.
    bool ReadClockRates(uint32_t& cpuHz, uint32_t& gpuHz, uint32_t& memoryHz);

    // Handheld only: asks apm for GPU 460.8 MHz with the memory clock kept at
    // 1331.2 MHz (configuration 0x92220008, what commercial games use; from
    // UnleashedRecomp-NX, ChanseyIsTheBest's fork). Reverts if the memory clock
    // moves. Runs on its own thread; logs the result to stderr. A game started
    // docked asks at the first undock. ExitNow puts the configuration back.
    void StartHandheldGpuBoost();

    // Why the screen stays on although nobody touches the controls. The system
    // dims the screen and sleeps after its own idle settings otherwise. Every
    // reason turns auto-sleep off and counts as player activity every 15 s
    // (idle:sys), which keeps the screen from dimming; Movie and Cutscene also
    // set the applet's media playback state. When a reason ends the idle time
    // starts again. Initialize sets Startup.
    enum AwakeReason : uint32_t
    {
        AwakeStartup = 1,     // until RunGuest: game files, XEX, install
        AwakeLoading = 2,     // a map or battle load (debug_menu::CurrentLoad)
        AwakePreparing = 4,   // the preparation screen (shaders, mods)
        AwakeMovie = 8,       // a cutscene video (movie::GuestPlaying)
        AwakeCutscene = 16,   // an in-engine cutscene (gpu::cinematic_bars::CutsceneActive)
        AwakeDebugClient = 32, // a debug-server client in the last few minutes (unattended tools)
        AwakeTestWatching = 64, // the debug server's "screen watch on": as Movie, for tests
        AwakeTestBusy = 128     // "screen busy on": as Loading, for tests
    };
    // Sets the reasons in mask to those in reasons. Any thread, as often as wanted: only changes reach the applet.
    // Nothing changes with LO_SWITCH_AUTO_SLEEP=0, where auto-sleep is off for the whole session.
    void SetScreenAwake(uint32_t mask, uint32_t reasons);
    // For the debug server's info: the reasons and what the applet was asked.
    std::string ScreenSummary();

    // Horizon thread priority of the calling thread (0x2C is above the 0x3B
    // every pthread gets; lower numbers run first).
    void SetCurrentThreadPriority(int priority);
    int CurrentThreadPriority(); // -1 if the priority cannot be read
    uint32_t SetThreadPriority(uint32_t handle, int priority);

    // Kernel handle of the calling thread, and its ideal core (0-2; allowed
    // cores stay 0-2). handle 0 means the calling thread.
    uint32_t CurrentThreadHandle();
    uint32_t SetThreadIdealCore(uint32_t handle, int core);
    void PinCurrentThread(int core);
    void SleepNanoseconds(int64_t nanoseconds);

    // Appends to a file in the app folder through a file system session of this module's own, opened
    // in Initialize: no newlib, no allocation and none of libnx's shared file system sessions, so it
    // still works when the rest of the process's file access is stuck (crash handler, hang watchdog).
    // False if the session is not available or a request failed.
    bool RawAppend(const char* fileName, const char* data, size_t size);

    // Kernel handle of the calling thread, and its ideal core (0-2; allowed
    // cores stay 0-2). handle 0 means the calling thread.
    uint32_t CurrentThreadHandle();
    uint32_t SetThreadIdealCore(uint32_t handle, int core);
    void PinCurrentThread(int core);
    void SleepNanoseconds(int64_t nanoseconds);

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
