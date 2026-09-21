// Compile the production Renderer itself. No game code, assets, or SDL window
// are linked. The only substituted GPU operation is the proprietary NGX call.
#include <gpu/renderer.cpp>
#include <gpu/vulkan_command_recording.h>
#include <gpu/vulkan_submission_state.h>
#include <stdexcept>

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
    fixture::lastListCount = count;
    return fixture::state.SubmitBatch([&] { return int32_t(vkResetFences(d->vk, 1, &f->vk)); },
        [&] { return int32_t(vkQueueSubmit(q->queue->vk, 1, &submit, f->vk)); }, serial, result);
}
}
namespace gpu::frame_plan {
void ReportPlanFailure(const PlanFailure&) {} // No CPU guest frame producer.
}
namespace gpu::taa_collection { bool Enabled() { return false; } }

namespace {
using namespace gpu::renderer;
using fixture::Require;
struct VendorFixture {
    gpu::dlss::SrStatus outcome = gpu::dlss::SrStatus::Executable;
    uint32_t calls = 0;
    gpu::dlss::SrStatus EnsureSession(const plume::VulkanDevice&) { return gpu::dlss::SrStatus::Executable; }
    gpu::dlss::SrAttempt RecordIsolated(plume::VulkanCommandList& list, const gpu::dlss::SrConfig& config,
        const gpu::temporal::TemporalFrameInputs& inputs, plume::VulkanTexture& output) {
        ++calls;
        Require(gpu::temporal::MatchesDepthConvention(inputs.depthConvention, config.depthInverted),
            "renderer must pass the producer depth ordering to NGX");
        Require(gpu::submission::BeginCommands(list) == VK_SUCCESS, "isolated begin");
        // Exact FP16 values; alpha is deliberately different from the guest.
        const VkClearColorValue value{{2.0f, .5f, .25f, 0.0f}};
        vkCmdClearColorImage(list.vk, output.vk, VK_IMAGE_LAYOUT_GENERAL, &value, 1, &output.imageSubresourceRange);
        Require(gpu::submission::EndCommands(list) == VK_SUCCESS, "isolated end");
        return {outcome};
    }
    void OnBatchDiscarded(uint64_t) {}
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
};
}
int main(int argc, char** argv) {
    try {
        const bool native = argc == 2 && std::string_view(argv[1]) == "--native";
        if (argc > 1 && !native) { std::fprintf(stderr, "usage: %s [--native]\n", argv[0]); return 2; }
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
        for (unsigned reason = 0; reason < 7; ++reason) {
            harness.Run(gpu::dlss::SrStatus::Executable, reason);
            harness.Run(gpu::dlss::SrStatus::Failed, reason);
        }
        std::printf("PASS: %u renderer checks; actual mapping/Flush/restore + synthetic vendor, no NGX/gameplay\n", fixture::checks);
    } catch (const SkipNative& e) { std::printf("SKIP: %s\n", e.what()); return 77;
    } catch (const std::exception& e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
}
