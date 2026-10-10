#pragma once

#include <os/platform.h>
#include <cstdlib>
#include <filesystem>
#include <string>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

// Nintendo Switch: like Android, everything lives in one app folder on the SD
// card (LO_SWITCH_DATA_ROOT, sdmc:/switch/LostOdysseyRecomp), which main.cpp
// passes to Initialize() as the "executable directory".

namespace os::user_paths
{
    inline bool g_usePortableLayout = true;
    inline std::filesystem::path g_executableDirectory;
    inline bool IsExecutableDirWritable(const std::filesystem::path& path);
    inline void Initialize(const std::filesystem::path& executableDirectory)
    {
        g_executableDirectory = executableDirectory;
#ifdef _WIN32
        g_usePortableLayout = true;
#elif LO_PLATFORM_ANDROID || LO_PLATFORM_SWITCH
        g_usePortableLayout = false;
#elif LO_PLATFORM_MACOS
        // Never write into an app bundle: it may be user-writable, but changing
        // its contents breaks the code signature. Plain folders stay portable.
        const auto directory = executableDirectory.generic_string();
        g_usePortableLayout = directory.find(".app/Contents/") == std::string::npos &&
            IsExecutableDirWritable(executableDirectory);
#else
        g_usePortableLayout = IsExecutableDirWritable(executableDirectory);
#endif
    }
    namespace detail
    {
        inline std::filesystem::path EnvironmentPath(const char* name,
                                                      const std::filesystem::path& fallback)
        {
            const char* value = std::getenv(name);
            return value != nullptr && *value != '\0' ? std::filesystem::path(value) : fallback;
        }
#if LO_PLATFORM_MACOS
        // macOS keeps per-user app files under ~/Library, not the XDG directories.
        inline std::filesystem::path LibraryPath(const char* folder)
        {
            return EnvironmentPath("HOME", std::filesystem::path{}) / "Library" / folder / "LostOdysseyRecomp";
        }
#endif
    }

    inline std::filesystem::path ConfigDir(const std::filesystem::path& executableDirectory = {})
    {
#ifdef _WIN32
        return executableDirectory.empty() ? std::filesystem::current_path() : executableDirectory;
#elif LO_PLATFORM_ANDROID || LO_PLATFORM_SWITCH
        return g_executableDirectory / "config";
#elif LO_PLATFORM_MACOS
        return detail::LibraryPath("Application Support");
#else
        const auto home = detail::EnvironmentPath("HOME", std::filesystem::path{});
        return detail::EnvironmentPath("XDG_CONFIG_HOME", home / ".config") / "lost-odyssey-recomp";
#endif
    }

    inline std::filesystem::path DataDir(const std::filesystem::path& executableDirectory = {})
    {
#ifdef _WIN32
        return executableDirectory.empty() ? std::filesystem::current_path() : executableDirectory;
#elif LO_PLATFORM_ANDROID || LO_PLATFORM_SWITCH
        return g_executableDirectory;
#elif LO_PLATFORM_MACOS
        return detail::LibraryPath("Application Support");
#else
        if (std::getenv("FLATPAK_ID") != nullptr || std::filesystem::exists("/.flatpak-info"))
            return "/var/data";
        const auto home = detail::EnvironmentPath("HOME", std::filesystem::path{});
        return detail::EnvironmentPath("XDG_DATA_HOME", home / ".local/share") / "lost-odyssey-recomp";
#endif
    }

    inline std::filesystem::path StateDir(const std::filesystem::path& executableDirectory = {})
    {
#ifdef _WIN32
        return executableDirectory.empty() ? std::filesystem::current_path() : executableDirectory;
#elif LO_PLATFORM_ANDROID || LO_PLATFORM_SWITCH
        return g_executableDirectory / "state";
#elif LO_PLATFORM_MACOS
        return detail::LibraryPath("Logs");
#else
        const auto home = detail::EnvironmentPath("HOME", std::filesystem::path{});
        return detail::EnvironmentPath("XDG_STATE_HOME", home / ".local/state") / "lost-odyssey-recomp";
#endif
    }

    inline bool IsExecutableDirWritable(const std::filesystem::path& path)
    {
        if (!std::filesystem::is_directory(path))
            return false;
#ifdef _WIN32
        return _waccess(path.c_str(), 2) == 0;
#else
        return access(path.c_str(), W_OK) == 0;
#endif
    }
    inline bool UsePortableLayout() { return g_usePortableLayout; }
    // The running executable's directory; empty before Initialize.
    inline const std::filesystem::path& ExecutableDir() { return g_executableDirectory; }

    // Portable launches deliberately retain the working-directory settings
    // contract, including isolated --game launches. Read-only installations
    // use the same XDG path for startup detection, preferences, and persistence.
    inline std::filesystem::path SettingsPath()
    {
        return UsePortableLayout() ? std::filesystem::path("settings.ini") : ConfigDir() / "settings.ini";
    }

    inline std::filesystem::path ProfileDir()
    {
        return detail::EnvironmentPath("LO_PROFILE_DIR",
            UsePortableLayout() ? std::filesystem::path("profile") : DataDir() / "profile");
    }
}
