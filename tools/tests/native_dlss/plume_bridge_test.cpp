// Vulkan bridge fixture: no NGX SDK, game assets, queue ownership, or submissions.
#include <plume_vulkan.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    enum class QueryMode {
        Failure,
        Missing,
        Duplicate,
        DeviceMissing,
    };

    struct QueryState {
        QueryMode mode;
        unsigned instanceQueries = 0;
        unsigned deviceQueries = 0;
    };

    void Require(bool condition, const char *message) {
        if (!condition) throw std::runtime_error(message);
    }

    VkExtensionProperties Extension(const char *name) {
        VkExtensionProperties property = {};
        std::strncpy(property.extensionName, name, VK_MAX_EXTENSION_NAME_SIZE - 1);
        return property;
    }

    bool QueryInstance(void *userData, std::vector<VkExtensionProperties> &required, std::string &reason) {
        auto &state = *static_cast<QueryState *>(userData);
        ++state.instanceQueries;
        if (state.mode == QueryMode::Failure) {
            reason = "synthetic instance query failure";
            return false;
        }
        if (state.mode == QueryMode::Missing) {
            required.push_back(Extension("VK_LOST_ODYSSEY_synthetic_missing"));
            return true;
        }
        required.push_back(Extension(VK_KHR_SURFACE_EXTENSION_NAME));
        required.push_back(Extension(VK_KHR_SURFACE_EXTENSION_NAME));
        return true;
    }

    bool QueryDevice(void *userData, VkInstance, VkPhysicalDevice, std::vector<VkExtensionProperties> &required, std::string &) {
        auto &state = *static_cast<QueryState *>(userData);
        ++state.deviceQueries;
        if (state.mode == QueryMode::DeviceMissing) {
            required.push_back(Extension("VK_LOST_ODYSSEY_synthetic_device_missing"));
        }
        return true;
    }

    std::unique_ptr<plume::RenderInterface> CreateInterface(const plume::VulkanExtensionHooks &hooks) {
#if PLUME_SDL_VULKAN_ENABLED
        return plume::CreateVulkanInterface(nullptr, hooks);
#else
        return plume::CreateVulkanInterface(hooks);
#endif
    }
}

int main() {
    try {
        using namespace plume;

        VulkanExtensionHooks noHooks = {};
        auto noHookInterface = CreateInterface(noHooks);
        Require(bool(noHookInterface), "no-hooks interface fallback");
        auto *noHookBridge = static_cast<VulkanInterface *>(noHookInterface.get());
        Require(noHookBridge->getExternalExtensionStatus().state == VulkanExtensionState::NotRequested, "no-hooks instance status");
        auto noHookDevice = noHookInterface->createDevice();
        Require(bool(noHookDevice), "no-hooks device fallback");
        auto *noHookDeviceBridge = static_cast<VulkanDevice *>(noHookDevice.get());
        Require(noHookDeviceBridge->getExternalExtensionStatus().state == VulkanExtensionState::NotRequested, "no-hooks device status");

        QueryState failure{QueryMode::Failure};
        VulkanExtensionHooks failingHooks{&failure, QueryInstance, QueryDevice};
        auto failedInterface = CreateInterface(failingHooks);
        Require(bool(failedInterface), "query failure baseline interface fallback");
        Require(static_cast<VulkanInterface *>(failedInterface.get())->getExternalExtensionStatus().state == VulkanExtensionState::Disabled, "query failure status");
        auto failedDevice = failedInterface->createDevice();
        Require(bool(failedDevice), "query failure baseline device fallback");
        Require(failure.deviceQueries == 0, "disabled instance group invoked device query");

        QueryState missing{QueryMode::Missing};
        VulkanExtensionHooks missingHooks{&missing, QueryInstance, QueryDevice};
        auto missingInterface = CreateInterface(missingHooks);
        Require(bool(missingInterface), "missing group baseline interface fallback");
        Require(static_cast<VulkanInterface *>(missingInterface.get())->getExternalExtensionStatus().state == VulkanExtensionState::Disabled, "missing group status");
        auto missingDevice = missingInterface->createDevice();
        Require(bool(missingDevice), "missing group baseline device fallback");
        Require(missing.deviceQueries == 0, "missing instance group invoked device query");

        QueryState duplicate{QueryMode::Duplicate};
        VulkanExtensionHooks duplicateHooks{&duplicate, QueryInstance, QueryDevice};
        auto duplicateInterface = CreateInterface(duplicateHooks);
        Require(bool(duplicateInterface), "duplicate group interface");
        Require(static_cast<VulkanInterface *>(duplicateInterface.get())->getExternalExtensionStatus().state == VulkanExtensionState::Enabled, "duplicate group status");
        Require(duplicate.instanceQueries == 1, "duplicate group queried more than once");

        QueryState deviceMissing{QueryMode::DeviceMissing};
        VulkanExtensionHooks deviceMissingHooks{&deviceMissing, QueryInstance, QueryDevice};
        auto deviceMissingInterface = CreateInterface(deviceMissingHooks);
        Require(bool(deviceMissingInterface), "device missing interface");
        auto deviceMissingDevice = deviceMissingInterface->createDevice();
        Require(bool(deviceMissingDevice), "device missing baseline fallback");
        Require(deviceMissing.deviceQueries == 1, "device query count");
        Require(static_cast<VulkanDevice *>(deviceMissingDevice.get())->getExternalExtensionStatus().state == VulkanExtensionState::Disabled, "device missing status");

        auto queue = noHookDevice->createCommandQueue(RenderCommandListType::DIRECT);
        auto commands = queue->createCommandList();
        auto target = noHookDevice->createTexture(RenderTextureDesc::Texture2D(4, 4, 1, RenderFormat::R32G32B32A32_FLOAT, RenderTextureFlag::RENDER_TARGET));
        const RenderTexture *attachments[] = { target.get() };
        auto framebuffer = noHookDevice->createFramebuffer(RenderFramebufferDesc(attachments, 1));
        Require(bool(queue) && bool(commands) && bool(target) && bool(framebuffer), "command fixture resources");
        auto *commandBridge = static_cast<VulkanCommandList *>(commands.get());
        commands->begin();
        commands->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(target.get(), RenderTextureLayout::COLOR_WRITE));
        commands->setFramebuffer(framebuffer.get());
        commands->clearColor(0, RenderColor(0, 0, 0, 1));
        Require(commandBridge->activeRenderPass != VK_NULL_HANDLE, "fixture did not enter render pass");
        commandBridge->activeGraphicsDescriptorSets.resize(1);
        Require(commandBridge->beginExternalCommands() == commandBridge->vk, "external command handle");
        Require(commandBridge->activeRenderPass == VK_NULL_HANDLE, "external commands did not terminate render pass");
        Require(commandBridge->targetFramebuffer == nullptr && commandBridge->activeGraphicsDescriptorSets.empty(), "external commands did not invalidate state");
        commandBridge->endExternalCommands();
        commands->end();

        std::puts("PASS: Plume external extension bridge and command-state fixture");
        return 0;
    }
    catch (const std::exception &exception) {
        std::fprintf(stderr, "FAIL: %s\n", exception.what());
        return 1;
    }
}
