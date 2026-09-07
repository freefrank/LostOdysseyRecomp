#pragma once

#include <string>
#include <cstdint>

#include <filesystem>
#include <string_view>

// Nt-level file system for the guest. Paths arrive as NT object names such as
// "\Device\Cdrom0\xenon_sys.fpd", "\??\game:\..." or "d:\..." and are mapped
// onto the extracted disc directory (see docs/notes/xex.md) and a writable
// save/cache directory.
struct FileSystem
{
    // Root registry and log strings use UTF-8, never the Windows ANSI code page.
    static std::string PathUtf8(const std::filesystem::path& path)
    {
        const auto utf8 = path.u8string();
        return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
    }

    // Name of the most recently opened game file, for the GPU heartbeat log.
    static std::string LastOpenedFile();

    static void Init(const std::filesystem::path& gameRoot);
    // Atomically select an installed volume. Existing file handles retain their
    // original volume until the guest closes them during its index reload.
    static bool SelectDisc(uint32_t discNumber);

    // Resolve a guest path to a host path. Returns an empty path when the
    // device is unknown.
    static std::filesystem::path ResolvePath(std::string_view guestPath);

    static std::filesystem::path GetGameRoot();
    static std::filesystem::path GetSaveRoot();
    static std::filesystem::path GetCacheRoot();
};

std::filesystem::path GetGamePath();
std::filesystem::path GetSavePath();
