#pragma once
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
#include "streamline_runtime.h"
#include "frame_generation_snapshot.h"
#include "dlss_fg_depth.h"
#include "dlss_fg_completion.h"
#include "dlss_fg_host_use.h"
#include "dlss_fg_policy.h"
#include "dlss_fg_runtime_policy.h"

namespace gpu::dlss_fg {
// Presentation-thread owner. The first implementation serializes input reuse
// behind a post-present queue fence; SDK diagnostics do not disable this wait.
class Session {
public:
    Session(Runtime& runtime, plume::VulkanDevice& device, plume::VulkanCommandQueue& queue);
    ~Session();
    bool Initialize();
    bool Prepare(const std::shared_ptr<frame_generation::ProducerSnapshot>& inputs,
        uint32_t width, uint32_t height, uint32_t buffers, VkFormat format,
        plume::RenderCommandList* commands = nullptr, double producerWaitMs = 0.0);
    void SubmitStart();
    void HostSubmitted(bool success, uint64_t serial, int32_t nativeResult);
    void CancelUnsubmitted(plume::RenderCommandList* commands);
    void SubmitEnd();
    void PresentStart();
    void Presented(bool accepted);
    void Disable();
    void Quiesce();
    void Shutdown();
    // GPU owner-thread query: last checked runtime state, not generated/display
    // evidence. Input capture stays independent so a blocked mode can recover.
    bool Available() const {
        return ready_ && enabled_ && !failed_ && runtimeState_.Enabled() &&
            runtime_.FeatureCreationFailureCount() == creationFailuresSeen_;
    }
private:
    bool Check(sl::Result result, const char* operation, bool featureRequest = false);
    bool Mode(bool enabled);
    void Mark(sl::PCLMarker marker);
    void DrainInputs();
    void SignalInputCompletion();
    void ObserveInputs(bool enabled, bool reset, Interruption reason);
    void LogContinuity();
    bool ObserveCreationFailure();
    [[noreturn]] void FailClosed(const char* operation, int32_t nativeResult = 0);
    Runtime& runtime_;
    plume::VulkanDevice& device_;
    plume::VulkanCommandQueue& queue_;
    sl::ViewportHandle viewport_{0};
    sl::FrameToken* token_ = nullptr;
    sl::DLSSGOptions options_{};
    VkFence completion_ = VK_NULL_HANDLE;
    PresentQueueCompletion inputCompletion_;
    HostInputUse hostUse_;
    std::shared_ptr<frame_generation::ProducerSnapshot> retained_;
    temporal::Matrix previousVP_{};
    temporal::Viewport previousRaster_{};
    frame_plan::FramePlan previousPlan_{};
    Continuity continuity_;
    FeatureRuntimeState runtimeState_;
    PresentCounterWindow presentCounter_;
    DepthRemapper depth_;
    uint64_t previousFrame_ = 0, previousEpoch_ = 0;
    upscaling::Upscaler previousProvider_ = upscaling::Upscaler::Off;
    uint64_t generatedIntervals_ = 0, actualPresents_ = 0;
    uint32_t frame_ = 0;
    unsigned creationFailuresSeen_ = 0;
    bool samplePending_ = false, sampleReset_ = false;
    bool enabled_ = false, ready_ = false, failed_ = false, used_ = false;
};
}
#endif
