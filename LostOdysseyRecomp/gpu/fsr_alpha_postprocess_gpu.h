#pragma once

#include "fsr_alpha_propagation_gpu.h"
#include "shader/fsr_alpha_postprocess_hlsl.h"
#include "shader/dxc_compiler.h"
#include <chrono>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef LO_GPU_PLUME
namespace gpu::fsr_alpha {

class PostprocessGPU {
    using Key = gpu::pipeline_cache::Key;
    using KeyHash = gpu::pipeline_cache::KeyHash;
    struct ProgramKey {
        Key pipeline{};
        uint32_t pointSlots = 0;
        bool operator==(const ProgramKey&) const = default;
    };
    struct ProgramHash {
        size_t operator()(const ProgramKey& value) const {
            return KeyHash{}(value.pipeline) ^ (size_t(value.pointSlots) << 1);
        }
    };
    struct Module {
        std::unique_ptr<plume::RenderShader> shader;
        std::future<xenos::CompiledShader> compilation;
        std::string source, error;
    };
    plume::RenderDevice* device_ = nullptr;
    plume::RenderPipelineLayout* layout_ = nullptr;
    plume::RenderShader* blitVertex_ = nullptr;
    plume::RenderShaderFormat shaderFormat_{};
    bool vulkan_ = false;
    uint32_t compiling_ = 0;
    std::string error_;
    std::unordered_map<uint64_t, Module> vertexModules_;
    std::map<std::pair<uint64_t, uint32_t>, Module> pixelModules_;
    std::unordered_map<ProgramKey, std::unique_ptr<plume::RenderPipeline>, ProgramHash> pipelines_;
    std::unique_ptr<plume::RenderShader> areaPixel_;
    std::unique_ptr<plume::RenderPipeline> areaPipeline_;

    static xenos::TranslatedShader OriginalShader(const xenos::TranslatedShader& known,
        const uint32_t* guestWords, uint32_t count, bool pixel) {
        if (!known.hlsl.empty() || !guestWords || !count) return known;
        std::vector<uint32_t> words(count);
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t w = guestWords[i];
            words[i] = (w >> 24) | ((w >> 8) & 0xff00u) |
                ((w << 8) & 0xff0000u) | (w << 24);
        }
        return xenos::TranslateShader(words.data(), count, pixel);
    }
    Module& GetModule(bool pixel, uint64_t hash, uint32_t pointSlots,
        const xenos::TranslatedShader& known, const uint32_t* guestWords, uint32_t count) {
        auto* module = [&]() -> Module* {
            if (pixel) return &pixelModules_[{hash, pointSlots}];
            return &vertexModules_[hash];
        }();
        if (module->source.empty() && module->error.empty() && !module->shader && !module->compilation.valid()) {
            auto original = OriginalShader(known, guestWords, count, pixel);
            module->source = pixel ? xenos::fsr_alpha_postprocess::Pixel(original, hash, pointSlots) :
                (!original.isPixelShader && original.errors.empty() ? original.hlsl : std::string{});
            if (module->source.empty()) module->error = "FSR postprocess shader translation unavailable";
        }
        if (module->shader || !module->error.empty()) return *module;
        if (!module->compilation.valid() && compiling_ < 2) {
            const auto source = module->source;
            const auto format = vulkan_ ? xenos::ShaderBinaryFormat::Spirv : xenos::ShaderBinaryFormat::Dxil;
            const char* profile = pixel ? "ps_6_0" : "vs_6_0";
            module->compilation = std::async(std::launch::async, [source, format, profile] {
                return xenos::CompileCachedHlsl(source, "main", profile, format);
            });
            ++compiling_;
        }
        if (!module->compilation.valid() ||
            module->compilation.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return *module;
        auto compiled = module->compilation.get();
        --compiling_;
        module->source.clear();
        if (!compiled.ok) { module->error = compiled.errors; return *module; }
        module->shader = device_->createShader(compiled.bytecode.data(), compiled.bytecode.size(), "main", shaderFormat_);
        if (!module->shader) module->error = "FSR postprocess shader module allocation failed";
        return *module;
    }
    static std::shared_ptr<MaskLease> NewTarget(plume::RenderDevice* device,
        uint32_t width, uint32_t height) {
        auto target = std::make_shared<MaskLease>();
        target->texture = device->createTexture(plume::RenderTextureDesc::Texture2D(
            width, height, 1, plume::RenderFormat::R8_UNORM, plume::RenderTextureFlag::RENDER_TARGET));
        if (target->texture) {
            const plume::RenderTexture* attachments[] = {target->texture.get()};
            target->clearFramebuffer = device->createFramebuffer(plume::RenderFramebufferDesc(attachments, 1));
        }
        return target->texture && target->clearFramebuffer ? target : nullptr;
    }
public:
    bool Init(plume::RenderDevice* device, plume::RenderPipelineLayout* layout,
        plume::RenderShader* blitVertex) {
        device_ = device; layout_ = layout; blitVertex_ = blitVertex;
        if (!device_ || !layout_ || !blitVertex_) return false;
        shaderFormat_ = device_->getCapabilities().shaderFormat;
        vulkan_ = shaderFormat_ == plume::RenderShaderFormat::SPIRV;
        return true;
    }
    const std::string& LastError() const { return error_; }
    plume::RenderPipeline* Prepare(const Key& key, uint32_t pointSlots,
        plume::RenderGraphicsPipelineDesc desc,
        const xenos::TranslatedShader& vs, const xenos::TranslatedShader& ps,
        const uint32_t* vsWords, uint32_t vsCount, const uint32_t* psWords, uint32_t psCount) {
        const ProgramKey program{key, pointSlots};
        if (auto existing = pipelines_.find(program); existing != pipelines_.end()) return existing->second.get();
        error_.clear();
        auto& vertex = GetModule(false, key.vs, 0, vs, vsWords, vsCount);
        auto& pixel = GetModule(true, key.ps, pointSlots, ps, psWords, psCount);
        if (!vertex.error.empty() || !pixel.error.empty()) {
            error_ = vertex.error + pixel.error; return nullptr;
        }
        if (!vertex.shader || !pixel.shader) return nullptr;
        desc.pipelineLayout = layout_;
        desc.vertexShader = vertex.shader.get(); desc.pixelShader = pixel.shader.get();
        desc.geometryShader = nullptr;
        desc.depthEnabled = false; desc.depthWriteEnabled = false;
        desc.depthFunction = plume::RenderComparisonFunction::ALWAYS;
        desc.depthTargetFormat = plume::RenderFormat::UNKNOWN;
        desc.stencilEnabled = false; desc.stencilWriteMask = 0;
        desc.logicOpEnabled = false;
        desc.renderTargetCount = 1;
        desc.renderTargetFormat[0] = plume::RenderFormat::R8_UNORM;
        desc.renderTargetBlend[0] = plume::RenderBlendDesc::Copy();
        desc.renderTargetBlend[0].renderTargetWriteMask = 1;
        auto pipeline = device_->createGraphicsPipeline(desc);
        if (!pipeline) { error_ = "FSR postprocess pipeline allocation failed"; return nullptr; }
        auto* result = pipeline.get();
        pipelines_.emplace(program, std::move(pipeline));
        return result;
    }
    std::shared_ptr<MaskLease> Draw(plume::RenderCommandList* commands,
        plume::RenderPipeline* pipeline, uint32_t width, uint32_t height,
        const plume::RenderBufferReference (&constants)[3],
        plume::RenderDescriptorSet* const* sets, uint32_t setCount,
        const plume::RenderViewport& viewport, const plume::RenderRect& scissor,
        bool indexed, uint32_t count, int32_t baseVertex,
        std::vector<std::shared_ptr<MaskLease>>& batchUses) {
        if (!commands || !pipeline || !width || !height || !count || setCount != (vulkan_ ? 5u : 4u))
            return nullptr;
        auto target = NewTarget(device_, width, height);
        if (!target) { error_ = "FSR postprocess target allocation failed"; return nullptr; }
        batchUses.push_back(target);
        commands->barriers(plume::RenderBarrierStage::GRAPHICS,
            plume::RenderTextureBarrier(target->texture.get(), plume::RenderTextureLayout::COLOR_WRITE));
        target->layout = plume::RenderTextureLayout::COLOR_WRITE;
        commands->setFramebuffer(target->clearFramebuffer.get());
        commands->clearColor(0, plume::RenderColor(0, 0, 0, 0));
        commands->barriers(plume::RenderBarrierStage::GRAPHICS,
            plume::RenderTextureBarrier(target->texture.get(), plume::RenderTextureLayout::COLOR_WRITE));
        commands->setViewports(&viewport, 1); commands->setScissors(&scissor, 1);
        commands->setGraphicsPipelineLayout(layout_); commands->setPipeline(pipeline);
        if (vulkan_) {
            uint64_t addresses[3];
            for (unsigned i = 0; i < 3; ++i)
                addresses[i] = constants[i].ref->getDeviceAddress() + constants[i].offset;
            commands->setGraphicsPushConstants(0, addresses);
        } else for (unsigned i = 0; i < 3; ++i) commands->setGraphicsRootDescriptor(constants[i], i);
        for (uint32_t i = 0; i < setCount; ++i) commands->setGraphicsDescriptorSet(sets[i], i);
        if (indexed) commands->drawIndexedInstanced(count, 1, 0, baseVertex, 0);
        else commands->drawInstanced(count, 1, uint32_t(baseVertex), 0);
        commands->barriers(plume::RenderBarrierStage::GRAPHICS,
            plume::RenderTextureBarrier(target->texture.get(), plume::RenderTextureLayout::SHADER_READ));
        target->layout = plume::RenderTextureLayout::SHADER_READ;
        return target;
    }
    std::shared_ptr<MaskLease> AreaMaximum(plume::RenderCommandList* commands,
        const std::shared_ptr<MaskLease>& parent,
        plume::RenderDescriptorSet* const* sets, uint32_t setCount,
        std::vector<std::shared_ptr<MaskLease>>& batchUses) {
        if (!commands || !parent || !parent->texture || !blitVertex_ ||
            setCount != (vulkan_ ? 5u : 4u)) return nullptr;
        if (!areaPipeline_) {
            const auto format = vulkan_ ? xenos::ShaderBinaryFormat::Spirv : xenos::ShaderBinaryFormat::Dxil;
            auto compiled = xenos::CompileCachedHlsl(xenos::fsr_alpha_postprocess::AreaMaximum,
                "main", "ps_6_0", format);
            if (!compiled.ok) { error_ = compiled.errors; return nullptr; }
            areaPixel_ = device_->createShader(compiled.bytecode.data(), compiled.bytecode.size(), "main", shaderFormat_);
            if (!areaPixel_) { error_ = "FSR area-mask shader allocation failed"; return nullptr; }
            plume::RenderGraphicsPipelineDesc desc;
            desc.pipelineLayout = layout_;
            desc.vertexShader = blitVertex_; desc.pixelShader = areaPixel_.get();
            desc.depthEnabled = false; desc.depthWriteEnabled = false;
            desc.depthFunction = plume::RenderComparisonFunction::ALWAYS;
            desc.depthTargetFormat = plume::RenderFormat::UNKNOWN;
            desc.renderTargetCount = 1;
            desc.renderTargetFormat[0] = plume::RenderFormat::R8_UNORM;
            desc.renderTargetBlend[0] = plume::RenderBlendDesc::Copy();
            desc.renderTargetBlend[0].renderTargetWriteMask = 1;
            desc.cullMode = plume::RenderCullMode::NONE;
            desc.primitiveTopology = plume::RenderPrimitiveTopology::TRIANGLE_LIST;
            areaPipeline_ = device_->createGraphicsPipeline(desc);
            if (!areaPipeline_) { error_ = "FSR area-mask pipeline allocation failed"; return nullptr; }
        }
        auto target = NewTarget(device_, 1280, 720);
        if (!target) { error_ = "FSR area-mask target allocation failed"; return nullptr; }
        target->parent = parent;
        batchUses.push_back(parent);
        batchUses.push_back(target);
        commands->barriers(plume::RenderBarrierStage::GRAPHICS,
            plume::RenderTextureBarrier(parent->texture.get(), plume::RenderTextureLayout::SHADER_READ));
        parent->layout = plume::RenderTextureLayout::SHADER_READ;
        commands->barriers(plume::RenderBarrierStage::GRAPHICS,
            plume::RenderTextureBarrier(target->texture.get(), plume::RenderTextureLayout::COLOR_WRITE));
        target->layout = plume::RenderTextureLayout::COLOR_WRITE;
        commands->setFramebuffer(target->clearFramebuffer.get());
        const plume::RenderViewport viewport(0, 0, 1280, 720);
        const plume::RenderRect scissor{0, 0, 1280, 720};
        commands->setViewports(&viewport, 1); commands->setScissors(&scissor, 1);
        commands->setGraphicsPipelineLayout(layout_); commands->setPipeline(areaPipeline_.get());
        for (uint32_t i = 0; i < setCount; ++i) commands->setGraphicsDescriptorSet(sets[i], i);
        commands->drawInstanced(3, 1, 0, 0);
        commands->barriers(plume::RenderBarrierStage::GRAPHICS,
            plume::RenderTextureBarrier(target->texture.get(), plume::RenderTextureLayout::SHADER_READ));
        target->layout = plume::RenderTextureLayout::SHADER_READ;
        return target;
    }
};

} // namespace gpu::fsr_alpha
#endif
