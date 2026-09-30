// Real Plume sampler/descriptor objects, Vulkan compute dispatch and readback.
// Two recorded command buffers intentionally reuse the same logical palette
// slot with different wrap/clamp states before either buffer is submitted.
#include <gpu/sampler_description.h>
#include <plume_render_interface.h>
#include <plume_render_interface_builders.h>
#include <plume_vulkan.h>
#include <atomic>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace plume { std::unique_ptr<RenderInterface> CreateVulkanInterface(); }
namespace {
using namespace plume;
using Palette = gpu::sampling::Palette<RenderSampler, RenderDescriptorSet, 2>;
std::atomic<unsigned> errors = 0;
void Check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
VKAPI_ATTR VkBool32 VKAPI_CALL Debug(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void*) {
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) { ++errors; std::cerr << data->pMessage << '\n'; }
    return VK_FALSE;
}
void Run(const char* shaderPath) {
    auto api = CreateVulkanInterface(); Check(bool(api), "Vulkan interface");
    auto* instance = static_cast<VulkanInterface*>(api.get());
    VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    debug.pfnUserCallback = Debug;
    VkDebugUtilsMessengerEXT messenger{};
    Check(vkCreateDebugUtilsMessengerEXT && vkCreateDebugUtilsMessengerEXT(instance->instance, &debug, nullptr, &messenger) == VK_SUCCESS,
          "validation messenger (build with Vulkan validation enabled)");
    {
        auto device = api->createDevice(); Check(bool(device), "device");
        auto* native = static_cast<VulkanDevice*>(device.get());
        VkPhysicalDeviceFeatures features{}; vkGetPhysicalDeviceFeatures(native->physicalDevice, &features);
        const auto limit = features.samplerAnisotropy ? uint32_t(native->physicalDeviceProperties.limits.maxSamplerAnisotropy) : 0;
        auto queue = device->createCommandQueue(RenderCommandListType::DIRECT); Check(bool(queue), "queue");
        auto* nativeQueue = static_cast<VulkanCommandQueue*>(queue.get());
        std::cout << "Vulkan AF device: " << device->getDescription().name << ", maximum=" << limit << '\n';
        RenderDescriptorSetBuilder dataBuilder, samplerBuilder;
        dataBuilder.begin(); dataBuilder.addTexture(0); dataBuilder.addReadWriteByteAddressBuffer(1); dataBuilder.end();
        samplerBuilder.begin(); samplerBuilder.addSampler(0,2); samplerBuilder.end();
        RenderPipelineLayoutBuilder lb; lb.begin(false,false);
        lb.addPushConstant(0,0,8,RenderShaderStageFlag::COMPUTE);
        lb.addDescriptorSet(dataBuilder); lb.addDescriptorSet(samplerBuilder); lb.end();
        auto layout = lb.create(device.get()); Check(bool(layout), "pipeline layout");
        std::ifstream input(shaderPath, std::ios::binary | std::ios::ate); Check(bool(input), "SPIR-V file");
        const size_t size = size_t(input.tellg()); Check(size && size % 4 == 0, "SPIR-V size");
        std::vector<uint32_t> code(size/4); input.seekg(0); input.read(reinterpret_cast<char*>(code.data()), size);
        auto shader = device->createShader(code.data(), size, "main", RenderShaderFormat::SPIRV);
        Check(bool(shader), "shader");
        RenderComputePipelineDesc pd; pd.pipelineLayout = layout.get(); pd.computeShader = shader.get();
        pd.threadGroupSizeX = pd.threadGroupSizeY = pd.threadGroupSizeZ = 1;
        auto pipeline = device->createComputePipeline(pd); Check(bool(pipeline), "compute pipeline");
        auto texture = device->createTexture(RenderTextureDesc::Texture2D(2,1,1,RenderFormat::R8G8B8A8_UNORM));
        auto upload = device->createBuffer(RenderBufferDesc::UploadBuffer(256));
        auto output = device->createBuffer(RenderBufferDesc::ReadbackBuffer(32,RenderBufferFlag::STORAGE));
        auto init = queue->createCommandList(); auto initFence = device->createCommandFence();
        Check(texture && upload && output && init && initFence, "resources");
        const uint8_t pixels[] = {255,0,0,255, 0,255,0,255};
        auto* mapped = upload->map(); Check(mapped != nullptr, "upload map");
        std::memcpy(mapped,pixels,sizeof(pixels)); upload->unmap();
        init->begin();
        init->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(texture.get(),RenderTextureLayout::COPY_DEST));
        init->copyTextureRegion(RenderTextureCopyLocation::Subresource(texture.get()),
            RenderTextureCopyLocation::PlacedFootprint(upload.get(),RenderFormat::R8G8B8A8_UNORM,2,1,1,64,0));
        init->barriers(RenderBarrierStage::COMPUTE,RenderTextureBarrier(texture.get(),RenderTextureLayout::SHADER_READ));
        init->end();
        const RenderCommandList* initLists[]{init.get()};
        queue->executeCommandLists(initLists,1,nullptr,0,nullptr,0,initFence.get());
        queue->waitForCommandFence(initFence.get());
        auto dataSet = dataBuilder.create(device.get()); Check(bool(dataSet), "data set");
        dataSet->setTexture(0,texture.get(),RenderTextureLayout::SHADER_READ);
        dataSet->setBuffer(1,output.get(),32);
        Palette palette;
        auto makeSampler = [&](uint64_t key) -> std::shared_ptr<RenderSampler> { return device->createSampler(gpu::sampling::Describe(key)); };
        auto makeSet = [&](const auto& handles) -> std::shared_ptr<RenderDescriptorSet> {
            auto set = samplerBuilder.create(device.get()); if (!set) return {};
            for (unsigned i=0;i<2;++i) set->setSampler(i,handles[i].get());
            return set;
        };
        Check(palette.Initialize(makeSampler,makeSet), "palette");
        std::array<std::unique_ptr<RenderCommandList>,2> lists{queue->createCommandList(),queue->createCommandList()};
        auto fence = device->createCommandFence(); Check(lists[0] && lists[1] && fence, "batch resources");
        const VkPipeline vkPipeline = static_cast<VulkanComputePipeline*>(pipeline.get())->vk;
        const VkPipelineLayout vkLayout = static_cast<VulkanPipelineLayout*>(layout.get())->vk;
        for (uint32_t requested : {0u,2u,4u,8u,16u,0u}) {
            palette.BeginDraw();
            Check(palette.Select(gpu::sampling::Recipe(0x15,true),makeSampler,makeSet).has_value(), "linear material");
            Check(palette.Reconfigure(gpu::sampling::ClampLevel(requested,limit),makeSampler,makeSet), "native AF creation");
            std::array<std::vector<Palette::Lease>,2> uses;
            for (unsigned i=0;i<2;++i) {
                palette.BeginDraw();
                const auto slot = palette.Select(gpu::sampling::Recipe(i ? (2u<<6) : 0u,false),makeSampler,makeSet);
                Check(slot && *slot == 1, "same logical slot reused");
                auto version = palette.Current(); gpu::sampling::Retain(uses[i],version);
                lists[i]->begin();
                auto commands = static_cast<VulkanCommandList*>(lists[i].get())->vk;
                vkCmdBindPipeline(commands,VK_PIPELINE_BIND_POINT_COMPUTE,vkPipeline);
                const VkDescriptorSet sets[] = {static_cast<VulkanDescriptorSet*>(dataSet.get())->vk,
                    static_cast<VulkanDescriptorSet*>(version->descriptors.get())->vk};
                vkCmdBindDescriptorSets(commands,VK_PIPELINE_BIND_POINT_COMPUTE,vkLayout,0,2,sets,0,nullptr);
                const uint32_t constants[]{*slot,i};
                vkCmdPushConstants(commands,vkLayout,VK_SHADER_STAGE_COMPUTE_BIT,0,8,constants);
                vkCmdDispatch(commands,1,1,1);
                VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
                vkCmdPipelineBarrier(commands,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
                lists[i]->end();
            }
            Check(uses[0][0]->descriptors != uses[1][0]->descriptors, "descriptor versions distinct");
            const VkCommandBuffer commands[] = {static_cast<VulkanCommandList*>(lists[0].get())->vk,
                static_cast<VulkanCommandList*>(lists[1].get())->vk};
            const auto vkFence = static_cast<VulkanCommandFence*>(fence.get())->vk;
            Check(vkResetFences(native->vk,1,&vkFence) == VK_SUCCESS,"reset fence");
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount=2;submit.pCommandBuffers=commands;
            Check(vkQueueSubmit(nativeQueue->queue->vk,1,&submit,vkFence)==VK_SUCCESS,"submit old and new tables");
            // A further CPU table publication while submitted work owns both versions.
            Check(palette.Reconfigure(gpu::sampling::ClampLevel(4,limit),makeSampler,makeSet),"publication while submitted");
            Check(vkWaitForFences(native->vk,1,&vkFence,VK_TRUE,UINT64_MAX)==VK_SUCCESS,"wait fence");
            auto* values=static_cast<float*>(output->map()); Check(values!=nullptr,"readback");
            Check(values[0]==0.f && values[1]==1.f && values[4]==1.f && values[5]==0.f,
                  "old wrap is green, replacement clamp is red");
            output->unmap(); uses[0].clear(); uses[1].clear();
        }
        Check(vkDeviceWaitIdle(native->vk)==VK_SUCCESS,"drain");
    }
    vkDestroyDebugUtilsMessengerEXT(instance->instance,messenger,nullptr);
    Check(errors==0,"Vulkan validation errors");
}
}
int main(int argc,char** argv) {
    try { Check(argc==2,"supply SPIR-V path"); Run(argv[1]); }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
    std::cout << "Vulkan AF dispatch/readback and validation passed\n";
}
