#include "fsr_upscaler_d3d12_backend.h"
#include "fsr_dispatch_policy.h"
#include "fsr_mask_policy.h"

#if defined(LO_GPU_PLUME) && defined(_WIN32)
#include <plume_d3d12.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <optional>
#include <vector>

#if defined(LO_HAS_FSR_D3D12) && LO_HAS_FSR_D3D12
#include <FidelityFX/host/ffx_fsr3upscaler.h>
#include <FidelityFX/host/backends/dx12/ffx_dx12.h>
#include "fsr_prepare_dxil.h"
#include "fsr_present_dxil.h"
#endif

namespace gpu::fsr {

#if defined(LO_HAS_FSR_D3D12) && LO_HAS_FSR_D3D12
namespace {
using Microsoft::WRL::ComPtr;

struct PrepareConstants {
    int32_t colorX, colorY, depthX, depthY, width, height;
    float depthScale, depthBias;
    int32_t maskX, maskY;
    uint32_t maskEnabled;
    float reactiveMax;
};
struct PresentConstants { int32_t width, height, renderWidth, renderHeight, colorX, colorY; };
static_assert(sizeof(PrepareConstants) % 4 == 0 && sizeof(PresentConstants) % 4 == 0);

bool DeviceLost(HRESULT result) {
    return result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET ||
        result == DXGI_ERROR_DEVICE_HUNG;
}

std::unique_ptr<plume::D3D12Texture> CreateTexture(plume::D3D12Device& device,
    uint32_t width, uint32_t height, plume::RenderFormat format) {
    auto base = device.createTexture(plume::RenderTextureDesc::Texture2D(width, height, 1,
        format, plume::RenderTextureFlag::STORAGE | plume::RenderTextureFlag::UNORDERED_ACCESS));
    if (!base) return {};
    auto* native = static_cast<plume::D3D12Texture*>(base.release());
    if (!native->d3d) { delete native; return {}; }
    return std::unique_ptr<plume::D3D12Texture>(native);
}

bool MatchesTexture(const plume::D3D12Texture& image, const temporal::TextureRegion& region,
    const plume::D3D12Device* device, DXGI_FORMAT format, plume::RenderTextureLayout layout) {
    if (!image.d3d || image.device != device || image.desc.dimension != plume::RenderTextureDimension::TEXTURE_2D ||
        image.desc.mipLevels != 1 || image.desc.arraySize != 1 || image.desc.width != region.allocation.width ||
        image.desc.height != region.allocation.height || image.d3d->GetDesc().Format != format ||
        image.layout != layout) return false;
    if (layout == plume::RenderTextureLayout::SHADER_READ)
        return (image.resourceStates & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) != 0;
    if (layout == plume::RenderTextureLayout::COPY_DEST)
        return image.resourceStates == D3D12_RESOURCE_STATE_COPY_DEST;
    return true;
}

bool ValidHybridConfidence(const temporal::TemporalFrameInputs& inputs, const plume::D3D12Device* device) {
    if (inputs.motionState != temporal::MotionState::Hybrid) return true;
    const auto& region = inputs.motionInvalidity;
    if (!region.Complete() || region.x || region.y || region.width != inputs.plan.width ||
        region.height != inputs.plan.height || region.allocation.width != region.width ||
        region.allocation.height != region.height) return false;
    return MatchesTexture(*static_cast<const plume::D3D12Texture*>(region.texture), region, device,
        DXGI_FORMAT_R8_UNORM, plume::RenderTextureLayout::SHADER_READ);
}

FsrMaskDecision QualifyNativeMask(const temporal::TemporalFrameInputs& inputs,
    const plume::D3D12Device* device) {
    auto decision = QualifyFsrMask(inputs);
    if (!decision.useReactive) return decision;
    const auto& region = inputs.fsrMask.sceneContribution;
    const auto& mask = *static_cast<const plume::D3D12Texture*>(region.texture);
    using Reject = FsrMaskRejection;
    if (!mask.d3d || mask.device != device) return {false, Reject::Image};
    if (mask.d3d->GetDesc().Format != DXGI_FORMAT_R8_UNORM ||
        mask.desc.format != plume::RenderFormat::R8_UNORM) return {false, Reject::Format};
    if (mask.desc.dimension != plume::RenderTextureDimension::TEXTURE_2D ||
        mask.desc.mipLevels != 1 || mask.desc.arraySize != 1 ||
        mask.desc.width != region.allocation.width || mask.desc.height != region.allocation.height)
        return {false, Reject::Allocation};
    if (mask.layout != plume::RenderTextureLayout::SHADER_READ ||
        !(mask.resourceStates & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))
        return {false, Reject::Layout};
    return decision;
}

FfxResource MakeResource(const plume::D3D12Texture& texture, FfxResourceStates state,
    FfxResourceUsage usage = FFX_RESOURCE_USAGE_READ_ONLY) {
    return ffxGetResourceDX12(texture.d3d, ffxGetResourceDescriptionDX12(texture.d3d, usage), nullptr, state);
}

FfxResourceStates SdkReadState(const plume::D3D12Texture& texture) {
    return (texture.resourceStates & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) ?
        FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ : FFX_RESOURCE_STATE_COMPUTE_READ;
}

bool MakePipeline(plume::D3D12Device& device, uint32_t sampled, uint32_t storage,
    uint32_t constants, const uint8_t* shader, size_t shaderBytes,
    ComPtr<ID3D12RootSignature>& signature, ComPtr<ID3D12PipelineState>& pipeline, HRESULT& error) {
    D3D12_DESCRIPTOR_RANGE ranges[2]{};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = sampled;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = 0;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].NumDescriptors = storage;
    ranges[1].BaseShaderRegister = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = sampled;
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable.NumDescriptorRanges = 2;
    parameters[0].DescriptorTable.pDescriptorRanges = ranges;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[1].Constants.Num32BitValues = constants;
    parameters[1].Constants.ShaderRegister = 0;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = 2;
    desc.pParameters = parameters;
    ComPtr<ID3DBlob> serialized, errors;
    error = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
        serialized.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(error)) return false;
    error = device.d3d->CreateRootSignature(0, serialized->GetBufferPointer(),
        serialized->GetBufferSize(), IID_PPV_ARGS(signature.GetAddressOf()));
    if (FAILED(error)) return false;
    D3D12_COMPUTE_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = signature.Get();
    pso.CS = {shader, shaderBytes};
    error = device.d3d->CreateComputePipelineState(&pso, IID_PPV_ARGS(pipeline.GetAddressOf()));
    return SUCCEEDED(error);
}

class StateTracker {
    struct Entry {
        plume::D3D12Texture* texture;
        D3D12_RESOURCE_STATES state;
        plume::RenderTextureLayout layout;
    };
    std::vector<Entry> entries_;
public:
    void Transition(ID3D12GraphicsCommandList* commands, plume::D3D12Texture& texture,
        D3D12_RESOURCE_STATES next, plume::RenderTextureLayout layout) {
        auto it = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& e) {
            return e.texture == &texture;
        });
        if (it == entries_.end()) {
            entries_.push_back({&texture, texture.resourceStates, texture.layout});
            it = entries_.end() - 1;
        }
        if (it->state != next) {
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = texture.d3d;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = it->state;
            barrier.Transition.StateAfter = next;
            commands->ResourceBarrier(1, &barrier);
        }
        it->state = next;
        it->layout = layout;
    }
    void Commit() const {
        for (const auto& e : entries_) {
            e.texture->resourceStates = e.state;
            e.texture->layout = e.layout;
        }
    }
};

void WriteSrv(plume::D3D12Device& device, ID3D12DescriptorHeap* heap, uint32_t index,
    const plume::D3D12Texture& texture) {
    D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.Format = texture.d3d->GetDesc().Format;
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Texture2D.MipLevels = 1;
    auto handle = heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += SIZE_T(index) * device.d3d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device.d3d->CreateShaderResourceView(texture.d3d, &desc, handle);
}

void WriteUav(plume::D3D12Device& device, ID3D12DescriptorHeap* heap, uint32_t index,
    const plume::D3D12Texture& texture) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
    desc.Format = texture.d3d->GetDesc().Format;
    desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    auto handle = heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += SIZE_T(index) * device.d3d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device.d3d->CreateUnorderedAccessView(texture.d3d, nullptr, &desc, handle);
}
} // namespace
#endif

struct D3D12Backend::Impl {
    Diagnostics diagnostics{};
#if defined(LO_HAS_FSR_D3D12) && LO_HAS_FSR_D3D12
    plume::D3D12Device* device = nullptr;
    Config config{};
    std::unique_ptr<FfxFsr3UpscalerContext> context;
    std::vector<uint8_t> backendScratch;
    bool contextReady = false, poisoned = false, sharedInitialized = false;
    bool reactiveScratchUnavailable = false;
    std::unique_ptr<plume::D3D12Texture> dilatedDepth, dilatedMotion, previousDepth;
    std::unique_ptr<plume::D3D12Texture> linearColor, canonicalDepth, reactiveMask, sdkOutput, encodedOutput;
    ComPtr<ID3D12RootSignature> prepareSignature, presentSignature;
    ComPtr<ID3D12PipelineState> preparePipeline, presentPipeline;
    ComPtr<ID3D12CommandQueue> queue;
    struct Use {
        uint64_t id = 0, serial = 0;
        ComPtr<ID3D12DescriptorHeap> descriptors;
        bool initializedShared = false, ready = false;
    };
    std::vector<Use> uses;
    uint64_t nextUse = 1, completed = 0;
    std::optional<uint64_t> lastRecordedRenderFrameId;
    std::optional<Config> lastGuardDiagnosticConfig;
    uint64_t lastGuardDiagnosticRequestSignature = 0, lastGuardDiagnosticGeometryEpoch = 0;

    void Destroy() {
        if (contextReady) ffxFsr3UpscalerContextDestroy(context.get());
        contextReady = false; poisoned = false; sharedInitialized = false;
        context.reset(); backendScratch.clear();
        uses.clear(); queue.Reset();
        dilatedDepth.reset(); dilatedMotion.reset(); previousDepth.reset();
        linearColor.reset(); canonicalDepth.reset(); reactiveMask.reset();
        sdkOutput.reset(); encodedOutput.reset();
        preparePipeline.Reset(); presentPipeline.Reset();
        prepareSignature.Reset(); presentSignature.Reset();
        reactiveScratchUnavailable = false;
        lastRecordedRenderFrameId.reset(); lastGuardDiagnosticConfig.reset();
        lastGuardDiagnosticRequestSignature = lastGuardDiagnosticGeometryEpoch = 0;
        device = nullptr;
    }

    Status Fail(const char* api, int64_t raw, std::chrono::steady_clock::time_point started,
        Status status = Status::Failed) {
        diagnostics.failedApi = api; diagnostics.rawResult = raw;
        diagnostics.lastPrepareMilliseconds =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        std::fprintf(stderr, "FSR D3D12 prepare failed: %s raw=%lld elapsed_ms=%.3f\n",
            api, static_cast<long long>(raw), diagnostics.lastPrepareMilliseconds);
        Destroy();
        return status;
    }

    void WaitForGpuIfNeeded() {
        if (uses.empty() || !queue || !device || !device->d3d) return;
        ComPtr<ID3D12Fence> fence;
        if (FAILED(device->d3d->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())))) return;
        if (FAILED(queue->Signal(fence.Get(), 1))) return;
        HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (event && SUCCEEDED(fence->SetEventOnCompletion(1, event))) WaitForSingleObject(event, INFINITE);
        if (event) CloseHandle(event);
    }
#endif
};

D3D12Backend::D3D12Backend() : impl_(std::make_unique<Impl>()) {}
D3D12Backend::~D3D12Backend() {
#if defined(LO_HAS_FSR_D3D12) && LO_HAS_FSR_D3D12
    impl_->WaitForGpuIfNeeded();
    impl_->Destroy();
#endif
}

Status D3D12Backend::EnsureSession(plume::D3D12Device& device, const Config& config) {
#if defined(LO_HAS_FSR_D3D12) && LO_HAS_FSR_D3D12
    const auto started = std::chrono::steady_clock::now();
    if (!device.d3d || !config.renderWidth || !config.renderHeight ||
        !config.outputWidth || !config.outputHeight || !config.deviceEpoch ||
        !upscaling::KnownFsrQuality(config.quality)) return Status::Unavailable;
    if (impl_->contextReady) return !impl_->poisoned && impl_->device == &device && impl_->config == config
        ? Status::Ready : Status::NeedsReconfigure;
    if (impl_->device) return Status::NeedsReconfigure;
    impl_->diagnostics = {};
    impl_->device = &device; impl_->config = config;
    const size_t scratchBytes = ffxGetScratchMemorySizeDX12(FFX_FSR3UPSCALER_CONTEXT_COUNT);
    if (!scratchBytes) return impl_->Fail("ffxGetScratchMemorySizeDX12", 0, started);
    impl_->backendScratch.resize(scratchBytes);
    FfxInterface backend{};
    const auto interfaceResult = ffxGetInterfaceDX12(&backend, ffxGetDeviceDX12(device.d3d),
        impl_->backendScratch.data(), impl_->backendScratch.size(), FFX_FSR3UPSCALER_CONTEXT_COUNT);
    if (interfaceResult != FFX_OK) return impl_->Fail("ffxGetInterfaceDX12", interfaceResult, started);
    FfxFsr3UpscalerContextDescription desc{};
    desc.flags = FFX_FSR3UPSCALER_ENABLE_DEPTH_INVERTED | FFX_FSR3UPSCALER_ENABLE_DEPTH_INFINITE;
    desc.maxRenderSize = {config.renderWidth, config.renderHeight};
    desc.maxUpscaleSize = {config.outputWidth, config.outputHeight};
    desc.backendInterface = backend;
    impl_->context = std::make_unique<FfxFsr3UpscalerContext>();
    const auto createResult = ffxFsr3UpscalerContextCreate(impl_->context.get(), &desc);
    if (createResult != FFX_OK) return impl_->Fail("ffxFsr3UpscalerContextCreate", createResult, started);
    impl_->contextReady = true;
    FfxFsr3UpscalerSharedResourceDescriptions shared{};
    const auto sharedResult = ffxFsr3UpscalerGetSharedResourceDescriptions(impl_->context.get(), &shared);
    if (sharedResult != FFX_OK)
        return impl_->Fail("ffxFsr3UpscalerGetSharedResourceDescriptions", sharedResult, started);
    auto makeShared = [&](const FfxCreateResourceDescription& resource) {
        plume::RenderFormat format = plume::RenderFormat::UNKNOWN;
        switch (resource.resourceDescription.format) {
        case FFX_SURFACE_FORMAT_R32_FLOAT: format = plume::RenderFormat::R32_FLOAT; break;
        case FFX_SURFACE_FORMAT_R16G16_FLOAT: format = plume::RenderFormat::R16G16_FLOAT; break;
        case FFX_SURFACE_FORMAT_R32_UINT: format = plume::RenderFormat::R32_UINT; break;
        default: return std::unique_ptr<plume::D3D12Texture>{};
        }
        return CreateTexture(device, resource.resourceDescription.width, resource.resourceDescription.height, format);
    };
    impl_->dilatedDepth = makeShared(shared.dilatedDepth);
    impl_->dilatedMotion = makeShared(shared.dilatedMotionVectors);
    impl_->previousDepth = makeShared(shared.reconstructedPrevNearestDepth);
    impl_->linearColor = CreateTexture(device, config.renderWidth, config.renderHeight,
        plume::RenderFormat::R16G16B16A16_FLOAT);
    impl_->canonicalDepth = CreateTexture(device, config.renderWidth, config.renderHeight,
        plume::RenderFormat::R32_FLOAT);
    impl_->sdkOutput = CreateTexture(device, config.outputWidth, config.outputHeight,
        plume::RenderFormat::R16G16B16A16_FLOAT);
    impl_->encodedOutput = CreateTexture(device, config.outputWidth, config.outputHeight,
        plume::RenderFormat::R8G8B8A8_UNORM);
    if (!impl_->dilatedDepth || !impl_->dilatedMotion || !impl_->previousDepth ||
        !impl_->linearColor || !impl_->canonicalDepth || !impl_->sdkOutput || !impl_->encodedOutput)
        return impl_->Fail("D3D12Device::createTexture(FSR shared or conversion)", 0, started);
    HRESULT error = S_OK;
    if (!MakePipeline(device, 3, 3, sizeof(PrepareConstants) / 4,
        lo_fsr_prepare_dxil, lo_fsr_prepare_dxil_size,
        impl_->prepareSignature, impl_->preparePipeline, error))
        return impl_->Fail("D3D12CreateComputePipelineState(prepare)", error, started,
            DeviceLost(error) ? Status::DeviceLost : Status::Failed);
    if (!MakePipeline(device, 2, 1, sizeof(PresentConstants) / 4,
        lo_fsr_present_dxil, lo_fsr_present_dxil_size,
        impl_->presentSignature, impl_->presentPipeline, error))
        return impl_->Fail("D3D12CreateComputePipelineState(present)", error, started,
            DeviceLost(error) ? Status::DeviceLost : Status::Failed);
    impl_->diagnostics.lastPrepareMilliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    std::fprintf(stderr, "FSR D3D12 prepare ready: %ux%u -> %ux%u quality=%u elapsed_ms=%.3f\n",
        config.renderWidth, config.renderHeight, config.outputWidth, config.outputHeight,
        unsigned(config.quality), impl_->diagnostics.lastPrepareMilliseconds);
    return Status::Ready;
#else
    (void)device; (void)config;
    return Status::Unavailable;
#endif
}

Attempt D3D12Backend::RecordIsolated(plume::D3D12CommandList& commands, const Config& config,
    const temporal::TemporalFrameInputs& inputs, const FrameMetadata& frame,
    plume::D3D12Texture& output, dlss::EvaluateCapture* capture) {
    Attempt attempt{};
#if defined(LO_HAS_FSR_D3D12) && LO_HAS_FSR_D3D12
    (void)capture; // Vulkan's diagnostic capture currently owns Vulkan images.
    const bool frameGap = impl_->lastRecordedRenderFrameId &&
        !(inputs.renderFrameId > *impl_->lastRecordedRenderFrameId &&
          inputs.renderFrameId - *impl_->lastRecordedRenderFrameId == 1);
    const bool effectiveReset = inputs.resetHistory || !impl_->lastRecordedRenderFrameId || frameGap;
    const auto guard = CheckRecordGuard(impl_->contextReady, impl_->poisoned,
        impl_->config == config, config, inputs, frame,
        {output.desc.width, output.desc.height}, effectiveReset, impl_->nextUse != 0);
    auto reject = [&](const char* reason, Status status = Status::Unavailable) {
        attempt.status = status;
        if (impl_->lastGuardDiagnosticConfig == config &&
            impl_->lastGuardDiagnosticRequestSignature == inputs.plan.requestSignature &&
            impl_->lastGuardDiagnosticGeometryEpoch == inputs.plan.geometryEpoch) return;
        impl_->lastGuardDiagnosticConfig = config;
        impl_->lastGuardDiagnosticRequestSignature = inputs.plan.requestSignature;
        impl_->lastGuardDiagnosticGeometryEpoch = inputs.plan.geometryEpoch;
        std::fprintf(stderr, "FSR D3D12 record guard rejected: frame=%llu reason=%s request=0x%llx geometry_epoch=%llu\n",
            static_cast<unsigned long long>(inputs.renderFrameId), reason,
            static_cast<unsigned long long>(inputs.plan.requestSignature),
            static_cast<unsigned long long>(inputs.plan.geometryEpoch));
    };
    if (guard.status != Status::Ready) { reject(guard.reason, guard.status); return attempt; }
    const auto& color = *static_cast<const plume::D3D12Texture*>(inputs.color.texture);
    const auto& depth = *static_cast<const plume::D3D12Texture*>(inputs.depth.texture);
    const auto& motion = *static_cast<const plume::D3D12Texture*>(inputs.motion.texture);
    if (!MatchesTexture(color, inputs.color, impl_->device, DXGI_FORMAT_R8G8B8A8_UNORM,
            plume::RenderTextureLayout::SHADER_READ) ||
        !MatchesTexture(depth, inputs.depth, impl_->device, DXGI_FORMAT_R32_FLOAT,
            plume::RenderTextureLayout::SHADER_READ) ||
        !MatchesTexture(motion, inputs.motion, impl_->device, DXGI_FORMAT_R16G16_FLOAT,
            plume::RenderTextureLayout::SHADER_READ) ||
        !MatchesTexture(output, {&output, {output.desc.width, output.desc.height}, 0, 0,
            output.desc.width, output.desc.height}, impl_->device, DXGI_FORMAT_R8G8B8A8_UNORM,
            plume::RenderTextureLayout::COPY_DEST)) {
        reject("native_format_or_state"); return attempt;
    }
    if (!ValidHybridConfidence(inputs, impl_->device)) {
        reject("hybrid_confidence_mask"); return attempt;
    }
    const auto* hybridConfidence = inputs.motionState == temporal::MotionState::Hybrid ?
        static_cast<const plume::D3D12Texture*>(inputs.motionInvalidity.texture) : nullptr;
    auto maskDecision = QualifyNativeMask(inputs, impl_->device);
    if (maskDecision.useReactive && !impl_->reactiveMask && !impl_->reactiveScratchUnavailable) {
        impl_->reactiveMask = CreateTexture(*impl_->device, config.renderWidth, config.renderHeight,
            plume::RenderFormat::R32_FLOAT);
        if (!impl_->reactiveMask) impl_->reactiveScratchUnavailable = true;
    }
    if (maskDecision.useReactive && !impl_->reactiveMask)
        maskDecision = {false, FsrMaskRejection::ScratchUnavailable};
    const auto& mask = maskDecision.useReactive ?
        *static_cast<const plume::D3D12Texture*>(inputs.fsrMask.sceneContribution.texture) : color;
    if (!commands.d3d || !commands.commandAllocator || !commands.queue || !commands.queue->d3d ||
        commands.queue->device != impl_->device || commands.open) {
        reject("isolated_command_list"); return attempt;
    }
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 9;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    Impl::Use use{};
    const HRESULT heapResult = impl_->device->d3d->CreateDescriptorHeap(&heapDesc,
        IID_PPV_ARGS(use.descriptors.GetAddressOf()));
    if (FAILED(heapResult)) {
        attempt.vkResult = int32_t(heapResult);
        attempt.status = DeviceLost(heapResult) ? Status::DeviceLost : Status::Failed;
        return attempt;
    }
    WriteSrv(*impl_->device, use.descriptors.Get(), 0, color);
    WriteSrv(*impl_->device, use.descriptors.Get(), 1, depth);
    WriteSrv(*impl_->device, use.descriptors.Get(), 2, mask);
    WriteUav(*impl_->device, use.descriptors.Get(), 3, *impl_->linearColor);
    WriteUav(*impl_->device, use.descriptors.Get(), 4, *impl_->canonicalDepth);
    WriteUav(*impl_->device, use.descriptors.Get(), 5,
        maskDecision.useReactive ? *impl_->reactiveMask : *impl_->canonicalDepth);
    WriteSrv(*impl_->device, use.descriptors.Get(), 6, *impl_->sdkOutput);
    WriteSrv(*impl_->device, use.descriptors.Get(), 7, color);
    WriteUav(*impl_->device, use.descriptors.Get(), 8, *impl_->encodedOutput);
    use.id = impl_->nextUse++;
    use.initializedShared = !impl_->sharedInitialized;
    attempt.useId = use.id;
    impl_->uses.push_back(std::move(use));
    commands.invalidateCachedNativeState();
    commands.resetRootBindingStats();
    const HRESULT allocatorResult = commands.commandAllocator->Reset();
    const HRESULT resetResult = SUCCEEDED(allocatorResult) ?
        commands.d3d->Reset(commands.commandAllocator, nullptr) : allocatorResult;
    if (FAILED(resetResult)) {
        attempt.vkResult = int32_t(resetResult);
        attempt.status = DeviceLost(resetResult) ? Status::DeviceLost : Status::Failed;
        impl_->poisoned = true;
        return attempt;
    }
    commands.open = true;
    auto* cmd = commands.d3d;
    StateTracker states;
    auto uav = [&](plume::D3D12Texture& texture) {
        states.Transition(cmd, texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            plume::RenderTextureLayout::GENERAL);
    };
    auto srv = [&](plume::D3D12Texture& texture) {
        states.Transition(cmd, texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            plume::RenderTextureLayout::SHADER_READ);
    };
    uav(*impl_->linearColor); uav(*impl_->canonicalDepth);
    if (maskDecision.useReactive) uav(*impl_->reactiveMask);
    uav(*impl_->sdkOutput); uav(*impl_->encodedOutput);
    if (!impl_->sharedInitialized) {
        uav(*impl_->dilatedDepth); uav(*impl_->dilatedMotion); uav(*impl_->previousDepth);
    }
    ID3D12DescriptorHeap* const heap = impl_->uses.back().descriptors.Get();
    cmd->SetDescriptorHeaps(1, &heap);
    cmd->SetComputeRootSignature(impl_->prepareSignature.Get());
    cmd->SetPipelineState(impl_->preparePipeline.Get());
    auto gpuHandle = heap->GetGPUDescriptorHandleForHeapStart();
    cmd->SetComputeRootDescriptorTable(0, gpuHandle);
    const PrepareConstants prepare{int32_t(inputs.color.x), int32_t(inputs.color.y),
        int32_t(inputs.depth.x), int32_t(inputs.depth.y), int32_t(config.renderWidth),
        int32_t(config.renderHeight), frame.depthScale, frame.depthBias,
        int32_t(inputs.fsrMask.sceneContribution.x), int32_t(inputs.fsrMask.sceneContribution.y),
        uint32_t(maskDecision.useReactive), kReactiveMax};
    cmd->SetComputeRoot32BitConstants(1, sizeof(prepare) / 4, &prepare, 0);
    cmd->Dispatch((config.renderWidth + 7) / 8, (config.renderHeight + 7) / 8, 1);
    srv(*impl_->linearColor); srv(*impl_->canonicalDepth);
    if (maskDecision.useReactive) srv(*impl_->reactiveMask);
    FfxFsr3UpscalerDispatchDescription dispatch{};
    dispatch.commandList = ffxGetCommandListDX12(cmd);
    dispatch.color = MakeResource(*impl_->linearColor, FFX_RESOURCE_STATE_COMPUTE_READ);
    dispatch.depth = MakeResource(*impl_->canonicalDepth, FFX_RESOURCE_STATE_COMPUTE_READ);
    dispatch.motionVectors = MakeResource(motion, SdkReadState(motion));
    dispatch.reactive = maskDecision.useReactive ?
        MakeResource(*impl_->reactiveMask, FFX_RESOURCE_STATE_COMPUTE_READ) : FfxResource{};
    dispatch.transparencyAndComposition = hybridConfidence ?
        MakeResource(*hybridConfidence, SdkReadState(*hybridConfidence)) : FfxResource{};
    dispatch.dilatedDepth = MakeResource(*impl_->dilatedDepth,
        FFX_RESOURCE_STATE_UNORDERED_ACCESS, FFX_RESOURCE_USAGE_UAV);
    dispatch.dilatedMotionVectors = MakeResource(*impl_->dilatedMotion,
        FFX_RESOURCE_STATE_UNORDERED_ACCESS, FFX_RESOURCE_USAGE_UAV);
    dispatch.reconstructedPrevNearestDepth = MakeResource(*impl_->previousDepth,
        FFX_RESOURCE_STATE_UNORDERED_ACCESS, FFX_RESOURCE_USAGE_UAV);
    dispatch.output = MakeResource(*impl_->sdkOutput,
        FFX_RESOURCE_STATE_UNORDERED_ACCESS, FFX_RESOURCE_USAGE_UAV);
    dispatch.jitterOffset = {float(inputs.jitter.pixelX), float(inputs.jitter.pixelY)};
    dispatch.motionVectorScale = {1.0f, 1.0f};
    dispatch.renderSize = {config.renderWidth, config.renderHeight};
    dispatch.upscaleSize = {config.outputWidth, config.outputHeight};
    dispatch.enableSharpening = frame.enableSharpening;
    dispatch.sharpness = frame.sharpness;
    dispatch.frameTimeDelta = frame.frameTimeDeltaMilliseconds;
    dispatch.preExposure = 1.0f;
    dispatch.reset = effectiveReset;
    dispatch.cameraNear = frame.cameraNear;
    dispatch.cameraFar = frame.cameraFar;
    dispatch.cameraFovAngleVertical = frame.verticalFovRadians;
    dispatch.viewSpaceToMetersFactor = frame.viewSpaceToMetersFactor;
    const auto sdkResult = ffxFsr3UpscalerContextDispatch(impl_->context.get(), &dispatch);
    attempt.sdkResult = int32_t(sdkResult);
    if (sdkResult == FFX_OK) {
        srv(*impl_->sdkOutput);
        cmd->SetDescriptorHeaps(1, &heap); // SDK binds its own heap.
        cmd->SetComputeRootSignature(impl_->presentSignature.Get());
        cmd->SetPipelineState(impl_->presentPipeline.Get());
        const auto increment = impl_->device->d3d->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        gpuHandle.ptr += UINT64(6) * increment;
        cmd->SetComputeRootDescriptorTable(0, gpuHandle);
        const PresentConstants present{int32_t(config.outputWidth), int32_t(config.outputHeight),
            int32_t(config.renderWidth), int32_t(config.renderHeight),
            int32_t(inputs.color.x), int32_t(inputs.color.y)};
        cmd->SetComputeRoot32BitConstants(1, sizeof(present) / 4, &present, 0);
        cmd->Dispatch((config.outputWidth + 7) / 8, (config.outputHeight + 7) / 8, 1);
        states.Transition(cmd, *impl_->encodedOutput, D3D12_RESOURCE_STATE_COPY_SOURCE,
            plume::RenderTextureLayout::COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION source{impl_->encodedOutput->d3d,
            D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {0}};
        D3D12_TEXTURE_COPY_LOCATION destination{output.d3d,
            D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {0}};
        cmd->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    }
    const HRESULT closeResult = cmd->Close();
    attempt.vkResult = int32_t(closeResult);
    commands.open = false;
    commands.invalidateCachedNativeState();
    const bool ready = sdkResult == FFX_OK && SUCCEEDED(closeResult);
    const HRESULT removedReason = sdkResult == FFX_OK ? S_OK : impl_->device->d3d->GetDeviceRemovedReason();
    attempt.status = DeviceLost(closeResult) || DeviceLost(removedReason) ? Status::DeviceLost :
        ready ? Status::Ready : Status::Failed;
    if (DeviceLost(removedReason)) attempt.vkResult = int32_t(removedReason);
    impl_->uses.back().ready = ready;
    if (ready) {
        states.Commit();
        impl_->queue = commands.queue->d3d;
        impl_->lastRecordedRenderFrameId = inputs.renderFrameId;
        impl_->diagnostics.lastDispatchRenderFrameId = inputs.renderFrameId;
        impl_->diagnostics.lastDispatchReset = effectiveReset;
        impl_->diagnostics.lastResetForFrameGap = frameGap;
    } else impl_->poisoned = true;
#else
    (void)commands; (void)config; (void)inputs; (void)frame; (void)output; (void)capture;
#endif
    return attempt;
}

void D3D12Backend::OnBatchSubmitted(uint64_t useId, uint64_t serial) {
#if defined(LO_HAS_FSR_D3D12) && LO_HAS_FSR_D3D12
    if (!useId || !serial || serial <= impl_->completed) return;
    for (auto& use : impl_->uses) if (use.id == useId && !use.serial) {
        use.serial = serial;
        if (use.ready && use.initializedShared) impl_->sharedInitialized = true;
        break;
    }
#else
    (void)useId; (void)serial;
#endif
}

void D3D12Backend::OnBatchDiscarded(uint64_t useId) {
#if defined(LO_HAS_FSR_D3D12) && LO_HAS_FSR_D3D12
    auto it = std::find_if(impl_->uses.begin(), impl_->uses.end(),
        [useId](const Impl::Use& use) { return use.id == useId && !use.serial; });
    if (it != impl_->uses.end()) {
        if (it->ready) impl_->poisoned = true;
        impl_->uses.erase(it);
    }
#else
    (void)useId;
#endif
}

void D3D12Backend::ReleaseCompletedThrough(uint64_t serial) {
#if defined(LO_HAS_FSR_D3D12) && LO_HAS_FSR_D3D12
    impl_->completed = std::max(impl_->completed, serial);
    auto it = impl_->uses.begin();
    while (it != impl_->uses.end()) {
        if (it->serial && it->serial <= impl_->completed) it = impl_->uses.erase(it);
        else ++it;
    }
#else
    (void)serial;
#endif
}

bool D3D12Backend::HasFeatureState() const {
#if defined(LO_HAS_FSR_D3D12) && LO_HAS_FSR_D3D12
    return impl_->device || !impl_->uses.empty();
#else
    return false;
#endif
}
const Diagnostics& D3D12Backend::LastDiagnostics() const { return impl_->diagnostics; }
void D3D12Backend::ReleaseFeatureAfterGpuDrain() {
#if defined(LO_HAS_FSR_D3D12) && LO_HAS_FSR_D3D12
    if (impl_->uses.empty()) impl_->Destroy();
#endif
}
void D3D12Backend::AbandonUsesAfterDeviceLoss() {
#if defined(LO_HAS_FSR_D3D12) && LO_HAS_FSR_D3D12
    impl_->uses.clear(); impl_->Destroy();
#endif
}
} // namespace gpu::fsr
#elif defined(LO_GPU_PLUME)
namespace gpu::fsr {
struct D3D12Backend::Impl { Diagnostics diagnostics{}; };
D3D12Backend::D3D12Backend() : impl_(std::make_unique<Impl>()) {}
D3D12Backend::~D3D12Backend() = default;
Status D3D12Backend::EnsureSession(plume::D3D12Device&, const Config&) { return Status::Unavailable; }
Attempt D3D12Backend::RecordIsolated(plume::D3D12CommandList&, const Config&,
    const temporal::TemporalFrameInputs&, const FrameMetadata&, plume::D3D12Texture&,
    dlss::EvaluateCapture*) { return {}; }
void D3D12Backend::OnBatchSubmitted(uint64_t, uint64_t) {}
void D3D12Backend::OnBatchDiscarded(uint64_t) {}
void D3D12Backend::ReleaseCompletedThrough(uint64_t) {}
bool D3D12Backend::HasFeatureState() const { return false; }
const Diagnostics& D3D12Backend::LastDiagnostics() const { return impl_->diagnostics; }
void D3D12Backend::ReleaseFeatureAfterGpuDrain() {}
void D3D12Backend::AbandonUsesAfterDeviceLoss() {}
} // namespace gpu::fsr
#endif
