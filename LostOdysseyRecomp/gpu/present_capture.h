#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// One XE_SWAP capture. Video records only this ticket's pre-present copy.
// It does not choose a previous presented image.
namespace gpu::present_capture
{
    struct Ticket
    {
        bool active = false;
        uint32_t rendererFrame = 0;
        uint32_t swap = 0;
        uint32_t frontbuffer = 0;
        uint64_t deviceEpoch = 0;
        bool hasSourcePlan = false;
        uint32_t planWidth = 0, planHeight = 0;
        uint32_t planOutputWidth = 0, planOutputHeight = 0;
        uint32_t planUpscaler = 0;
        uint32_t planQuality = 0;
        uint64_t planCpuSerial = 0;
    };

    struct Result
    {
        bool attempted = false;
        bool available = false;
        std::string reason = "not_recorded";
        uint32_t rendererFrame = 0;
        uint32_t swap = 0;
        uint64_t deviceEpoch = 0;
        uint32_t width = 0, height = 0;
        std::string format;
        std::string backend;
        bool presentAccepted = false;
        bool hasSubmissionSerial = false;
        uint64_t submissionSerial = 0;
        bool hasFenceValue = false;
        uint64_t fenceValue = 0;
        std::vector<uint32_t> pixels;
    };

    struct GuestImage
    {
        bool available = false;
        std::string reason = "not_read";
        uint32_t address = 0;
        uint32_t width = 0, height = 0;
        uint64_t sourceWriteFrame = 0;
        uint64_t sourceWriteOrdinal = 0;
    };

    inline bool ShouldPublishArchive(uint32_t completed, uint32_t total, bool frameOk)
    {
        return frameOk && total && completed == total;
    }

    // Frozen before member reset or any close I/O. attemptFrame/attemptSwap are the
    // capture ticket, not the renderer's already incremented live frame.
    struct CaptureClose
    {
        uint32_t completedFrames = 0;
        uint32_t requestedFrames = 0;
        uint32_t attemptFrame = 0;
        uint32_t attemptSwap = 0;
        bool frameOk = false;
        bool publish = false;
        bool continueNext = false;
    };

    inline CaptureClose BeginCaptureClose(uint32_t completedBefore, uint32_t requested, uint32_t attemptFrame, uint32_t attemptSwap, bool frameOk)
    {
        CaptureClose close;
        close.completedFrames = frameOk ? completedBefore + 1 : completedBefore;
        close.requestedFrames = requested;
        close.attemptFrame = attemptFrame;
        close.attemptSwap = attemptSwap;
        close.frameOk = frameOk;
        close.publish = ShouldPublishArchive(close.completedFrames, requested, frameOk);
        close.continueNext = frameOk && close.completedFrames < requested;
        return close;
    }

    inline void WriteCaptureManifest(std::ostream &out, const CaptureClose &close, uint32_t firstFrame)
    {
        out << "requested_frames=" << close.requestedFrames
            << "\ncompleted_frames=" << close.completedFrames
            << "\nfirst_frame=" << firstFrame
            << "\nlast_attempted_frame=" << close.attemptFrame
            << "\nlast_attempted_swap=" << close.attemptSwap
            << "\nstatus=" << (!close.frameOk ? "incomplete" : close.completedFrames == close.requestedFrames ? "complete" : "capturing")
            << "\n";
    }

    inline void WriteRuntimeFrameStatus(std::ostream &out, const CaptureClose &close)
    {
        out << "frame=" << close.attemptFrame << "\nswap=" << close.attemptSwap << '\n';
    }

    // Generic early-present result. The GPU queued-copy callback is supplied only
    // when LO_GPU_PLUME built the readback; callers pass gpuQueued=false otherwise.
    template <class OnQueued>
    inline void FinishPresentCaptureExit(const Ticket *ticket, Result *result, bool gpuQueued, OnQueued onQueued)
    {
        if (result && result->attempted) return;
        if (gpuQueued)
        {
            onQueued();
            return;
        }
        if (result && ticket && ticket->active)
        {
            result->attempted = true;
            result->available = false;
            result->reason = "present_ended_before_capture";
            result->rendererFrame = ticket->rendererFrame;
            result->swap = ticket->swap;
            result->deviceEpoch = ticket->deviceEpoch;
        }
    }

    inline bool WriteRgbaBmp(const std::filesystem::path &path, const std::vector<uint32_t> &pixels, uint32_t width, uint32_t height)
    {
        std::error_code error;
        if (!width || !height || pixels.size() != size_t(width) * height)
        {
            std::filesystem::remove(path, error);
            return false;
        }
        const auto temporary = std::filesystem::path(path.wstring() + L".partial");
        std::filesystem::remove(temporary, error);
        bool ok = false;
        {
            std::ofstream bmp(temporary, std::ios::binary);
            auto put = [&](uint32_t value, int bytes) { for (int i = 0; i < bytes; ++i) bmp.put(char(value >> (8 * i))); };
            bmp.write("BM", 2); put(54 + width * height * 4, 4); put(0, 4); put(54, 4);
            put(40, 4); put(width, 4); put(uint32_t(-int32_t(height)), 4); put(1, 2); put(32, 2);
            put(0, 4); put(width * height * 4, 4); put(0, 4); put(0, 4); put(0, 4); put(0, 4);
            for (auto pixel : pixels) { bmp.put(char(pixel >> 16)); bmp.put(char(pixel >> 8)); bmp.put(char(pixel)); bmp.put(0); }
            bmp.close();
            ok = !bmp.fail();
        }
        if (!ok)
        {
            std::filesystem::remove(temporary, error);
            std::filesystem::remove(path, error);
            return false;
        }
        std::filesystem::rename(temporary, path, error);
        if (error)
        {
            std::filesystem::remove(temporary, error);
            std::filesystem::remove(path, error);
            return false;
        }
        return true;
    }

    inline bool CommitFinalImage(Result &image, const std::filesystem::path &path)
    {
        std::error_code error;
        if (!image.available)
        {
            std::filesystem::remove(path, error);
            return false;
        }
        if (!WriteRgbaBmp(path, image.pixels, image.width, image.height))
        {
            image.available = false;
            image.pixels.clear();
            image.reason = "bmp_write_failed";
            std::filesystem::remove(path, error);
            return false;
        }
        return true;
    }

    inline void AppendFrameMetadata(std::ostream &out, const Ticket &ticket, const GuestImage &guest, const Result &finalImage)
    {
        out << "capture renderer_frame=" << ticket.rendererFrame << " swap=" << ticket.swap
            << " device_epoch=" << ticket.deviceEpoch << "\n";
        out << "guest_image file=guest-frontbuffer.bmp status=" << (guest.available ? "available" : "unavailable")
            << " reason=" << guest.reason
            << " source=renderer-resolved-frontbuffer address=" << guest.address
            << " size=" << guest.width << "x" << guest.height
            << " source_write_frame=" << guest.sourceWriteFrame
            << " source_write_ordinal=" << guest.sourceWriteOrdinal << "\n";
        if (ticket.hasSourcePlan)
            out << "guest_source_plan cpu_serial=" << ticket.planCpuSerial
                << " size=" << ticket.planWidth << "x" << ticket.planHeight
                << " output=" << ticket.planOutputWidth << "x" << ticket.planOutputHeight
                << " upscaler=" << ticket.planUpscaler << " dlss_quality=" << ticket.planQuality << "\n";
        out << "final_image file=screenshot.bmp status=" << (finalImage.available ? "available" : "unavailable")
            << " reason=" << finalImage.reason
            << " source=swapchain-pre-present renderer_frame=" << finalImage.rendererFrame
            << " swap=" << finalImage.swap
            << " size=" << finalImage.width << "x" << finalImage.height
            << " format=" << (finalImage.format.empty() ? "unavailable" : finalImage.format)
            << " backend=" << (finalImage.backend.empty() ? "unavailable" : finalImage.backend)
            << " present_accepted=" << (finalImage.presentAccepted ? "true" : "false")
            << " presentation_submission_serial=" << (finalImage.hasSubmissionSerial ? std::to_string(finalImage.submissionSerial) : "unavailable")
            << " presentation_fence_value=" << (finalImage.hasFenceValue ? std::to_string(finalImage.fenceValue) : "unavailable")
            << "\n";
    }
}
