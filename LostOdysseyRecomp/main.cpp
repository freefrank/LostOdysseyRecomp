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
#include <os/thread_name.h>
#include <os/user_paths.h>
#include <os/shader_log.h>
#include <os/log_file.h>
#include <os/crash_handler.h>
#include <os/startup_diagnostics.h>
#include <cstring>
#include <ctime>
#include <chrono>
#include "settings/first_run.h"
#include "settings/config.h"
#include "settings/game_path.h"
#include "settings/restart.h"
#include "updater/update.h"
#include "updater/game_prompt.h"
#include "updater/apply_mode.h"
#include "version.h"
#include "install/host.h"
#include "modding/mod_api.h"

#ifdef _WIN32
#include <timeapi.h>
#include <shellapi.h>
#endif
#if defined(__linux__) && !defined(_WIN32)
#include <spawn.h>
#include <unistd.h>
extern char** environ;
#endif

// Runtime entry: set up guest memory, load default.xex and run its entry point
// on the first guest thread. Everything else is driven by the game through the
// kernel imports (see kernel/imports.cpp).

static std::filesystem::path ExecutableDirectory()
{
#if defined(_WIN32)
    wchar_t executable[32768]{};
    if (GetModuleFileNameW(nullptr, executable, 32768))
        return std::filesystem::path(executable).parent_path();
#elif defined(__linux__)
    char path[4096]{};
    const ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n > 0)
    {
        path[n] = '\0';
        return std::filesystem::path(path).parent_path();
    }
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
#if defined(__linux__) && !defined(_WIN32)
    // Park a restart child before even the updater's startup cleanup runs.
    if (settings::restart::WaitForParentIfRestartChild(argc, argv) == settings::restart::ChildHandshake::Invalid)
        return 1;
#endif
#if defined(_WIN32) || defined(__linux__)
    if (const auto applyResult = updater::TryRunApplyMode()) return *applyResult;
#ifdef _WIN32
    // A restart child must park before touching logs, settings, profiles,
    // saves, caches, or guest state. Invalid handshake arguments fail closed.
    if (settings::restart::WaitForParentIfRestartChild() == settings::restart::ChildHandshake::Invalid)
        return 1;
#endif
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
#if defined(LO_RENDERER_P2_SELFTEST)
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--self-test-scene-copy-promotion") != 0)
            continue;
        if (i + 1 >= argc)
            return 1;
        return gpu::renderer::RunSceneCopyPromotionSelfTest(std::filesystem::u8path(argv[i + 1]));
    }
#endif
    bool explicitGame=false, requestedSetup=false, setupOnly=false, prepareShadersOnly=false, requestedInstall=false;
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
        requestedInstall |= strcmp(argv[i],"--install")==0;
    }
    const auto executableDirectory = ExecutableDirectory();
    os::user_paths::Initialize(executableDirectory);
    const auto modsRoot = os::user_paths::UsePortableLayout()
        ? executableDirectory / "mods"
        : os::user_paths::DataDir() / "mods";
    modding::Initialize(modsRoot);
#if defined(_WIN32) || defined(__linux__)
    // Direct launches keep all portable data beside the executable. Explicit
    // --game launches retain their caller's working directory for isolated tests.
    if(!explicitGame && os::user_paths::UsePortableLayout()) {
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
            : (os::user_paths::UsePortableLayout() ? std::filesystem::path(fmt::format("logs/runtime-{}.log", ticks)) : os::user_paths::StateDir() / "logs" / fmt::format("runtime-{}.log", ticks));
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
    os::diagnostics::LogStartupEnvironment();

#if defined(_WIN32) || defined(__linux__)
    // Check for a newer runtime before opening the content importer or setup.
    if (!getenv("LO_HEADLESS") && !getenv("LO_BACKGROUND"))
    {
        const auto startupPreferences = updater::ReadStartupPreferences(
            os::user_paths::UsePortableLayout()
                ? std::filesystem::current_path() / "settings.ini"
                : os::user_paths::ConfigDir() / "settings.ini");
        if (startupPreferences.automaticUpdates)
        {
            updater::StartupOptions updateOptions;
            updateOptions.currentVersion = lo_version::Source;
            updateOptions.installRoot = executableDirectory;
            updateOptions.executable = updater::CurrentExecutablePath();
            updateOptions.launchArguments = updater::CurrentLaunchArguments();
            updateOptions.automaticUpdates = true;
            updateOptions.uiLanguage = startupPreferences.uiLanguage;
            updateOptions.confirmUpdate = &updater::game_prompt::ConfirmBeforeImport;
            const auto updateResult = updater::PrepareAtStartup(updateOptions);
            LOG_INFO("update check: {} {}", updater::StatusName(updateResult.status), updateResult.detail);
            if (updateResult.status == updater::StartupStatus::Ready && updateResult.update)
            {
                const auto &prepared = *updateResult.update;
                // The helper waits for this process to exit before replacing it.
#ifdef _WIN32
                if (settings::restart::LaunchWaitingProcess(prepared.runnerPath.wstring(),
                                                             updater::ApplyHelperArguments(prepared.planPath)))
                    return 0;
#elif defined(__linux__)
                const std::string selfExe = updater::CurrentExecutablePath().string();
                const std::string planStr = prepared.planPath.string();
                const std::string waitPid = std::to_string(getpid());
                std::vector<char*> args;
                args.push_back(const_cast<char*>(selfExe.c_str()));
                args.push_back(const_cast<char*>("--apply-plan"));
                args.push_back(const_cast<char*>(planStr.c_str()));
                args.push_back(const_cast<char*>("--wait-process"));
                args.push_back(const_cast<char*>(waitPid.c_str()));
                args.push_back(nullptr);
                pid_t pid = 0;
                if (posix_spawn(&pid, selfExe.c_str(), nullptr, nullptr, args.data(), environ) == 0)
                    return 0;
#endif
                LOG_ERROR("update runner failed its restart handshake; staged update preserved at {}",
                          FileSystem::PathUtf8(prepared.operationRoot));
                std::ofstream diagnostic(prepared.operationRoot / "handoff-failure.txt", std::ios::trunc);
                diagnostic << "The staged update runner did not complete the restart handshake.\n"
                           << "Staged files were preserved for inspection.\n";
                return 1;
            }
        }
    }
#endif

    const auto gameResolution = FindGameRoot(executableDirectory, explicitGamePath);
    auto gameRoot = gameResolution.root;
    if (gameResolution.configuredPathRejected)
        LOG_WARNING("game-path.txt did not identify a default.xex; retaining the configured path for installer/error handling");

    if (requestedInstall)
    {
        const auto result = install::RunHost(executableDirectory, &gameRoot, true);
        return (result == install::HostResult::Installed || result == install::HostResult::AlreadyPresent) ? 0 : 1;
    }

    if (!explicitGame && !std::filesystem::exists(gameRoot / "default.xex"))
    {
        if (getenv("LO_HEADLESS") || getenv("LO_BACKGROUND"))
        {
            LOG_ERROR("missing default.xex in game root: headless or background environment prevents opening installer UI");
            return 1;
        }
        const auto result = install::RunHost(executableDirectory, &gameRoot);
        if (result != install::HostResult::Installed && result != install::HostResult::AlreadyPresent)
            return result == install::HostResult::Cancelled ? 0 : 1;
        if (!std::filesystem::exists(gameRoot / "default.xex"))
            return 1;
    }
    settings::ConfigureGameLanguages(gameRoot / "default.xex");
    gpu::taa_collection::Initialize();
    struct CollectionShutdown
    {
        ~CollectionShutdown() { gpu::taa_collection::Shutdown(); }
    } collectionShutdown;
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
        LOG_ERROR("guest allocation failure site: api={} utc_filetime_100ns={} uptime_ms={} thread={} process_handle={:#x} (0=current_process) backing_handle={:#x} flags={:#x} protection={:#x}",
            GuestAddressSpace::FailureApiName(failure.operation), failure.utcFileTime, failure.uptimeMilliseconds,
            failure.threadId, failure.processHandle, failure.backingHandle, failure.flags, failure.protection);
        const auto& memory = failure.memory;
        if (memory.valid)
            LOG_ERROR("host memory at allocation failure (bytes): available_commit={} total_commit={} available_physical={} total_physical={} available_virtual={} total_virtual={} load_percent={}",
                memory.availableCommit, memory.totalCommit, memory.availablePhysical, memory.totalPhysical,
                memory.availableVirtual, memory.totalVirtual, memory.loadPercent);
        else LOG_ERROR("host memory at allocation failure: unavailable api=GlobalMemoryStatusEx Win32={:#010x}", memory.error);
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
    os::SetCurrentThreadName("Guest Main");
    GuestThread::Start({ entry, 0, 0 });

    LOG_INFO("guest main thread returned");
    return 0;
}
