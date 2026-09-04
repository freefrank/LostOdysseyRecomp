#include <stdafx.h>
#include <cpu/guest_thread.h>
#include <kernel/function.h>
#include <kernel/memory.h>
#include <kernel/heap.h>
#include <kernel/xex_loader.h>
#include <kernel/xam.h>
#include <kernel/io/file_system.h>
#include <gpu/command_processor.h>
#include <apu/audio.h>
#include <apu/xma.h>
#include <hid/hid.h>
#include <os/logger.h>
#include <cstring>
#include <ctime>
#include <chrono>

#ifdef _WIN32
#include <timeapi.h>
#endif

// Runtime entry: set up guest memory, load default.xex and run its entry point
// on the first guest thread. Everything else is driven by the game through the
// kernel imports (see kernel/imports.cpp).

static std::filesystem::path FindGameRoot(int argc, char* argv[])
{
    for (int i = 1; i + 1 < argc; i++)
    {
        if (strcmp(argv[i], "--game") == 0)
            return argv[i + 1];
    }

    // Default: the extracted disc 1 next to the executable, or the dev tree.
    const char* candidates[] = { "game", "../../../LostOdysseyRecompLib/private/disc1", "LostOdysseyRecompLib/private/disc1" };
    for (auto c : candidates)
    {
        std::error_code ec;
        if (std::filesystem::exists(std::filesystem::path(c) / "default.xex", ec))
            return std::filesystem::absolute(c);
    }
    return "game";
}

void InstallCrashHandler();
void InstallPhysicalWatchpoint();

int main(int argc, char* argv[])
{
    InstallCrashHandler();
#ifdef _WIN32
    timeBeginPeriod(1);
#endif

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--quiet-kernel") == 0)
            os::logger::g_kernelTrace = false;
    }

    {
        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char stamp[64] = {};
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);
        std::string cmdline;
        for (int i = 0; i < argc; i++)
            cmdline += fmt::format("{}{}", i ? " " : "", argv[i]);
        LOG_INFO("LostOdysseyRecomp starting at {} : {}", stamp, cmdline);
        std::string switches;
        for (char** e = environ; e && *e; e++)
            if (strncmp(*e, "LO_", 3) == 0)
                switches += fmt::format(" {}", *e);
        LOG_INFO("LO_* switches:{}", switches.empty() ? " (none)" : switches.c_str());
    }

    if (g_memory.base == nullptr)
    {
        LOG_ERROR("failed to reserve the 4 GiB guest address space");
        return 1;
    }
    InstallPhysicalWatchpoint();

    g_userHeap.Init();
    g_pageAllocator.Init();

    const auto gameRoot = FindGameRoot(argc, argv);
    LOG_INFO("game root: {}", gameRoot.string());

    FileSystem::Init(gameRoot);
    XamInit();

    uint32_t entry = XexLoader::Load(gameRoot / "default.xex");
    if (entry == 0)
        return 1;

    XexLoader::StartTimeStampThread();
    gpu::g_commandProcessor.Init();
    apu::Init();
    apu::xma::Init();
    if (getenv("LO_HEADLESS"))
        hid::Init(); // otherwise the video thread initialises it

    LOG_INFO("starting guest at {:#x}", entry);
    GuestThread::Start({ entry, 0, 0 });

    LOG_INFO("guest main thread returned");
    return 0;
}
