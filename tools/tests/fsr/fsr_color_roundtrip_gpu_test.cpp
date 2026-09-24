// Conversion-only probe of the shipped GLSL binaries. No temporal SDK dispatch.
#include <plume_vulkan.h>
#include "fsr_prepare_spv.h"
#include "fsr_present_spv.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace plume { std::unique_ptr<RenderInterface> CreateVulkanInterface(); }

namespace {
using namespace plume;
constexpr uint32_t kWidth = 256, kHeight = 8, kStorageWidth = 264, kStorageHeight = 12;
constexpr uint32_t kOriginX = 3, kOriginY = 2, kUploadStride = 1280;
constexpr uint32_t kPixels = kWidth * kHeight;
static_assert(kStorageWidth * 4 <= kUploadStride && kOriginX + kWidth <= kStorageWidth &&
    kOriginY + kHeight <= kStorageHeight);

void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

// Independent IEEE 754 round-to-nearest-even FP32 -> FP16 reference, including
// the dark subnormal range reached by (1/255)^2.2.
uint16_t HalfBits(float value) {
    const uint32_t bits = std::bit_cast<uint32_t>(value);
    const uint32_t sign = (bits >> 16) & 0x8000u;
    int32_t exponent = int32_t((bits >> 23) & 255u) - 127 + 15;
    uint32_t mantissa = bits & 0x7fffffu;
    if (exponent >= 31) return uint16_t(sign | 0x7c00u);
    if (exponent <= 0) {
        if (exponent < -10) return uint16_t(sign);
        mantissa |= 0x800000u;
        const uint32_t shift = uint32_t(14 - exponent);
        uint32_t result = mantissa >> shift;
        const uint32_t remainder = mantissa & ((1u << shift) - 1u);
        const uint32_t halfway = 1u << (shift - 1u);
        if (remainder > halfway || (remainder == halfway && (result & 1u))) ++result;
        return uint16_t(sign | result);
    }
    uint32_t result = (uint32_t(exponent) << 10) | (mantissa >> 13);
    const uint32_t remainder = mantissa & 0x1fffu;
    if (remainder > 0x1000u || (remainder == 0x1000u && (result & 1u))) ++result;
    return uint16_t(sign | result);
}

float FromHalf(uint16_t value) {
    const uint32_t sign = uint32_t(value & 0x8000u) << 16;
    const uint32_t mantissa = value & 1023u;
    const uint32_t exponent = (value >> 10) & 31u;
    if (exponent == 0 && mantissa == 0) return std::bit_cast<float>(sign);
    if (exponent == 0) return std::ldexp(float(mantissa), -24);
    return std::bit_cast<float>(sign | ((exponent + 112) << 23) | (mantissa << 13));
}

std::array<uint8_t, 4> Pattern(uint32_t x, uint32_t y) {
    switch (y) {
    case 0: return {uint8_t(x), uint8_t(x), uint8_t(x), uint8_t((x * 47 + 31) & 255)};
    case 1: return {uint8_t(x % 32), uint8_t((x * 3) % 32), uint8_t((x * 5) % 32), uint8_t(x)};
    case 2: return {uint8_t(224 + (x % 32)), uint8_t(255 - (x % 32)),
        uint8_t(224 + ((x * 7) % 32)), uint8_t(255 - x)};
    default: return {uint8_t((x * 73 + y * 19) & 255), uint8_t((x * 37 + y * 53) & 255),
        uint8_t((x * 11 + y * 103) & 255), uint8_t((x * 29 + y * 17) & 255)};
    }
}

void ImageBarrier(VkCommandBuffer command, VkImage image, VkImageLayout before, VkImageLayout after) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = before == VK_IMAGE_LAYOUT_UNDEFINED ? 0 :
        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = before; barrier.newLayout = after;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image; barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
}

struct PrepareParams {
    int32_t colorX, colorY, depthX, depthY, width, height;
    float depthScale, depthBias;
    int32_t maskX, maskY;
    uint32_t maskEnabled;
    float reactiveMax;
};
struct PresentParams { int32_t width, height, renderWidth, renderHeight, colorX, colorY; };
static_assert(sizeof(PrepareParams) == 48 && sizeof(PresentParams) == 24);

struct Fixture {
    std::unique_ptr<RenderInterface> api;
    std::unique_ptr<RenderDevice> device;
    std::unique_ptr<RenderCommandQueue> queue;
    std::unique_ptr<RenderCommandList> commands;
    std::unique_ptr<RenderCommandFence> fence;
    std::unique_ptr<RenderTexture> source, linear, canonical, encoded;
    std::unique_ptr<RenderBuffer> upload, linearReadback, encodedReadback;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout prepareSetLayout = VK_NULL_HANDLE, presentSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout prepareLayout = VK_NULL_HANDLE, presentLayout = VK_NULL_HANDLE;
    VkPipeline preparePipeline = VK_NULL_HANDLE, presentPipeline = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet prepareSet = VK_NULL_HANDLE, presentSet = VK_NULL_HANDLE;
    std::vector<uint8_t> sourceBytes;

    VulkanDevice& Device() { return static_cast<VulkanDevice&>(*device); }
    VulkanTexture& Native(RenderTexture& texture) { return static_cast<VulkanTexture&>(texture); }
    VkDevice Vk() { return Device().vk; }

    Fixture() {
        api = CreateVulkanInterface(); Check(bool(api), "Vulkan interface");
        device = api->createDevice(); Check(bool(device), "Vulkan device");
        queue = device->createCommandQueue(RenderCommandListType::DIRECT);
        commands = queue ? queue->createCommandList() : nullptr;
        fence = device->createCommandFence(); Check(commands && fence, "queue, commands and fence");
        source = device->createTexture(RenderTextureDesc::Texture2D(kStorageWidth, kStorageHeight, 1,
            RenderFormat::R8G8B8A8_UNORM));
        linear = device->createTexture(RenderTextureDesc::Texture2D(kWidth, kHeight, 1,
            RenderFormat::R16G16B16A16_FLOAT, RenderTextureFlag::STORAGE));
        canonical = device->createTexture(RenderTextureDesc::Texture2D(kWidth, kHeight, 1,
            RenderFormat::R32_FLOAT, RenderTextureFlag::STORAGE));
        encoded = device->createTexture(RenderTextureDesc::Texture2D(kWidth, kHeight, 1,
            RenderFormat::R8G8B8A8_UNORM, RenderTextureFlag::STORAGE));
        upload = device->createBuffer(RenderBufferDesc::UploadBuffer(kUploadStride * kStorageHeight));
        linearReadback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(kPixels * 8));
        encodedReadback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(kPixels * 4));
        Check(source && linear && canonical && encoded && upload && linearReadback && encodedReadback,
            "conversion-only resources");
        SetupPipelines();
        sourceBytes.assign(kStorageWidth * kStorageHeight * 4, 0xd3);
        for (uint32_t y = 0; y < kHeight; ++y) for (uint32_t x = 0; x < kWidth; ++x) {
            const auto pixel = Pattern(x, y);
            std::copy(pixel.begin(), pixel.end(), sourceBytes.begin() +
                (size_t(y + kOriginY) * kStorageWidth + x + kOriginX) * 4);
        }
        auto* mapped = static_cast<uint8_t*>(upload->map()); Check(mapped != nullptr, "upload map");
        std::memset(mapped, 0xd3, kUploadStride * kStorageHeight);
        for (uint32_t y = 0; y < kStorageHeight; ++y)
            std::memcpy(mapped + y * kUploadStride, sourceBytes.data() + size_t(y) * kStorageWidth * 4,
                kStorageWidth * 4);
        upload->unmap();
    }

    ~Fixture() {
        if (!device || !Device().vk) return;
        vkDeviceWaitIdle(Vk());
        if (preparePipeline) vkDestroyPipeline(Vk(), preparePipeline, nullptr);
        if (presentPipeline) vkDestroyPipeline(Vk(), presentPipeline, nullptr);
        if (prepareLayout) vkDestroyPipelineLayout(Vk(), prepareLayout, nullptr);
        if (presentLayout) vkDestroyPipelineLayout(Vk(), presentLayout, nullptr);
        if (prepareSetLayout) vkDestroyDescriptorSetLayout(Vk(), prepareSetLayout, nullptr);
        if (presentSetLayout) vkDestroyDescriptorSetLayout(Vk(), presentSetLayout, nullptr);
        if (pool) vkDestroyDescriptorPool(Vk(), pool, nullptr);
        if (sampler) vkDestroySampler(Vk(), sampler, nullptr);
    }

    VkDescriptorSetLayout Layout(uint32_t sampled, uint32_t storage) {
        std::array<VkDescriptorSetLayoutBinding, 6> bindings{};
        for (uint32_t i = 0; i < sampled + storage; ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorType = i < sampled ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER :
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            bindings[i].descriptorCount = 1; bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.bindingCount = sampled + storage; info.pBindings = bindings.data();
        VkDescriptorSetLayout result = VK_NULL_HANDLE;
        Check(vkCreateDescriptorSetLayout(Vk(), &info, nullptr, &result) == VK_SUCCESS, "descriptor layout");
        return result;
    }

    VkPipelineLayout PipelineLayout(VkDescriptorSetLayout set, uint32_t bytes) {
        VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, bytes};
        VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        info.setLayoutCount = 1; info.pSetLayouts = &set;
        info.pushConstantRangeCount = 1; info.pPushConstantRanges = &push;
        VkPipelineLayout result = VK_NULL_HANDLE;
        Check(vkCreatePipelineLayout(Vk(), &info, nullptr, &result) == VK_SUCCESS, "pipeline layout");
        return result;
    }

    VkPipeline Pipeline(VkPipelineLayout layout, const uint32_t* words, size_t bytes) {
        VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        moduleInfo.codeSize = bytes; moduleInfo.pCode = words;
        VkShaderModule module = VK_NULL_HANDLE;
        Check(vkCreateShaderModule(Vk(), &moduleInfo, nullptr, &module) == VK_SUCCESS, "production SPIR-V module");
        VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; info.stage.module = module;
        info.stage.pName = "main"; info.layout = layout;
        VkPipeline result = VK_NULL_HANDLE;
        const auto code = vkCreateComputePipelines(Vk(), VK_NULL_HANDLE, 1, &info, nullptr, &result);
        vkDestroyShaderModule(Vk(), module, nullptr);
        Check(code == VK_SUCCESS, "production compute pipeline");
        return result;
    }

    void SetupPipelines() {
        VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        info.magFilter = info.minFilter = VK_FILTER_NEAREST;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        info.addressModeU = info.addressModeV = info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        Check(vkCreateSampler(Vk(), &info, nullptr, &sampler) == VK_SUCCESS, "sampler");
        prepareSetLayout = Layout(3, 3); presentSetLayout = Layout(2, 1);
        prepareLayout = PipelineLayout(prepareSetLayout, sizeof(PrepareParams));
        presentLayout = PipelineLayout(presentSetLayout, sizeof(PresentParams));
        preparePipeline = Pipeline(prepareLayout, lo_fsr_prepare_spv, sizeof(lo_fsr_prepare_spv));
        presentPipeline = Pipeline(presentLayout, lo_fsr_present_spv, sizeof(lo_fsr_present_spv));
        VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4}};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = 2; poolInfo.poolSizeCount = 2; poolInfo.pPoolSizes = sizes;
        Check(vkCreateDescriptorPool(Vk(), &poolInfo, nullptr, &pool) == VK_SUCCESS, "descriptor pool");
        VkDescriptorSetLayout layouts[] = {prepareSetLayout, presentSetLayout};
        VkDescriptorSet sets[2]{};
        VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        alloc.descriptorPool = pool; alloc.descriptorSetCount = 2; alloc.pSetLayouts = layouts;
        Check(vkAllocateDescriptorSets(Vk(), &alloc, sets) == VK_SUCCESS, "descriptor sets");
        prepareSet = sets[0]; presentSet = sets[1];
        VkDescriptorImageInfo prepareImages[] = {
            {sampler, Native(*source).imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {sampler, Native(*source).imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {sampler, Native(*source).imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {VK_NULL_HANDLE, Native(*linear).imageView, VK_IMAGE_LAYOUT_GENERAL},
            {VK_NULL_HANDLE, Native(*canonical).imageView, VK_IMAGE_LAYOUT_GENERAL},
            {VK_NULL_HANDLE, Native(*canonical).imageView, VK_IMAGE_LAYOUT_GENERAL}};
        VkDescriptorImageInfo presentImages[] = {
            {sampler, Native(*linear).imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {sampler, Native(*source).imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {VK_NULL_HANDLE, Native(*encoded).imageView, VK_IMAGE_LAYOUT_GENERAL}};
        VkWriteDescriptorSet writes[9]{};
        for (uint32_t i = 0; i < 9; ++i) {
            auto& w = writes[i]; w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = i < 6 ? prepareSet : presentSet;
            w.dstBinding = i < 6 ? i : i - 6;
            w.descriptorType = (i < 3 || i == 6 || i == 7) ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER :
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            w.descriptorCount = 1; w.pImageInfo = i < 6 ? &prepareImages[i] : &presentImages[i - 6];
        }
        vkUpdateDescriptorSets(Vk(), 9, writes, 0, nullptr);
    }

    void Run() {
        commands->begin();
        commands->barriers(RenderBarrierStage::COPY,
            RenderTextureBarrier(source.get(), RenderTextureLayout::COPY_DEST));
        commands->copyTextureRegion(RenderTextureCopyLocation::Subresource(source.get()),
            RenderTextureCopyLocation::PlacedFootprint(upload.get(), RenderFormat::R8G8B8A8_UNORM,
                kStorageWidth, kStorageHeight, 1, kUploadStride / 4));
        commands->barriers(RenderBarrierStage::COMPUTE,
            RenderTextureBarrier(source.get(), RenderTextureLayout::SHADER_READ));
        auto& list = static_cast<VulkanCommandList&>(*commands);
        const VkCommandBuffer command = list.beginExternalCommands(); Check(command != VK_NULL_HANDLE, "external command list");
        ImageBarrier(command, Native(*linear).vk, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        ImageBarrier(command, Native(*canonical).vk, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        ImageBarrier(command, Native(*encoded).vk, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, preparePipeline);
        vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, prepareLayout, 0, 1, &prepareSet, 0, nullptr);
        const PrepareParams prep{int32_t(kOriginX), int32_t(kOriginY), int32_t(kOriginX), int32_t(kOriginY),
            int32_t(kWidth), int32_t(kHeight), 1.0f, 0.0f, 0, 0, 0, 0.9f};
        vkCmdPushConstants(command, prepareLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(prep), &prep);
        vkCmdDispatch(command, (kWidth + 7) / 8, (kHeight + 7) / 8, 1);
        ImageBarrier(command, Native(*linear).vk, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, presentPipeline);
        vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, presentLayout, 0, 1, &presentSet, 0, nullptr);
        const PresentParams present{int32_t(kWidth), int32_t(kHeight), int32_t(kWidth), int32_t(kHeight),
            int32_t(kOriginX), int32_t(kOriginY)};
        vkCmdPushConstants(command, presentLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(present), &present);
        vkCmdDispatch(command, (kWidth + 7) / 8, (kHeight + 7) / 8, 1);
        ImageBarrier(command, Native(*linear).vk, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        ImageBarrier(command, Native(*encoded).vk, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        auto copy = [&](RenderTexture& texture, RenderBuffer& buffer, uint32_t width, uint32_t height) {
            VkBufferImageCopy region{};
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.imageExtent = {width, height, 1};
            vkCmdCopyImageToBuffer(command, Native(texture).vk, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                static_cast<VulkanBuffer&>(buffer).vk, 1, &region);
        };
        copy(*linear, *linearReadback, kWidth, kHeight);
        copy(*encoded, *encodedReadback, kWidth, kHeight);
        list.endExternalCommands(); commands->end();
        const RenderCommandList* lists[] = {commands.get()};
        queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence.get());
        const VkFence checked = static_cast<VulkanCommandFence&>(*fence).vk;
        Check(vkWaitForFences(Vk(), 1, &checked, VK_TRUE, UINT64_MAX) == VK_SUCCESS &&
            vkGetFenceStatus(Vk(), checked) == VK_SUCCESS, "checked submission fence completion");
    }

    template<typename T> static std::vector<T> Download(RenderBuffer& buffer, size_t count) {
        auto* mapped = buffer.map(); Check(mapped != nullptr, "readback map");
        std::vector<T> copy(count);
        std::memcpy(copy.data(), mapped, count * sizeof(T));
        buffer.unmap(); return copy;
    }
};

void WriteBinary(const std::filesystem::path& path, const void* data, size_t bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(static_cast<const char*>(data), std::streamsize(bytes));
    Check(bool(file), "write bounded evidence");
}

void Compare(Fixture& f, const std::filesystem::path& dir) {
    const auto linear = Fixture::Download<uint16_t>(*f.linearReadback, kPixels * 4);
    const auto encoded = Fixture::Download<uint8_t>(*f.encodedReadback, kPixels * 4);
    uint32_t halfExact = 0, halfWithinOne = 0, outputExact = 0, outputWithinOne = 0, alphaExact = 0;
    uint32_t maxHalfUlp = 0, maxOutputCodeError = 0, belowSrgb = 0;
    double maxLinearError = 0;
    for (uint32_t y = 0; y < kHeight; ++y) for (uint32_t x = 0; x < kWidth; ++x) {
        const auto original = Pattern(x, y);
        const size_t pixel = size_t(y) * kWidth + x;
        for (uint32_t c = 0; c < 4; ++c) {
            const float input = float(original[c]) / 255.0f;
            const float target = c == 3 ? input : std::pow(input, 2.2f);
            const uint16_t expectedHalf = HalfBits(target);
            const uint16_t actualHalf = linear[pixel * 4 + c];
            const uint32_t ulp = uint32_t(std::abs(int(actualHalf) - int(expectedHalf)));
            halfExact += ulp == 0; halfWithinOne += ulp <= 1;
            maxHalfUlp = std::max(maxHalfUlp, ulp);
            maxLinearError = std::max(maxLinearError, double(std::abs(FromHalf(actualHalf) - FromHalf(expectedHalf))));
            Check(ulp <= 1, "prepare FP16 output differs by more than one ULP");
            if (c == 3) {
                alphaExact += encoded[pixel * 4 + c] == original[c];
                Check(encoded[pixel * 4 + c] == original[c], "present must preserve original alpha");
                continue;
            }
            const float encodedReference = std::pow(std::clamp(FromHalf(expectedHalf), 0.0f, 1.0f), 1.0f / 2.2f);
            const int expectedCode = std::clamp(int(std::lround(encodedReference * 255.0f)), 0, 255);
            const int error = std::abs(int(encoded[pixel * 4 + c]) - expectedCode);
            outputExact += error == 0; outputWithinOne += error <= 1;
            maxOutputCodeError = std::max(maxOutputCodeError, uint32_t(error));
            Check(error <= 1, "present UNORM8 output exceeds one-code tolerance");
        }
        if (y == 0 && x == 128) {
            const float v = 128.0f / 255.0f;
            const float srgb = std::pow((v + 0.055f) / 1.055f, 2.4f);
            const float observed = FromHalf(linear[pixel * 4]);
            Check(std::abs(observed - std::pow(v, 2.2f)) < 0.0005f &&
                std::abs(observed - srgb) > 0.003f, "gamma 2.2 intermediate is not sRGB decode");
            ++belowSrgb;
        }
    }
    std::filesystem::create_directories(dir);
    WriteBinary(dir / "source-rgba8-allocation.bin", f.sourceBytes.data(), f.sourceBytes.size());
    WriteBinary(dir / "prepared-rgba16f.bin", linear.data(), linear.size() * sizeof(uint16_t));
    WriteBinary(dir / "presented-rgba8.bin", encoded.data(), encoded.size());
    std::ofstream result(dir / "result.json", std::ios::trunc);
    result << "{\"case\":\"conversion_only_no_sdk\",\"source\":\"current_fsr_prepare_and_present_spv\""
           << ",\"source_allocation\":[" << kStorageWidth << ',' << kStorageHeight << "]"
           << ",\"color_origin\":[" << kOriginX << ',' << kOriginY << "]"
           << ",\"render_extent\":[" << kWidth << ',' << kHeight << "]"
           << ",\"mask_enabled\":false,\"fence_completed\":true,\"submission_serial\":1"
           << ",\"pixels\":" << kPixels << ",\"linear_channels_compared\":" << kPixels * 4
           << ",\"half_exact\":" << halfExact << ",\"half_within_one_ulp\":" << halfWithinOne
           << ",\"max_half_ulp\":" << maxHalfUlp << ",\"max_linear_abs_error\":" << maxLinearError
           << ",\"rgb_codes_compared\":" << kPixels * 3 << ",\"rgb_code_exact\":" << outputExact
           << ",\"rgb_code_within_one\":" << outputWithinOne
           << ",\"max_rgb_code_error\":" << maxOutputCodeError
           << ",\"alpha_codes_exact\":" << alphaExact << ",\"srgb_separation_samples\":" << belowSrgb
           << ",\"half_budget_ulp\":1,\"rgb_budget_codes\":1}\n";
    Check(bool(result), "write comparison results");
    std::printf("FSR color conversion: %u pixels, %u FP16 channels (%u exact, max %u ULP), "
        "%u RGB codes (%u exact, max %u code), %u exact alpha bytes; gamma2.2 != sRGB: PASS\n",
        kPixels, kPixels * 4, halfExact, maxHalfUlp, kPixels * 3, outputExact, maxOutputCodeError, alphaExact);
}
} // namespace

int main(int argc, char** argv) {
    try {
        Check(argc == 3 && std::strcmp(argv[1], "--output") == 0,
            "usage: LoFsrColorRoundtripGpuTest --output <evidence-directory>");
        Fixture fixture;
        fixture.Run();
        Compare(fixture, argv[2]);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FSR color conversion: FAIL: %s\n", e.what());
        return 1;
    }
}
