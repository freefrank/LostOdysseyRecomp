#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

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
    struct ScopeDrain {
        gpu::dlss::Controller& c;
        ~ScopeDrain() { c.ShutdownAfterGpuDrain(); }
    } drainGuard{controller};
    if (argc >= 2 && std::string_view(argv[1]) == "--production-sizing") {
        auto* vkInterface = static_cast<plume::VulkanInterface*>(renderInterface.get());
        auto* vkDevice = static_cast<plume::VulkanDevice*>(device.get());
        controller.ProbeOnce(*vkInterface, *vkDevice);
        if (controller.Report().state != gpu::dlss::ProbeState::Available) {
            std::printf("{\"status\":\"probe_unavailable\",\"state\":\"%s\",\"reason\":\"%s\"}\n",
                gpu::dlss::ProbeStateName(controller.Report().state),
                Escape(controller.Report().reason).c_str());
            return gpu::dlss::ProbeExitCode(controller.Report());
        }
        const auto sizing720 = controller.QueryOutputSizing(*vkInterface, *vkDevice, {1, 1280, 720});
        const auto sizing1440 = controller.QueryOutputSizing(*vkInterface, *vkDevice, {1, 2560, 1440});

        const auto& q720 = sizing720.modes[0];
        const auto& q1440 = sizing1440.modes[0];
        const bool ok720 = q720.state == gpu::upscaling::SizingState::Ready &&
            q720.optimal.width > 0 && q720.optimal.height > 0 &&
            q720.optimal.width < 1280 && q720.optimal.height < 720;
        const bool ok1440 = q1440.state == gpu::upscaling::SizingState::Ready &&
            q1440.optimal.width > 0 && q1440.optimal.height > 0 &&
            q1440.optimal.width < 2560 && q1440.optimal.height < 1440;

        std::printf("{\"status\":\"%s\",\"calls\":[", (ok720 && ok1440) ? "ok" : "failed");
        for (size_t i = 0; i < controller.Report().calls.size(); ++i) {
            std::printf("%s{\"name\":\"%s\",\"raw\":%d}", i ? "," : "",
                Escape(controller.Report().calls[i].name).c_str(), controller.Report().calls[i].result);
        }
        std::printf("],\"sizing720\":{\"state\":%u,\"optimal\":[%u,%u],\"min\":[%u,%u],\"max\":[%u,%u]},"
                    "\"sizing1440\":{\"state\":%u,\"optimal\":[%u,%u],\"min\":[%u,%u],\"max\":[%u,%u]}}\n",
            uint32_t(q720.state), q720.optimal.width, q720.optimal.height, q720.minimum.width, q720.minimum.height, q720.maximum.width, q720.maximum.height,
            uint32_t(q1440.state), q1440.optimal.width, q1440.optimal.height, q1440.minimum.width, q1440.minimum.height, q1440.maximum.width, q1440.maximum.height);
        return (ok720 && ok1440) ? 0 : 1;
    }
    if (argc == 4 && std::string_view(argv[1]) == "--sizing") {
        const uint32_t width = uint32_t(std::strtoul(argv[2], nullptr, 10));
        const uint32_t height = uint32_t(std::strtoul(argv[3], nullptr, 10));
        const auto sizing = controller.QueryOutputSizing(*static_cast<plume::VulkanInterface*>(renderInterface.get()),
            *static_cast<plume::VulkanDevice*>(device.get()), {1, width, height});
        std::printf("{\"sizing_key\":[%llu,%u,%u],\"revision\":%llu,\"modes\":[",
            static_cast<unsigned long long>(sizing.key.deviceEpoch), sizing.key.outputWidth, sizing.key.outputHeight,
            static_cast<unsigned long long>(sizing.revision));
        bool ready = false, unavailable = false;
        for (size_t index = 0; index < sizing.modes.size(); ++index) {
            const auto& mode = sizing.modes[index];
            ready |= mode.state == gpu::upscaling::SizingState::Ready;
            unavailable |= mode.state == gpu::upscaling::SizingState::Unavailable;
            std::printf("%s{\"state\":%u,\"optimal\":[%u,%u],\"min\":[%u,%u],\"max\":[%u,%u],\"raw\":",
                index ? "," : "", uint32_t(mode.state), mode.optimal.width, mode.optimal.height,
                mode.minimum.width, mode.minimum.height, mode.maximum.width, mode.maximum.height);
            if (mode.ngxResult) std::printf("%d", *mode.ngxResult); else std::fputs("null", stdout);
            std::fputc('}', stdout);
        }
        std::puts("]}");
        return ready ? 0 : unavailable ? 77 : 1;
    }
    controller.ProbeOnce(*static_cast<plume::VulkanInterface*>(renderInterface.get()), *static_cast<plume::VulkanDevice*>(device.get()));
    PrintReport(controller.Report());
    return gpu::dlss::ProbeExitCode(controller.Report());
}
