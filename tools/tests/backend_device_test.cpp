// No HWND, SDL, guest, shaders, command execution or presentation.
#include "gpu/backend_device.h"
#include <cassert>
#include <cstdio>
using namespace gpu::backend;
namespace plume {
std::unique_ptr<RenderInterface> CreateD3D12Interface();
std::unique_ptr<RenderInterface> CreateVulkanInterface();
}
int main() {
    for (const auto backend : {Backend::D3D12, Backend::Vulkan}) {
        auto api = backend == Backend::D3D12 ? plume::CreateD3D12Interface() : plume::CreateVulkanInterface();
        auto device = api ? api->createDevice() : nullptr;
        const auto caps = Inspect(backend, device.get());
        const auto missing = Missing(backend, caps);
        std::printf("%s device=%s api=%u sm=%u tier=%u gs=%d bda=%d int64=%d scalar=%d sets=%u samplers=%u images=%u push=%u verdict=%s\n",
            Name(backend), device ? device->getDescription().name.c_str() : "none", caps.apiVersion, caps.shaderModel, caps.bindingTier,
            caps.geometryShader, caps.bufferDeviceAddress, caps.shaderInt64, caps.scalarBlockLayout,
            caps.boundSets, caps.samplers, caps.sampledImages, caps.pushConstants, missing.empty() ? "ready" : missing.c_str());
        assert(missing.empty());
        auto queue = device->createCommandQueue(plume::RenderCommandListType::DIRECT); assert(queue);
        auto list = queue->createCommandList(); assert(list);
        auto fence = device->createCommandFence(); assert(fence);
        auto semaphore = device->createCommandSemaphore(); assert(semaphore);
        auto buffer = device->createBuffer(plume::RenderBufferDesc::UploadBuffer(4096)); assert(buffer);
        auto* mapped = buffer->map(); assert(mapped); buffer->unmap();
        // Scope destroys buffer/sync/list/queue before device, then interface.
        // Nothing was submitted. The next API starts with no owned resources.
    }
    std::puts("PASS: native capability/factory/lifetime probe; no windows or GPU commands");
}
