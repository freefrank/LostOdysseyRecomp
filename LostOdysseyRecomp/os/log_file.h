#pragma once

#include <filesystem>
#include <system_error>

namespace os::logger
{
    // Open the process log once in append mode, retaining its absolute path.
    // The caller creates any parent directory. An existing sink is not replaced.
    bool OpenFile(const std::filesystem::path& path);

    // Flush and copy the complete active log while holding the logger mutex.
    // Returns an error if the sink is absent, flushing fails or copying fails.
    // Logging stays open after the snapshot, including on failure.
    // The source itself and equivalent destination aliases are rejected.
    // A failed copy can leave partial destination bytes; callers must not label
    // that output as a complete snapshot (or may remove their own destination).
    std::error_code SnapshotFile(const std::filesystem::path& destination);
}
