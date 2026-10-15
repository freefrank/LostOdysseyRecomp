#pragma once

#include <string>
#include <cstdint>
#include <cstdio>

#include <filesystem>
#include <string_view>
#include <memory>

struct KernelObject;

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

    // Called only with diagnostics enabled and a retained object reference.
    static void TraceHandleClose(uint32_t handle, const std::shared_ptr<KernelObject>& object, bool completed);

    // Resolve a guest path to a host path. Returns an empty path when the
    // device is unknown.
    static std::filesystem::path ResolvePath(std::string_view guestPath);

    static std::filesystem::path GetGameRoot();
    // The folder of the selected disc (game: and d: resolve into it).
    static std::filesystem::path GetDiscRoot();
    static std::filesystem::path GetSaveRoot();
    static std::filesystem::path GetCacheRoot();

    // Save data must reach the disk before the game reports success: a crash
    // or power loss right after saving otherwise leaves zero-filled files.
    static bool SyncFile(std::FILE* file);
    static void SyncFiles(const std::filesystem::path& directory);
    // Write to a sibling temporary file, sync it, then replace the target.
    static bool WriteFileDurably(const std::filesystem::path& path, const void* data, size_t size);
};

std::filesystem::path GetGamePath();
std::filesystem::path GetSavePath();
