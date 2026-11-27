#include "os/user_paths.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#if !defined(_WIN32)
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;

static void Check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

#if !defined(_WIN32) && !LO_PLATFORM_MACOS
static std::string ReadText(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

static void WriteText(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary | std::ios::trunc) << text;
}

// RecompLauncher's data folder takes over from the folder beside the executable.
static void CheckLauncherData(const fs::path& root)
{
    namespace data = os::launcher_data;
    const auto program = root / "launcher/usr/bin";
    const auto recompdata = root / "launcher/recompdata";
    const auto marker = recompdata / data::CopiedMarker;
    fs::create_directories(program);
    unsetenv("APPIMAGE");

    // A fresh start: nothing to copy, the data folder is used at once.
    setenv("RECOMPLAUNCHER_DATA", recompdata.c_str(), 1);
    os::user_paths::Initialize(program);
    Check(fs::is_regular_file(marker), "fresh start did not write the marker");
    Check(os::user_paths::UsePortableLayout(), "launcher data is not a portable layout");
    Check(os::user_paths::PortableRoot(program) == recompdata, "fresh start did not use the data folder");
    Check(!fs::exists(recompdata / "settings.ini"), "fresh start invented a settings file");
    fs::remove_all(root / "launcher");

    // A start with an earlier build's files beside the executable.
    fs::create_directories(program);
    WriteText(program / "settings.ini", "vsync=0\n");
    WriteText(program / "mod-list.ini", "a\n");
    WriteText(program / "game-path.txt", "/games/lo/game\n");
    WriteText(program / "save/user00/slot.bin", "save");
    WriteText(program / "profile/achievements.ini", "earned");
    WriteText(program / "mods/a/mod.ini", "id=a");
    WriteText(program / "cache/shaders/x.bin", "cache");
    // RecompLauncher wrote its settings into the data folder before the start.
    WriteText(recompdata / "settings.ini", "vsync=1\n");
    // Saves already in the data folder are never replaced.
    WriteText(recompdata / "save/user01/slot.bin", "newer");
    // A copy cut short leaves .part items behind; they are cleared first.
    WriteText(recompdata / "profile.part/stale.ini", "stale");
    // An item that cannot be copied stops the copy.
    Check(mkfifo((program / "mods/a/pipe").c_str(), 0600) == 0, "could not make a FIFO");
    os::user_paths::Initialize(program);
    Check(!fs::exists(marker), "an interrupted copy wrote the marker");
    Check(os::user_paths::PortableRoot(program) == program, "an interrupted copy left the program folder");
    Check(!fs::exists(recompdata / "mods") && !fs::exists(recompdata / "mods.part"), "a failed item was left in place");
    Check(!fs::exists(recompdata / "profile.part"), "a leftover .part item was kept");
    // The items this run copied are taken back; what was there before stays.
    Check(!fs::exists(recompdata / "profile") && !fs::exists(recompdata / "game-path.txt") &&
          !fs::exists(recompdata / "settings.ini"), "a failed copy kept the items it had copied");
    Check(!fs::exists(recompdata / ".copying-items"), "a failed copy kept its journal");
    Check(ReadText(recompdata / "save/user01/slot.bin") == "newer", "a failed copy removed an existing save");

    // That start played from the program folder, so its saves there are newer.
    WriteText(program / "profile/achievements.ini", "earned more");
    // A start that stopped before its marker leaves journaled items behind.
    WriteText(recompdata / "mod-list.ini", "stale\n");
    WriteText(recompdata / ".copying-items", "mod-list.ini\n");

    // The next start finishes the copy.
    fs::remove(program / "mods/a/pipe");
    WriteText(recompdata / "settings.ini", "vsync=1\n");
    os::user_paths::Initialize(program);
    Check(fs::is_regular_file(marker), "the next start did not finish the copy");
    Check(os::user_paths::PortableRoot(program) == recompdata, "finished copy did not use the data folder");
    Check(ReadText(recompdata / "profile/achievements.ini") == "earned more", "the next start kept an older copy");
    Check(!fs::exists(recompdata / ".copying-items"), "a finished copy kept its journal");
    Check(ReadText(recompdata / "settings.ini") == "vsync=0\n", "old settings did not replace the launcher's");
    Check(ReadText(recompdata / "mod-list.ini") == "a\n", "mod list not copied");
    Check(ReadText(recompdata / "game-path.txt") == "/games/lo/game\n", "game path not copied");
    Check(ReadText(recompdata / "mods/a/mod.ini") == "id=a", "mods not copied");
    Check(ReadText(recompdata / "save/user01/slot.bin") == "newer", "existing saves were replaced");
    Check(!fs::exists(recompdata / "save/user00"), "saves copied over an existing save folder");
    Check(!fs::exists(recompdata / "cache"), "cache copied");
    for (const auto& entry : fs::recursive_directory_iterator(recompdata))
        Check(entry.path().extension() != ".part", "a .part item was left behind");
    Check(ReadText(program / "settings.ini") == "vsync=0\n" && fs::exists(program / "save/user00/slot.bin"),
          "the originals were changed");

    // A finished copy is not repeated.
    WriteText(program / "settings.ini", "vsync=2\n");
    WriteText(recompdata / "settings.ini", "vsync=1\n");
    os::user_paths::Initialize(program);
    Check(ReadText(recompdata / "settings.ini") == "vsync=1\n", "a finished copy ran again");
    Check(os::user_paths::PortableRoot(program) == recompdata, "a finished copy left the data folder");

    // Without the launcher nothing changes.
    unsetenv("RECOMPLAUNCHER_DATA");
    os::user_paths::Initialize(program);
    Check(os::user_paths::PortableRoot(program) == program, "a start without the launcher used its data folder");
    setenv("RECOMPLAUNCHER_DATA", "", 1);
    os::user_paths::Initialize(program);
    Check(os::user_paths::PortableRoot(program) == program, "an empty RECOMPLAUNCHER_DATA was used");
    unsetenv("RECOMPLAUNCHER_DATA");
    fs::remove_all(root / "launcher");
}
#endif

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
    unsetenv("APPIMAGE");
    unsetenv("LO_SAVE_DIR");
    Check(os::user_paths::ConfigDir() == root / "xdg/config/lost-odyssey-recomp", "XDG config path mismatch");
    Check(os::user_paths::DataDir() == root / "xdg/data/lost-odyssey-recomp", "XDG data path mismatch");
    Check(os::user_paths::StateDir() == root / "xdg/state/lost-odyssey-recomp", "XDG state path mismatch");
    os::user_paths::Initialize(root);
    Check(os::user_paths::ProfileDir() == "profile", "portable profile path changed");
    const auto extractedExecutable = root / "extracted/usr/bin";
    fs::create_directories(extractedExecutable);
    const auto appImage = root / fs::path(std::u8string(u8"游戏 folder/Lost Odyssey.AppImage"));
    setenv("APPIMAGE", appImage.c_str(), 1);
    os::user_paths::Initialize(extractedExecutable);
    Check(!os::user_paths::UsePortableLayout(), "writable extracted AppImage selected portable layout");
    Check(os::user_paths::ExecutableDir() == extractedExecutable,
          "AppImage changed the executable directory needed for runtime libraries");
    Check(os::user_paths::SettingsPath() == root / "xdg/config/lost-odyssey-recomp/settings.ini",
          "extracted AppImage settings are not in XDG config");
    Check(os::user_paths::ProfileDir() == root / "xdg/data/lost-odyssey-recomp/profile",
          "extracted AppImage profile is not in XDG data");
    Check(os::user_paths::SaveDir() == root / "xdg/data/lost-odyssey-recomp/save",
          "extracted AppImage saves are not in XDG data");
    setenv("APPIMAGE", "", 1);
    os::user_paths::Initialize(extractedExecutable);
    Check(os::user_paths::UsePortableLayout(), "empty APPIMAGE changed writable portable layout");
    unsetenv("APPIMAGE");
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
    CheckLauncherData(root);
    os::user_paths::Initialize(root / "missing");
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
