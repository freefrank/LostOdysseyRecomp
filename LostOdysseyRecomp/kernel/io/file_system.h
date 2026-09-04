#pragma once

#include <string>

#include <filesystem>
#include <string_view>

// Nt-level file system for the guest. Paths arrive as NT object names such as
// "\Device\Cdrom0\xenon_sys.fpd", "\??\game:\..." or "d:\..." and are mapped
// onto the extracted disc directory (see docs/notes/xex.md) and a writable
// save/cache directory.
struct FileSystem
{
    // Name of the most recently opened game file, for the GPU heartbeat log.
    static std::string LastOpenedFile();

    static void Init(const std::filesystem::path& gameRoot);

    // Resolve a guest path to a host path. Returns an empty path when the
    // device is unknown.
    static std::filesystem::path ResolvePath(std::string_view guestPath);

    static std::filesystem::path GetGameRoot();
    static std::filesystem::path GetSaveRoot();
    static std::filesystem::path GetCacheRoot();
};

std::filesystem::path GetGamePath();
std::filesystem::path GetSavePath();
