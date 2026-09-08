#pragma once
#include <array>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gpu::backend {
// Persisted IDs. D3D11 is recognized, but has no renderer implementation.
enum class Backend : uint32_t { D3D12 = 0, Vulkan = 1, D3D11 = 2 };
inline const char* Name(Backend b) {
    switch (b) { case Backend::D3D12: return "D3D12"; case Backend::Vulkan: return "Vulkan"; case Backend::D3D11: return "D3D11"; }
    return "Unknown";
}
inline bool Known(Backend b) { return b == Backend::D3D12 || b == Backend::Vulkan || b == Backend::D3D11; }
inline bool Equal(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        const auto c = a[i] >= 'A' && a[i] <= 'Z' ? char(a[i] + ('a' - 'A')) : a[i];
        if (c != b[i]) return false;
    }
    return true;
}
inline std::optional<Backend> Parse(std::string_view text) {
    if (Equal(text, "d3d12") || Equal(text, "dx12")) return Backend::D3D12;
    if (Equal(text, "vulkan")) return Backend::Vulkan;
    if (Equal(text, "d3d11") || Equal(text, "dx11")) return Backend::D3D11;
    return std::nullopt;
}
inline std::optional<Backend> Requested(Backend configured, const char* environment) {
    if (!environment || Equal(environment, "auto")) return Known(configured) ? std::optional(configured) : std::nullopt;
    return Parse(environment);
}
struct Capabilities {
    bool device = false, geometryShader = false, bufferDeviceAddress = false;
    bool shaderInt64 = false, scalarBlockLayout = false;
    uint32_t apiVersion = 0, shaderModel = 0, bindingTier = 0;
    uint32_t boundSets = 0, samplers = 0, sampledImages = 0, storageBuffers = 0, pushConstants = 0;
};
inline std::string Missing(Backend backend, const Capabilities& c) {
    if (backend == Backend::D3D11) return "Unsupported: D3D11 renderer is not implemented";
    if (!Known(backend)) return "Unsupported: unknown backend";
    if (!c.device) return "device creation failed";
    if (!c.geometryShader) return "geometry shaders unavailable";
    if (backend == Backend::D3D12) {
        if (c.shaderModel < 0x60) return "Shader Model 6.0 required";
        if (c.bindingTier < 2) return "Resource Binding Tier 2 required (32 samplers and 192 SRVs)";
    } else {
        if (c.apiVersion < ((1u << 22) | (2u << 12))) return "Vulkan 1.2 required by SPIR-V target";
        if (!c.bufferDeviceAddress || !c.shaderInt64) return "bufferDeviceAddress and shaderInt64 required";
        if (!c.scalarBlockLayout) return "scalarBlockLayout required by DX layout";
        if (c.boundSets < 5 || c.samplers < 32 || c.sampledImages < 96 || c.storageBuffers < 1 || c.pushConstants < 24)
            return "descriptor/push-constant limits below renderer layout (5 sets, 32 samplers, 96 images, 1 storage buffer, 24 bytes)";
    }
    return {};
}
struct Attempt { Backend backend; std::string error; };
struct Selection {
    Backend requested = Backend::D3D12;
    std::optional<Backend> selected;
    std::vector<Attempt> attempts;
    std::string Describe() const {
        std::string text = std::string("requested=") + Name(requested) + " selected=" + (selected ? Name(*selected) : "none");
        for (const auto& a : attempts) text += std::string("; ") + Name(a.backend) + ": " + (a.error.empty() ? "ready" : a.error);
        return text;
    }
};
// Attempt owns temporary renderer/device state. A failure (including exceptions)
// must be fully reset before the next candidate; successful state stays alive.
// There are at most two implemented attempts. Unsupported DX11 never creates a device.
template<class Try, class Reset>
Selection Select(Backend requested, Try&& attempt, Reset&& reset) {
    Selection result; result.requested = requested;
    if (!Known(requested)) { result.attempts.push_back({requested, "Unsupported: unknown backend"}); return result; }
    if (requested == Backend::D3D11) result.attempts.push_back({requested, Missing(requested, {})});
    const auto first = requested == Backend::D3D11 ? Backend::D3D12 : requested;
    for (const auto backend : std::array{first, first == Backend::Vulkan ? Backend::D3D12 : Backend::Vulkan}) {
        std::string error;
        try { error = attempt(backend); }
        catch (const std::exception& e) { error = std::string("initialization exception: ") + e.what(); }
        catch (...) { error = "unknown initialization exception"; }
        if (error.empty()) { result.attempts.push_back({backend, {}}); result.selected = backend; return result; }
        reset();
        result.attempts.push_back({backend, std::move(error)});
    }
    return result;
}
}
