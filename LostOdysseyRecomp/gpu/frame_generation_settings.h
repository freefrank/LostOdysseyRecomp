#pragma once
#include "../settings/config.h"
#include "vrr_policy.h"
#include "../../shared/frame_generation/environment.h"
#include <string>

namespace gpu::frame_generation {
// Build availability only. The SDK still checks the actual adapter and runtime.
inline constexpr bool D3D12CompiledProvider(framegen::Provider provider) {
#if defined(_WIN32) && defined(LO_ENABLE_D3D12_FG)
#ifdef FRAMEGEN_WITH_DLSS
    if (provider == framegen::Provider::Dlss) return true;
#endif
#ifdef FRAMEGEN_WITH_FSR
    if (provider == framegen::Provider::Fsr) return true;
#endif
#endif
    return false;
}

// Explicit LO_FG_PROVIDER (or legacy LO_DLSS_FG) keeps its existing whole-request
// defaults. Individual LO_FG_* switches otherwise override persisted values.
inline framegen::EnvironmentSelection ResolveD3D12Selection(const settings::Config& saved,
    const char* provider, const char* mode, const char* multiplier,
    const char* targetFps, const char* legacyDlss, uint32_t refreshHz = 0) {
    const bool providerOverride = provider || legacyDlss;
    const char* savedProvider = saved.frameGenerationProvider == framegen::Provider::Dlss ? "dlss" :
        saved.frameGenerationProvider == framegen::Provider::Fsr ? "fsr" : "off";
    const char* savedMode = saved.frameGenerationMode == framegen::Mode::Dynamic ? "dynamic" : "fixed";
    const auto savedMultiplier = std::to_string(saved.frameGenerationMultiplier);
    const auto savedTargetFps = std::to_string(saved.frameGenerationTargetFps);
    auto selection = framegen::ParseEnvironment(providerOverride ? provider : savedProvider,
        mode ? mode : (providerOverride ? nullptr : savedMode),
        multiplier ? multiplier : (providerOverride ? nullptr : savedMultiplier.c_str()),
        targetFps ? targetFps : (providerOverride ? nullptr : savedTargetFps.c_str()), legacyDlss);
    if (selection.Enabled() && !D3D12CompiledProvider(selection.config.provider))
        selection.error = "selected D3D12 FG provider was not compiled";
    if (selection.Enabled() && selection.config.mode == framegen::Mode::Dynamic)
        selection.config.targetFrameRate = vrr::DynamicTarget(selection.config.targetFrameRate,
            saved.variableRefreshRate, refreshHz);
    return selection;
}
} // namespace gpu::frame_generation
