#pragma once

#include <os/user_paths.h>
#include <filesystem>
#include <fstream>

namespace gpu::shader_preparation
{
// Keep the one-shot request beside settings.ini, including Android's private
// config directory. The installed shader pack and both caches remain untouched.
inline std::filesystem::path RequestPath()
{
    return os::user_paths::SettingsPath().parent_path() / "shader-preparation-requested.txt";
}

inline bool RequestNextStart(const std::filesystem::path& path)
{
    std::error_code error;
    if (const auto parent = path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) return false;
    }
    std::ofstream marker(path, std::ios::binary | std::ios::trunc);
    marker << "1\n";
    marker.close();
    return !marker.fail();
}

inline bool RequestNextStart() { return RequestNextStart(RequestPath()); }

inline bool PendingNextStart(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error;
}

inline bool PendingNextStart() { return PendingNextStart(RequestPath()); }

// Consume only when pipeline preparation can start. A shader-stage skip leaves
// the request available for the next launch.
inline bool ConsumeNextStart(const std::filesystem::path& path)
{
    if (!PendingNextStart(path)) return false;
    std::error_code error;
    return std::filesystem::remove(path, error) && !error;
}

inline bool ConsumeNextStart() { return ConsumeNextStart(RequestPath()); }
}
