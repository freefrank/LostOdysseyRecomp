#include "os/user_paths.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

static void Check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

int main()
{
#ifdef _WIN32
    const fs::path executable = fs::path("C:/Games/LostOdyssey");
    Check(os::user_paths::ConfigDir(executable) == executable, "Windows config path is not beside executable");
    Check(os::user_paths::DataDir(executable) == executable, "Windows data path is not beside executable");
    Check(os::user_paths::StateDir(executable) == executable, "Windows state path is not beside executable");
#else
    const fs::path root = fs::temp_directory_path() / "lo-user-paths-test";
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root / "xdg" / "config");
    fs::create_directories(root / "xdg" / "data");
    fs::create_directories(root / "xdg" / "state");
    setenv("HOME", (root / "home").c_str(), 1);
    setenv("XDG_CONFIG_HOME", (root / "xdg/config").c_str(), 1);
    setenv("XDG_DATA_HOME", (root / "xdg/data").c_str(), 1);
    setenv("XDG_STATE_HOME", (root / "xdg/state").c_str(), 1);
    unsetenv("FLATPAK_ID");
    unsetenv("LO_PROFILE_DIR");
    Check(os::user_paths::ConfigDir() == root / "xdg/config/lost-odyssey-recomp", "XDG config path mismatch");
    Check(os::user_paths::DataDir() == root / "xdg/data/lost-odyssey-recomp", "XDG data path mismatch");
    Check(os::user_paths::StateDir() == root / "xdg/state/lost-odyssey-recomp", "XDG state path mismatch");
    os::user_paths::Initialize(root);
    Check(os::user_paths::ProfileDir() == "profile", "portable profile path changed");
    // A missing executable directory selects the same non-portable branch as
    // an AppImage's read-only mount, without relying on root permission checks.
    os::user_paths::Initialize(root / "missing");
    const auto profile = root / "xdg/data/lost-odyssey-recomp/profile";
    Check(os::user_paths::ProfileDir() == profile, "non-portable profile is not in XDG data");
    const auto previousDirectory = fs::current_path();
    fs::current_path(root / "xdg/config");
    Check(os::user_paths::ProfileDir() == profile, "profile changed with launch directory");
    fs::current_path(previousDirectory);
    setenv("LO_PROFILE_DIR", (root / "custom-profile").c_str(), 1);
    Check(os::user_paths::ProfileDir() == root / "custom-profile", "profile override ignored");
    unsetenv("LO_PROFILE_DIR");
    setenv("FLATPAK_ID", "com.example.LostOdyssey", 1);
    Check(os::user_paths::DataDir() == "/var/data", "Flatpak data path mismatch");
    Check(os::user_paths::ProfileDir() == "/var/data/profile", "Flatpak profile path mismatch");
    Check(os::user_paths::IsExecutableDirWritable(root), "existing temporary directory is not writable");
    Check(!os::user_paths::IsExecutableDirWritable(root / "missing"), "missing directory reported writable");
    fs::remove_all(root, error);
#endif
    std::cout << "PASS: user paths fixture\n";
}
