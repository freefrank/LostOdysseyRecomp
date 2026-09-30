#pragma once
#include "fsr_upscaler.h"

#if defined(LO_GPU_PLUME)
namespace gpu::fsr {
class D3D12Backend {
public:
    D3D12Backend();
    ~D3D12Backend();
    Status EnsureSession(plume::D3D12Device& device, const Config& config);
    Attempt RecordIsolated(plume::D3D12CommandList& commands, const Config& config,
        const temporal::TemporalFrameInputs& inputs, const FrameMetadata& frame,
        plume::D3D12Texture& output, dlss::EvaluateCapture* capture);
    void OnBatchSubmitted(uint64_t useId, uint64_t serial);
    void OnBatchDiscarded(uint64_t useId);
    void ReleaseCompletedThrough(uint64_t serial);
    bool HasFeatureState() const;
    const Diagnostics& LastDiagnostics() const;
    void ReleaseFeatureAfterGpuDrain();
    void AbandonUsesAfterDeviceLoss();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace gpu::fsr
#endif
