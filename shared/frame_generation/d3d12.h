#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "core.h"
#include "camera.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <filesystem>
#include <string>
#include <optional>

namespace framegen {
struct D3D12Frame {
    uint64_t frameId=0, deviceEpoch=0, temporalEpoch=0;
    uint32_t width=0, height=0, inputWidth=0, inputHeight=0, backBuffers=0;
    DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;
    ID3D12Resource* depth=nullptr;
    ID3D12Resource* motion=nullptr;
    D3D12_RESOURCE_STATES depthState=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    D3D12_RESOURCE_STATES motionState=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    Camera camera{};
    float deltaMilliseconds=0;
    bool reset=true;
    // Owns all producer and conversion resources referenced by this frame.
    std::shared_ptr<void> lifetime;
};

// One presentation-thread owner. No engine types; APIs and SDK-specific details
// remain in the adapters. Exactly one adapter is installed for a swapchain.
class D3D12Session {
protected:
    template<class T> using ComPtr=Microsoft::WRL::ComPtr<T>;
    ComPtr<ID3D12Device> device_;
    ComPtr<IDXGIFactory4> factory_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<ID3D12Fence> fence_;
    HANDLE event_=nullptr;
    Config requested_{};
    Capabilities caps_{};
    InputLease lease_;
    History history_;
    PresentStatistics statistics_;
    HistoryKey key_{};
    std::optional<HistoryKey> blocked_;
    uint64_t sourceFrame_=0, fenceValue_=0;
    bool active_=false, prepared_=false, initialized_=false, closed_=false, nativeFeatureReleased_=false;
    virtual bool PrepareNative(const D3D12Frame&,ID3D12GraphicsCommandList*,bool,std::string&)=0;
    virtual bool DisableNative(std::string&)=0;
    virtual bool WaitNative(std::string&) { return true; }
    virtual bool PresentedNative(bool,std::string&)=0;
    virtual bool ReleaseNativeFeature(std::string&) { return true; }
    virtual bool ShutdownNative(std::string&)=0;
    bool InitializeBase(ID3D12Device*,IDXGIFactory4*,const Config&,std::string&);
    bool WaitFence(std::string&);
    bool SignalFence(std::string&);
public:
    virtual ~D3D12Session();
    D3D12Session()=default;
    D3D12Session(const D3D12Session&)=delete;
    D3D12Session& operator=(const D3D12Session&)=delete;
    virtual HRESULT CreateQueue(const D3D12_COMMAND_QUEUE_DESC&,REFIID,void**);
    virtual HRESULT CreateSwapchain(ID3D12CommandQueue*,HWND,const DXGI_SWAP_CHAIN_DESC1&,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*,IDXGISwapChain1**)=0;
    bool Prepare(const D3D12Frame&,ID3D12GraphicsCommandList*,std::string&);
    virtual bool SubmitStart(std::string&);
    bool Submitted(bool,uint64_t,std::string&);
    virtual bool PresentStart(std::string&) { return true; }
    bool Presented(bool,std::string&);
    bool CancelRecorded(bool commandReset,bool producerCompleted,std::string&);
    bool Disable(std::string&);
    bool Drain(std::string&);
    bool Quiesce(std::string&);
    bool Reconfigure(const Config&,std::string&); // Same provider and swapchain, after checked drain.
    bool ReleaseFeatureAfterGpuDrain(std::string&); // FG feature before shared NGX SR session shutdown.
    bool Shutdown(std::string&);
    bool Active() const { return active_; }
    bool Pending() const { return lease_.Pending(); }
    const Capabilities& Supported() const { return caps_; }
    const PresentStatistics& Statistics() const { return statistics_; }
};

std::unique_ptr<D3D12Session> CreateDlssD3D12(ID3D12Device*,IDXGIFactory4*,const Config&,
    const std::filesystem::path& runtimeDirectory,std::string&);
std::unique_ptr<D3D12Session> CreateFsrD3D12(ID3D12Device*,IDXGIFactory4*,const Config&,
    const std::filesystem::path& runtimeFile,std::string&);
} // namespace framegen
#endif
