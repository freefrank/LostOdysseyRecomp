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
#include <hid/hid.h>
#include <os/logger.h>

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
    hid::Init();

    LOG_INFO("starting guest at {:#x}", entry);
    GuestThread::Start({ entry, 0, 0 });

    LOG_INFO("guest main thread returned");
    return 0;
}
