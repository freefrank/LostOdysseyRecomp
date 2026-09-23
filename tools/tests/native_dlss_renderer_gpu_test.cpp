// Compile the production Renderer itself. No game code, assets, or SDL window
// are linked. The only substituted GPU operation is the proprietary NGX call.
#define LO_RENDERER_P2_EMBEDDED_TEST 1
#include <gpu/renderer.cpp>
#include <gpu/vulkan_command_recording.h>
#include <gpu/vulkan_submission_state.h>
#include <gpu/fsr_upscaler.h>
#include <cfloat>
#include <stdexcept>
#include <json.hpp>

namespace fixture {
using namespace plume;
RenderDevice* device = nullptr;
RenderCommandQueue* queue = nullptr;
gpu::submission::VulkanState state;
uint32_t lastListCount = 0;
uint32_t checks = 0;
void Require(bool ok, const char* message) {
    ++checks;
    if (!ok) throw std::runtime_error(message);
}
}
// Platform boundary only: real Vulkan commands/submissions/fences, with the
// same production checked recording helpers and submission state as video.cpp.
namespace fixture {
gpu::frame_plan::PlannerState* statusPlanner = nullptr;
struct ExecutionReport {
    gpu::frame_plan::DlssExecutionOutcome outcome{};
    gpu::frame_plan::DlssEffectReason reason{};
    gpu::frame_plan::FramePlan plan{};
    uint64_t serial = 0;
    uint64_t frame = 0;
};
std::vector<ExecutionReport> executions;
bool rejectSubmit = false;
bool rejectWaitOnce = false;
}
namespace gpu::video {
bool GpuWorkStopped() { return fixture::state.Stopped(); }
void StopGpuWork(int32_t value) { fixture::state.Stop(value); }
bool BeginGpuCommands(plume::RenderCommandList* list) {
    if (GpuWorkStopped() || !list) return false;
    const auto result = submission::BeginCommands(*static_cast<plume::VulkanCommandList*>(list));
    if (result != VK_SUCCESS) StopGpuWork(result);
    return result == VK_SUCCESS;
}
bool EndGpuCommands(plume::RenderCommandList* list) {
    if (GpuWorkStopped() || !list) return false;
    const auto result = submission::EndCommands(*static_cast<plume::VulkanCommandList*>(list));
    if (result != VK_SUCCESS) StopGpuWork(result);
    return result == VK_SUCCESS;
}
bool WaitForGpuFence(plume::RenderCommandFence* fence) {
    if (fixture::rejectWaitOnce) { fixture::rejectWaitOnce = false; return false; }
    auto* f = static_cast<plume::VulkanCommandFence*>(fence);
    auto* d = static_cast<plume::VulkanDevice*>(fixture::device);
    return fixture::state.WaitSubmitted([&] { return int32_t(vkWaitForFences(d->vk, 1, &f->vk, VK_TRUE, UINT64_MAX)); });
}
bool WaitForPresentGpu() { return true; } // No swapchain in this fixture.
bool SubmitRendererBatch(const plume::RenderCommandList* const* lists, uint32_t count,
    plume::RenderCommandFence* fence, uint64_t& serial, int32_t& result) {
    auto* d = static_cast<plume::VulkanDevice*>(fixture::device);
    auto* q = static_cast<plume::VulkanCommandQueue*>(fixture::queue);
    auto* f = static_cast<plume::VulkanCommandFence*>(fence);
    std::vector<VkCommandBuffer> buffers;
    for (uint32_t i = 0; i < count; ++i) buffers.push_back(static_cast<const plume::VulkanCommandList*>(lists[i])->vk);
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = count; submit.pCommandBuffers = buffers.data();
    if (fixture::rejectSubmit) {
        fixture::rejectSubmit = false;
        fixture::lastListCount = 0;
        result = -3;
        serial = 0;
        return false;
    }
    fixture::lastListCount = count;
    return fixture::state.SubmitBatch([&] { return int32_t(vkResetFences(d->vk, 1, &f->vk)); },
        [&] { return int32_t(vkQueueSubmit(q->queue->vk, 1, &submit, f->vk)); }, serial, result);
}
}
namespace gpu::frame_plan {
void ReportPlanFailure(const PlanFailure& failure) {
    if (fixture::statusPlanner) fixture::statusPlanner->ReportFailure(failure);
}
void ReportDlssExecution(const DlssExecutionObservation& observation) {
    fixture::executions.push_back({observation.outcome, observation.reason, observation.plan, observation.submissionSerial, observation.renderFrame});
    if (fixture::statusPlanner) fixture::statusPlanner->ReportExecution(observation);
}
}
namespace gpu::taa_collection { bool Enabled() { return false; } }

namespace {
using namespace gpu::renderer;
using fixture::Require;
struct VendorFixture {
    gpu::dlss::SrStatus outcome = gpu::dlss::SrStatus::Executable;
    uint32_t calls = 0;
    uint32_t vendorCalls = 0;
    uint64_t failedUseId = 0, discardedUseId = 0;
    bool created = true;
    bool failCreate = false;
    gpu::dlss::SrStatus EnsureSession(const plume::VulkanDevice&) { return gpu::dlss::SrStatus::Executable; }
    gpu::dlss::SrAttempt RecordIsolated(plume::VulkanCommandList& list, const gpu::dlss::SrConfig& config,
        const gpu::temporal::TemporalFrameInputs& inputs, plume::VulkanTexture& output,
        gpu::dlss::EvaluateCapture* capture = nullptr) {
        ++calls;
        if (capture && (outcome == gpu::dlss::SrStatus::NeedsReconfigure || failCreate)) {
            capture->stage = failCreate ? "create" : "validation";
            capture->reason = failCreate ? "feature_create_failed" : "needs_reconfigure";
            if (failCreate) capture->createResult = -23;
            return {failCreate ? gpu::dlss::SrStatus::Failed : outcome};
        }
        Require(gpu::temporal::MatchesDepthConvention(inputs.depthConvention, config.depthInverted),
            "renderer must pass the producer depth ordering to NGX");
        Require(gpu::submission::BeginCommands(list) == VK_SUCCESS, "isolated begin");
        // Exact FP16 values; alpha is deliberately different from the guest.
        const VkClearColorValue value{{2.0f, .5f, .25f, 0.0f}};
        if (capture) {
            gpu::dlss::capture::Parameters sdk{};
            sdk.jitterX = float(inputs.jitter.pixelX); sdk.jitterY = float(inputs.jitter.pixelY);
            sdk.mvScaleX = sdk.mvScaleY = sdk.preExposure = sdk.exposureScale = 1;
            sdk.renderWidth = config.renderExtent.width; sdk.renderHeight = config.renderExtent.height;
            sdk.colorX = inputs.color.x; sdk.colorY = inputs.color.y;
            sdk.depthX = inputs.depth.x; sdk.depthY = inputs.depth.y;
            sdk.mvX = inputs.motion.x; sdk.mvY = inputs.motion.y;
            sdk.featureCreated = created; sdk.inputHistoryReset = inputs.resetHistory;
            sdk.reset = created || inputs.resetHistory;
            const auto& color = *static_cast<plume::VulkanTexture*>(inputs.color.texture);
            gpu::dlss::capture::InvokeEvaluate(list.vk, capture, color, output, sdk, [&] {
                ++vendorCalls;
                if (outcome == gpu::dlss::SrStatus::Executable)
                    vkCmdClearColorImage(list.vk, output.vk, VK_IMAGE_LAYOUT_GENERAL, &value, 1, &output.imageSubresourceRange);
                return outcome == gpu::dlss::SrStatus::Executable ? 0 : -17;
            }, [](int32_t value) { return value == 0; });
        } else {
            ++vendorCalls;
            vkCmdClearColorImage(list.vk, output.vk, VK_IMAGE_LAYOUT_GENERAL, &value, 1, &output.imageSubresourceRange);
        }
        Require(gpu::submission::EndCommands(list) == VK_SUCCESS, "isolated end");
        gpu::dlss::SrAttempt result{outcome};
        result.useId = outcome == gpu::dlss::SrStatus::Failed ? failedUseId : 0;
        return result;
    }
    void OnBatchDiscarded(uint64_t id) { discardedUseId = id; }
};
struct SkipNative : std::runtime_error { using std::runtime_error::runtime_error; };
// On an assertion exception, never submit a partly recorded list during stack
// unwinding. Drain previously submitted work before local image owners die.
struct LocalImageDrain {
    plume::RenderDevice* device;
    int exceptions = std::uncaught_exceptions();
    ~LocalImageDrain() {
        if (std::uncaught_exceptions() > exceptions)
            gpu::video::StopGpuWork(gpu::submission::VulkanState::InvalidState);
        const auto result = vkDeviceWaitIdle(static_cast<plume::VulkanDevice*>(device)->vk);
        if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST) std::_Exit(1);
    }
};
class Harness {
    std::unique_ptr<gpu::dlss::Controller> nativeController;
    std::unique_ptr<plume::RenderInterface> api;
    std::unique_ptr<plume::RenderDevice> device;
    std::unique_ptr<plume::RenderCommandQueue> queue;
    std::unique_ptr<Renderer> owner;
    Renderer& R() { return *owner; }
    std::unique_ptr<plume::RenderBuffer> readback;
    uint32_t readbackPitch = 32;
    std::unique_ptr<HostTexture> Texture(uint32_t w, uint32_t h, plume::RenderFormat format,
        uint32_t flags = plume::RenderTextureFlag::RENDER_TARGET) {
        auto t = std::make_unique<HostTexture>();
        t->allocationSerial = ++R().nextTargetAllocation;
        t->format = format; t->width = w; t->height = h;
        t->guestWidth = 1280; t->guestHeight = 720; t->resolutionSize = {w, h};
        t->texture = device->createTexture(plume::RenderTextureDesc::Texture2D(w, h, 1, format, flags));
        Require(bool(t->texture), "texture allocation"); return t;
    }
    void Clear(HostTexture& t, const plume::RenderColor& color) {
        Require(R().Begin(), "clear begin");
        R().Transition(t, plume::RenderTextureLayout::COLOR_WRITE, plume::RenderBarrierStage::GRAPHICS);
        R().commandList->setFramebuffer(R().GetFramebuffer(&t, nullptr));
        R().commandList->clearColor(0, color);
    }
    void UploadPattern(HostTexture& t, const std::vector<uint64_t>& pixels) {
        auto& r = R();
        Require(r.Begin(), "upload pattern begin");
        const uint32_t pitch = t.width;
        const uint64_t bytes = uint64_t(pitch) * t.height * sizeof(uint64_t);
        const uint64_t offset = r.Upload(pixels.data(), bytes, 512);
        Require(offset != UINT64_MAX, "upload pattern buffer space");
        r.Transition(t, plume::RenderTextureLayout::COPY_DEST, plume::RenderBarrierStage::COPY);
        r.commandList->copyTextureRegion(
            plume::RenderTextureCopyLocation::Subresource(t.texture.get()),
            plume::RenderTextureCopyLocation::PlacedFootprint(r.uploadRing, t.format, t.width, t.height, 1, pitch, offset));
        r.Transition(t, plume::RenderTextureLayout::COLOR_WRITE, plume::RenderBarrierStage::GRAPHICS);
    }
    std::vector<uint64_t> Pixels(HostTexture& t) {
        auto& r = R(); Require(r.Begin(), "readback begin");
        r.Transition(t, plume::RenderTextureLayout::COPY_SOURCE, plume::RenderBarrierStage::COPY);
        r.commandList->copyTextureRegion(plume::RenderTextureCopyLocation::PlacedFootprint(readback.get(), t.format,
            t.width, t.height, 1, readbackPitch), plume::RenderTextureCopyLocation::Subresource(t.texture.get()));
        Require(r.Flush() && r.WaitForReadback(), "readback fence");
        const auto* p = static_cast<const uint64_t*>(readback->map()); Require(p, "readback map");
        std::vector<uint64_t> result;
        for (uint32_t y = 0; y < t.height; ++y)
            result.insert(result.end(), p + y * readbackPitch, p + y * readbackPitch + t.width);
        readback->unmap(); return result;
    }
    void All(HostTexture& t, uint64_t expected, const char* message) {
        for (uint64_t pixel : Pixels(t)) Require(pixel == expected, message);
    }
public:
    void Init(bool native = false, const std::filesystem::path& runtime = {}) {
        if (native) {
            nativeController = std::make_unique<gpu::dlss::Controller>(
                std::filesystem::current_path() / "native-dlss-renderer-data", runtime);
            if (nativeController->Report().state == gpu::dlss::ProbeState::SdkDisabled)
                throw SkipNative("built without NGX SDK");
        }
        api = plume::CreateVulkanInterface(nativeController ? nativeController->ExtensionHooks() : plume::VulkanExtensionHooks{});
        Require(bool(api), "interface");
        device = api->createDevice(); Require(bool(device), "device");
        queue = device->createCommandQueue(plume::RenderCommandListType::DIRECT); Require(bool(queue), "queue");
        fixture::device = device.get(); fixture::queue = queue.get();
        std::printf("DEVICE=%s MODE=%s\n", device->getDescription().name.c_str(), native ? "native_ngx" : "synthetic_vendor");
        if (nativeController) {
            nativeController->ProbeOnce(*static_cast<plume::VulkanInterface*>(api.get()), *static_cast<plume::VulkanDevice*>(device.get()));
            const auto& report = nativeController->Report();
            if (report.state == gpu::dlss::ProbeState::Unavailable) throw SkipNative(report.reason);
            Require(report.state == gpu::dlss::ProbeState::Available, report.reason.c_str());
            Require(nativeController->EnsureSession(*static_cast<plume::VulkanDevice*>(device.get())) == gpu::dlss::SrStatus::Executable,
                "native persistent session initialization failed");
        }
        owner = std::make_unique<Renderer>(); auto& r = R();
        r.device = device.get(); r.queue = queue.get(); r.vulkan = true;
        r.dlssController = nativeController.get();
        r.binaryFormat = xenos::ShaderBinaryFormat::Spirv; r.renderFormat = plume::RenderShaderFormat::SPIRV;
        r.frame = 1; r.internalSize = {4, 4}; r.activePlan.width = 4; r.activePlan.height = 4;
        r.activePlan.cpuSerial = r.activePlan.geometryEpoch = r.activePlan.deviceEpoch = 1;
        r.activePlan.consumer = gpu::upscaling::TemporalConsumer::DlssSr;
        r.activePlan.output.width = 8; r.activePlan.output.height = 8;
        for (auto& slot : r.gpuSlots) {
            slot.list = queue->createCommandList(); slot.srIsolated = queue->createCommandList();
            slot.srContinuation = queue->createCommandList(); slot.fence = device->createCommandFence();
            slot.uploadRing = device->createBuffer(plume::RenderBufferDesc::UploadBuffer(kUploadRingSize,
                plume::RenderBufferFlag::DEVICE_ADDRESSABLE | plume::RenderBufferFlag::INDEX | plume::RenderBufferFlag::STORAGE));
            Require(slot.list && slot.srIsolated && slot.srContinuation && slot.fence && slot.uploadRing, "slot allocation");
            slot.uploadMapped = static_cast<uint8_t*>(slot.uploadRing->map()); Require(slot.uploadMapped, "ring map");
        }
        r.BindGpuSlot();
        plume::RenderPipelineLayoutBuilder layout;
        layout.begin(false, false);
        layout.addPushConstant(0, 0, 24, plume::RenderShaderStageFlag::VERTEX | plume::RenderShaderStageFlag::PIXEL);
        r.setBuilders[0].begin(); r.setBuilders[0].addByteAddressBuffer(0); r.setBuilders[0].end();
        for (int i = 1; i < 4; ++i) {
            r.setBuilders[i].begin();
            for (uint32_t j = 0; j < kTextureSlots; ++j) r.setBuilders[i].addTexture(j);
            r.setBuilders[i].end();
        }
        r.setBuilders[4].begin(); r.setBuilders[4].addSampler(0, 64); r.setBuilders[4].end();
        for (auto& b : r.setBuilders) layout.addDescriptorSet(b);
        layout.end(); r.pipelineLayout = layout.create(device.get()); Require(bool(r.pipelineLayout), "layout");
        r.staticSet0 = r.setBuilders[0].create(device.get()); r.staticSamplerSet = r.setBuilders[4].create(device.get());
        r.CreateDummyTexture(r.dummyTexture2D, plume::RenderTextureDimension::TEXTURE_2D, 0);
        r.CreateDummyTexture(r.dummyTexture3D, plume::RenderTextureDimension::TEXTURE_3D, 0);
        r.CreateDummyTexture(r.dummyTextureCube, plume::RenderTextureDimension::TEXTURE_2D, plume::RenderTextureFlag::CUBE);
        r.CompileBlitShaders(); r.CompileSceneCopyPromotionShaders();
        Require(r.blitVs && r.sceneCopyPromotionPs && r.sceneCopyPromotionRgbPs, "production shaders");
        readbackPitch = 32;
        readback = device->createBuffer(plume::RenderBufferDesc::ReadbackBuffer(readbackPitch * 32 * 8));
        Require(bool(readback), "readback allocation");
    }
    ~Harness() {
        if (owner) {
            R().Flush();
            const auto result = vkDeviceWaitIdle(static_cast<plume::VulkanDevice*>(device.get())->vk);
            if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST) std::_Exit(1);
            if (nativeController) {
                if (result == VK_ERROR_DEVICE_LOST) nativeController->AbandonUsesAfterDeviceLoss();
                else {
                    for (auto& slot : R().gpuSlots)
                        if (slot.srUseId && !slot.submitted) nativeController->OnBatchDiscarded(slot.srUseId);
                    nativeController->ReleaseCompletedThrough(fixture::state.LastSubmission());
                }
            }
            for (auto& slot : R().gpuSlots) if (slot.uploadMapped) slot.uploadRing->unmap();
        }
        if (nativeController) nativeController->ShutdownAfterGpuDrain();
    }
    void NativeRun(gpu::upscaling::DlssQuality quality, uint32_t outputWidth, uint32_t outputHeight,
        bool reset, bool injectedFailure = false) {
        Require(bool(nativeController), "native mode required");
        auto& r = R(); ++r.frame;
        const auto sizing = nativeController->QueryOutputSizing(*static_cast<plume::VulkanInterface*>(api.get()),
            *static_cast<plume::VulkanDevice*>(device.get()), {1, outputWidth, outputHeight});
        const auto& mode = sizing.modes[static_cast<unsigned>(quality)];
        Require(mode.state == gpu::upscaling::SizingState::Ready && mode.optimal.width && mode.optimal.height,
            "native sizing failed");
        auto plan = r.activePlan;
        if (plan.width != mode.optimal.width || plan.height != mode.optimal.height || plan.dlssQuality != quality ||
            plan.output.width != outputWidth || plan.output.height != outputHeight) ++plan.geometryEpoch;
        ++plan.cpuSerial;
        plan.width = mode.optimal.width; plan.height = mode.optimal.height; plan.dlssQuality = quality;
        plan.output = {{outputWidth, outputHeight}, 0, 0, outputWidth, outputHeight};
        { std::lock_guard lock(gpu::renderer::framePlanMutex); gpu::renderer::committedPlan = plan; }
        Require(r.ApplyInternalResolution(), "native plan reconfiguration did not drain old feature");
        readbackPitch = (outputWidth + 31) & ~31u;
        readback = device->createBuffer(plume::RenderBufferDesc::ReadbackBuffer(uint64_t(readbackPitch) * outputHeight * 8));
        Require(bool(readback), "native readback allocation");
        const RenderTargetKey key{0, 3, 1280, 0, false};
        auto base = Texture(plan.width, plan.height, plume::RenderFormat::R16G16B16A16_FLOAT);
        HostTexture* original = base.get(); r.renderTargets[key] = std::move(base);
        Clear(*original, plume::RenderColor(.25f, .5f, .75f, .5f));
        auto depth = Texture(plan.width, plan.height, plume::RenderFormat::R32_FLOAT);
        auto motion = Texture(plan.width, plan.height, plume::RenderFormat::R16G16_FLOAT);
        auto invalid = Texture(plan.width, plan.height, plume::RenderFormat::R8_UNORM);
        LocalImageDrain imageDrain{device.get()};
        Clear(*depth, plume::RenderColor(.5f, 0, 0, 0));
        Clear(*motion, plume::RenderColor(0, 0, 0, 0)); Clear(*invalid, plume::RenderColor(0, 0, 0, 0));
        gpu::temporal::TemporalFrameInputs inputs{};
        inputs.plan = plan; inputs.renderFrameId = r.frame; inputs.currentInputsComplete = true;
        inputs.resetHistory = reset;
        inputs.motionState = reset ? gpu::temporal::MotionState::ResetInitialization : gpu::temporal::MotionState::Tracked;
        inputs.depthConvention = gpu::temporal::DepthConvention::Reversed;
        inputs.colorEncoding = gpu::temporal::ColorEncoding::Sdr; // Known synthetic signal, NOT a game qualification.
        const auto region = [&](HostTexture& t) { return gpu::temporal::TextureRegion{t.texture.get(),
            {plan.width, plan.height}, 0, 0, plan.width, plan.height}; };
        inputs.color = region(*original); inputs.depth = region(*depth);
        inputs.motion = region(*motion); inputs.motionInvalidity = region(*invalid);
        Require(r.PrepareSceneCopyDestination(key, *original, inputs), "native mapping prepare");
        HostTexture* color = original; HostTexture* raster = color;
        plume::RenderViewport guest(0,0,1280,720), viewport;
        plume::RenderRect guestScissor(0,0,1280,720), scissor;
        Require(r.ActivateSceneCopyDestination(color, raster, viewport, scissor, guest, guestScissor), "native mapping activation");
        Clear(*color, plume::RenderColor(.25f, .5f, .75f, .5f));
        // NGX scratch begins as an explicit zero sentinel, without adding a
        // framebuffer or different format to the production storage allocation.
        auto& scratch = *static_cast<plume::VulkanTexture*>(r.sceneCopyPromotion.scratch->texture.get());
        r.commandList->barriers(plume::RenderBarrierStage::COPY, plume::RenderTextureBarrier(&scratch, plume::RenderTextureLayout::COPY_DEST));
        const VkClearColorValue zero{};
        vkCmdClearColorImage(static_cast<plume::VulkanCommandList*>(r.commandList)->vk, scratch.vk,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &zero, 1, &scratch.imageSubresourceRange);
        struct InjectScope {
            InjectScope(bool enable) {
#ifdef _WIN32
                _putenv_s("LO_DLSS_TEST_INJECT_EVALUATE_FAILURE", enable ? "1" : "");
#else
                if (enable) setenv("LO_DLSS_TEST_INJECT_EVALUATE_FAILURE", "1", 1);
                else unsetenv("LO_DLSS_TEST_INJECT_EVALUATE_FAILURE");
#endif
            }
            ~InjectScope() {
#ifdef _WIN32
                _putenv_s("LO_DLSS_TEST_INJECT_EVALUATE_FAILURE", "");
#else
                unsetenv("LO_DLSS_TEST_INJECT_EVALUATE_FAILURE");
#endif
            }
        } inject(injectedFailure);
        const auto creates = [&] {
            return std::count_if(nativeController->Report().calls.begin(), nativeController->Report().calls.end(),
                [](const gpu::dlss::ApiCall& call) { return call.name == "CREATE_DLSS_EXT1"; });
        };
        const auto createsBefore = creates();
        Require(r.RecordSceneCopyDlss(color, raster) == !injectedFailure, "native renderer dispatch/fallback result");
        Require(creates() == createsBefore + (reset ? 1 : 0), "native feature reuse/recreation call count");
        Require(r.Flush(), "native renderer batch submit");
        Require(fixture::lastListCount == (injectedFailure ? 2u : 3u), "native batch inclusion");
        const auto pixels = Pixels(*color);
        Require(pixels.size() == size_t(outputWidth) * outputHeight, "native output extent");
        bool anyRgb = false;
        for (const uint64_t pixel : pixels) {
            Require(uint16_t(pixel >> 48) == 0x3800, "native composite lost post-copy guest alpha");
            for (unsigned channel = 0; channel < 3; ++channel)
                Require((uint16_t(pixel >> (channel * 16)) & 0x7c00) != 0x7c00, "native RGB contains NaN/Inf");
            anyRgb |= (pixel & 0x0000ffffffffffffull) != 0;
            if (injectedFailure) Require(pixel == 0x38003a0038003400ull, "excluded NGX list corrupted fallback pixels");
        }
        Require(anyRgb, "NGX did not replace zero RGB sentinel");
        Clear(*color, plume::RenderColor(.5f, .25f, .125f, .75f));
        Require(r.PreparePromotionAccess(key, 720, true), "native incompatible-depth restoration");
        Require(r.renderTargets.at(key).get() == original && !r.sceneCopyPromotion.activeMapping, "native parked target restoration");
        All(*original, 0x3a00300034003800ull, "native restore lost later UI-like RGBA write");
        Require(r.Flush() && r.WaitForGpu(), "native inputs fence retirement");
        r.framebuffers.clear(); r.renderTargets.clear();
        std::printf("NATIVE_CASE quality=%u input=%ux%u output=%ux%u failure=%u pixels=%zu alpha_mismatches=0\n",
            unsigned(quality), plan.width, plan.height, outputWidth, outputHeight, injectedFailure, pixels.size());
    }
    void Run(gpu::dlss::SrStatus outcome, unsigned restoreReason) {
        auto& r = R(); ++r.frame;
        const RenderTargetKey key{0, 3, 1280, 0, false};
        auto base = Texture(4, 4, plume::RenderFormat::R16G16B16A16_FLOAT);
        // Padded guest height, so next-frame height alignment does not request growth.
        base->guestHeight = 736;
        HostTexture* original = base.get(); r.renderTargets[key] = std::move(base);
        Clear(*original, plume::RenderColor(.25f, .5f, .75f, .5f));
        auto depth = Texture(4, 4, plume::RenderFormat::R32_FLOAT);
        auto motion = Texture(4, 4, plume::RenderFormat::R16G16_FLOAT);
        auto invalid = Texture(4, 4, plume::RenderFormat::R8_UNORM);
        LocalImageDrain imageDrain{device.get()};
        gpu::temporal::TemporalFrameInputs inputs{};
        inputs.plan = r.activePlan; inputs.currentInputsComplete = true;
        inputs.depthConvention = (restoreReason & 1) ? gpu::temporal::DepthConvention::Reversed : gpu::temporal::DepthConvention::Forward;
        inputs.motionState = gpu::temporal::MotionState::Tracked;
        inputs.color = {original->texture.get(), {4,4}, 0,0,4,4};
        inputs.depth = {depth->texture.get(), {4,4}, 0,0,4,4};
        inputs.motion = {motion->texture.get(), {4,4}, 0,0,4,4};
        inputs.motionInvalidity = {invalid->texture.get(), {4,4}, 0,0,4,4};
        Require(!r.PrepareSceneCopyDestination(key, *original, inputs), "Unknown must not promote");
        inputs.colorEncoding = gpu::temporal::ColorEncoding::HdrLinear; // Synthetic known-linear fixture ONLY.
        const auto oldOffset = r.Gpu().uploadOffset;
        r.Gpu().uploadOffset = kUploadRingSize - 1;
        const auto oldSlot = r.gpuSlot;
        Require(!r.PrepareSceneCopyDestination(key, *original, inputs) && oldSlot == r.gpuSlot, "late promotion must never Flush");
        r.Gpu().uploadOffset = oldOffset;
        Require(r.PrepareSceneCopyDestination(key, *original, inputs), "prepare mapping");
        HostTexture* color = original; HostTexture* raster = color;
        plume::RenderViewport guest(0,0,1280,720), viewport;
        plume::RenderRect guestScissor(0,0,1280,720), scissor;
        Require(r.ActivateSceneCopyDestination(color, raster, viewport, scissor, guest, guestScissor), "activate mapping");
        Require(color != original && raster == color && r.sceneCopyPromotion.parkedLow.get() == original, "active and parked ownership");
        Require(viewport.width == 8 && viewport.height == 8 && scissor.right == 8 && scissor.bottom == 8, "promoted coordinates");
        // Equivalent guest-copy destination write after promotion, including its
        // destination alpha. Full guest DrawImpl/PM4 decoding is outside this fixture.
        Clear(*color, plume::RenderColor(.25f, .5f, .75f, .5f));
        VendorFixture vendor; vendor.outcome = outcome;
        const bool applied = r.RecordSceneCopyDlssUsing(vendor, color, raster);
        Require(applied == (outcome == gpu::dlss::SrStatus::Executable) && vendor.calls == 1, "production SR route result");
        Require(r.Flush(), "three-list Flush");
        Require(fixture::lastListCount == (applied ? 3u : 2u), "failed isolated list excluded");
        Require(r.sceneCopyPromotion.activeMapping && r.sceneCopyPromotion.active == color, "mapping survives Flush");
        constexpr uint64_t fallback = 0x38003a0038003400ull;
        constexpr uint64_t sr = 0x3800340038004000ull;
        All(*color, applied ? sr : fallback, "post-copy alpha or RGB mismatch");
        // A UI-like destination write must survive the subsequent restore.
        Clear(*color, plume::RenderColor(.5f, .25f, .125f, .75f));
        if (restoreReason == 0) ++r.frame;
        if (restoreReason == 1) ++r.activePlan.geometryEpoch;
        auto nextKey = key; if (restoreReason == 2) nextKey.pitch = 1312;
        if (restoreReason == 3) nextKey.format = 1;
        if (restoreReason == 4) nextKey.base = 5;
        Require(r.PreparePromotionAccess(nextKey, restoreReason == 6 ? 768 : 720, restoreReason == 5), "restore boundary");
        Require(!r.sceneCopyPromotion.activeMapping && r.renderTargets.at(key).get() == original, "parked map restored");
        All(*original, 0x3a00300034003800ull, "restore must preserve later RGBA writes");
        // Drain before local temporal input images are destroyed.
        Require(r.Flush() && r.WaitForGpu(), "end case drain");
        r.framebuffers.clear(); r.renderTargets.clear();
    }
    void RunFsrFallback() {
#if !defined(LO_HAS_FSR) || !LO_HAS_FSR
        throw SkipNative("FSR SDK is disabled for this fixture");
#else
        // The embedded renderer uses the legacy fixture controller signature.
        // This bridge records with the real FSR controller and only injects a
        // single rejection AFTER successful SDK recording. No SDK call is faked.
        struct FsrBridge {
            gpu::fsr::Controller actual;
            gpu::fsr::Config config{32, 32, 64, 64, gpu::upscaling::FsrQuality::Quality, 1};
            bool rejectOnce = false;
            uint64_t recorded = 0, discarded = 0;
            unsigned dispatches = 0;
            gpu::dlss::SrStatus EnsureSession(const plume::VulkanDevice& device) {
                Require(actual.EnsureSession(const_cast<plume::VulkanDevice&>(device), config) ==
                    gpu::fsr::Status::Ready, "real FSR session ready");
                return gpu::dlss::SrStatus::Executable;
            }
            gpu::dlss::SrAttempt RecordIsolated(plume::VulkanCommandList& commands,
                const gpu::dlss::SrConfig&, const gpu::temporal::TemporalFrameInputs& inputs,
                plume::VulkanTexture& output, gpu::dlss::EvaluateCapture*) {
                gpu::fsr::FrameMetadata metadata{true, FLT_MAX, 10.0f, .7f, 1.0f,
                    16.6f, 1.0f / .999f, -.001f / .999f};
                const auto result = actual.RecordIsolated(commands, config, inputs, metadata, output);
                Require(result.status == gpu::fsr::Status::Ready && result.useId,
                    "real FSR SDK record must succeed before test rejection");
                ++dispatches; recorded = result.useId;
                gpu::dlss::SrAttempt translated{rejectOnce ? gpu::dlss::SrStatus::Failed : gpu::dlss::SrStatus::Executable};
                translated.useId = result.useId;
                rejectOnce = false;
                return translated;
            }
            void OnBatchDiscarded(uint64_t use) {
                discarded = use;
                actual.OnBatchDiscarded(use);
            }
        } bridge;
        auto& r = R();
        r.activePlan.requestedUpscaler = gpu::upscaling::Upscaler::Fsr;
        r.activePlan.consumer = gpu::upscaling::TemporalConsumer::FsrSr;
        r.activePlan.width = r.activePlan.height = 32;
        r.activePlan.output = {{64,64}, 0,0,64,64};
        r.activePlan.fsrQuality = gpu::upscaling::FsrQuality::Quality;
        r.internalSize = {32,32};
        readbackPitch = 64;
        readback = device->createBuffer(plume::RenderBufferDesc::ReadbackBuffer(64 * 64 * 8));
        Require(bool(readback), "FSR final readback allocation");
        const RenderTargetKey key{0, 3, 1280, 0, false};
        for (unsigned step = 0; step < 3; ++step) {
            ++r.frame;
            auto base = Texture(32,32,plume::RenderFormat::R16G16B16A16_FLOAT);
            HostTexture* original = base.get(); r.renderTargets[key] = std::move(base);
            auto source = Texture(32,32,plume::RenderFormat::R8G8B8A8_UNORM);
            auto depth = Texture(32,32,plume::RenderFormat::R32_FLOAT);
            auto motion = Texture(32,32,plume::RenderFormat::R16G16_FLOAT);
            auto invalid = Texture(32,32,plume::RenderFormat::R8_UNORM);
            LocalImageDrain drain{device.get()};
            // Current fallback is always green; real SDK frame 1 is red and
            // recovery is blue. Thus success cannot be the unchanged fallback.
            Clear(*original, plume::RenderColor(0,1,0,.5f));
            Clear(*source, step == 0 ? plume::RenderColor(1,0,0,1) : plume::RenderColor(0,0,1,1));
            Clear(*depth, plume::RenderColor(.5f,0,0,0));
            Clear(*motion, plume::RenderColor(0,0,0,0));
            Clear(*invalid, plume::RenderColor(0,0,0,0));
            gpu::temporal::TemporalFrameInputs inputs{};
            inputs.plan = r.activePlan; inputs.currentInputsComplete = true;
            inputs.renderFrameId = r.frame;
            inputs.resetHistory = step == 0;
            inputs.colorEncoding = gpu::temporal::ColorEncoding::Sdr;
            inputs.depthConvention = gpu::temporal::DepthConvention::Reversed;
            inputs.motionState = gpu::temporal::MotionState::Tracked;
            inputs.color = {source->texture.get(), {32,32},0,0,32,32};
            inputs.depth = {depth->texture.get(), {32,32},0,0,32,32};
            inputs.motion = {motion->texture.get(), {32,32},0,0,32,32};
            inputs.motionInvalidity = {invalid->texture.get(), {32,32},0,0,32,32};
            Require(inputs.CompleteForConsumer(), "fixture supplies complete FSR motion/depth inputs");
            Require(r.PrepareSceneCopyDestination(key, *original, inputs), "real renderer FSR promotion prepare");
            Require(r.sceneCopyPromotion.scratch->format == plume::RenderFormat::R8G8B8A8_UNORM,
                "real FSR provider selects R8 SDK scratch");
            HostTexture* color = original; HostTexture* raster = color;
            plume::RenderViewport viewport, guest(0,0,1280,720);
            plume::RenderRect scissor, guestScissor(0,0,1280,720);
            Require(r.ActivateSceneCopyDestination(color,raster,viewport,scissor,guest,guestScissor), "FSR activate");
            Clear(*color, plume::RenderColor(0,1,0,.5f));
            bridge.rejectOnce = step == 1;
            const bool accepted = r.RecordSceneCopyDlssUsing(bridge,color,raster);
            Require(accepted == (step != 1), "single post-record rejection boundary");
            if (step == 1) Require(bridge.discarded == bridge.recorded && !r.Gpu().srUseId,
                "renderer itself discards and clears rejected real FSR token");
            Require(r.Flush(), "FSR renderer flush");
            Require(fixture::lastListCount == (step == 1 ? 2u : 3u), "rejected SDK list must not be submitted");
            if (accepted) bridge.actual.OnBatchSubmitted(bridge.recorded,fixture::state.LastSubmission());
            const auto pixels = Pixels(*color);
            for (uint64_t pixel : pixels) {
                Require((pixel >> 48) == 0x3800, "guest alpha must survive FSR and fallback");
                if (step == 1) Require(pixel == 0x380000003c000000ull,
                    "rejected frame must contain current green, not previous FSR red or blank");
                else {
                    const uint16_t red = uint16_t(pixel), green = uint16_t(pixel >> 16), blue = uint16_t(pixel >> 32);
                    Require(step == 0 ? (red > 0x3800 && green < 0x3000 && blue < 0x3000) :
                        (blue > 0x3800 && red < 0x3000 && green < 0x3000), "actual SDK final color red then blue");
                }
            }
            bridge.actual.ReleaseCompletedThrough(fixture::state.LastSubmission());
            if (step == 2) Require(!inputs.resetHistory && bridge.actual.LastDiagnostics().lastDispatchReset,
                "recovery SDK resets history despite false input reset");
            std::printf("FSR_FALLBACK frame=%llu accepted=%u discarded=%llu sdk_reset=%u pixels=%zu\n",
                (unsigned long long)r.frame,accepted,(unsigned long long)bridge.discarded,
                bridge.actual.LastDiagnostics().lastDispatchReset,pixels.size());
            Require(r.WaitForGpu(), "FSR frame drain");
            r.framebuffers.clear(); r.renderTargets.clear(); r.sceneCopyPromotion = {};
            if (step == 1) {
                Require(bridge.actual.EnsureSession(*static_cast<plume::VulkanDevice*>(device.get()),bridge.config) ==
                    gpu::fsr::Status::NeedsReconfigure,"discarded SDK recording poisons history");
                bridge.actual.ReleaseFeatureAfterGpuDrain();
            }
        }
        Require(bridge.dispatches == 3 && bridge.discarded, "three actual SDK records and one renderer discard");
        bridge.actual.ShutdownAfterGpuDrain();
        std::puts("PASS: real FSR + renderer current-frame fallback after injected post-record rejection; recovery SDK reset; not SDK-internal-failure or production-facade coverage");
#endif
    }
    void RunFsrScratchFormat() {
        auto& r = R();
        auto guest = Texture(4, 4, plume::RenderFormat::R16G16B16A16_FLOAT);
        uint64_t lastAllocation = 0;
        // Real renderer allocation path, including switching back to FSR.
        for (const auto provider : {gpu::upscaling::Upscaler::Fsr, gpu::upscaling::Upscaler::Dlss,
                                   gpu::upscaling::Upscaler::Fsr}) {
            r.activePlan.requestedUpscaler = provider;
            auto scratch = r.CreateSceneCopyScratch(*guest, {8, 8});
            auto composite = r.CreatePromotedTarget(*guest, {8, 8});
            Require(scratch && composite, "provider output and guest composite allocate");
            const auto expected = provider == gpu::upscaling::Upscaler::Fsr ?
                VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R16G16B16A16_SFLOAT;
            Require(static_cast<plume::VulkanTexture*>(scratch->texture.get())->imageFormat == expected,
                "actual scratch VkImage format follows the selected provider contract");
            Require(scratch->width == 8 && scratch->height == 8 && scratch->allocationSerial > lastAllocation,
                "provider switch uses a new unpadded scratch allocation");
            Require(composite->format == plume::RenderFormat::R16G16B16A16_FLOAT &&
                guest->format == plume::RenderFormat::R16G16B16A16_FLOAT,
                "encoded SR scratch does not change the guest composite or destination format");
            lastAllocation = scratch->allocationSerial;
        }
        std::puts("PASS: real renderer FSR/DLSS/FSR scratch Vulkan formats and preserved FP16 guest destination");
    }
    void RunStatus() {
        gpu::frame_plan::PlannerState planner;
        fixture::statusPlanner = &planner;
        fixture::executions.clear();
        fixture::rejectSubmit = false;
        auto& r = R();
        r.frame = 1;
        r.dlssSrRequested = true;
        gpu::upscaling::BackendDeviceSnapshot capability{gpu::backend::Backend::Vulkan, 1, true, true};
        gpu::upscaling::OutputSizing sizing;
        sizing.key = {1, 8, 8};
        sizing.revision = 1;
        for (auto& mode : sizing.modes) {
            mode.state = gpu::upscaling::SizingState::Ready;
            mode.optimal = mode.minimum = mode.maximum = {4, 4};
        }
        gpu::frame_plan::PlannerInput input;
        input.internalResolution = 0;
        input.antialiasing = 0;
        input.scalingQuality = 1;
        input.upscaler = gpu::upscaling::Upscaler::Dlss;
        input.quality = gpu::upscaling::DlssQuality::Quality;
        input.output = {{8, 8}, 0, 0, 8, 8};
        input.device = capability;
        input.sizing = &sizing;
        const auto effectOf = [&] {
            return gpu::frame_plan::DescribeDlssRuntime(capability, planner.Observe(), &sizing);
        };
        const auto install = [&](gpu::upscaling::DlssQuality quality) {
            input.quality = quality;
            const auto plan = planner.Begin(input);
            Require(plan.consumer == gpu::upscaling::TemporalConsumer::DlssSr && plan.cpuSerial &&
                plan.width == 4 && plan.height == 4 && plan.output.width == 8 && plan.output.height == 8,
                "status planner did not produce the 4x4 to 8x8 DLSS SR plan");
            r.activePlan = plan;
            r.internalSize = {plan.width, plan.height};
            r.dlssDisableReportedEpoch = ~0ull;
            return plan;
        };
        const auto region = [](HostTexture& texture) {
            return gpu::temporal::TextureRegion{texture.texture.get(), {4, 4}, 0, 0, 4, 4};
        };
        const RenderTargetKey key{0, 3, 1280, 0, false};
        struct Scene {
            std::unique_ptr<HostTexture> depth, motion, invalid, held;
            HostTexture* color = nullptr;
        };
        const auto openScene = [&](Scene& scene) {
            auto base = Texture(4, 4, plume::RenderFormat::R16G16B16A16_FLOAT);
            base->guestHeight = 736;
            HostTexture* original = base.get();
            r.renderTargets[key] = std::move(base);
            scene.depth = Texture(4, 4, plume::RenderFormat::R32_FLOAT);
            scene.motion = Texture(4, 4, plume::RenderFormat::R16G16_FLOAT);
            scene.invalid = Texture(4, 4, plume::RenderFormat::R8_UNORM);
            gpu::temporal::TemporalFrameInputs inputs{};
            inputs.plan = r.activePlan;
            inputs.currentInputsComplete = true;
            inputs.motionState = gpu::temporal::MotionState::Tracked;
            inputs.depthConvention = gpu::temporal::DepthConvention::Reversed;
            inputs.colorEncoding = gpu::temporal::ColorEncoding::HdrLinear;
            inputs.color = region(*original);
            inputs.depth = region(*scene.depth);
            inputs.motion = region(*scene.motion);
            inputs.motionInvalidity = region(*scene.invalid);
            Require(r.Begin(), "status begin");
            Require(r.PrepareSceneCopyDestination(key, *original, inputs), "status prepare");
            HostTexture* color = original;
            HostTexture* raster = color;
            plume::RenderViewport guest(0, 0, 1280, 720), viewport;
            plume::RenderRect guestScissor(0, 0, 1280, 720), scissor;
            Require(r.ActivateSceneCopyDestination(color, raster, viewport, scissor, guest, guestScissor), "status activate");
            scene.color = color;
            scene.held = nullptr;
        };
        const auto retire = [&] {
            if (fixture::state.Stopped()) return;
            if (r.listOpen) Require(r.Flush(), "status retire flush");
            Require(r.WaitForGpu(), "status retire wait");
            r.framebuffers.clear();
            r.renderTargets.clear();
            r.sceneCopyPromotion = {};
        };
        const auto plan = install(gpu::upscaling::DlssQuality::Quality);
        Require(r.Flush() && fixture::executions.empty(), "closed list is not a DLSS submission");
        Scene scene;
        LocalImageDrain imageDrain{device.get()};
        openScene(scene);
        VendorFixture vendor;
        HostTexture* color = scene.color;
        HostTexture* raster = color;
        Require(r.RecordSceneCopyDlssUsing(vendor, color, raster) && r.sceneCopyPromotion.srApplied, "status record");
        r.PublishDlssFrameOutcome();
        Require(fixture::executions.empty() && effectOf().phase == gpu::frame_plan::DlssEffectPhase::AwaitingExecution,
            "recorded SR is not submitted until the checked flush");
        Require(r.Flush() && fixture::lastListCount == 3, "checked flush omitted isolated or continuation");
        r.PublishDlssFrameOutcome();
        Require(fixture::executions.size() == 1 && fixture::executions[0].outcome == gpu::frame_plan::DlssExecutionOutcome::Submitted &&
            fixture::executions[0].serial != 0 && fixture::executions[0].frame == r.frame, "frame-end submit");
        auto effect = effectOf();
        Require(effect.phase == gpu::frame_plan::DlssEffectPhase::Active && effect.execution &&
            effect.execution->outcome == gpu::frame_plan::DlssExecutionOutcome::Submitted &&
            effect.execution->plan.width == 4 && effect.execution->plan.output.width == 8, "submitted plan is the active effect");
        const auto submittedReports = fixture::executions.size();
        Require(r.Flush(), "same-frame ordinary flush");
        r.PublishDlssFrameOutcome();
        Require(fixture::executions.size() == submittedReports && effectOf().phase == gpu::frame_plan::DlssEffectPhase::Active,
            "same-frame ordinary flush downgraded a submission");
        retire();
        ++r.frame;
        r.PublishDlssFrameOutcome();
        effect = effectOf();
        Require(effect.phase != gpu::frame_plan::DlssEffectPhase::Active && effect.reason == gpu::frame_plan::DlssEffectReason::NoEligibleScene,
            "a frame with no eligible scene must replace the previous active submission");

        ++r.frame;
        openScene(scene);
        vendor.outcome = gpu::dlss::SrStatus::NeedsReconfigure;
        color = scene.color;
        raster = color;
        const auto beforeReconfigure = fixture::executions.size();
        Require(!r.RecordSceneCopyDlssUsing(vendor, color, raster) && !r.sceneCopyPromotion.srApplied, "reconfigure is not applied");
        r.PublishDlssFrameOutcome();
        Require(r.Flush() && fixture::lastListCount == 2, "reconfigure included the isolated list");
        Require(fixture::executions.size() == beforeReconfigure + 1 &&
            fixture::executions.back().outcome != gpu::frame_plan::DlssExecutionOutcome::Submitted &&
            fixture::executions.back().reason == gpu::frame_plan::DlssEffectReason::FeatureReconfigurePending &&
            !planner.Observe().persistentFailure, "reconfigure is a frame fallback, not a latched failure");
        retire();

        ++r.frame;
        r.motionReplay = std::make_unique<gpu::temporal::MotionReplayGPU>();
        r.motionReplay->BeginFrame(r.frame, 1);
        r.motionReplay->InjectNextPreparePending();
        gpu::pipeline_cache::Key pipelineKey{};
        plume::RenderGraphicsPipelineDesc pipelineDesc{};
        r.motionReplay->PreparePipeline(pipelineKey, pipelineDesc, nullptr, 0, nullptr, 0, false);
        gpu::temporal::TemporalFrameInputs selectedInputs{};
        selectedInputs.plan = r.activePlan;
        selectedInputs.currentInputsComplete = true;
        selectedInputs.motionState = gpu::temporal::MotionState::Tracked;
        selectedInputs.depthConvention = gpu::temporal::DepthConvention::Reversed;
        selectedInputs.colorEncoding = gpu::temporal::ColorEncoding::Sdr;
        auto dummyRegion = gpu::temporal::TextureRegion{r.dummyTexture2D.texture.get(), {1, 1}, 0, 0, 1, 1};
        selectedInputs.color = selectedInputs.depth = selectedInputs.motion = selectedInputs.motionInvalidity = dummyRegion;
        auto choice = r.SelectDlssSceneCopyInputs(selectedInputs);
        Require(choice.kind == Renderer::DlssSceneInputSelection::Kind::Pending, "pending motion branch");
        r.PublishDlssFrameOutcome();
        Require(effectOf().reason == gpu::frame_plan::DlssEffectReason::MotionPipelinePending &&
            effectOf().phase != gpu::frame_plan::DlssEffectPhase::Active, "pending motion is not active");
        r.motionReplay.reset();
        ++r.frame;
        selectedInputs.colorEncoding = gpu::temporal::ColorEncoding::Unknown;
        choice = r.SelectDlssSceneCopyInputs(selectedInputs);
        Require(choice.kind == Renderer::DlssSceneInputSelection::Kind::UnknownColor, "unknown color branch");
        r.PublishDlssFrameOutcome();
        Require(effectOf().reason == gpu::frame_plan::DlssEffectReason::UnknownColorEncoding &&
            effectOf().phase != gpu::frame_plan::DlssEffectPhase::Active, "unknown color is not active");

        ++r.frame;
        r.Gpu().srPrefixClosed = true;
        Require(!r.PrepareSceneCopyDestination(key, *Texture(4, 4, plume::RenderFormat::R16G16B16A16_FLOAT), selectedInputs),
            "closed prefix must not prepare");
        r.Gpu().srPrefixClosed = false;
        selectedInputs.colorEncoding = gpu::temporal::ColorEncoding::HdrLinear;
        r.PublishDlssFrameOutcome();
        Require(effectOf().reason == gpu::frame_plan::DlssEffectReason::PromotionUnavailable, "prepare failure");
        auto prepared = Texture(4, 4, plume::RenderFormat::R16G16B16A16_FLOAT);
        prepared->guestHeight = 736;
        HostTexture* preparedColor = prepared.get();
        r.renderTargets[key] = std::move(prepared);
        gpu::temporal::TemporalFrameInputs promoteInputs = selectedInputs;
        promoteInputs.color = region(*preparedColor);
        promoteInputs.depth = dummyRegion;
        // Activate's identity check needs the prepared source to remain alive.
        Require(r.Begin() && r.PrepareSceneCopyDestination(key, *preparedColor, promoteInputs), "activate setup");
        scene.held = std::move(r.renderTargets[key]);
        r.renderTargets[key] = Texture(4, 4, plume::RenderFormat::R16G16B16A16_FLOAT);
        HostTexture* mismatched = scene.held.get();
        HostTexture* mismatchedRaster = mismatched;
        plume::RenderViewport guest(0, 0, 1280, 720), viewport;
        plume::RenderRect guestScissor(0, 0, 1280, 720), scissor;
        Require(!r.ActivateSceneCopyDestination(mismatched, mismatchedRaster, viewport, scissor, guest, guestScissor),
            "mismatched target must not activate");
        r.PublishDlssFrameOutcome();
        Require(effectOf().reason == gpu::frame_plan::DlssEffectReason::PromotionUnavailable, "activate failure");
        retire();

        ++r.frame;
        vendor.outcome = gpu::dlss::SrStatus::Failed;
        vendor.failedUseId = 73;
        openScene(scene);
        color = scene.color;
        raster = color;
        const auto beforeVendor = fixture::executions.size();
        Require(!r.RecordSceneCopyDlssUsing(vendor, color, raster), "vendor failure is not applied");
        Require(vendor.discardedUseId == 73 && !r.Gpu().srUseId && !r.Gpu().srIsolatedAccepted,
            "failed SDK list returns its allocated use to the owner before the fallback batch can submit");
        r.PublishDlssFrameOutcome();
        Require(r.Flush(), "vendor-failure batch");
        Require(std::none_of(fixture::executions.begin() + beforeVendor, fixture::executions.end(), [](const fixture::ExecutionReport& report) {
            return report.outcome == gpu::frame_plan::DlssExecutionOutcome::Submitted;
        }), "vendor failure reported a submission");
        effect = effectOf();
        Require(effect.failure && *effect.failure == gpu::frame_plan::FailureReason::DlssUnavailable &&
            effect.phase != gpu::frame_plan::DlssEffectPhase::Active && effect.reason == gpu::frame_plan::DlssEffectReason::RequestFailure,
            "vendor failure latches the request");
        input.quality = gpu::upscaling::DlssQuality::Quality;
        const auto fallen = planner.Begin(input);
        Require(fallen.consumer != gpu::upscaling::TemporalConsumer::DlssSr, "latched request must leave SR");
        gpu::frame_plan::DlssExecutionObservation late = {};
        late.plan = plan;
        late.renderFrame = r.frame + 1;
        late.submissionSerial = 99;
        late.outcome = gpu::frame_plan::DlssExecutionOutcome::Submitted;
        Require(!planner.ReportExecution(late), "late success must not replace the latched request");
        effect = effectOf();
        Require(effect.failure && *effect.failure == gpu::frame_plan::FailureReason::DlssUnavailable &&
            effect.phase != gpu::frame_plan::DlssEffectPhase::Active, "failure remains after a late success");
        retire();

        install(gpu::upscaling::DlssQuality::Balanced);
        ++r.frame;
        openScene(scene);
        r.sceneCopyPromotionRgbPs.reset();
        r.sceneCopyPromotionRgbPipelines.clear();
        vendor.outcome = gpu::dlss::SrStatus::Executable;
        color = scene.color;
        raster = color;
        const auto beforeComposite = fixture::executions.size();
        Require(!r.RecordSceneCopyDlssUsing(vendor, color, raster) && !r.sceneCopyPromotion.srApplied, "composite failure is not applied");
        Require(r.Flush() && fixture::lastListCount == 3, "unadopted composite still closed its lists");
        r.PublishDlssFrameOutcome();
        Require(std::none_of(fixture::executions.begin() + beforeComposite, fixture::executions.end(), [](const fixture::ExecutionReport& report) {
            return report.outcome == gpu::frame_plan::DlssExecutionOutcome::Submitted;
        }) && effectOf().phase != gpu::frame_plan::DlssEffectPhase::Active, "unadopted composite is not a submission");
        retire();
        r.CompileSceneCopyPromotionShaders();
        Require(bool(r.sceneCopyPromotionRgbPs), "restore promotion shader");

        install(gpu::upscaling::DlssQuality::Performance);
        ++r.frame;
        openScene(scene);
        vendor.outcome = gpu::dlss::SrStatus::Executable;
        color = scene.color;
        raster = color;
        Require(r.RecordSceneCopyDlssUsing(vendor, color, raster), "submit-failure record");
        const auto beforeRejected = fixture::executions.size();
        fixture::rejectSubmit = true;
        Require(!r.Flush() && fixture::lastListCount == 0 && fixture::state.Stopped(), "rejected submit must not reach the queue");
        r.PublishDlssFrameOutcome();
        Require(fixture::executions.size() == beforeRejected, "rejected submit reported success");
        capability.gpuWorkStopped = true;
        effect = effectOf();
        Require(effect.phase == gpu::frame_plan::DlssEffectPhase::GpuStopped && !effect.execution &&
            effect.phase != gpu::frame_plan::DlssEffectPhase::Active, "stopped device hides the old submission");
        fixture::statusPlanner = nullptr;
    }
    static bool SameIdentity(const gpu::frame_plan::FramePlan& a, const gpu::frame_plan::FramePlan& b)
    {
        return a.deviceEpoch == b.deviceEpoch && a.requestSignature == b.requestSignature &&
            a.geometryEpoch == b.geometryEpoch;
    }
    void RunPlanIdentity()
    {
        gpu::frame_plan::PlannerState planner;
        fixture::statusPlanner = &planner;
        fixture::executions.clear();
        auto& r = R();
        r.frame = 8;
        r.dlssSrRequested = true;
        gpu::upscaling::BackendDeviceSnapshot capability{gpu::backend::Backend::Vulkan, 1, true, true};
        gpu::upscaling::OutputSizing sizing;
        sizing.key = {1, 8, 8};
        for (auto& mode : sizing.modes) {
            mode.state = gpu::upscaling::SizingState::Ready;
            mode.optimal = mode.minimum = mode.maximum = {4, 4};
        }
        gpu::frame_plan::PlannerInput input;
        input.upscaler = gpu::upscaling::Upscaler::Dlss;
        input.output = {{8, 8}, 0, 0, 8, 8};
        input.device = capability;
        input.sizing = &sizing;
        const auto install = [&](gpu::upscaling::DlssQuality quality) {
            input.quality = quality;
            const auto plan = planner.Begin(input);
            Require(plan.consumer == gpu::upscaling::TemporalConsumer::DlssSr, "identity planner lost DLSS SR");
            r.activePlan = plan;
            r.internalSize = {plan.width, plan.height};
            return plan;
        };
        const auto effectOf = [&] {
            return gpu::frame_plan::DescribeDlssRuntime(capability, planner.Observe(), &sizing);
        };
        const auto planA = install(gpu::upscaling::DlssQuality::Quality);
        r.NoteDlssFrameFallback(gpu::frame_plan::DlssEffectReason::MotionPipelinePending);
        const auto planB = install(gpu::upscaling::DlssQuality::Balanced);
        Require(!SameIdentity(planA, planB), "quality change did not create a new plan identity");
        r.NoteDlssFrameFallback(gpu::frame_plan::DlssEffectReason::UnknownColorEncoding);
        r.PublishDlssFrameOutcome();
        Require(fixture::executions.size() == 1 &&
            fixture::executions.back().reason == gpu::frame_plan::DlssEffectReason::UnknownColorEncoding &&
            SameIdentity(fixture::executions.back().plan, planB) &&
            effectOf().phase != gpu::frame_plan::DlssEffectPhase::Active,
            "same-frame fallback B must replace fallback A");

        ++r.frame;
        const auto submittedA = install(gpu::upscaling::DlssQuality::Quality);
        const RenderTargetKey key{0, 3, 1280, 0, false};
        auto base = Texture(4, 4, plume::RenderFormat::R16G16B16A16_FLOAT);
        base->guestHeight = 736;
        HostTexture* original = base.get();
        r.renderTargets[key] = std::move(base);
        auto depth = Texture(4, 4, plume::RenderFormat::R32_FLOAT);
        auto motion = Texture(4, 4, plume::RenderFormat::R16G16_FLOAT);
        auto invalid = Texture(4, 4, plume::RenderFormat::R8_UNORM);
        LocalImageDrain imageDrain{device.get()};
        const auto region = [](HostTexture& texture) {
            return gpu::temporal::TextureRegion{texture.texture.get(), {4, 4}, 0, 0, 4, 4};
        };
        gpu::temporal::TemporalFrameInputs inputs{};
        inputs.plan = r.activePlan;
        inputs.currentInputsComplete = true;
        inputs.motionState = gpu::temporal::MotionState::Tracked;
        inputs.depthConvention = gpu::temporal::DepthConvention::Reversed;
        inputs.colorEncoding = gpu::temporal::ColorEncoding::HdrLinear;
        inputs.color = region(*original);
        inputs.depth = region(*depth);
        inputs.motion = region(*motion);
        inputs.motionInvalidity = region(*invalid);
        Require(r.Begin(), "identity begin");
        Require(r.PrepareSceneCopyDestination(key, *original, inputs), "identity prepare");
        HostTexture* color = original;
        HostTexture* raster = color;
        plume::RenderViewport guest(0, 0, 1280, 720), viewport;
        plume::RenderRect guestScissor(0, 0, 1280, 720), scissor;
        Require(r.ActivateSceneCopyDestination(color, raster, viewport, scissor, guest, guestScissor), "identity activate");
        VendorFixture vendor;
        Require(r.RecordSceneCopyDlssUsing(vendor, color, raster) && r.Gpu().dlssSubmit.pending, "identity arm");
        Require(r.Flush() && fixture::lastListCount == 3, "submitted plan A");
        const auto submittedB = install(gpu::upscaling::DlssQuality::Balanced);
        r.NoteDlssFrameFallback(gpu::frame_plan::DlssEffectReason::UnknownColorEncoding);
        r.PublishDlssFrameOutcome();
        Require(fixture::executions.back().outcome == gpu::frame_plan::DlssExecutionOutcome::Fallback &&
            fixture::executions.back().reason == gpu::frame_plan::DlssEffectReason::UnknownColorEncoding &&
            SameIdentity(fixture::executions.back().plan, submittedB) &&
            !SameIdentity(fixture::executions.back().plan, submittedA) &&
            effectOf().phase != gpu::frame_plan::DlssEffectPhase::Active,
            "same-frame fallback B must replace submitted A");
        Require(r.WaitForGpu(), "drain submitted A");
        r.framebuffers.clear();
        r.renderTargets.clear();
        r.sceneCopyPromotion = {};

        ++r.frame;
        const auto armedPlan = install(gpu::upscaling::DlssQuality::Performance);
        auto armedBase = Texture(4, 4, plume::RenderFormat::R16G16B16A16_FLOAT);
        armedBase->guestHeight = 736;
        HostTexture* armedOriginal = armedBase.get();
        r.renderTargets[key] = std::move(armedBase);
        auto armedDepth = Texture(4, 4, plume::RenderFormat::R32_FLOAT);
        auto armedMotion = Texture(4, 4, plume::RenderFormat::R16G16_FLOAT);
        auto armedInvalid = Texture(4, 4, plume::RenderFormat::R8_UNORM);
        inputs.plan = r.activePlan;
        inputs.color = region(*armedOriginal);
        inputs.depth = region(*armedDepth);
        inputs.motion = region(*armedMotion);
        inputs.motionInvalidity = region(*armedInvalid);
        Require(r.Begin() && r.PrepareSceneCopyDestination(key, *armedOriginal, inputs), "armed prepare");
        color = armedOriginal;
        raster = color;
        Require(r.ActivateSceneCopyDestination(color, raster, viewport, scissor, guest, guestScissor), "armed activate");
        Require(r.RecordSceneCopyDlssUsing(vendor, color, raster) && r.Gpu().dlssSubmit.pending &&
            SameIdentity(r.Gpu().dlssSubmit.plan, armedPlan), "slot captured the armed plan");
        auto switched = r.activePlan;
        switched.geometryEpoch += 1;
        switched.requestSignature ^= 0x5a5a5a5a5a5a5a5aull;
        r.activePlan = switched;
        Require(r.Flush() && fixture::lastListCount == 3, "flush after activePlan switch");
        r.activePlan = armedPlan;
        r.PublishDlssFrameOutcome();
        Require(fixture::executions.back().outcome == gpu::frame_plan::DlssExecutionOutcome::Submitted &&
            SameIdentity(fixture::executions.back().plan, armedPlan) &&
            !SameIdentity(fixture::executions.back().plan, switched),
            "submit must keep the armed plan rather than activePlan");
        const auto afterArmed = fixture::executions.size();
        Require(r.Begin(), "ordinary batch begin");
        r.commandList->setFramebuffer(r.GetFramebuffer(color, nullptr));
        r.commandList->clearColor(0, plume::RenderColor(.25f, .5f, .75f, .5f));
        Require(r.Flush() && fixture::lastListCount == 1, "non-empty ordinary batch was not submitted");
        r.PublishDlssFrameOutcome();
        Require(fixture::executions.size() == afterArmed && effectOf().phase == gpu::frame_plan::DlssEffectPhase::Active &&
            SameIdentity(planner.Observe().execution->plan, armedPlan),
            "a later ordinary batch downgraded the submitted plan");
        Require(r.WaitForGpu(), "identity drain");
        r.framebuffers.clear();
        r.renderTargets.clear();
        r.sceneCopyPromotion = {};
        fixture::statusPlanner = nullptr;
    }
    void RunExtentGrowth() {
        auto& r = R(); ++r.frame;
        const uint32_t base = 0;
        const uint32_t pitch = 1280;
        const uint32_t oldHeight = 736;
        const uint32_t newHeight = 768;
        const RenderTargetKey key{base, Renderer::ColorClassOf(0), pitch, 0, false};

        // 1. Configure frame plan and internal resolution for scene target
        // plan: input 160x90, output 320x180 (2x scaling).
        r.internalSize = {160, 90};
        r.activePlan.width = 160;
        r.activePlan.height = 90;
        r.activePlan.legacyWidth = 160;
        r.activePlan.legacyHeight = 90;
        r.activePlan.cpuSerial = r.activePlan.geometryEpoch = r.activePlan.deviceEpoch = r.frame;
        r.activePlan.consumer = gpu::upscaling::TemporalConsumer::DlssSr;
        r.activePlan.output = {{320, 180}, 0, 0, 320, 180};

        // Mark surface role as Scene in catalog
        {
            std::lock_guard lock(gpu::renderer::catalogMutex);
            gpu::renderer::catalogRoles[(uint64_t(base) << 32) | pitch] = gpu::frame_plan::SurfaceRole::Scene;
        }

        // 2. Allocate initial target via production GetRenderTarget
        // Scale: 160/1280 = 1/8. Physical dims: guestWidth*1/8 = 160, guestHeight*1/8: 736/8 = 92.
        HostTexture* initial = r.GetRenderTarget(base, 0, pitch, oldHeight, false);
        Require(initial != nullptr, "initial GetRenderTarget failed");
        Require(initial->width == 160 && initial->height == 92, "initial physical dimensions 160x92");
        Require(initial->guestWidth == 1280 && initial->guestHeight == 736, "initial guest dimensions 1280x736");
        const uint64_t initialAllocId = initial->allocationSerial;
        const uint32_t initialWidth = initial->width;
        const uint32_t initialHeight = initial->height;

        // 3. Upload non-uniform RGBA16F pattern into initial target
        std::vector<uint64_t> pattern(size_t(initialWidth) * initialHeight);
        for (uint32_t y = 0; y < initialHeight; ++y) {
            for (uint32_t x = 0; x < initialWidth; ++x) {
                const uint16_t rCh = uint16_t(0x3000 + ((x * 17) & 0x3ff));
                const uint16_t gCh = uint16_t(0x3400 + ((y * 23) & 0x3ff));
                const uint16_t bCh = uint16_t(0x3800 + (((x * 7) + (y * 11)) & 0x3ff));
                const uint16_t aCh = uint16_t(0x3c00 + ((x ^ y) & 0x3ff));
                pattern[y * initialWidth + x] =
                    (uint64_t(aCh) << 48) | (uint64_t(bCh) << 32) |
                    (uint64_t(gCh) << 16) | uint64_t(rCh);
            }
        }
        UploadPattern(*initial, pattern);

        // 4. Synthetic temporal inputs and promote scene copy destination
        auto depth = Texture(160, 92, plume::RenderFormat::R32_FLOAT);
        auto motion = Texture(160, 92, plume::RenderFormat::R16G16_FLOAT);
        auto invalid = Texture(160, 92, plume::RenderFormat::R8_UNORM);
        LocalImageDrain imageDrain{device.get()};

        gpu::temporal::TemporalFrameInputs inputs{};
        inputs.plan = r.activePlan; inputs.currentInputsComplete = true;
        inputs.depthConvention = gpu::temporal::DepthConvention::Forward;
        inputs.motionState = gpu::temporal::MotionState::Tracked;
        inputs.colorEncoding = gpu::temporal::ColorEncoding::HdrLinear;
        inputs.color = {initial->texture.get(), {160, 90}, 0, 0, 160, 90};
        inputs.depth = {depth->texture.get(), {160, 90}, 0, 0, 160, 90};
        inputs.motion = {motion->texture.get(), {160, 90}, 0, 0, 160, 90};
        inputs.motionInvalidity = {invalid->texture.get(), {160, 90}, 0, 0, 160, 90};

        Require(r.PrepareSceneCopyDestination(key, *initial, inputs), "prepare scene copy promotion");
        HostTexture* color = initial;
        HostTexture* raster = color;
        plume::RenderViewport guestVp(0, 0, 1280, 720), rasterVp;
        plume::RenderRect guestScissor{0, 0, 1280, 720}, physScissor;
        Require(r.ActivateSceneCopyDestination(color, raster, rasterVp, physScissor, guestVp, guestScissor), "activate promotion");
        Require(r.sceneCopyPromotion.activeMapping && color != initial, "promotion mapping active");
        Require(color->width == 320 && color->height == 184, "promoted target extent 320x184");

        // 5. Clear a sub-rectangle in the promoted target: [32..64) x [24..48)
        // With 2x scale (nearest/Load integer resample with p.x=2, p.y=2 during restore),
        // each destination pixel at (pos.x, pos.y) samples base at (pos.x * 2, pos.y * 2).
        // During restore: destination is parkedLow (160x92), source is promoted active (320x184).
        // restore constants: transfer[0]=320/160=2.0, transfer[1]=184/92=2.0.
        // pos in parkedLow in [16..32) x [12..24) will sample active in [32..64) x [24..48)!
        constexpr uint64_t promotedClearPixel = 0x3c00000000000000ull; // black RGB, alpha 1.0 (0x3c00)
        {
            r.commandList->setFramebuffer(r.GetFramebuffer(color, nullptr));
            plume::RenderRect promClearRect{32, 24, 64, 48};
            r.commandList->clearColor(0, plume::RenderColor(0.0f, 0.0f, 0.0f, 1.0f), &promClearRect, 1);
        }
        Require(r.Flush(), "flush promoted clear");

        // 6. Call GetRenderTarget with larger guest height 768 on the same base/pitch/format
        // This triggers PreparePromotionAccess -> restores RGBA to parkedLow -> detects height 768 > 736
        // -> allocates new target (160x96) -> copies overlapping 160x92 from old parkedLow into new target!
        HostTexture* grown = r.GetRenderTarget(base, 0, pitch, newHeight, false);
        Require(grown != nullptr, "grown target allocated");
        Require(grown->allocationSerial != initialAllocId, "reallocated for growth");
        Require(grown->width == 160 && grown->height == 96, "grown physical extent 160x96");
        Require(grown->guestWidth == 1280 && grown->guestHeight == 768, "grown guest extent 1280x768");
        Require(!r.sceneCopyPromotion.activeMapping, "promotion mapping closed by restore");

        // 7. Perform a partial clear on the grown target at [4..12) x [5..9)
        constexpr uint64_t grownClearPixel = 0x380000003c000000ull; // green 1.0, alpha 0.5 (0x3800)
        Require(r.Begin(), "begin for grown clear");
        r.Transition(*grown, plume::RenderTextureLayout::COLOR_WRITE, plume::RenderBarrierStage::GRAPHICS);
        r.commandList->setFramebuffer(r.GetFramebuffer(grown, nullptr));
        plume::RenderRect grownClearRect{4, 5, 12, 9};
        r.commandList->clearColor(0, plume::RenderColor(0.0f, 1.0f, 0.0f, 0.5f), &grownClearRect, 1);

        // 8. Read back 160x96 texels and verify
        readbackPitch = 160;
        readback = device->createBuffer(plume::RenderBufferDesc::ReadbackBuffer(uint64_t(readbackPitch) * grown->height * 8));
        Require(bool(readback), "readback buffer allocation");

        const auto grownPixels = Pixels(*grown);
        Require(grownPixels.size() == size_t(grown->width) * grown->height, "grown readback size");

        uint32_t checksCount = 0;
        for (uint32_t y = 0; y < initialHeight; ++y) {
            for (uint32_t x = 0; x < initialWidth; ++x) {
                const uint64_t actual = grownPixels[y * grown->width + x];
                uint64_t expected = pattern[y * initialWidth + x];

                if (x >= 4 && x < 12 && y >= 5 && y < 9) {
                    expected = grownClearPixel;
                } else if (x >= 16 && x < 32 && y >= 12 && y < 24) {
                    expected = promotedClearPixel;
                }

                if (actual != expected) {
                    std::fprintf(stderr, "EXTENT_FAIL at (%u, %u): actual=0x%016llx expected=0x%016llx (alloc=%llu initial=%ux%u grown=%ux%u)\n",
                        x, y, (unsigned long long)actual, (unsigned long long)expected,
                        (unsigned long long)grown->allocationSerial, initialWidth, initialHeight, grown->width, grown->height);
                    Require(false, "pixel mismatch in grown overlap region");
                }
                ++checksCount;
            }
        }

        Require(r.Flush() && r.WaitForGpu(), "drain extent test");
        r.framebuffers.clear(); r.renderTargets.clear();
        {
            std::lock_guard lock(gpu::renderer::catalogMutex);
            gpu::renderer::catalogRoles.erase((uint64_t(base) << 32) | pitch);
        }
        std::printf("PASS_EXTENT: verified %u overlapping pixels (pattern, promoted-restore clear, and grown clear) across 160x92 -> 160x96 growth\n", checksCount);
    }
    void RunEvaluateCapture() {
        auto& r = R();
        const auto dir = std::filesystem::temp_directory_path() /
            ("lo-evaluate-gpu-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(dir);
        struct OwnDirectory {
            std::filesystem::path path;
            ~OwnDirectory() { std::error_code ec; std::filesystem::remove_all(path, ec); }
        } owned{dir};
        unsigned evaluatedCalls = 0;
        auto run = [&](const char* label, gpu::dlss::SrStatus result, bool createFailure,
                       bool compositeFailure, bool submitFailure, bool captureEnabled,
                       bool subrect = false, bool completionUnconfirmed = false,
                       bool featureCreated = true, bool historyReset = false) {
            ++r.frame; ++r.activePlan.cpuSerial; ++r.activePlan.requestSignature;
            r.activePlan.requestedUpscaler = gpu::upscaling::Upscaler::Dlss;
            auto subdir = dir / label;
            std::filesystem::create_directory(subdir);
            if (captureEnabled) {
                r.evaluatePage = std::make_shared<gpu::dlss::capture::Page>();
                r.evaluatePage->number = r.frame - 1; r.evaluatePage->frame = r.frame;
            } else r.evaluatePage.reset();
            const RenderTargetKey key{0, 3, 1280, 0, false};
            auto base = Texture(subrect ? 6 : 4, subrect ? 6 : 4, plume::RenderFormat::R16G16B16A16_FLOAT);
            base->guestHeight = 736;
            HostTexture* original = base.get(); r.renderTargets[key] = std::move(base);
            auto depth = Texture(4, 4, plume::RenderFormat::R32_FLOAT);
            auto motion = Texture(4, 4, plume::RenderFormat::R16G16_FLOAT);
            auto invalid = Texture(4, 4, plume::RenderFormat::R8_UNORM);
            LocalImageDrain imageDrain{device.get()};
            Clear(*original, plume::RenderColor(.25f, .5f, .75f, .5f));
            if (subrect) {
                plume::RenderRect rect{1, 1, 5, 5};
                r.commandList->setFramebuffer(r.GetFramebuffer(original, nullptr));
                r.commandList->clearColor(0, plume::RenderColor(.125f, .25f, .5f, .75f), &rect, 1);
            }
            gpu::temporal::TemporalFrameInputs inputs{};
            inputs.plan = r.activePlan; inputs.renderFrameId = r.frame;
            inputs.currentInputsComplete = true; inputs.motionState = gpu::temporal::MotionState::Tracked;
            inputs.colorEncoding = gpu::temporal::ColorEncoding::HdrLinear;
            inputs.depthConvention = gpu::temporal::DepthConvention::Reversed;
            inputs.resetHistory = historyReset;
            inputs.resetReasons = gpu::temporal::TemporalResetReason::None;
            inputs.temporalEpoch = r.temporalEpoch;
            inputs.jitter.pixelX = .25; inputs.jitter.pixelY = -.375;
            const auto region = [](HostTexture& t) { return gpu::temporal::TextureRegion{t.texture.get(), {4,4}, 0,0,4,4}; };
            inputs.color = region(*original); inputs.depth = region(*depth);
            if (subrect) inputs.color = {original->texture.get(), {6, 6}, 1, 1, 4, 4};
            inputs.motion = region(*motion); inputs.motionInvalidity = region(*invalid);
            Require(r.PrepareSceneCopyDestination(key, *original, inputs), "capture prepare");
            HostTexture* color = original; HostTexture* raster = color;
            plume::RenderViewport guest(0,0,1280,720), viewport;
            plume::RenderRect guestScissor(0,0,1280,720), scissor;
            Require(r.ActivateSceneCopyDestination(color, raster, viewport, scissor, guest, guestScissor), "capture activate");
            Clear(*color, plume::RenderColor(.25f, .5f, .75f, .5f));
            VendorFixture vendor; vendor.outcome = result; vendor.failCreate = createFailure;
            vendor.created = featureCreated;
            if (compositeFailure) { r.sceneCopyPromotionRgbPs.reset(); r.sceneCopyPromotionRgbPipelines.clear(); }
            const auto originalSerial = inputs.plan.cpuSerial;
            const bool applied = r.RecordSceneCopyDlssUsing(vendor, color, raster);
            if (vendor.vendorCalls) ++evaluatedCalls;
            Require(applied == (result == gpu::dlss::SrStatus::Executable && !createFailure && !compositeFailure),
                "capture applied status");
            if (captureEnabled) {
                auto& page = *r.evaluatePage;
                Require(page.attempts == 1 && page.evaluates == vendor.vendorCalls, "attempt/evaluate count");
                const auto& e = *page.entries[0];
                Require(e.inputs.plan.cpuSerial == originalSerial &&
                    e.sdk.reset == (vendor.vendorCalls != 0 && (featureCreated || historyReset)),
                    "frozen plan and created reset");
                if (vendor.vendorCalls) Require(e.sdk.jitterX == .25f && e.sdk.jitterY == -.375f,
                    "actual input pixel jitter");
                if (vendor.vendorCalls && result == gpu::dlss::SrStatus::Executable)
                    Require(e.input.recorded && e.output.recorded, "pre/post copies recorded");
                if (createFailure || result == gpu::dlss::SrStatus::NeedsReconfigure)
                    Require(!e.evaluated && !e.evaluateIndex && !e.input.buffer, "pre-evaluate failure allocated or called");
            }
            if (applied) Clear(*color, plume::RenderColor(.5f, .25f, .125f, .75f)); // UI-like third color.
            if (captureEnabled && vendor.vendorCalls) {
                auto changed = r.activePlan;
                changed.cpuSerial += 123; changed.requestSignature ^= 1234;
                r.activePlan = changed;
            }
            if (submitFailure) fixture::rejectSubmit = true;
            const bool submitted = r.Flush();
            Require(submitted == !submitFailure, "checked capture submission");
            if (captureEnabled) {
                auto& page = *r.evaluatePage;
                auto& e = *page.entries[0];
                Require(e.inputs.plan.cpuSerial == originalSerial, "Flush read changed CPU plan");
                if (result == gpu::dlss::SrStatus::Executable && !createFailure && !submitFailure) {
                    Require(e.checkedSubmit && e.submissionSerial && !e.completed,
                        "capture completed before fence");
                    Require(e.isolatedIncluded && e.vendorSuccess && e.adopted == applied,
                        "vendor and adoption must be independent");
                } else Require(!e.checkedSubmit && !e.completed, "discarded list obtained a completion");
                if (submitFailure) {
                    Require(!page.Export(subdir) && !std::filesystem::exists(subdir / "dlss-output-001.bin"),
                        "failed submit published pixels");
                    Require(!r.gpuSlots[0].evaluateCaptures.empty() || !r.gpuSlots[1].evaluateCaptures.empty(),
                        "failed submit discarded readback owner early");
                } else {
                    if (completionUnconfirmed) {
                        fixture::rejectWaitOnce = true;
                        Require(!r.WaitForGpu() && !e.completed, "unconfirmed completion was accepted");
                        Require(!page.Export(subdir) && !std::filesystem::exists(subdir / "dlss-output-001.bin") &&
                            !std::filesystem::exists(subdir / "dlss-input-001.bin"), "unconfirmed completion published pixels");
                        Require(!r.gpuSlots[0].evaluateCaptures.empty() || !r.gpuSlots[1].evaluateCaptures.empty(),
                            "unconfirmed completion released readback buffer");
                    }
                    Require(r.WaitForGpu(), "capture checked fence completion");
                    Require(page.Export(subdir), "capture export");
                    std::ifstream jsonFile(subdir / "dlss-evaluations.json");
                    const auto json = nlohmann::json::parse(jsonFile);
                    const auto& row = json["evaluations"][0];
                    Require(row["attempt_index"].get<unsigned>() == 1 &&
                        row["plan"]["cpu_serial"].get<uint64_t>() == originalSerial, "typed export identity");
                    Require(row["evaluate_called"].get<bool>() == bool(vendor.vendorCalls), "typed evaluate flag");
                    if (vendor.vendorCalls) Require(row["sdk"]["reset"].get<bool>() ==
                        (featureCreated || historyReset), "reset frozen from final SDK parameter");
                    if (e.vendorSuccess) {
                        Require(row["gpu_completed"].get<bool>() && row["input"]["available"].get<bool>() &&
                            row["output"]["available"].get<bool>(), "completed readback missing");
                        auto pixel = [&](const char* path, size_t size, uint64_t expected) {
                            std::ifstream stream(subdir / path, std::ios::binary);
                            std::vector<char> data((std::istreambuf_iterator<char>(stream)), {});
                            Require(data.size() == size, "tight raw extent");
                            uint64_t first = 0; std::memcpy(&first, data.data(), 8);
                            Require(first == expected, "before/after/UI pixel mismatch");
                        };
                        pixel("dlss-input-001.bin", 4 * 4 * 8,
                            subrect ? 0x3a00380034003000ull : 0x38003a0038003400ull);
                        if (subrect) Require(row["input"]["content_rect"] == nlohmann::json::array({1,1,4,4}) &&
                            row["input"]["storage"] == nlohmann::json::array({6,6}), "subrect storage metadata");
                        pixel("dlss-output-001.bin", 8 * 8 * 8, 0x0000340038004000ull);
                        Require(std::filesystem::exists(subdir / "dlss-input-001-preview.bmp") &&
                            std::filesystem::exists(subdir / "dlss-output-001-preview.bmp"), "previews missing");
                    } else {
                        Require(!std::filesystem::exists(subdir / "dlss-input-001.bin") &&
                            !std::filesystem::exists(subdir / "dlss-output-001.bin"), "discarded pixels published");
                    }
                }
            } else Require(r.WaitForGpu(), "no-capture drain");
            if (applied && !submitFailure) All(*color, 0x3a00300034003800ull, "UI-like final color changed by capture");
            if (!submitFailure) {
                r.framebuffers.clear(); r.renderTargets.clear(); r.sceneCopyPromotion = {};
                if (compositeFailure) r.CompileSceneCopyPromotionShaders();
            }
            r.evaluatePage.reset();
        };
        run("capture-frame-a", gpu::dlss::SrStatus::Executable, false, false, false, true);
        run("capture-frame-b", gpu::dlss::SrStatus::Executable, false, false, false, true, true,
            false, false, false);
        run("input-history-reset", gpu::dlss::SrStatus::Executable, false, false, false, true,
            false, false, false, true);
        run("no-capture", gpu::dlss::SrStatus::Executable, false, false, false, false);
        run("reconfigure", gpu::dlss::SrStatus::NeedsReconfigure, false, false, false, true);
        run("create-fail", gpu::dlss::SrStatus::Executable, true, false, false, true);
        run("vendor-fail", gpu::dlss::SrStatus::Failed, false, false, false, true);
        run("composite-fail", gpu::dlss::SrStatus::Executable, false, true, false, true);
        run("completion-unconfirmed", gpu::dlss::SrStatus::Executable, false, false, false, true, false, true);
        run("submit-fail", gpu::dlss::SrStatus::Executable, false, false, true, true);
        Require(evaluatedCalls == 8, "capture changed vendor call count");
        std::printf("PASS_EVALUATE_CAPTURE: calls=%u checked Vulkan, shared synthetic vendor boundary; no NGX quality claim\n", evaluatedCalls);
    }
};
}
int main(int argc, char** argv) {
    try {
        const bool native = argc == 2 && std::string_view(argv[1]) == "--native";
        const bool extentOnly = argc == 2 && std::string_view(argv[1]) == "--extent-only";
        const bool statusOnly = argc == 2 && std::string_view(argv[1]) == "--status-only";
        const bool fsrScratchOnly = argc == 2 && std::string_view(argv[1]) == "--fsr-scratch-only";
        const bool fsrFallbackOnly = argc == 2 && std::string_view(argv[1]) == "--fsr-fallback-only";
        const bool planIdentity = argc == 2 && std::string_view(argv[1]) == "--plan-identity";
        const bool evaluateCapture = argc == 2 && std::string_view(argv[1]) == "--evaluate-capture-only";
        if (argc > 1 && !native && !extentOnly && !statusOnly && !planIdentity && !evaluateCapture && !fsrScratchOnly && !fsrFallbackOnly) {
            std::fprintf(stderr, "usage: %s [--native|--extent-only|--status-only|--plan-identity|--evaluate-capture-only|--fsr-scratch-only]\n", argv[0]);
            return 2;
        }
        std::setvbuf(stdout, nullptr, _IONBF, 0);
        Harness harness;
        harness.Init(native, std::filesystem::absolute(argv[0]).parent_path());
        if (native) {
            using Q = gpu::upscaling::DlssQuality;
            harness.NativeRun(Q::Quality, 1280, 720, true);
            harness.NativeRun(Q::Quality, 1280, 720, false);
            harness.NativeRun(Q::Balanced, 1280, 720, true);
            harness.NativeRun(Q::Performance, 1920, 1080, true);
            harness.NativeRun(Q::Performance, 1920, 1080, false, true);
            std::puts("PASS: actual NGX + Renderer mapping/submit/alpha/restore; no gameplay or motion-response quality claim");
            return 0;
        }
        if (extentOnly) {
            harness.RunExtentGrowth();
            return 0;
        }
        if (fsrScratchOnly) { harness.RunFsrScratchFormat(); return 0; }
        if (fsrFallbackOnly) { harness.RunFsrFallback(); return 0; }
        if (statusOnly) {
            harness.RunStatus();
            std::puts("PASS: DLSS execution status from the real planner and checked Vulkan submit; no NGX, present, or gameplay claim");
            return 0;
        }
        if (planIdentity) {
            harness.RunPlanIdentity();
            std::puts("PASS: same-frame plan identity, armed-plan submit, and non-empty ordinary batch; no NGX or present claim");
            return 0;
        }
        if (evaluateCapture) {
            harness.RunEvaluateCapture();
            return 0;
        }
        for (unsigned reason = 0; reason < 7; ++reason) {
            harness.Run(gpu::dlss::SrStatus::Executable, reason);
            harness.Run(gpu::dlss::SrStatus::Failed, reason);
        }
        std::printf("PASS: %u renderer checks; actual mapping/Flush/restore + synthetic vendor, no NGX/gameplay\n", fixture::checks);
    } catch (const SkipNative& e) { std::printf("SKIP: %s\n", e.what()); return 77;
    } catch (const std::exception& e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
}
