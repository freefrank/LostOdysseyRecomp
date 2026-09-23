#include "gpu/present_capture.h"

#if defined(LO_PRESENT_CAPTURE_MODE_PLUME)
#define LO_GPU_PLUME 1
static constexpr bool kGpu = true;
static constexpr bool kUnit = false;
int LoPresentCapturePreprocessPlume()
#elif defined(LO_PRESENT_CAPTURE_MODE_OFF)
static constexpr bool kGpu = false;
static constexpr bool kUnit = false;
int LoPresentCapturePreprocessOff()
#elif defined(LO_PRESENT_CAPTURE_MODE_UNIT)
#define LO_GPU_PLUME 1
#define LO_VIDEO_SUBMISSION_UNIT 1
static constexpr bool kGpu = true;
static constexpr bool kUnit = true;
int LoPresentCapturePreprocessUnit()
#else
#error present capture preprocess mode is required
#endif
{
#if defined(LO_GPU_PLUME)
    static_assert(kGpu, "LO_GPU_PLUME on");
#else
    static_assert(!kGpu, "LO_GPU_PLUME off");
#endif
#if defined(LO_VIDEO_SUBMISSION_UNIT)
    static_assert(kUnit, "submission unit");
#else
    static_assert(!kUnit, "not the submission unit");
#endif
    gpu::present_capture::Ticket ticket;
    ticket.active = true;
    ticket.rendererFrame = 7;
    ticket.swap = 7;
    gpu::present_capture::Result result;
    bool queued = false;
    gpu::present_capture::FinishPresentCaptureExit(&ticket, &result, kGpu, [&] {
        queued = true;
        result.attempted = true;
        result.reason = "gpu_queued";
    });
    if (kGpu) return queued && result.reason == "gpu_queued" ? 0 : 1;
    return !queued && result.reason == "present_ended_before_capture" ? 0 : 2;
}
