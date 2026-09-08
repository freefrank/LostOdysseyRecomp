#pragma once

#include <filesystem>
#include <cstddef>
#include <system_error>

namespace os::logger
{
    // Open the process log once in append mode, retaining its absolute path.
    // The caller creates any parent directory. An existing sink is not replaced.
    bool OpenFile(const std::filesystem::path& path);

    // Call only after opening a default logs/runtime-<digits>.log sink.
    // Keep that file and the two greatest other numeric filename timestamps.
    // Never recurse or remove non-regular files. Busy/inaccessible old logs are
    // left for a later launch, so the directory may temporarily contain more.
    // Custom LO_LOG_FILE sinks must not opt in to this best-effort cleanup.
    void PruneDefaultLogs(const std::filesystem::path& currentLog) noexcept;

    // Last-resort crash output. Uses a separately opened OS append handle,
    // never the logger mutex, a FILE* lock, formatting, or allocation. The
    // handle stays open until process exit. Also attempts raw stderr output;
    // a pipe is skipped when a file sink exists so a full pipe cannot block
    // the report. Missing sinks are harmless; pending CRT bytes are not flushed.
    void EmergencyWrite(const char* data, size_t size) noexcept;

    // Flush and copy the complete active log while holding the logger mutex.
    // Returns an error if the sink is absent, flushing fails or copying fails.
    // Logging stays open after the snapshot, including on failure.
    // The source itself and equivalent destination aliases are rejected.
    // A failed copy can leave partial destination bytes; callers must not label
    // that output as a complete snapshot (or may remove their own destination).
    std::error_code SnapshotFile(const std::filesystem::path& destination);
}
