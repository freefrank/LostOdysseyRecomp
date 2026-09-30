#pragma once

#include <filesystem>

namespace install
{
enum class HostResult
{
    Installed,
    AlreadyPresent,
    Cancelled,
    Failed,
};

// Blocking installer UI. Runs before GPU/guest init. Gamepad and keyboard
// can browse a source directory. On success, gameRoot is the boot disc path
// containing default.xex (typically dest/disc1).
HostResult RunHost(const std::filesystem::path& executableDirectory,
                   std::filesystem::path* gameRoot,
                   bool force = false);
}
