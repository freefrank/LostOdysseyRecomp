#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "../gpu/backend_selection.h"

namespace updater::shader_pack
{
struct StartupRequest
{
    gpu::backend::Backend configuredBackend = gpu::backend::Backend::D3D12;
    uint32_t uiLanguage = 0;
    std::span<const uint8_t> unboundXex; // XexLoader::UnboundIdentityPrefix()
    // Download the published pack without asking, even when one is installed
    // or the offer was skipped (TakeRedownloadRequest).
    bool redownload = false;
};

// Runs after the update check and before the renderer prepares shaders. When no
// installed pack matches the configured renderer, offers the published one and
// installs it. Never fails startup: the renderer compiles locally without it.
// Returns one line for the log.
std::string PrepareAtStartup(const StartupRequest &request);

// Settings -> System -> Shader bundle: the next start downloads the pack and the
// pipeline corpus again. Returns false when the request could not be written.
bool RequestRedownload();
// Once at startup, before PrepareAtStartup: whether a redownload was requested.
// Consumes the request, so a failed download is not retried on every start.
bool TakeRedownloadRequest();
}
