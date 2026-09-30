#pragma once

#include "frame_plan.h"
#include <plume_render_interface.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace gpu::frame_generation::ui_diagnostic {

// One explicitly requested render frame. These resources are owned by the
// diagnostic and retained by every GPU batch that records a command using them.
// A successful replay remains a numeric observation, never a UI/FG capability.
struct Capture {
    uint64_t frame = 0, epoch = 0, sourceAllocation = 0;
    frame_plan::FramePlan plan{};
    std::filesystem::path directory;
    plume::RenderFormat format = plume::RenderFormat::UNKNOWN;
    uint32_t sourceWidth = 0, sourceHeight = 0;
    uint32_t width = 0, height = 0, bytesPerPixel = 0, rowPitch = 0;
    uint32_t sceneCopyDraw = 0, overlayDrawCount = 0, lastOverlayDraw = 0;
    uint64_t resolveOrdinal = 0, submissionSerial = 0;
    uint32_t destinationAddress = 0;
    plume::RenderFormat resolveDestinationFormat = plume::RenderFormat::UNKNOWN;
    uint32_t resolveDestinationGuestFormat = 0;
    uint32_t resolveDestinationWidth = 0, resolveDestinationHeight = 0;
    std::array<std::unique_ptr<plume::RenderTexture>, 3> images; // scene, black, white
    std::array<std::unique_ptr<plume::RenderFramebuffer>, 2> framebuffers;
    std::vector<std::unique_ptr<plume::RenderPipeline>> replayPipelines;
    std::array<std::unique_ptr<plume::RenderBuffer>, 4> readbacks;
    const char* rejectionReason = nullptr;
    bool finalRecorded = false, finalSubmitted = false, waitFailed = false, exported = false;

    bool Valid() const { return !rejectionReason; }
    void Reject(const char* reason) {
        if (!rejectionReason) rejectionReason = reason;
    }
};

} // namespace gpu::frame_generation::ui_diagnostic
