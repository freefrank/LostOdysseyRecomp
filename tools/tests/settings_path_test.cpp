#include "os/user_paths.h"
#include "updater/update.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void WriteSettings(const std::filesystem::path& path, const char* contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << contents;
    Check(bool(output), "write isolated settings fixture");
}
}

int main()
{
    namespace fs = std::filesystem;
    const auto previousDirectory = fs::current_path();
    const auto root = fs::temp_directory_path() /
        ("lo-settings-path-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
#ifndef _WIN32
    const char* originalXdg = std::getenv("XDG_CONFIG_HOME");
    const bool hadXdg = originalXdg != nullptr;
    const std::string previousXdg = hadXdg ? originalXdg : "";
    const char* actualHome = std::getenv("HOME");
#if LO_PLATFORM_MACOS
    const bool hadHome = actualHome != nullptr;
    const std::string previousHome = hadHome ? actualHome : "";
    const auto fallback = root / "home/Library/Application Support/LostOdysseyRecomp/settings.ini";
#else
    const auto fallback = (actualHome ? fs::path(actualHome) : fs::path{}) /
        ".config/lost-odyssey-recomp/settings.ini";
#endif
#endif
    int result = 0;
    try
    {
        fs::create_directories(root / "portable");
        fs::create_directories(root / "launch");
        fs::current_path(root / "launch");
        os::user_paths::Initialize(root / "portable");
        Check(os::user_paths::SettingsPath() == "settings.ini", "portable settings must retain working-directory contract");
        Check(!fs::exists(os::user_paths::SettingsPath()), "missing portable settings trigger initial setup");
        Check(updater::StartupPreferences{}.debugLog &&
              updater::ReadStartupPreferences(os::user_paths::SettingsPath()).debugLog,
              "startup diagnostic log defaults on without a settings file");
        for (const auto* contents : {"", "debug_log=0\n", "debug_log=1\n", "diagnostic_log=2\n",
                                     "diagnostic_log=-1\n", "diagnostic_log=invalid\n"})
        {
            WriteSettings(root / "launch/settings.ini", contents);
            Check(updater::ReadStartupPreferences(os::user_paths::SettingsPath()).debugLog,
                  "startup ignores legacy and invalid diagnostic values");
        }
        for (const bool enabled : {false, true})
        {
            WriteSettings(root / "launch/settings.ini", enabled
                ? "diagnostic_log=1\ndebug_log=0\n" : "diagnostic_log=0\ndebug_log=1\n");
            Check(updater::ReadStartupPreferences(os::user_paths::SettingsPath()).debugLog == enabled,
                  "startup diagnostic_log respects opt-out and overrides the legacy key");
        }
        WriteSettings(root / "launch/settings.ini", "automatic_updates=0\nui_language=2\n");
        Check(fs::exists(os::user_paths::SettingsPath()), "saved portable settings bypass initial setup");
        auto preferences = updater::ReadStartupPreferences(os::user_paths::SettingsPath());
        Check(!preferences.automaticUpdates && preferences.uiLanguage == 2, "startup reads the portable settings path");

#ifndef _WIN32
        setenv("XDG_CONFIG_HOME", (root / "xdg").c_str(), 1);
#if LO_PLATFORM_MACOS
        setenv("HOME", (root / "home").c_str(), 1);
        const auto bundle = root / "LostOdysseyRecomp.app/Contents/MacOS";
        fs::create_directories(bundle);
        os::user_paths::Initialize(bundle);
        const auto expected = fallback;
#else
        os::user_paths::Initialize(root / "read-only-mount");
        const auto expected = root / "xdg/lost-odyssey-recomp/settings.ini";
#endif
        Check(os::user_paths::SettingsPath() == expected, "installed applications must use the platform settings file");
        Check(!fs::exists(os::user_paths::SettingsPath()), "launch-directory settings must not suppress installed initial setup");
        WriteSettings(expected, "automatic_updates=1\nui_language=4\n");
        Check(fs::exists(os::user_paths::SettingsPath()), "saved installed settings bypass initial setup");
        preferences = updater::ReadStartupPreferences(os::user_paths::SettingsPath());
        Check(preferences.automaticUpdates && preferences.uiLanguage == 4, "startup reads installed settings rather than portable settings");
        fs::current_path(root / "portable");
        Check(os::user_paths::SettingsPath() == expected && fs::exists(expected), "installed settings stay stable across launch directories");
        preferences = updater::ReadStartupPreferences(os::user_paths::SettingsPath());
        Check(preferences.automaticUpdates && preferences.uiLanguage == 4, "changed launch directory does not alter settings readback");
        unsetenv("XDG_CONFIG_HOME");
        Check(os::user_paths::SettingsPath() == fallback, "unset XDG variable uses platform HOME path");
        setenv("XDG_CONFIG_HOME", "", 1);
        Check(os::user_paths::SettingsPath() == fallback, "empty XDG variable uses platform HOME path");
#endif
        std::cout << "PASS: shared portable/installed settings path and startup preference readback\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        result = 1;
    }
    fs::current_path(previousDirectory);
#ifndef _WIN32
    if (hadXdg) setenv("XDG_CONFIG_HOME", previousXdg.c_str(), 1);
    else unsetenv("XDG_CONFIG_HOME");
#if LO_PLATFORM_MACOS
    if (hadHome) setenv("HOME", previousHome.c_str(), 1);
    else unsetenv("HOME");
#endif
#endif
    std::error_code error;
    fs::remove_all(root, error);
    return result;
}
