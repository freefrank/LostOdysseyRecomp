#if !defined(__linux__)
#error This synthetic restart fixture requires Linux
#endif
#include <settings/restart.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <sys/wait.h>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

static void Require(bool condition, const char *message)
{
    if (!condition) { std::fprintf(stderr, "restart Linux fixture: %s\n", message); std::exit(1); }
}

static bool EventuallyExists(const fs::path &path, int count = 150)
{
    for (int i = 0; i < count; ++i)
    {
        if (fs::exists(path)) return true;
        std::this_thread::sleep_for(20ms);
    }
    return false;
}

static void Write(const fs::path &path, const char *text)
{
    std::ofstream file(path);
    Require(bool(file << text), "could not write fixture marker");
}

static std::string Read(const fs::path &path)
{
    std::ifstream file(path);
    std::string value;
    std::getline(file, value);
    return value;
}

int main(int argc, char *argv[])
{
    // Synthetic AppImage shim: it is deliberately not a mounted real AppImage.
    // Both the direct-exec and forked-wrapper paths use the production spawn
    // contract and inherited descriptors, without opening game data.
    const char *fixture = getenv("LO_RESTART_FIXTURE_PATH");
    const char *shim = getenv("LO_RESTART_SHIM_MODE");
    const char *appImage = getenv("APPIMAGE");
    if (shim && fixture && appImage && fs::equivalent("/proc/self/exe", appImage))
    {
        if (std::string(shim) == "stall")
        {
            const pid_t descendant = fork();
            if (descendant == 0)
            {
                std::this_thread::sleep_for(500ms);
                execv(fixture, argv);
                _exit(75);
            }
            Require(descendant > 0, "shim fork failed");
            for (;;) pause();
        }
        execv(fixture, argv);
        return 76;
    }

    const auto handshake = settings::restart::WaitForParentIfRestartChild(argc, argv);
    if (handshake == settings::restart::ChildHandshake::Invalid) return 73;
    if (handshake == settings::restart::ChildHandshake::Waited)
    {
        Require(argc >= 4 && std::string(argv[1]) == "--test-parent", "child lost parent arguments");
        const fs::path directory(argv[2]);
        const std::string scenario(argv[3]);
        int installs = 0;
        bool unicode = false;
        for (int i = 1; i < argc; ++i)
        {
            if (std::string(argv[i]) == "--install") ++installs;
            if (std::string(argv[i]) == "参数 with spaces") unicode = true;
        }
        const auto relaunch = settings::restart::LaunchArguments(true);
        int relaunchInstalls = 0;
        for (const auto &argument : relaunch)
        {
            Require(argument != "--restart-parent-fd" && argument != "--restart-ready-fd",
                    "relaunch retained stale handshake flags");
            if (argument == "--install") ++relaunchInstalls;
        }
        Require(relaunchInstalls == 1, "relaunch duplicated --install");
        Require(installs == 1 && unicode && scenario != "", "argv or --install was not preserved exactly once");
        Require(fs::current_path() == directory, "child lost working directory");
        Require(getenv("LO_RESTART_TEST_VALUE") &&
                std::string(getenv("LO_RESTART_TEST_VALUE")) == "环境 with spaces", "child lost environment");
        Write(directory / "child.marker", "parent-gone\n");
        return 0;
    }

    if (argc >= 4 && std::string(argv[1]) == "--test-parent")
    {
        const fs::path directory(argv[2]);
        const std::string scenario(argv[3]);
        settings::restart::RequestInstall();
        bool launched;
        if (scenario == "spawn-fail")
            launched = settings::restart::LaunchWaitingProcess("/definitely/missing/restart-fixture",
                {"fixture", "--test-parent", directory.string(), scenario}, 120);
        else
            launched = settings::restart::LaunchWaitingChild(scenario == "timeout" ? 120 : 2000);
        if (!launched && scenario != "spawn-fail" && scenario != "timeout")
            std::fprintf(stderr, "failed %s launch: errno=%d\n", scenario.c_str(), errno);
        if (scenario == "spawn-fail" || scenario == "timeout")
            Require(!launched && settings::restart::ConsumeLaunchFailure(), "failed spawn did not preserve parent");
        else
            Require(launched, "child did not acknowledge pre-init readiness");
        Write(directory / "parent.ready", "alive\n");
        Require(EventuallyExists(directory / "release"), "test controller did not release parent");
        return 0;
    }

    Require(handshake == settings::restart::ChildHandshake::NotChild, "invalid handshake was accepted");
    char path[4096];
    const ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    Require(n > 0 && n < static_cast<ssize_t>(sizeof(path) - 1), "fixture executable unavailable");
    path[n] = '\0';
    const fs::path executable(path);
    char pattern[] = "/tmp/lo-restart-中文 space-XXXXXX";
    char *directoryName = mkdtemp(pattern);
    Require(directoryName != nullptr, "could not create synthetic test directory");
    const fs::path directory(directoryName);
    Require(chdir(directoryName) == 0, "could not enter synthetic test directory");
    Require(setenv("LO_RESTART_TEST_VALUE", "环境 with spaces", 1) == 0, "setenv failed");
    const fs::path shimPath = directory / "shim appimage";
    fs::copy_file(executable, shimPath);
    fs::permissions(shimPath, fs::perms::owner_exec, fs::perm_options::add);
    Require(setenv("LO_RESTART_FIXTURE_PATH", path, 1) == 0, "fixture env failed");

    auto run = [&](const std::string &scenario) {
        fs::remove(directory / "parent.ready");
        fs::remove(directory / "release");
        fs::remove(directory / "child.marker");
        if (scenario == "shim")
        {
            Require(setenv("APPIMAGE", shimPath.c_str(), 1) == 0, "APPIMAGE env failed");
            Require(setenv("LO_RESTART_SHIM_MODE", "exec", 1) == 0, "shim mode failed");
        }
        else if (scenario == "timeout")
        {
            Require(setenv("APPIMAGE", shimPath.c_str(), 1) == 0, "APPIMAGE env failed");
            Require(setenv("LO_RESTART_SHIM_MODE", "stall", 1) == 0, "shim mode failed");
        }
        else { unsetenv("APPIMAGE"); unsetenv("LO_RESTART_SHIM_MODE"); }
        pid_t parent = fork();
        Require(parent >= 0, "could not fork fixture parent");
        if (parent == 0)
        {
            const std::string dir = directory.string();
            execl(executable.c_str(), executable.c_str(), "--test-parent", dir.c_str(), scenario.c_str(),
                  "参数 with spaces", "--install", "--install", nullptr);
            _exit(77);
        }
        Require(EventuallyExists(directory / "parent.ready"), "parent failed to finish handoff");
        Require(kill(parent, 0) == 0, "parent exited before child acknowledgement was observed");
        Require(!fs::exists(directory / "child.marker"), "child initialized while parent alive");
        if (scenario == "timeout")
        {
            std::this_thread::sleep_for(650ms);
            Require(!fs::exists(directory / "child.marker"), "timeout left a late shim descendant");
        }
        Write(directory / "release", "go\n");
        int status = 0;
        Require(waitpid(parent, &status, 0) == parent && WIFEXITED(status) && WEXITSTATUS(status) == 0,
                "fixture parent failed");
        if (scenario == "spawn-fail" || scenario == "timeout")
        {
            std::this_thread::sleep_for(80ms);
            Require(!fs::exists(directory / "child.marker"), "failed launch initialized a child");
        }
        else
        {
            Require(EventuallyExists(directory / "child.marker"), "child never resumed after parent exited");
            Require(Read(directory / "child.marker") == "parent-gone", "child marker invalid");
        }
    };
    run("direct");
    run("shim");
    run("spawn-fail");
    run("timeout");

    pid_t invalid = fork();
    Require(invalid >= 0, "invalid-FD fork failed");
    if (invalid == 0)
    {
        unsetenv("APPIMAGE");
        execl(executable.c_str(), executable.c_str(), "--restart-parent-fd", "100",
              "--restart-ready-fd", "101", nullptr);
        _exit(78);
    }
    int status = 0;
    Require(waitpid(invalid, &status, 0) == invalid && WIFEXITED(status) && WEXITSTATUS(status) == 73,
            "invalid restart FDs did not fail closed");
    Require(!fs::exists(directory / "child.marker"), "invalid handshake initialized a child");
    fs::remove_all(directory);
    std::puts("Linux restart fixture: parked child, fail-closed FDs/spawn/timeout, argv/env/cwd, AppImage shim passed");
    return 0;
}
