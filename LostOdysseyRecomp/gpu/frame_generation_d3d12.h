#pragma once
#if defined(_WIN32) && defined(LO_ENABLE_D3D12_FG)
#include "frame_generation_composite.h"
#include "dlss_fg_depth.h"
#include "../../shared/frame_generation/d3d12.h"
#include "../../shared/frame_generation/environment.h"
#include <plume_d3d12.h>

namespace gpu::frame_generation {
// GPU/presentation-thread owner; never called by the SDL/window thread.
// The generic SDK session owns the swapchain; this bridge only translates
// game snapshots and Plume resources to the reusable provider-neutral API.
class D3D12Bridge {
public:
    ~D3D12Bridge();
    bool Initialize(plume::D3D12Device&, const framegen::Config&,
        const std::filesystem::path& runtime, std::string& reason);
    void PrepareAfterHostDrain(const CompositeHandoff&, plume::D3D12SwapChain&,
        plume::D3D12CommandList&, uint64_t deviceEpoch);
    void SubmitStart();
    void HostSubmitted(bool, uint64_t);
    void PresentStart();
    void Presented(bool);
    void CancelUnsubmitted(plume::RenderCommandList*);
    void Quiesce();
    void Suspend();
    void ReleaseFeatureAfterGpuDrain();
    bool Reconfigure(const framegen::Config&, std::string&);
    void Shutdown(); // after every external swapchain COM reference is released
    bool Enabled() const { return session_ && !suspended_; }
    framegen::Provider Provider() const { return config_.provider; }
    bool Available() const { return Enabled() && session_->Active(); }
    framegen::Capabilities Supported() const { return session_ ? session_->Supported() : framegen::Capabilities{}; }
    uint64_t ActualPresents() const { return session_ ? session_->Statistics().actualPresents : 0; }
private:
    void Require(bool, const std::string&);
    plume::D3D12Device* device_ = nullptr;
    std::unique_ptr<framegen::D3D12Session> session_;
    std::shared_ptr<dlss_fg::DepthRemapper> depth_;
    plume::D3D12CommandList* recording_ = nullptr;
    bool attempted_ = false;
    bool suspended_ = false;
    framegen::Config config_{};
    temporal::Matrix previousVP_{};
    temporal::Viewport previousRaster_{};
    frame_plan::FramePlan previousPlan_{};
    uint64_t previousFrame_ = 0, previousEpoch_ = 0, rejected_ = 0;
};
} // namespace gpu::frame_generation
#endif
