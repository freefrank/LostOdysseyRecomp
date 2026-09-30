#include "dlss_frame_generation.h"
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
#include "dlss_fg_constants.h"
#include "vulkan_command_recording.h"
#include <os/logger.h>
#include <cstdlib>
#include <cstdio>
#include <mutex>
#include <chrono>

namespace gpu::dlss_fg {
namespace {
uint64_t SteadyMs() {
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
}
Session::Session(Runtime& runtime, plume::VulkanDevice& device, plume::VulkanCommandQueue& queue)
    : runtime_(runtime), device_(device), queue_(queue) {}
Session::~Session() { Shutdown(); }
bool Session::Check(sl::Result result, const char* operation, bool featureRequest) {
    if (result == sl::Result::eOk) return true;
    LOG_ERROR("DLSS FG: {} result={}", operation, int(result));
    if (!featureRequest) failed_ = true;
    runtimeState_.Fail();
    return false;
}
bool Session::Initialize() {
    if (ready_) return !failed_;
    std::string reason;
    if (!runtime_.ImportFunctions(reason)) { LOG_ERROR("DLSS FG: {}", reason); return false; }
    sl::ReflexOptions reflex{};
    reflex.mode = sl::ReflexMode::eLowLatency;
    if (!Check(runtime_.ReflexSetOptions(reflex), "Reflex options")) return false;
    VkFenceCreateInfo info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (!depth_.Initialize(&device_)) return false;
    if (vkCreateFence(device_.vk, &info, nullptr, &completion_) != VK_SUCCESS) return false;
    options_.numFramesToGenerate = 1;
    options_.queueParallelismMode = sl::DLSSGQueueParallelismMode::eBlockPresentingClientQueue;
    creationFailuresSeen_ = runtime_.FeatureCreationFailureCount();
    ready_ = true;
    return Mode(false);
}
bool Session::Mode(bool enabled) {
    options_.mode = enabled ? sl::DLSSGMode::eOn : sl::DLSSGMode::eOff;
    used_ |= enabled; // Even a failed enable may have partial SDK resources.
    if (!Check(runtime_.DLSSGSetOptions(viewport_, options_), "FG options", enabled)) return false;
    if (enabled != enabled_) LOG_INFO("DLSS FG: mode_request={} runtime={} input=composited_backbuffer ui_separation=unavailable multiplier=2", enabled ? "on" : "off", Name(runtimeState_.Phase()));
    enabled_ = enabled;
    if (!enabled) runtimeState_.SuspendInputs();
    return true;
}
void Session::Mark(sl::PCLMarker marker) {
    if (token_) Check(runtime_.PCLSetMarker(marker, *token_), "PCL marker");
}
bool Session::Prepare(const std::shared_ptr<frame_generation::ProducerSnapshot>& inputs,
    uint32_t width, uint32_t height, uint32_t buffers, VkFormat format, plume::RenderCommandList* commands,
    double producerWaitMs) {
    if (!ready_) return false;
    using Clock = std::chrono::steady_clock;
    const auto begin = Clock::now();
    DrainInputs(); // Retire the previous SDK frame only when its input slot is reused.
    const auto drained = Clock::now();
    // A recorded/tagged input without Presented's marker has unknown use.
    // Do not overwrite its only retained owner with the next snapshot.
    if (retained_) FailClosed("prepare before previous input completion");
    ObserveCreationFailure(); // A worker-thread error may arrive after GetState.
    token_ = nullptr;
    ++frame_;
    if (!Check(runtime_.NewFrameToken(token_, &frame_), "frame token") || !token_) {
        Disable(); ObserveInputs(false, false, Interruption::SdkFailure); return false;
    }
    const auto sleepBegin = Clock::now();
    Check(runtime_.ReflexSleep(*token_), "Reflex sleep");
    const auto slept = Clock::now();
    // These markers cover the host presentation transaction. Full guest input
    // latency instrumentation is separate from this experimental FG path.
    Mark(sl::PCLMarker::eSimulationStart);
    Mark(sl::PCLMarker::eSimulationEnd);
    const bool reset = !inputs || !enabled_ || inputs->inputs.resetHistory ||
        inputs->inputs.motionState == temporal::MotionState::ResetInitialization ||
        previousEpoch_ != inputs->inputs.temporalEpoch || previousFrame_ + 1 != inputs->inputs.renderFrameId ||
        previousProvider_ != inputs->inputs.plan.requestedUpscaler ||
        !SameHistoryConfiguration(previousPlan_, inputs->inputs.plan) ||
        options_.colorWidth != width || options_.colorHeight != height ||
        options_.colorBufferFormat != uint32_t(format) || options_.numBackBuffers != buffers ||
        options_.mvecDepthWidth != inputs->inputs.depth.width || options_.mvecDepthHeight != inputs->inputs.depth.height;
    sl::Constants constants{};
    DepthRemap remap{};
    // Renderer snapshots are already submitted on this presenting queue.
    // The present semaphore orders their copies and this depth pass before
    // Streamline reads them (DLSS-G Vulkan guide section 16). CPU completion
    // is unnecessary; retained_ still protects them through the SDK drain.
    if (failed_ || !inputs || !inputs->producerSerial || !inputs->producerOnPresentQueue ||
        !inputs->inputsQualifiedAtCapture || !inputs->inputs.CompleteForFrameGeneration() ||
        inputs->producerDiscarded || inputs->producerWaitFailed || inputs->lineageCanceled ||
        !width || !height || !buffers || format == VK_FORMAT_UNDEFINED ||
        width != inputs->inputs.plan.output.width || height != inputs->inputs.plan.output.height ||
        !commands || !BuildConstants(inputs->inputs, reset ? nullptr : &previousVP_, constants, &remap,
            reset ? nullptr : &previousRaster_)) {
        if (frame_ % 120 == 0 && std::getenv("LO_MV_LOG"))
            LOG_INFO("DLSS FG rejected: frame={} failed={} inputs={} qualified={} motion={} input_epoch={} producer_serial={} on_queue={} discarded={} wait_failed={} canceled={} output={}x{} plan_output={}x{} buffers={} format={} commands={} reset={}",
                frame_, failed_, bool(inputs), inputs && inputs->inputsQualifiedAtCapture,
                inputs ? uint32_t(inputs->inputs.motionState) : 0, inputs ? inputs->inputs.temporalEpoch : 0,
                inputs ? inputs->producerSerial : 0, inputs && inputs->producerOnPresentQueue,
                inputs && inputs->producerDiscarded, inputs && inputs->producerWaitFailed, inputs && inputs->lineageCanceled,
                width, height, inputs ? inputs->inputs.plan.output.width : 0, inputs ? inputs->inputs.plan.output.height : 0,
                buffers, uint32_t(format), commands != nullptr, reset);
        Disable();
        ObserveInputs(false, false, failed_ ? Interruption::SdkFailure :
            !inputs ? Interruption::NoInputs : Interruption::InvalidInputs);
        return false;
    }
    const auto& in = inputs->inputs;
    const bool retry = runtimeState_.Phase() == RuntimePhase::Unavailable;
    if (!runtimeState_.Request(MakeFeatureKey(in.plan, in.depth.width, in.depth.height,
            width, height, buffers, uint32_t(format)))) {
        Disable();
        ObserveInputs(false, false, Interruption::SdkFailure);
        return false;
    }
    if (retry) LOG_INFO("DLSS FG: retry after feature configuration/resource boundary; runtime=pending provider={} request={} output={}x{}", uint32_t(in.plan.requestedUpscaler), in.plan.requestSignature, width, height);
    if (!hostUse_.Begin(commands)) FailClosed("overlapping FG input recording");
    retained_ = inputs; // Retain source images before recording their GPU read.
    const auto depthBegin = Clock::now();
    auto* remappedDepth = depth_.Record(commands, in.depth, remap);
    const auto depthRecorded = Clock::now();
    if (!remappedDepth) { Disable(); ObserveInputs(false, false, Interruption::InvalidInputs); return false; }
    options_.numBackBuffers = buffers;
    options_.colorWidth = width; options_.colorHeight = height; options_.colorBufferFormat = format;
    options_.mvecDepthWidth = in.depth.width; options_.mvecDepthHeight = in.depth.height;
    options_.depthBufferFormat = VK_FORMAT_R32_SFLOAT;
    options_.mvecBufferFormat = VK_FORMAT_R16G16_SFLOAT;
    if (!Mode(true)) { Disable(); ObserveInputs(false, false, Interruption::SdkFailure); return false; }
    sl::Resource resources[2] = {{sl::ResourceType::eTex2d, nullptr}, {sl::ResourceType::eTex2d, nullptr}};
    const temporal::TextureRegion regions[] = {{remappedDepth, {in.depth.width, in.depth.height},
        0, 0, in.depth.width, in.depth.height}, in.motion};
    sl::ResourceTag tags[] = {{nullptr, sl::kBufferTypeDepth, sl::eValidUntilPresent},
        {nullptr, sl::kBufferTypeMotionVectors, sl::eValidUntilPresent},
        {nullptr, sl::kBufferTypeHUDLessColor, sl::eValidUntilPresent},
        {nullptr, sl::kBufferTypeUIColorAndAlpha, sl::eValidUntilPresent}};
    for (unsigned i = 0; i < 2; ++i) {
        auto& texture = *static_cast<plume::VulkanTexture*>(regions[i].texture);
        resources[i] = {sl::ResourceType::eTex2d, reinterpret_cast<void*>(texture.vk),
            reinterpret_cast<void*>(texture.allocationInfo.deviceMemory), reinterpret_cast<void*>(texture.imageView),
            uint32_t(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};
        resources[i].width = texture.desc.width; resources[i].height = texture.desc.height;
        resources[i].nativeFormat = texture.imageFormat; resources[i].mipLevels = 1; resources[i].arrayLayers = 1;
        resources[i].usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        if (texture.desc.flags & plume::RenderTextureFlag::RENDER_TARGET) resources[i].usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (texture.desc.flags & plume::RenderTextureFlag::STORAGE) resources[i].usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        tags[i].resource = &resources[i];
        tags[i].extent = {regions[i].y, regions[i].x, regions[i].width, regions[i].height};
    }
    if (!Check(runtime_.SetConstants(constants, *token_, viewport_), "constants") ||
        !Check(runtime_.SetTagForFrame(*token_, viewport_, tags, 4, nullptr), "composited inputs")) {
        Disable(); ObserveInputs(false, false, Interruption::SdkFailure); return false;
    }
    previousPlan_ = in.plan;
    // SetOptions/SetTag succeeding only requests FG. Account this sample after
    // Present has exposed a runtime status and any deferred creation error.
    samplePending_ = true;
    sampleReset_ = reset;
    previousVP_ = in.cameraViewProjection;
    previousRaster_ = in.cameraRaster;
    previousFrame_ = in.renderFrameId; previousEpoch_ = in.temporalEpoch;
    previousProvider_ = in.plan.requestedUpscaler;
    if (frame_ % 60 == 0) {
        const auto ms = [](auto elapsed) { return std::chrono::duration<double, std::milli>(elapsed).count(); };
        LOG_INFO("DLSS FG timing: frame={} producer_acquire_ms={} input_drain_ms={} reflex_sleep_ms={} depth_record_ms={} prepare_ms={} scope=cpu_wall_not_gpu_execution",
            frame_, producerWaitMs, ms(drained - begin), ms(slept - sleepBegin),
            ms(depthRecorded - depthBegin), ms(Clock::now() - begin));
    }
    return true;
}
void Session::LogContinuity() {
    const double ratio = continuity_.Samples() ?
        100.0 * double(continuity_.Enabled()) / double(continuity_.Samples()) : 0.0;
    LOG_INFO("DLSS FG continuity: samples={} enabled_samples={} enabled_percent={} longest_interruption_ms={} resumes={} reset_samples={} reason={} scope=present_checked_attempts_not_display",
        continuity_.Samples(), continuity_.Enabled(), ratio, continuity_.LongestInterruption(SteadyMs()),
        continuity_.Resumes(), continuity_.Resets(), Name(continuity_.Reason()));
}
void Session::ObserveInputs(bool enabled, bool reset, Interruption reason) {
    const auto previousReason = continuity_.Reason();
    continuity_.Observe(enabled, reset, reason, SteadyMs());
    if (continuity_.Samples() <= 5 || continuity_.Samples() % 60 == 0 ||
        previousReason != continuity_.Reason()) LogContinuity();
}
void Session::SubmitStart() {
    if (!hostUse_.SubmissionStarted()) FailClosed("duplicate host submit attempt");
    Mark(sl::PCLMarker::eRenderSubmitStart);
}
void Session::HostSubmitted(bool success, uint64_t serial, int32_t nativeResult) {
    if (!hostUse_.Submitted(success, serial))
        FailClosed("host submission failed or missing serial", nativeResult);
}
void Session::CancelUnsubmitted(plume::RenderCommandList* commands) {
    if (!hostUse_.Matches(commands) || hostUse_.Serial()) return;
    if (!hostUse_.CanCancel(commands)) FailClosed("cancellation after submit attempt");
    auto& list = *static_cast<plume::VulkanCommandList*>(commands);
    // A reset of this never-submitted list revokes all recorded host reads.
    // It is not evidence that any SDK work or producer copy has completed.
    const auto reset = vkResetCommandBuffer(list.vk, 0);
    if (reset != VK_SUCCESS) FailClosed("cancel command reset failed", int32_t(reset));
    submission::ClearBindings(list);
    list.recording = false;
    list.externalCommandsOpen = false;
    list.activeRenderPass = VK_NULL_HANDLE;
    Disable();
    sl::ResourceTag tags[] = {{nullptr, sl::kBufferTypeDepth, sl::eValidUntilPresent},
        {nullptr, sl::kBufferTypeMotionVectors, sl::eValidUntilPresent},
        {nullptr, sl::kBufferTypeHUDLessColor, sl::eValidUntilPresent},
        {nullptr, sl::kBufferTypeUIColorAndAlpha, sl::eValidUntilPresent}};
    if (!token_ || !Check(runtime_.SetTagForFrame(*token_, viewport_, tags, 4, nullptr), "cancel input tags"))
        FailClosed("cancel input tags failed");
    // Producer copies preceded this recording on the same queue. They still
    // own these images even though our host list never reached submission.
    // Previous SDK input use was already drained before Prepare began.
    VkResult drained;
    {
        std::unique_lock lock(*queue_.queue->mutex);
        drained = vkQueueWaitIdle(queue_.queue->vk);
    }
    if (drained != VK_SUCCESS) FailClosed("cancel producer drain failed", int32_t(drained));
    if (!hostUse_.Canceled(commands, true, true, true)) FailClosed("invalid input cancellation");
    retained_.reset();
    // Plume's layout cache was changed while recording. An unexecuted image
    // must not enter the completed-input reuse pool with that cached layout.
    depth_.DiscardUnsubmitted();
    presentCounter_.Reset();
    if (samplePending_) {
        ObserveInputs(false, false, Interruption::Canceled);
        samplePending_ = false;
    }
    continuity_.Suspend(Interruption::Canceled, SteadyMs());
    LogContinuity();
    token_ = nullptr;
    LOG_INFO("DLSS FG: unsubmitted input canceled commands_reset=true tags_revoked=true producer_drained=true retained=false");
}
void Session::SubmitEnd() { Mark(sl::PCLMarker::eRenderSubmitEnd); }
void Session::PresentStart() { Mark(sl::PCLMarker::ePresentStart); }
[[noreturn]] void Session::FailClosed(const char* operation, int32_t nativeResult) {
    LOG_ERROR("DLSS FG: {} native_result={} frame={} submitted_input_serial={} completed_input_serial={} retained={}; terminating without releasing unresolved resources",
        operation, nativeResult, frame_, inputCompletion_.SubmittedSerial(),
        inputCompletion_.CompletedSerial(), bool(retained_));
    std::fflush(nullptr);
    std::_Exit(EXIT_FAILURE);
}
void Session::SignalInputCompletion() {
    if (!completion_ || !inputCompletion_.CanSubmit())
        FailClosed("missing or still-pending input completion fence");
    // BlockPresentingClientQueue makes the first submit following Present wait
    // for SDK input processing. Only a successful submit publishes a serial.
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkResult result = vkResetFences(device_.vk, 1, &completion_);
    if (result == VK_SUCCESS) {
        std::unique_lock lock(*queue_.queue->mutex);
        result = vkQueueSubmit(queue_.queue->vk, 1, &submit, completion_);
    }
    if (!inputCompletion_.Submitted(result == VK_SUCCESS))
        FailClosed("input completion submit failed", int32_t(result));
}
void Session::DrainInputs() {
    if (!inputCompletion_.Pending()) return;
    const auto serial = inputCompletion_.SubmittedSerial();
    const auto result = vkWaitForFences(device_.vk, 1, &completion_, VK_TRUE, 10'000'000'000ull);
    if (!inputCompletion_.Completed(serial, result == VK_SUCCESS))
        FailClosed("input completion wait failed", int32_t(result));
    if (!hostUse_.Completed()) FailClosed("SDK completion without host submission");
    // Neither a mode request nor device-idle can take this release path.
    retained_.reset();
    depth_.ReleaseAfterInputDrain();
    if (serial <= 5 || serial % 60 == 0)
        LOG_INFO("DLSS FG input completion: submitted_serial={} completed_serial={} pending=false retained=false evidence=checked_post_present_queue_fence",
            inputCompletion_.SubmittedSerial(), inputCompletion_.CompletedSerial());
}
bool Session::ObserveCreationFailure() {
    const auto count = runtime_.FeatureCreationFailureCount();
    if (count == creationFailuresSeen_) return false;
    const auto newFailures = count - creationFailuresSeen_;
    creationFailuresSeen_ = count;
    runtimeState_.Fail();
    Disable(); // Retained input ownership still waits on its checked marker.
    continuity_.Suspend(Interruption::SdkFailure, SteadyMs());
    LOG_ERROR("DLSS FG: runtime=unavailable reason=ngx_feature_creation new_failures={} creation_failures={} enabled=false retry=configuration_or_resource_boundary", newFailures, count);
    return true;
}
void Session::Presented(bool accepted) {
    if (!ready_ || !token_) { presentCounter_.Reset(); return; }
    if (retained_ && !hostUse_.Serial()) FailClosed("present before host submission");
    Mark(sl::PCLMarker::ePresentEnd);
    // A rejected Present provides no documented input-completion ordering.
    // Keep uncertain resources alive and stop instead of calling them drained.
    if (!accepted && (retained_ || enabled_)) FailClosed("present rejected with unresolved FG input");
    // GetState is telemetry, not the synchronization boundary. Even if it
    // fails, an accepted Present must still submit and wait its queue marker.
    if (accepted) SignalInputCompletion();
    sl::DLSSGState state{};
    const bool stateQueried = enabled_ || runtimeState_.Phase() != RuntimePhase::Unavailable;
    const bool stateOk = !stateQueried || Check(runtime_.DLSSGGetState(viewport_, state, nullptr), "present state", true);
    if (!stateOk) {
        // State retrieval is not the completion boundary. Check the already
        // submitted post-Present marker before releasing anything; do not
        // advance the retry generation or add a device-wide wait on failure.
        Disable();
        DrainInputs();
    }
    const bool creationFailed = ObserveCreationFailure();
    const bool statusFailed = enabled_ && stateOk && state.status != sl::DLSSGStatus::eOk;
    if (statusFailed) {
        runtimeState_.Fail();
        Disable();
        LOG_ERROR("DLSS FG: runtime=unavailable reason=runtime_status status={} enabled=false retry=configuration_or_resource_boundary", unsigned(state.status));
    }
    if (samplePending_) {
        runtimeState_.Presented(accepted && stateOk && !failed_ && !creationFailed && !statusFailed);
        ObserveInputs(runtimeState_.Enabled(), sampleReset_, runtimeState_.Enabled() ?
            Interruption::None : Interruption::SdkFailure);
        samplePending_ = false;
    }
    if (accepted && stateQueried && stateOk) actualPresents_ += state.numFramesActuallyPresented;
    const bool active = Available();
    const bool contiguousDelta = presentCounter_.Observe(stateQueried && stateOk, accepted);
    generatedIntervals_ += accepted && active && contiguousDelta && state.numFramesActuallyPresented > 1;
    if (frame_ <= 5 || frame_ % 60 == 0 || creationFailed || statusFailed || !stateOk)
        LOG_INFO("DLSS FG: frame={} source_frame={} enabled={} mode_requested={} runtime={} accepted={} state_queried={} present_delta_contiguous={} status={} actual_presents={} generated_intervals={} total_presents={} sdk_errors={} creation_failures={}",
            frame_, previousFrame_, active, enabled_, Name(runtimeState_.Phase()), accepted, stateQueried, contiguousDelta, unsigned(state.status), state.numFramesActuallyPresented,
            generatedIntervals_, actualPresents_, runtime_.ErrorCount(), creationFailuresSeen_);
    if (!accepted) Quiesce();
    token_ = nullptr;
    if (!accepted || !stateOk) Disable();
}
void Session::Disable() {
    if (!ready_) return;
    // A failed Prepare may already have recorded the depth conversion into
    // the host list. Keep that allocation until Presented's checked fence.
    if (enabled_ || options_.mode != sl::DLSSGMode::eOff) {
        // A failed eOn call can have partially changed SDK options. Explicitly
        // confirm eOff even when the host never marked that enable successful.
        if (token_) {
            sl::ResourceTag tags[] = {{nullptr, sl::kBufferTypeDepth, sl::eValidUntilPresent},
                {nullptr, sl::kBufferTypeMotionVectors, sl::eValidUntilPresent},
                {nullptr, sl::kBufferTypeHUDLessColor, sl::eValidUntilPresent},
                {nullptr, sl::kBufferTypeUIColorAndAlpha, sl::eValidUntilPresent}};
            if (!Check(runtime_.SetTagForFrame(*token_, viewport_, tags, 4, nullptr), "disable input tags"))
                FailClosed("disable input tags failed");
        }
        if (!Mode(false)) FailClosed("disable options failed");
    }
    runtimeState_.SuspendInputs();
    previousFrame_ = previousEpoch_ = 0;
    previousProvider_ = upscaling::Upscaler::Off;
}
void Session::Quiesce() {
    if (!ready_) return;
    continuity_.Suspend(Interruption::ResourceBoundary, SteadyMs());
    DrainInputs();
    // The recording exit guard cancels only lists with no submit attempt.
    // Submitted/uncertain input use still requires the checked SDK marker.
    if (retained_ || hostUse_.Pending()) FailClosed("quiesce without post-present input completion");
    Disable();
    const auto result = vkDeviceWaitIdle(device_.vk);
    if (result != VK_SUCCESS) FailClosed("SDK quiesce failed", int32_t(result));
    runtimeState_.ResourceBoundary();
    presentCounter_.Reset();
    token_ = nullptr;
    LOG_INFO("DLSS FG quiesce: submitted_input_serial={} completed_input_serial={} pending=false retained=false",
        inputCompletion_.SubmittedSerial(), inputCompletion_.CompletedSerial());
}
void Session::Shutdown() {
    if (!ready_) return;
    // Off applies at a later Present. Do not issue a fake frame. First retire
    // inputs by their checked marker, then drain before SDK/device teardown.
    Quiesce();
    // Release the FG feature while both Streamline and native NGX sessions
    // are alive. A local fence drain does not prove SDK destruction succeeded.
    if (used_ && !runtime_.FreeResources(sl::kFeatureDLSS_G, viewport_))
        FailClosed("FG feature release failed");
    used_ = false;
    LogContinuity();
    vkDestroyFence(device_.vk, completion_, nullptr);
    completion_ = VK_NULL_HANDLE; ready_ = false;
    LOG_INFO("DLSS FG: session ended generated_intervals={} actual_presents={} submitted_input_serial={} completed_input_serial={} cleanup=complete",
        generatedIntervals_, actualPresents_, inputCompletion_.SubmittedSerial(), inputCompletion_.CompletedSerial());
}
}
#endif
