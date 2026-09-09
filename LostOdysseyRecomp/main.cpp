#include <gpu/taa_collection.h>
#include <stdafx.h>
#include <cpu/guest_thread.h>
#include <kernel/function.h>
#include <kernel/memory.h>
#include <kernel/guest_address_space.h>
#include <kernel/heap.h>
#include <kernel/xex_loader.h>
#include <kernel/xam.h>
#include <kernel/io/file_system.h>
#include <gpu/command_processor.h>
#include <gpu/renderer.h>
#include <gpu/video.h>
#include <apu/audio.h>
#include <apu/xma.h>
#include <hid/hid.h>
#include <os/logger.h>
#include <os/shader_log.h>
#include <os/log_file.h>
#include <os/crash_handler.h>
#include <cstring>
#include <ctime>
#include <chrono>
#include "settings/first_run.h"
#include "settings/config.h"
#include "settings/game_path.h"
#include "settings/restart.h"
#include "updater/update.h"
#include "version.h"

#ifdef _WIN32
#include <timeapi.h>
#include <shellapi.h>
#endif

// Runtime entry: set up guest memory, load default.xex and run its entry point
// on the first guest thread. Everything else is driven by the game through the
// kernel imports (see kernel/imports.cpp).

static std::filesystem::path ExecutableDirectory()
{
#ifdef _WIN32
    wchar_t executable[32768]{};
    if (GetModuleFileNameW(nullptr, executable, 32768))
        return std::filesystem::path(executable).parent_path();
#endif
    return std::filesystem::current_path();
}

static settings::game_path::Resolution FindGameRoot(
    const std::filesystem::path& executableDirectory,
    const std::optional<std::filesystem::path>& explicitGame)
{
    return settings::game_path::Resolve(executableDirectory, explicitGame);
}

void InstallPhysicalWatchpoint();

int main(int argc, char* argv[])
{
#ifdef _WIN32
    // A restart child must park before touching logs, settings, profiles,
    // saves, caches, or guest state. Invalid handshake arguments fail closed.
    if (settings::restart::WaitForParentIfRestartChild() == settings::restart::ChildHandshake::Invalid)
        return 1;
#endif
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
    bool explicitGame=false, requestedSetup=false, setupOnly=false, prepareShadersOnly=false;
    std::optional<std::filesystem::path> explicitGamePath;
    for(int i=1;i<argc;++i) {
        if (strcmp(argv[i], "--game") == 0)
        {
            explicitGame = true;
            explicitGamePath = i + 1 < argc ? std::filesystem::u8path(argv[i + 1])
                                            : std::filesystem::path{};
        }
        requestedSetup |= strcmp(argv[i],"--setup")==0 || strcmp(argv[i],"--setup-only")==0;
        setupOnly |= strcmp(argv[i],"--setup-only")==0;
        prepareShadersOnly |= strcmp(argv[i],"--prepare-shaders-only")==0;
    }
    const auto executableDirectory = ExecutableDirectory();
#ifdef _WIN32
    // Direct launches keep all portable data beside the executable. Explicit
    // --game launches retain their caller's working directory for isolated tests.
    if(!explicitGame) {
        std::filesystem::current_path(executableDirectory);
    }
#endif
    // Keep each run separately, including launches without a terminal. Tests
    // can select a path or disable the duplicate sink with LO_LOG_FILE=0.
    const char* logOverride = getenv("LO_LOG_FILE");
    if (!logOverride || strcmp(logOverride, "0") != 0)
    {
        const auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const std::filesystem::path logPath = logOverride
            ? std::filesystem::u8path(logOverride)
            : std::filesystem::path(fmt::format("logs/runtime-{}.log", ticks));
        std::error_code ec;
        if (logPath.has_parent_path())
            std::filesystem::create_directories(logPath.parent_path(), ec);
        if (os::logger::OpenFile(logPath))
        {
            if (!logOverride)
                os::logger::PruneDefaultLogs(logPath);
            LOG_INFO("log file: {}", FileSystem::PathUtf8(logPath));
        }
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
        LOG_INFO("source version: {}", lo_version::Source);
        std::string switches;
        for (char** e = environ; e && *e; e++)
            if (strncmp(*e, "LO_", 3) == 0)
                switches += fmt::format(" {}", *e);
        LOG_INFO("LO_* switches:{}", switches.empty() ? " (none)" : switches.c_str());
    }

    const auto gameResolution = FindGameRoot(executableDirectory, explicitGamePath);
    auto gameRoot = gameResolution.root;
    if (gameResolution.configuredPathRejected)
        LOG_WARNING("game-path.txt did not identify a default.xex; retaining the configured path for installer/error handling");
#ifdef _WIN32
    if(!explicitGame && !std::filesystem::exists(gameRoot/"default.xex") && std::filesystem::exists("InstallGame.exe")) {
        const auto installer=std::filesystem::absolute("InstallGame.exe").wstring();
        SHELLEXECUTEINFOW launch{sizeof(launch)};
        launch.fMask=SEE_MASK_NOCLOSEPROCESS; launch.lpFile=installer.c_str(); launch.nShow=SW_SHOWNORMAL;
        launch.lpParameters=L"--return-to-game";
        if(!ShellExecuteExW(&launch)) return 1;
        if(launch.hProcess) { WaitForSingleObject(launch.hProcess,INFINITE); CloseHandle(launch.hProcess); }
        gameRoot=FindGameRoot(executableDirectory, explicitGamePath).root;
        if(!std::filesystem::exists(gameRoot/"default.xex")) return 0;
    }
#endif
    settings::ConfigureGameLanguages(gameRoot / "default.xex");
#ifdef _WIN32
    // Preserve edition-aware lazy settings validation before consulting the
    // persisted updater opt-out. A ready helper takes over before first-run,
    // profile, cache, or guest initialization; every other result fails open.
    const auto startupConfig = settings::GetConfig();
    updater::StartupOptions updateOptions;
    updateOptions.currentVersion = lo_version::Source;
    updateOptions.installRoot = executableDirectory;
    updateOptions.executable = updater::CurrentExecutablePath();
    updateOptions.launchArguments = updater::CurrentLaunchArguments();
    updateOptions.automaticUpdates = startupConfig.automaticUpdates;
    updateOptions.uiLanguage = startupConfig.uiLanguage;
    const auto updateResult = updater::PrepareAtStartup(updateOptions);
    LOG_INFO("update check: {} ({})", updater::StatusName(updateResult.status), updateResult.detail);
    if (updateResult.status == updater::StartupStatus::Ready && updateResult.update)
    {
        const auto &prepared = *updateResult.update;
        if (settings::restart::LaunchWaitingProcess(prepared.runnerPath.wstring(),
                                                    updater::ApplyHelperArguments(prepared.planPath)))
            return 0;
        LOG_WARNING("update helper did not complete its readiness handshake; continuing current version");
    }
#endif
    gpu::taa_collection::Initialize();
    if(requestedSetup || (!getenv("LO_BACKGROUND") && !getenv("LO_HEADLESS") && !std::filesystem::exists("settings.ini"))) {
        if(!settings::FirstRunSetup(&gameRoot)) return 0;
        gpu::taa_collection::PromptFirstRun(settings::GetConfig().uiLanguage);
        if(setupOnly) return 0;
    }
    if (g_memory.base == nullptr)
    {
        const auto failure = GuestAddressSpace::GetFailureInfo();
        LOG_ERROR("failed to initialize the 4 GiB guest address space: operation={} view={} address={:#x} size={:#x} offset={:#x}",
                  GuestAddressSpace::FailureOperationName(failure.operation), failure.viewIndex,
                  failure.address, failure.size, failure.offset);
#ifdef _WIN32
        char message[1024]{};
        DWORD length = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                      nullptr, failure.error, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                                      message, DWORD(std::size(message)), nullptr);
        while (length && (message[length - 1] == '\r' || message[length - 1] == '\n'))
            message[--length] = '\0';
        LOG_ERROR("guest address space: Win32 error={} ({:#x}): {}", failure.error, failure.error,
                  length ? message : "English system error description unavailable");
        if (failure.preferredReservationError)
            LOG_ERROR("guest address space: preferred-address reservation also failed with Win32 error={} ({:#x})",
                      failure.preferredReservationError, failure.preferredReservationError);
        MEMORYSTATUSEX memoryStatus{sizeof(memoryStatus)};
        if (GlobalMemoryStatusEx(&memoryStatus))
            LOG_ERROR("host memory at error report (bytes): available_commit={} total_commit={} available_physical={} total_physical={} available_virtual={}",
                      memoryStatus.ullAvailPageFile, memoryStatus.ullTotalPageFile, memoryStatus.ullAvailPhys,
                      memoryStatus.ullTotalPhys, memoryStatus.ullAvailVirtual);
        if (failure.error == ERROR_COMMITMENT_LIMIT)
            LOG_ERROR("Windows reported its commit limit was reached. Close memory-heavy applications or increase Windows paging-file capacity, then retry.");
#else
        LOG_ERROR("guest address space: errno={}: {}", failure.error, std::strerror(int(failure.error)));
#endif
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

    // Exercise the same renderer preparation as ordinary startup, without
    // starting guest threads or opening game saves/profiles. This also provides
    // a bounded cache warmup command for portable installations.
    if (prepareShadersOnly) {
        const bool prepared = gpu::video::Init() && !getenv("LO_NO_RENDERER");
        LOG_INFO("shader preparation only: {}, guest not started", prepared ? "complete" : "failed");
        fflush(stdout);
        (os::shaderlog::CloseForExit(), std::_Exit(prepared ? 0 : 1));
    }

    if (!gpu::g_commandProcessor.Init()) {
        LOG_ERROR("graphics initialization failed; guest not started (see backend selection errors above)");
        return 1;
    }
    XexLoader::StartTimeStampThread();
    apu::Init();
    apu::xma::Init();
    if (getenv("LO_HEADLESS"))
        hid::Init(); // otherwise the video thread initialises it

    LOG_INFO("starting guest at {:#x}", entry);
    GuestThread::Start({ entry, 0, 0 });

    LOG_INFO("guest main thread returned");
    return 0;
}
