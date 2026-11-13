// Game-free Vulkan A/B: real translator + rect wrapper, FP16 blending, depth,
// clipping/perspective and pipeline statistics. Software ICD results are NOT
// hardware performance measurements. Test-only fence/query waits are explicit.
#include <gpu/shader/rect_list_hlsl.h>
#include <gpu/shader/dxc_compiler.h>
#include <gpu/shader/vertex_fetch_contract.h>
#include "motion_replay_fixture.h"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

static void Check(VkResult r, const char* what) {
    if (r != VK_SUCCESS) throw std::runtime_error(std::string(what) + ": " + std::to_string(r));
}
static void Require(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
static xenos::TranslatedShader Vertex(bool skin, bool ubo) {
    auto program = motion_fixture::Vertex(skin);
    auto out = motion_fixture::Alu(); out.exportData = 1; out.vectorDest = 0;
    out.vectorOpcode = xenos::AluVectorOpcode::Max;
    out.src1Register = out.src2Register = 1; program.Add(out);
    auto words = program.Host();
    auto translated = xenos::TranslateShader(words.data(), uint32_t(words.size()), false, ubo);
    Require(translated.errors.empty(), translated.errors.c_str());
    return translated;
}
static std::atomic<unsigned> validationErrors{0};
static VKAPI_ATTR VkBool32 VKAPI_CALL Debug(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void*) {
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        ++validationErrors; std::fprintf(stderr, "validation: %s\n", data->pMessage);
    }
    return VK_FALSE;
}

class Fixture {
public:
    static constexpr uint32_t W = 128, H = 96, Rectangles = 64, Base = 7;
    struct Buffer { VkBuffer handle{}; VkDeviceMemory memory{}; void* mapped{}; VkDeviceAddress address{}; };
    struct Image { VkImage handle{}; VkDeviceMemory memory{}; VkImageView view{}; };
    struct Result { std::vector<uint8_t> pixels; std::array<uint64_t, 4> statistics{}; double queueMs{}; };
    VkInstance instance{};
    VkDebugUtilsMessengerEXT messenger{};
    VkPhysicalDevice physical{};
    VkDevice device{};
    VkQueue queue{};
    VkCommandPool pool{};
    VkCommandBuffer commands{};
    VkFence fence{};
    VkPipelineLayout layout{};
    VkRenderPass pass{};
    VkFramebuffer framebuffer{};
    VkPipeline pipelines[3]{};
    VkShaderModule vs{}, ps{};
    VkQueryPool stats{}, timestamps{};
    VkPhysicalDeviceMemoryProperties memory{};
    VkPhysicalDeviceProperties properties{};
    uint32_t family{};
    uint32_t timestampBits{};
    Buffer vertices, shared, constants, indices[2], readback;
    Image color, depth;

    uint32_t MemoryType(uint32_t mask, VkMemoryPropertyFlags flags) {
        for (uint32_t i = 0; i < memory.memoryTypeCount; ++i)
            if ((mask & (1u << i)) && (memory.memoryTypes[i].propertyFlags & flags) == flags) return i;
        throw std::runtime_error("required memory type missing");
    }
    Buffer NewBuffer(VkDeviceSize size, VkBufferUsageFlags usage, bool addressable = false) {
        Buffer b;
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = size; info.usage = usage | (addressable ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0);
        Check(vkCreateBuffer(device, &info, nullptr, &b.handle), "create buffer");
        VkMemoryRequirements req{}; vkGetBufferMemoryRequirements(device, b.handle, &req);
        VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
        flags.flags = addressable ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT : 0;
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        alloc.pNext = addressable ? &flags : nullptr; alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = MemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        Check(vkAllocateMemory(device, &alloc, nullptr, &b.memory), "allocate buffer");
        Check(vkBindBufferMemory(device, b.handle, b.memory, 0), "bind buffer");
        Check(vkMapMemory(device, b.memory, 0, size, 0, &b.mapped), "map buffer");
        std::memset(b.mapped, 0, size);
        if (addressable) {
            VkBufferDeviceAddressInfo address{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO}; address.buffer = b.handle;
            b.address = vkGetBufferDeviceAddress(device, &address);
        }
        return b;
    }
    Image NewImage(VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect) {
        Image image;
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = VK_IMAGE_TYPE_2D; info.format = format; info.extent = {W, H, 1};
        info.mipLevels = info.arrayLayers = 1; info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL; info.usage = usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        Check(vkCreateImage(device, &info, nullptr, &image.handle), "create image");
        VkMemoryRequirements req{}; vkGetImageMemoryRequirements(device, image.handle, &req);
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = MemoryType(req.memoryTypeBits, 0);
        Check(vkAllocateMemory(device, &alloc, nullptr, &image.memory), "allocate image");
        Check(vkBindImageMemory(device, image.handle, image.memory, 0), "bind image");
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image = image.handle; view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.format = format;
        view.subresourceRange = {aspect, 0, 1, 0, 1};
        Check(vkCreateImageView(device, &view, nullptr, &image.view), "image view"); return image;
    }
    VkShaderModule Compile(const std::string& source, const char* profile) {
        const auto binary = xenos::CompileHlsl(source, "main", profile, xenos::ShaderBinaryFormat::Spirv);
        Require(binary.ok, binary.errors.c_str());
        std::vector<uint32_t> words(binary.bytecode.size() / 4);
        std::memcpy(words.data(), binary.bytecode.data(), binary.bytecode.size());
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = binary.bytecode.size(); info.pCode = words.data(); VkShaderModule module{};
        Check(vkCreateShaderModule(device, &info, nullptr, &module), "shader module"); return module;
    }
    explicit Fixture(bool validation) {
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.apiVersion = VK_API_VERSION_1_2;
        const char* layer = "VK_LAYER_KHRONOS_validation";
        const char* extension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug.pfnUserCallback = Debug;
        VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; info.pApplicationInfo = &app;
        if (validation) { info.enabledLayerCount = info.enabledExtensionCount = 1; info.ppEnabledLayerNames = &layer;
            info.ppEnabledExtensionNames = &extension; info.pNext = &debug; }
        Check(vkCreateInstance(&info, nullptr, &instance), "instance");
        if (validation) {
            const auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
            Require(create != nullptr, "debug utils unavailable"); Check(create(instance, &debug, nullptr, &messenger), "debug messenger");
        }
        uint32_t count = 0; Check(vkEnumeratePhysicalDevices(instance, &count, nullptr), "devices");
        Require(count != 0, "no Vulkan device"); std::vector<VkPhysicalDevice> devices(count);
        Check(vkEnumeratePhysicalDevices(instance, &count, devices.data()), "devices"); physical = devices[0];
        vkGetPhysicalDeviceProperties(physical, &properties); vkGetPhysicalDeviceMemoryProperties(physical, &memory);
        VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; features.pNext = &f12;
        vkGetPhysicalDeviceFeatures2(physical, &features);
        Require(f12.bufferDeviceAddress && features.features.shaderInt64, "device-address shader features unavailable");
        uint32_t queueCount = 0; vkGetPhysicalDeviceQueueFamilyProperties(physical, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(queueCount); vkGetPhysicalDeviceQueueFamilyProperties(physical, &queueCount, families.data());
        family = queueCount;
        for (uint32_t i = 0; i < queueCount; ++i) if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { family = i; break; }
        Require(family < queueCount, "no graphics queue"); timestampBits = families[family].timestampValidBits;
        float priority = 1; VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        q.queueFamilyIndex = family; q.queueCount = 1; q.pQueuePriorities = &priority;
        VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; di.queueCreateInfoCount = 1; di.pQueueCreateInfos = &q;
        VkPhysicalDeviceFeatures enable{}; enable.shaderInt64 = VK_TRUE; enable.pipelineStatisticsQuery = features.features.pipelineStatisticsQuery;
        VkPhysicalDeviceVulkan12Features enable12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES}; enable12.bufferDeviceAddress = VK_TRUE;
        di.pNext = &enable12; di.pEnabledFeatures = &enable; Check(vkCreateDevice(physical, &di, nullptr, &device), "device");
        vkGetDeviceQueue(device, family, 0, &queue);
        VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; cp.queueFamilyIndex = family; cp.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        Check(vkCreateCommandPool(device, &cp, nullptr, &pool), "command pool");
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; ca.commandPool = pool; ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ca.commandBufferCount = 1;
        Check(vkAllocateCommandBuffers(device, &ca, &commands), "commands");
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; Check(vkCreateFence(device, &fi, nullptr, &fence), "fence");
        VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT, 0, 24};
        VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; li.pushConstantRangeCount = 1; li.pPushConstantRanges = &range;
        Check(vkCreatePipelineLayout(device, &li, nullptr, &layout), "layout");
        vertices = NewBuffer((Base + 3 * Rectangles) * 16, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
        shared = NewBuffer(1104, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
        constants = NewBuffer(4096, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
        std::memcpy(static_cast<uint8_t*>(shared.mapped) + xenos::VertexArenaAddressOffset, &vertices.address, 8);
        readback = NewBuffer(W * H * 12, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        for (unsigned reuse = 0; reuse < 2; ++reuse) {
            std::vector<uint32_t> expanded, scratch; bool indexed = false;
            xenos::rect_list::ExpandIndices(expanded, scratch, indexed, Rectangles * 3, Base, reuse != 0);
            indices[reuse] = NewBuffer(expanded.size() * 4, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
            std::memcpy(indices[reuse].mapped, expanded.data(), expanded.size() * 4);
        }
        color = NewImage(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        depth = NewImage(VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
        std::array<VkAttachmentDescription, 2> attachments{};
        for (auto& a : attachments) { a.samples = VK_SAMPLE_COUNT_1_BIT; a.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            a.storeOp = VK_ATTACHMENT_STORE_OP_STORE; a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; a.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; }
        attachments[0].format = VK_FORMAT_R16G16B16A16_SFLOAT; attachments[1].format = VK_FORMAT_D32_SFLOAT;
        VkAttachmentReference cr{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}, dr{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{}; sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; sub.colorAttachmentCount = 1; sub.pColorAttachments = &cr; sub.pDepthStencilAttachment = &dr;
        VkSubpassDependency dependency{}; dependency.srcSubpass = 0; dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; dependency.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; rp.attachmentCount = 2; rp.pAttachments = attachments.data();
        rp.subpassCount = 1; rp.pSubpasses = &sub; rp.dependencyCount = 1; rp.pDependencies = &dependency;
        Check(vkCreateRenderPass(device, &rp, nullptr, &pass), "render pass");
        VkImageView views[] = {color.view, depth.view}; VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fb.renderPass = pass; fb.attachmentCount = 2; fb.pAttachments = views; fb.width = W; fb.height = H; fb.layers = 1;
        Check(vkCreateFramebuffer(device, &fb, nullptr, &framebuffer), "framebuffer");
        const auto source = xenos::rect_list::Vertex(Vertex(false, false));
        Require(!source.empty(), "rect wrapper");
        vs = Compile(source, "vs_6_0");
        ps = Compile(R"HLSL(
void main(float4 p : SV_Position, float4 uv : TEXCOORD0, out float4 color : SV_Target0) {
    float edge = abs(ddx(uv.x)) + abs(ddy(uv.y));
    float a = saturate(0.37 + uv.y * 0.16);
    color = float4(0.8 + uv.x * 0.3, 0.2 + edge, 0.07 + uv.y * 0.03, a);
})HLSL", "ps_6_0");
        VkPipelineShaderStageCreateInfo stages[2]{};
        for (auto& s : stages) { s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; s.pName = "main"; }
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vs;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = ps;
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport vp{0, 0, float(W), float(H), 0, 1}; VkRect2D sc{{0, 0}, {W, H}};
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; viewport.viewportCount = viewport.scissorCount = 1; viewport.pViewports = &vp; viewport.pScissors = &sc;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; raster.polygonMode = VK_POLYGON_MODE_FILL; raster.lineWidth = 1; raster.cullMode = VK_CULL_MODE_NONE;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO}; ds.depthTestEnable = VK_TRUE; ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        VkPipelineColorBlendAttachmentState blend{}; blend.colorWriteMask = 15; blend.colorBlendOp = blend.alphaBlendOp = VK_BLEND_OP_ADD;
        VkPipelineColorBlendStateCreateInfo bs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; bs.attachmentCount = 1; bs.pAttachments = &blend;
        VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; gp.stageCount = 2; gp.pStages = stages; gp.pVertexInputState = &vi; gp.pInputAssemblyState = &ia;
        gp.pViewportState = &viewport; gp.pRasterizationState = &raster; gp.pMultisampleState = &ms; gp.pDepthStencilState = &ds; gp.pColorBlendState = &bs; gp.layout = layout; gp.renderPass = pass;
        for (unsigned mode = 0; mode < 3; ++mode) {
            blend.blendEnable = mode != 0; blend.srcColorBlendFactor = mode == 1 ? VK_BLEND_FACTOR_SRC_ALPHA : VK_BLEND_FACTOR_ONE;
            blend.dstColorBlendFactor = mode == 1 ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ONE;
            blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            ds.depthWriteEnable = mode == 0;
            Check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gp, nullptr, &pipelines[mode]), "pipeline");
        }
        VkQueryPoolCreateInfo qi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO}; qi.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS; qi.queryCount = 1;
        qi.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT | VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT | VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT;
        if (enable.pipelineStatisticsQuery) Check(vkCreateQueryPool(device, &qi, nullptr, &stats), "statistics query");
        qi.queryType = VK_QUERY_TYPE_TIMESTAMP; qi.queryCount = 2; qi.pipelineStatistics = 0;
        if (timestampBits) Check(vkCreateQueryPool(device, &qi, nullptr, &timestamps), "timestamp query");
        std::printf("device=%s type=%u timestamp_period_ns=%.6f statistics=%d\n", properties.deviceName, properties.deviceType, properties.limits.timestampPeriod, stats != VK_NULL_HANDLE);
    }
    void Scene(unsigned scenario) {
        motion_fixture::Shared state; state.ndcScale[2] = 1; state.ndcOffset[2] = 0;
        std::memcpy(shared.mapped, &state, sizeof(state));
        auto* v = static_cast<float*>(vertices.mapped);
        for (unsigned r = 0; r < Rectangles; ++r) {
            const float x = scenario & 4 ? -1.1f + float(r % 8) * .31f : -.7f + float(r % 5) * .035f;
            const float y = scenario & 4 ? -.95f + float(r / 8) * .25f : -.65f + float(r % 7) * .025f;
            const float width = scenario & 4 ? .32f : 1.4f, height = scenario & 4 ? .3f : 1.3f;
            const float points[3][2] = {{x, y}, {x + width, y}, {x, y + height}};
            for (unsigned j = 0; j < 3; ++j) {
                const unsigned c = (j + scenario % 3) % 3;
                const float w = scenario & 8 ? .7f + .4f * j : 1.0f;
                const size_t at = 4 * (Base + 3 * r + j);
                v[at] = points[c][0] * w * (scenario & 16 ? -1.f : 1.f);
                v[at + 1] = points[c][1] * w; v[at + 2] = (.15f + .01f * (r % 50)) * w; v[at + 3] = w;
            }
        }
    }
    Result Run(bool reuse, unsigned mode) {
        Check(vkResetFences(device, 1, &fence), "reset fence"); Check(vkResetCommandBuffer(commands, 0), "reset commands");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; Check(vkBeginCommandBuffer(commands, &begin), "begin");
        if (stats) { vkCmdResetQueryPool(commands, stats, 0, 1); vkCmdBeginQuery(commands, stats, 0, 0); }
        if (timestamps) { vkCmdResetQueryPool(commands, timestamps, 0, 2); vkCmdWriteTimestamp(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestamps, 0); }
        VkClearValue clears[2]{}; clears[0].color = {{.125f, .25f, .5f, .75f}}; clears[1].depthStencil.depth = 1;
        VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; rp.renderPass = pass; rp.framebuffer = framebuffer;
        rp.renderArea.extent = {W, H}; rp.clearValueCount = 2; rp.pClearValues = clears;
        vkCmdBeginRenderPass(commands, &rp, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[mode]);
        const uint64_t addresses[] = {constants.address, shared.address, constants.address};
        vkCmdPushConstants(commands, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(addresses), addresses);
        vkCmdBindIndexBuffer(commands, indices[reuse ? 1 : 0].handle, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commands, Rectangles * 6, 1, 0, 0, 0); vkCmdEndRenderPass(commands);
        if (stats) vkCmdEndQuery(commands, stats, 0);
        if (timestamps) vkCmdWriteTimestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestamps, 1);
        VkBufferImageCopy copy{}; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}; copy.imageExtent = {W, H, 1};
        vkCmdCopyImageToBuffer(commands, color.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.handle, 1, &copy);
        copy.bufferOffset = W * H * 8; copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        vkCmdCopyImageToBuffer(commands, depth.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.handle, 1, &copy);
        VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER}; barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; barrier.buffer = readback.handle; barrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
        Check(vkEndCommandBuffer(commands), "end");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &commands;
        Check(vkQueueSubmit(queue, 1, &submit, fence), "submit");
        Check(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), "test-only fence wait");
        Result result; result.pixels.resize(W * H * 12); std::memcpy(result.pixels.data(), readback.mapped, result.pixels.size());
        if (stats) Check(vkGetQueryPoolResults(device, stats, 0, 1, sizeof(result.statistics), result.statistics.data(), sizeof(result.statistics), VK_QUERY_RESULT_64_BIT), "completed stats");
        if (timestamps) {
            uint64_t values[2]{}; Check(vkGetQueryPoolResults(device, timestamps, 0, 2, sizeof(values), values, 8, VK_QUERY_RESULT_64_BIT), "completed timestamps");
            const auto mask = timestampBits == 64 ? UINT64_MAX : (uint64_t(1) << timestampBits) - 1;
            result.queueMs = double((values[1] - values[0]) & mask) * properties.limits.timestampPeriod / 1e6;
        }
        return result;
    }
    ~Fixture() {
        if (device) {
            vkDeviceWaitIdle(device);
            if (stats) vkDestroyQueryPool(device, stats, nullptr); if (timestamps) vkDestroyQueryPool(device, timestamps, nullptr);
            for (auto p : pipelines) if (p) vkDestroyPipeline(device, p, nullptr);
            if (vs) vkDestroyShaderModule(device, vs, nullptr); if (ps) vkDestroyShaderModule(device, ps, nullptr);
            if (framebuffer) vkDestroyFramebuffer(device, framebuffer, nullptr); if (pass) vkDestroyRenderPass(device, pass, nullptr);
            for (auto* i : {&color, &depth}) { if (i->view) vkDestroyImageView(device, i->view, nullptr); if (i->handle) vkDestroyImage(device, i->handle, nullptr); if (i->memory) vkFreeMemory(device, i->memory, nullptr); }
            for (auto* b : {&vertices, &shared, &constants, &indices[0], &indices[1], &readback}) {
                if (b->mapped) vkUnmapMemory(device, b->memory); if (b->handle) vkDestroyBuffer(device, b->handle, nullptr); if (b->memory) vkFreeMemory(device, b->memory, nullptr);
            }
            if (layout) vkDestroyPipelineLayout(device, layout, nullptr); if (fence) vkDestroyFence(device, fence, nullptr); if (pool) vkDestroyCommandPool(device, pool, nullptr);
            vkDestroyDevice(device, nullptr);
        }
        if (messenger) reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"))(instance, messenger, nullptr);
        if (instance) vkDestroyInstance(instance, nullptr);
    }
};
int main(int argc, char** argv) {
    try {
        Require(xenos::DxcAvailable(), "DXC unavailable (set LO_DXC_PATH)");
        for (bool ubo : {false, true}) for (bool skin : {false, true}) {
            const auto source = xenos::rect_list::Vertex(Vertex(skin, ubo));
            for (auto format : {xenos::ShaderBinaryFormat::Spirv, xenos::ShaderBinaryFormat::Dxil}) {
                const auto binary = xenos::CompileHlsl(source, "main", "vs_6_0", format);
                Require(binary.ok, binary.errors.c_str());
            }
        }
        std::puts("rect shader variants: 8 SPIR-V/DXIL pointer/UBO/static/relative compiles passed");
        bool validation = false, benchmark = false;
        for (int i = 1; i < argc; ++i) {
            if (std::strcmp(argv[i], "--validation") == 0) validation = true;
            else if (std::strcmp(argv[i], "--benchmark") == 0) benchmark = true;
            else throw std::runtime_error("usage: LoRectListGpuTest [--validation] [--benchmark]");
        }
        uint64_t compared = 0, changed = 0, vertexBefore = 0, vertexAfter = 0;
        {
            Fixture f(validation);
            for (unsigned scenario = 0; scenario < 32; ++scenario) for (unsigned mode = 0; mode < 3; ++mode) {
                f.Scene(scenario);
                // Alternating order avoids making one arm exclusively first-use.
                Fixture::Result a, b;
                if (scenario & 1) { b = f.Run(true, mode); a = f.Run(false, mode); }
                else { a = f.Run(false, mode); b = f.Run(true, mode); }
                Require(a.pixels == b.pixels, "FP16 color/depth A/B mismatch");
                if (f.stats) {
                    Require(a.statistics[0] == b.statistics[0] && a.statistics[1] == b.statistics[1], "same assembled vertices and primitives");
                    Require(a.statistics[1] == Fixture::Rectangles * 2, "all triangles emitted");
                    vertexBefore += a.statistics[2]; vertexAfter += b.statistics[2];
                }
                compared += a.pixels.size();
                // A blank/stale image cannot satisfy this witness and the scene/mode variation.
                f.Scene(scenario ^ 16); const auto different = f.Run(false, mode);
                changed += different.pixels != a.pixels;
                if (scenario == 0) std::printf("mode=%u IA_vertices=%llu IA_primitives=%llu VS=%llu->%llu FS=%llu->%llu\n", mode,
                    (unsigned long long)a.statistics[0], (unsigned long long)a.statistics[1], (unsigned long long)a.statistics[2], (unsigned long long)b.statistics[2],
                    (unsigned long long)a.statistics[3], (unsigned long long)b.statistics[3]);
            }
            Require(changed > 80, "nonempty scene-dependent readback witness");
            if (benchmark && f.timestamps) {
                f.Scene(8); // Identical overlapping perspective rectangles.
                for (unsigned mode = 0; mode < 3; ++mode) {
                    for (unsigned warm = 0; warm < 8; ++warm) { f.Run(false, mode); f.Run(true, mode); }
                    std::vector<double> times[2];
                    for (unsigned round = 0; round < 15; ++round) {
                        // ABBA: identical scene, shader, resources, state and warm caches.
                        for (bool reuse : {false, true, true, false}) times[reuse].push_back(f.Run(reuse, mode).queueMs);
                    }
                    for (unsigned reuse = 0; reuse < 2; ++reuse) {
                        auto& t = times[reuse]; std::sort(t.begin(), t.end());
                        const auto percentile = [&](double p) { return t[size_t(std::ceil(p * t.size())) - 1]; };
                        std::printf("synthetic_benchmark mode=%u reuse=%u samples=%zu median_ms=%.6f p95_ms=%.6f p99_ms=%.6f scope=completed_queue_interval_includes_clear_excludes_readback host_waits=test_only\n",
                            mode, reuse, t.size(), (t[t.size()/2-1] + t[t.size()/2]) * .5, percentile(.95), percentile(.99));
                    }
                }
            }
        }
        Require(validationErrors == 0, "Vulkan validation errors");
        std::printf("PASS pairs=96 compared_bytes=%llu nonempty_witnesses=%llu VS_total=%llu->%llu validation_errors=%u\n", (unsigned long long)compared,
            (unsigned long long)changed, (unsigned long long)vertexBefore, (unsigned long long)vertexAfter, validationErrors.load());
        return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
