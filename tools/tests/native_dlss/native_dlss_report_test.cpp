#include <gpu/dlss_ngx.h>

#include <cstdint>
#include <cstdio>
#include <string>

int main() {
    using namespace gpu::dlss;
    constexpr int32_t kSuccess = 1;
    constexpr int32_t kFeatureNotSupported = int32_t(0xBAD00001u);
    constexpr int32_t kPlatformError = int32_t(0xBAD00002u);
    if (ProbeExitCode(ProbeState::Available) != 0) return 1;
    if (ProbeExitCode(ProbeState::SdkDisabled) != 77 || ProbeExitCode(ProbeState::Unavailable) != 77) return 2;
    // The executable's base Vulkan interface/device early returns use this same policy.
    if (ProbeExitCode(ProbeState::ApiError) != 1 || ProbeExitCode(ProbeState::NotProbed) != 1) return 3;

    const auto capability = [&](int32_t available, int32_t needsDriver, int32_t initResult) {
        return ClassifySuperSamplingCapabilities({kSuccess, available}, {kSuccess, needsDriver}, {kSuccess, 470},
            {kSuccess, 0}, {kSuccess, initResult}, kSuccess, kFeatureNotSupported);
    };
    if (capability(1, 0, kSuccess).decision != CapabilityDecision::Proceed) return 4;
    if (capability(0, 0, kSuccess).decision != CapabilityDecision::Unavailable) return 5;
    const auto driverUpdate = capability(1, 1, kSuccess);
    if (driverUpdate.decision != CapabilityDecision::Unavailable || driverUpdate.reason.find("470.0") == std::string::npos) return 6;
    if (capability(1, 0, kFeatureNotSupported).decision != CapabilityDecision::Unavailable) return 7;
    if (capability(1, 0, kPlatformError).decision != CapabilityDecision::ApiError) return 8;
    if (ClassifySuperSamplingCapabilities({0, 1}, {kSuccess, 0}, {kSuccess, 470}, {kSuccess, 0},
        {kSuccess, kSuccess}, kSuccess, kFeatureNotSupported).decision != CapabilityDecision::ApiError) return 9;

    ProbeReport report;
    report.optimalSettings = {
        {"quality", 1280, 720, 960, 540, 1920, 1080, 0.35f, 1},
        {"balanced", 1114, 626, 960, 540, 1920, 1080, 0.35f, 1},
        {"performance", 960, 540, 960, 540, 1920, 1080, 0.35f, 1},
    };
    if (!HasValidOptimalSettings(report)) return 10;
    report.optimalSettings[1].optimalWidth = 0;
    if (HasValidOptimalSettings(report)) return 11;
    std::puts("PASS: native DLSS report exit and optimal-settings semantics");
    return 0;
}
