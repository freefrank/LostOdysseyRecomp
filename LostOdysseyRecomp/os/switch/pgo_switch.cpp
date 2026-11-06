// Training builds only (cmake -DLO_SWITCH_PGO=GEN, docs/SWITCH-PGO.md): the debug server's "pgo" command, from SwitchWakerHD's
// runtime/src/platform/pgo_switch.cpp. libgcov keeps the counters in memory. "pgo dump" writes them to
// sdmc:/switch/LostOdysseyRecomp/pgo/*.gcda, adding to what an earlier dump left there (libgcov merges), and zeroes
// them, so every scene is counted once.
#include <os/platform.h>
#if LO_PLATFORM_SWITCH && defined(LO_SWITCH_PGO_GEN)
#include "debug_server_core.h"
#include <sys/stat.h>
#include <cstdlib>
#include <string>

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
int g_dumps = 0;

// The folder must exist: libgcov's own mkdir does not cope with the "sdmc:" device.
void PrepareFolder()
{
    setenv("GCOV_PREFIX", kPrefix, 1);
    setenv("GCOV_PREFIX_STRIP", "0", 1);
    mkdir(kFolder, 0777);
}

debugsrv::Reply Command(const debugsrv::Args& args)
{
    const std::string what = args.empty() ? "status" : args[0];
    if (what == "reset")
    {
        __gcov_reset();
        return debugsrv::ok("pgo: counters zeroed");
    }
    if (what == "dump")
    {
        PrepareFolder();
        __gcov_dump();
        __gcov_reset();
        ++g_dumps;
        return debugsrv::ok("pgo: dumped to " + std::string(kFolder) + " (dump " + std::to_string(g_dumps) + " of this session)");
    }
    if (what == "status")
        return debugsrv::ok("pgo: training build, " + std::to_string(g_dumps) + " dumps this session");
    return debugsrv::err("pgo dump | reset | status");
}
}

void RegisterCommands()
{
    PrepareFolder();
    debugsrv::add_command("pgo", "pgo dump | reset | status  (training build) write / zero the profile counters", Command);
}
}
#endif
