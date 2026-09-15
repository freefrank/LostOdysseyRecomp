#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <cstdint>
#include <cstdio>

namespace gpu::d3d11
{
// Creates a hardware D3D11 device for capability probing. This is not a
// renderer: Plume has no D3D11 backend, and game draws still require one.
struct ProbeResult
{
    HRESULT hr = E_FAIL;
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_9_1;
    uint32_t vendorId = 0;
    uint32_t deviceId = 0;
};

inline void Format(char* buffer, size_t size, const ProbeResult& probe)
{
    if (!buffer || !size)
        return;
    if (SUCCEEDED(probe.hr))
        std::snprintf(buffer, size,
            "ok feature_level=0x%x vendor=%u device=%u HRESULT=0x%08lX",
            unsigned(probe.level), probe.vendorId, probe.deviceId,
            static_cast<unsigned long>(probe.hr));
    else
        std::snprintf(buffer, size, "failed HRESULT=0x%08lX",
            static_cast<unsigned long>(probe.hr));
}

inline ProbeResult CreateDevice(ID3D11Device** device, ID3D11DeviceContext** context)
{
    ProbeResult result{};
    if (device)
        *device = nullptr;
    if (context)
        *context = nullptr;
    static const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
    };
    result.hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, UINT(sizeof(levels) / sizeof(levels[0])), D3D11_SDK_VERSION,
        device, &result.level, context);
    if (FAILED(result.hr) || !device || !*device)
        return result;
    IDXGIDevice* dxgiDevice = nullptr;
    if (SUCCEEDED((*device)->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice))) &&
        dxgiDevice)
    {
        IDXGIAdapter* adapter = nullptr;
        if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter)) && adapter)
        {
            DXGI_ADAPTER_DESC desc{};
            if (SUCCEEDED(adapter->GetDesc(&desc)))
            {
                result.vendorId = desc.VendorId;
                result.deviceId = desc.DeviceId;
            }
            adapter->Release();
        }
        dxgiDevice->Release();
    }
    return result;
}

inline ProbeResult ProbeHardware()
{
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    const auto result = CreateDevice(&device, &context);
    if (context)
        context->Release();
    if (device)
        device->Release();
    return result;
}
}
