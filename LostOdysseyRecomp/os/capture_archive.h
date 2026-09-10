#pragma once

#include <filesystem>
#include <future>
#include <functional>
#include <system_error>

namespace os
{
    struct CaptureArchiveResult
    {
        std::filesystem::path directory, archive;
        bool saved = false;
        std::error_code error, cleanupError;
    };

    // Takes a completed, closed render-* directory under captures/. The worker
    // owns only paths and optional detached CPU data, never renderer resources.
    // prepare runs on the worker after path validation and before compression.
    // A successful ZIP is published
    // before its source is removed; failures retain the source for recovery.
    // Keep the future alive and only get() after ready (or when exiting).
    std::future<CaptureArchiveResult> StartCaptureArchive(std::filesystem::path directory,
        std::function<void(const std::filesystem::path&)> prepare = {});
}
