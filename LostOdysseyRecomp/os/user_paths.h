#pragma once

#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace os::user_paths
{
    inline bool g_usePortableLayout = true;
    inline bool IsExecutableDirWritable(const std::filesystem::path& path);
    inline void Initialize(const std::filesystem::path& executableDirectory)
    {
#ifdef _WIN32
        g_usePortableLayout = true;
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
    }

    inline std::filesystem::path ConfigDir(const std::filesystem::path& executableDirectory = {})
    {
#ifdef _WIN32
        return executableDirectory.empty() ? std::filesystem::current_path() : executableDirectory;
#else
        const auto home = detail::EnvironmentPath("HOME", std::filesystem::path{});
        return detail::EnvironmentPath("XDG_CONFIG_HOME", home / ".config") / "lost-odyssey-recomp";
#endif
    }

    inline std::filesystem::path DataDir(const std::filesystem::path& executableDirectory = {})
    {
#ifdef _WIN32
        return executableDirectory.empty() ? std::filesystem::current_path() : executableDirectory;
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

    inline std::filesystem::path ProfileDir()
    {
        return detail::EnvironmentPath("LO_PROFILE_DIR",
            UsePortableLayout() ? std::filesystem::path("profile") : DataDir() / "profile");
    }
}
