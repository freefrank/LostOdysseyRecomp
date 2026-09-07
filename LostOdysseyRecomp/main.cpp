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
#include "settings/first_run.h"
#include "settings/config.h"

#ifdef _WIN32
#include <timeapi.h>
#include <shellapi.h>
#endif

// Runtime entry: set up guest memory, load default.xex and run its entry point
// on the first guest thread. Everything else is driven by the game through the
// kernel imports (see kernel/imports.cpp).

static std::filesystem::path FindGameRoot(int argc, char* argv[])
{
    for (int i = 1; i + 1 < argc; i++)
    {
        if (strcmp(argv[i], "--game") == 0)
            return std::filesystem::u8path(argv[i + 1]);
    }

    // Default: the extracted disc 1 next to the executable, or the dev tree.
    std::ifstream location("game-path.txt");
    std::string selected;
    if (std::getline(location,selected) && !selected.empty()) {
        if (selected.back()=='\r') selected.pop_back();
        const auto disc=std::filesystem::u8path(selected)/"disc1";
        if (std::filesystem::exists(disc/"default.xex")) return disc;
    }
    const char* candidates[] = { "game/disc1", "game", "../../../LostOdysseyRecompLib/private/disc1", "LostOdysseyRecompLib/private/disc1" };
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
#ifdef _WIN32
    // The CRT's narrow argv can best-fit Unicode (for example acute -> prime)
    // before we see it. Decode the original Windows command line instead.
    int wideArgc = 0;
    auto wideArgv = CommandLineToArgvW(GetCommandLineW(), &wideArgc);
    if (!wideArgv) return 1;
    std::vector<std::string> utf8Arguments;
    utf8Arguments.reserve(wideArgc);
    for (int i = 0; i < wideArgc; ++i)
        utf8Arguments.push_back(FileSystem::PathUtf8(std::filesystem::path(wideArgv[i])));
    LocalFree(wideArgv);
    std::vector<char*> argumentPointers;
    for (auto& argument : utf8Arguments) argumentPointers.push_back(argument.data());
    argumentPointers.push_back(nullptr);
    argc = wideArgc;
    argv = argumentPointers.data();
#endif
    bool explicitGame=false, requestedSetup=false, setupOnly=false;
    for(int i=1;i<argc;++i) {
        explicitGame |= strcmp(argv[i],"--game")==0;
        requestedSetup |= strcmp(argv[i],"--setup")==0 || strcmp(argv[i],"--setup-only")==0;
        setupOnly |= strcmp(argv[i],"--setup-only")==0;
    }
#ifdef _WIN32
    // Direct launches keep all portable data beside the executable. Explicit
    // --game launches retain their caller's working directory for isolated tests.
    if(!explicitGame) {
        wchar_t executable[32768]{};
        if(GetModuleFileNameW(nullptr,executable,32768))
            std::filesystem::current_path(std::filesystem::path(executable).parent_path());
    }
#endif
    // Keep each run separately, including launches without a terminal. Tests
    // can select a path or disable the duplicate sink with LO_LOG_FILE=0.
    const char* logOverride = getenv("LO_LOG_FILE");
    if (!logOverride || strcmp(logOverride, "0") != 0)
    {
        const auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const std::filesystem::path logPath = logOverride ? logOverride
            : fmt::format("logs/runtime-{}.log", ticks);
        std::error_code ec;
        if (logPath.has_parent_path())
            std::filesystem::create_directories(logPath.parent_path(), ec);
#ifdef _WIN32
        os::logger::g_file = _wfopen(logPath.c_str(), L"ab");
#else
        os::logger::g_file = fopen(logPath.c_str(), "ab");
#endif
        if (os::logger::g_file) LOG_INFO("log file: {}", FileSystem::PathUtf8(logPath));
        else LOG_WARNING("could not open log file: {}", FileSystem::PathUtf8(logPath));
    }
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

    auto gameRoot=FindGameRoot(argc,argv);
#ifdef _WIN32
    if(!explicitGame && !std::filesystem::exists(gameRoot/"default.xex") && std::filesystem::exists("InstallGame.exe")) {
        const auto installer=std::filesystem::absolute("InstallGame.exe").wstring();
        SHELLEXECUTEINFOW launch{sizeof(launch)};
        launch.fMask=SEE_MASK_NOCLOSEPROCESS; launch.lpFile=installer.c_str(); launch.nShow=SW_SHOWNORMAL;
        launch.lpParameters=L"--return-to-game";
        if(!ShellExecuteExW(&launch)) return 1;
        if(launch.hProcess) { WaitForSingleObject(launch.hProcess,INFINITE); CloseHandle(launch.hProcess); }
        gameRoot=FindGameRoot(argc,argv);
        if(!std::filesystem::exists(gameRoot/"default.xex")) return 0;
    }
#endif
    settings::ConfigureGameLanguages(gameRoot / "default.xex");
    if(requestedSetup || (!getenv("LO_BACKGROUND") && !getenv("LO_HEADLESS") && !std::filesystem::exists("settings.ini"))) {
        if(!settings::FirstRunSetup()) return 0;
        if(setupOnly) return 0;
    }
    if (g_memory.base == nullptr)
    {
        LOG_ERROR("failed to reserve the 4 GiB guest address space");
        return 1;
    }
    InstallPhysicalWatchpoint();

    g_userHeap.Init();
    g_pageAllocator.Init();

    LOG_INFO("game root: {}", FileSystem::PathUtf8(gameRoot));

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
