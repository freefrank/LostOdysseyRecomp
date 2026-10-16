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
#elif LO_PLATFORM_MACOS
    const fs::path root = fs::temp_directory_path() / "lo-user-paths-test";
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root / "Games");
    fs::create_directories(root / "LostOdysseyRecomp.app/Contents/MacOS");
    setenv("HOME", (root / "home").c_str(), 1);
    unsetenv("LO_PROFILE_DIR");
    const auto support = root / "home/Library/Application Support/LostOdysseyRecomp";
    Check(os::user_paths::ConfigDir() == support, "macOS config path is not in Application Support");
    Check(os::user_paths::DataDir() == support, "macOS data path is not in Application Support");
    Check(os::user_paths::StateDir() == root / "home/Library/Logs/LostOdysseyRecomp", "macOS state path is not in Logs");
    os::user_paths::Initialize(root / "Games");
    Check(os::user_paths::ProfileDir() == "profile", "portable profile path changed");
    // A writable app bundle is still not portable: writing there breaks its signature.
    os::user_paths::Initialize(root / "LostOdysseyRecomp.app/Contents/MacOS");
    Check(os::user_paths::ProfileDir() == support / "profile", "app bundle selected the portable layout");
    setenv("LO_PROFILE_DIR", (root / "custom-profile").c_str(), 1);
    Check(os::user_paths::ProfileDir() == root / "custom-profile", "profile override ignored");
    unsetenv("LO_PROFILE_DIR");
    fs::remove_all(root, error);
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
    // Runtime libraries are found from here even when the layout is not portable.
    Check(os::user_paths::ExecutableDir() == root / "missing", "executable directory not kept");
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
    // An explicit shared save root is independent of profile/config paths.
#ifdef _WIN32
    _putenv_s("LO_SAVE_DIR", "");
#else
    unsetenv("LO_SAVE_DIR");
#endif
    const auto defaultSave = (os::user_paths::UsePortableLayout() ? fs::current_path() : os::user_paths::DataDir()) / "save";
    Check(os::user_paths::SaveDir() == defaultSave, "default save path changed");
    const auto sharedSave = fs::temp_directory_path() / "lo-shared-save";
#ifdef _WIN32
    _putenv_s("LO_SAVE_DIR", sharedSave.string().c_str());
#else
    setenv("LO_SAVE_DIR", sharedSave.c_str(), 1);
#endif
    Check(os::user_paths::SaveDir() == sharedSave, "save override ignored");
#ifdef _WIN32
    _putenv_s("LO_SAVE_DIR", "");
#else
    setenv("LO_SAVE_DIR", "", 1);
#endif
    Check(os::user_paths::SaveDir() == defaultSave, "empty save override did not use the default");
    std::cout << "PASS: user paths fixture\n";
}
