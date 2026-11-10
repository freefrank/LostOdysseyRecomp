// Training builds only (cmake -DLO_SWITCH_PGO=GEN, docs/SWITCH-PGO.md), from SwitchWakerHD's
// runtime/src/platform/pgo_switch.cpp. libgcov keeps the counters in memory; a save writes them to
// sdmc:/switch/LostOdysseyRecomp/pgo/*.gcda, adding to what an earlier save left there (libgcov merges, also across
// sessions), and zeroes them, so nothing is counted twice.
//
// Saves happen on their own, so a profile can be recorded by just playing, with no PC on the network:
//   - every LO_PGO_SAVE_SECONDS seconds of play (default 60; 0 saves only on quit), on a thread of their own;
//   - when the game is quit from its menus (ExitNow). Closing it from the HOME menu loses the time since the last save.
// Each save adds a line to pgo/pgo.log. The debug server's "pgo dump | reset | status" (tools/switch/pgo_train.py)
// still works next to it.
#include <os/platform.h>
#if LO_PLATFORM_SWITCH && defined(LO_SWITCH_PGO_GEN)
#include "debug_server_core.h"
#include <os/logger.h>
#include <sys/stat.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

extern "C"
{
void __gcov_dump(void);
void __gcov_reset(void);
}

namespace os::pgo
{
namespace
{
constexpr const char* kPrefix = "sdmc:/switch/LostOdysseyRecomp";
constexpr const char* kFolder = "sdmc:/switch/LostOdysseyRecomp/pgo";
constexpr const char* kLog = "sdmc:/switch/LostOdysseyRecomp/pgo/pgo.log";
std::mutex g_mutex;
int g_dumps = 0;
const auto g_start = std::chrono::steady_clock::now();

// The folder must exist: libgcov's own mkdir does not cope with the "sdmc:" device.
void PrepareFolder()
{
    setenv("GCOV_PREFIX", kPrefix, 1);
    setenv("GCOV_PREFIX_STRIP", "0", 1);
    mkdir(kFolder, 0777);
}

double Seconds()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - g_start).count();
}

// Writes the counters to the SD card and zeroes them. Returns the save's number in this session.
int Save(const char* why)
{
    std::lock_guard lock(g_mutex);
    PrepareFolder();
    const auto started = std::chrono::steady_clock::now();
    __gcov_dump();
    __gcov_reset();
    const int number = ++g_dumps;
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    if (FILE* log = std::fopen(kLog, "a"))
    {
        std::fprintf(log, "save %d (%s) at %.0f s of the session, %.0f ms\n", number, why, Seconds(), ms);
        std::fclose(log);
    }
    return number;
}

debugsrv::Reply Command(const debugsrv::Args& args)
{
    const std::string what = args.empty() ? "status" : args[0];
    if (what == "reset")
    {
        std::lock_guard lock(g_mutex);
        __gcov_reset();
        return debugsrv::ok("pgo: counters zeroed");
    }
    if (what == "dump")
    {
        const int number = Save("debug server");
        return debugsrv::ok("pgo: dumped to " + std::string(kFolder) + " (dump " + std::to_string(number) + " of this session)");
    }
    if (what == "status")
    {
        std::lock_guard lock(g_mutex);
        return debugsrv::ok("pgo: training build, " + std::to_string(g_dumps) + " dumps this session");
    }
    return debugsrv::err("pgo dump | reset | status");
}
}

void RegisterCommands()
{
    PrepareFolder();
    debugsrv::add_command("pgo", "pgo dump | reset | status  (training build) write / zero the profile counters", Command);
}

void StartAutoSave()
{
    PrepareFolder();
    const char* value = std::getenv("LO_PGO_SAVE_SECONDS");
    const long seconds = value && *value ? std::strtol(value, nullptr, 10) : 60;
    if (FILE* log = std::fopen(kLog, "a"))
    {
        std::fprintf(log, "session started: training build, saving %s\n",
                     seconds > 0 ? ("every " + std::to_string(seconds) + " s and on quit").c_str() : "on quit only");
        std::fclose(log);
    }
    LOG_NOTICE("pgo: training build, profile saved to {} {}", kFolder,
               seconds > 0 ? "every " + std::to_string(seconds) + " s and on quit" : std::string("on quit only"));
    if (seconds <= 0)
        return;
    std::thread([seconds] {
        for (;;)
        {
            std::this_thread::sleep_for(std::chrono::seconds(seconds));
            const int number = Save("timer");
            LOG_INFO("pgo: profile save {}", number);
        }
    }).detach();
}

void SaveOnExit()
{
    Save("quit");
}
}
#endif
