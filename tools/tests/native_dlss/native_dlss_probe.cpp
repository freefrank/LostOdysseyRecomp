#include <gpu/dlss_ngx.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

namespace {
std::string Escape(std::string_view value) {
    std::string result;
    for (const auto ch : value) {
        if (ch == '\\' || ch == '"') result += '\\';
        if (ch == '\n') result += "\\n";
        else if (ch != '\n') result += ch;
    }
    return result;
}

std::unique_ptr<plume::RenderInterface> CreateInterface(const plume::VulkanExtensionHooks& hooks) {
#if PLUME_SDL_VULKAN_ENABLED
    return plume::CreateVulkanInterface(nullptr, hooks);
#else
    return plume::CreateVulkanInterface(hooks);
#endif
}

void PrintReport(const gpu::dlss::ProbeReport& report) {
    const auto printOptionalUnsigned = [](const std::optional<uint32_t>& value) {
        if (value) std::printf("%u", *value); else std::fputs("null", stdout);
    };
    const auto printCapability = [](const gpu::dlss::CapabilityValue& value) {
        std::fputs("{\"raw\":", stdout);
        if (value.raw) std::printf("%d", *value.raw); else std::fputs("null", stdout);
        std::fputs(",\"value\":", stdout);
        if (value.value) std::printf("%d", *value.value); else std::fputs("null", stdout);
        std::fputc('}', stdout);
    };
    const auto printExtensions = [](const gpu::dlss::ExtensionStatus& status) {
        std::printf("{\"state\":\"%s\",\"reason\":\"%s\",\"create_failure\":%d,\"required\":[",
            Escape(status.state).c_str(), Escape(status.reason).c_str(), status.createFailure);
        for (size_t i = 0; i < status.required.size(); ++i)
            std::printf("%s\"%s\"", i ? "," : "", Escape(status.required[i]).c_str());
        std::fputc(']', stdout); std::fputc('}', stdout);
    };
    std::printf("{\"state\":\"%s\",\"reason\":\"%s\",\"sdk\":\"%s\",\"runtime_path\":\"%s\","
                "\"device\":\"%s\",\"vendor_id\":%u,\"device_id\":%u,\"driver\":\"%s\",\"driver_raw\":%u,"
                "\"feature_support\":",
        gpu::dlss::ProbeStateName(report.state), Escape(report.reason).c_str(), Escape(report.sdkVersion).c_str(),
        Escape(report.runtimePath).c_str(), Escape(report.deviceName).c_str(), report.vendorId, report.deviceId, Escape(report.driverVersionText).c_str(),
        report.driverVersion);
    printOptionalUnsigned(report.featureSupport);
    std::fputs(",\"requested_output\":[", stdout);
    std::printf("%u,%u],\"sr_implemented\":%s,\"sr_evaluated\":%s,\"capability\":{\"available\":",
        report.requestedOutputWidth, report.requestedOutputHeight, report.srImplemented ? "true" : "false", report.srEvaluated ? "true" : "false");
    printCapability(report.srAvailable); std::fputs(",\"needs_updated_driver\":", stdout); printCapability(report.needsUpdatedDriver);
    std::fputs(",\"min_driver_major\":", stdout); printCapability(report.minDriverVersionMajor);
    std::fputs(",\"min_driver_minor\":", stdout); printCapability(report.minDriverVersionMinor);
    std::fputs(",\"feature_init_result\":", stdout); printCapability(report.featureInitResult);
    std::fputs("},\"extensions\":{\"instance\":", stdout); printExtensions(report.instanceExtensions);
    std::fputs(",\"device\":", stdout); printExtensions(report.deviceExtensions);
    std::fputs("},\"fg\":{\"state\":\"not_probed\",\"implementation\":\"not_implemented\"},\"calls\":[", stdout);
    for (size_t i = 0; i < report.calls.size(); ++i)
        std::printf("%s{\"name\":\"%s\",\"raw\":%d}", i ? "," : "", Escape(report.calls[i].name).c_str(), report.calls[i].result);
    std::printf("],\"optimal\":[");
    for (size_t i = 0; i < report.optimalSettings.size(); ++i) {
        const auto& value = report.optimalSettings[i];
        std::printf("%s{\"quality\":\"%s\",\"optimal\":[%u,%u],\"min\":[%u,%u],\"max\":[%u,%u],\"sharpness\":%.7g,\"raw\":%d}",
            i ? "," : "", value.quality.c_str(), value.optimalWidth, value.optimalHeight, value.minWidth, value.minHeight,
            value.maxWidth, value.maxHeight, value.sharpness, value.result.value_or(0));
    }
    std::puts("]}");
}
}

int main(int argc, char** argv) {
    const auto dataPath = std::filesystem::temp_directory_path() / "lost-odyssey-recomp-ngX-probe";
    const char* configuredRuntime = std::getenv("LO_DLSS_RUNTIME_PATH");
    const auto runtimePath = configuredRuntime && *configuredRuntime ? std::filesystem::path(configuredRuntime)
        : std::filesystem::absolute(argc > 0 ? argv[0] : "LoNativeDlssProbe").parent_path();
    gpu::dlss::Controller controller(dataPath, runtimePath);
    auto renderInterface = CreateInterface(controller.ExtensionHooks());
    if (!renderInterface) {
        std::fputs("{\"state\":\"api_error\",\"reason\":\"Vulkan interface creation failed\",\"stage\":\"base_vulkan_interface\"}\n", stdout);
        return gpu::dlss::ProbeExitCode(gpu::dlss::ProbeState::ApiError);
    }
    auto device = renderInterface->createDevice();
    if (!device) {
        std::fputs("{\"state\":\"api_error\",\"reason\":\"Vulkan device creation failed\",\"stage\":\"base_vulkan_device\"}\n", stdout);
        return gpu::dlss::ProbeExitCode(gpu::dlss::ProbeState::ApiError);
    }
    controller.ProbeOnce(*static_cast<plume::VulkanInterface*>(renderInterface.get()), *static_cast<plume::VulkanDevice*>(device.get()));
    PrintReport(controller.Report());
    return gpu::dlss::ProbeExitCode(controller.Report());
}
