#pragma once
#include "fsr_alpha_mask_policy.h"
#include "pipeline_cache.h"
#include "shader/fsr_alpha_replay_hlsl.h"
#include "shader/dxc_compiler.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef LO_GPU_PLUME
#include <plume_render_interface.h>
#include <plume_render_interface_builders.h>

namespace gpu::fsr_alpha {

struct FrameIdentity {
    uint64_t renderFrame = 0, geometryEpoch = 0;
    uint64_t colorAllocation = 0, depthAllocation = 0;
    // A raw producer has no resolved write ordinal yet. The next slice must
    // bind this to an actual resolve version before any SDK mask is consumed.
    uint64_t resolveWriteOrdinal = 0;
    uint32_t width = 0, height = 0;
    std::array<uint32_t, 4> viewportBits{};
    std::array<uint32_t, 2> jitterBits{};
    bool operator==(const FrameIdentity&) const = default;
};

// Kept by every submitted producer batch and independently by the renderer's
// frame graph. A future consumer can retain the same lease across Flush.
struct FrameLease {
    FrameIdentity identity{};
    std::unique_ptr<plume::RenderTexture> texture;
    std::unique_ptr<plume::RenderFramebuffer> clearFramebuffer;
    plume::RenderTextureLayout layout = plume::RenderTextureLayout::UNKNOWN;
    uint32_t auditedDraws = 0;
    uint64_t lastDrawOrdinal = 0;
};

struct FrameView {
    Coverage coverage = Coverage::Unavailable;
    FrameIdentity identity{};
    std::shared_ptr<FrameLease> lease;
    plume::RenderTexture* texture = nullptr;
    uint32_t auditedDraws = 0;
    explicit operator bool() const { return coverage == Coverage::PartialCoverage && texture && lease; }
};

struct BatchUse {
    std::shared_ptr<FrameLease> lease;
    const plume::RenderTexture* borrowedDepth = nullptr;
    std::unique_ptr<plume::RenderFramebuffer> drawFramebuffer;
    uint64_t submissionSerial = 0;
};

class ReplayGPU {
    using Key = gpu::pipeline_cache::Key;
    using KeyHash = gpu::pipeline_cache::KeyHash;
    struct Module {
        std::unique_ptr<plume::RenderShader> shader;
        std::future<xenos::CompiledShader> compilation;
        std::string source, error;
    };
    plume::RenderDevice* device_ = nullptr;
    bool vulkan_ = false, ready_ = false, failedFrame_ = false;
    uint64_t frame_ = ~0ull, epoch_ = 0;
    uint32_t compiling_ = 0;
    std::string error_;
    std::unique_ptr<plume::RenderPipelineLayout> layout_;
    std::unordered_map<uint64_t, Module> vertexModules_, pixelModules_;
    std::unordered_map<Key, std::unique_ptr<plume::RenderPipeline>, KeyHash> pipelines_;
    std::vector<std::shared_ptr<FrameLease>> frames_;

    Module& GetModule(bool pixel, uint64_t hash, const xenos::TranslatedShader& known,
        const uint32_t* guestWords, uint32_t count) {
        auto& modules = pixel ? pixelModules_ : vertexModules_;
        auto [it, inserted] = modules.try_emplace(hash);
        Module& module = it->second;
        if (inserted) {
            // Renderer::GetShader releases generated HLSL outside capture mode.
            // Re-translate the original microcode once for this independent PSO.
            xenos::TranslatedShader translated = known;
            if (translated.hlsl.empty() && guestWords && count) {
                std::vector<uint32_t> words(count);
                for (uint32_t i = 0; i < count; ++i) {
                    const uint32_t w = guestWords[i];
                    words[i] = (w >> 24) | ((w >> 8) & 0xff00u) |
                        ((w << 8) & 0xff0000u) | (w << 24);
                }
                translated = xenos::TranslateShader(words.data(), count, pixel);
            }
            module.source = pixel ? xenos::fsr_alpha_replay::Pixel(translated) :
                (!translated.isPixelShader && translated.errors.empty() ? translated.hlsl : std::string{});
            if (module.source.empty()) module.error = "FSR alpha shader translation is unavailable";
        }
        if (module.shader || !module.error.empty()) return module;
        if (!module.compilation.valid() && compiling_ < 2) {
            const auto source = module.source;
            const auto format = vulkan_ ? xenos::ShaderBinaryFormat::Spirv : xenos::ShaderBinaryFormat::Dxil;
            const char* profile = pixel ? "ps_6_0" : "vs_6_0";
            module.compilation = std::async(std::launch::async, [source, format, profile] {
                return xenos::CompileCachedHlsl(source, "main", profile, format);
            });
            ++compiling_;
        }
        if (!module.compilation.valid() ||
            module.compilation.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return module;
        auto compiled = module.compilation.get();
        --compiling_;
        module.source.clear();
        if (!compiled.ok) { module.error = compiled.errors; return module; }
        module.shader = device_->createShader(compiled.bytecode.data(), compiled.bytecode.size(), "main",
            vulkan_ ? plume::RenderShaderFormat::SPIRV : plume::RenderShaderFormat::DXIL);
        if (!module.shader) module.error = "FSR alpha shader module allocation failed";
        return module;
    }

public:
    bool Init(plume::RenderDevice* device, const plume::RenderDescriptorSetBuilder* originalSets, uint32_t setCount) {
        if (!device || !originalSets || (setCount != 4 && setCount != 5)) return false;
        device_ = device;
        vulkan_ = device->getCapabilities().shaderFormat == plume::RenderShaderFormat::SPIRV;
        plume::RenderPipelineLayoutBuilder builder;
        builder.begin(false, false);
        if (vulkan_) builder.addPushConstant(0, 0, 3 * sizeof(uint64_t),
            plume::RenderShaderStageFlag::VERTEX | plume::RenderShaderStageFlag::PIXEL);
        else for (unsigned i = 0; i < 3; ++i)
            builder.addRootDescriptor(i, 0, plume::RenderRootDescriptorType::CONSTANT_BUFFER);
        for (uint32_t i = 0; i < setCount; ++i) builder.addDescriptorSet(originalSets[i]);
        builder.end();
        layout_ = builder.create(device);
        return ready_ = bool(layout_);
    }
    bool Ready() const { return ready_; }
    bool FailedThisFrame() const { return failedFrame_; }
    const std::string& LastError() const { return error_; }
    void BeginFrame(uint64_t frame, uint64_t epoch) {
        if (frame_ == frame && epoch_ == epoch) return;
        frame_ = frame; epoch_ = epoch; failedFrame_ = false; frames_.clear();
    }
    void ReleaseDepthBindingsAfterGpuDrain() { frames_.clear(); }

    plume::RenderPipeline* Prepare(const Key& key, plume::RenderGraphicsPipelineDesc desc,
        const xenos::TranslatedShader& vs, const xenos::TranslatedShader& ps,
        const uint32_t* vsWords, uint32_t vsCount, const uint32_t* psWords, uint32_t psCount) {
        if (!ready_ || failedFrame_ || !AuditedPair(key.vs, key.ps)) return nullptr;
        auto found = pipelines_.find(key);
        if (found != pipelines_.end()) return found->second.get();
        auto& vertex = GetModule(false, key.vs, vs, vsWords, vsCount);
        auto& pixel = GetModule(true, key.ps, ps, psWords, psCount);
        if (!vertex.error.empty() || !pixel.error.empty()) {
            error_ = vertex.error + pixel.error; failedFrame_ = true; return nullptr;
        }
        if (!vertex.shader || !pixel.shader) return nullptr; // asynchronous compilation
        desc.pipelineLayout = layout_.get();
        desc.vertexShader = vertex.shader.get(); desc.pixelShader = pixel.shader.get();
        desc.geometryShader = nullptr;
        desc.depthEnabled = true; desc.depthWriteEnabled = false;
        desc.depthFunction = plume::RenderComparisonFunction::GREATER_EQUAL;
        desc.stencilEnabled = false; desc.stencilWriteMask = 0;
        desc.logicOpEnabled = false;
        desc.renderTargetCount = 1;
        desc.renderTargetFormat[0] = plume::RenderFormat::R8_UNORM;
        auto& blend = desc.renderTargetBlend[0] = plume::RenderBlendDesc::Copy();
        blend.blendEnabled = true;
        blend.srcBlend = plume::RenderBlend::ONE; blend.dstBlend = plume::RenderBlend::ONE;
        blend.blendOp = plume::RenderBlendOperation::MAX;
        blend.srcBlendAlpha = plume::RenderBlend::ONE; blend.dstBlendAlpha = plume::RenderBlend::ONE;
        blend.blendOpAlpha = plume::RenderBlendOperation::MAX;
        blend.renderTargetWriteMask = 1;
        auto pipeline = device_->createGraphicsPipeline(desc);
        if (!pipeline) { error_ = "FSR alpha replay pipeline allocation failed"; failedFrame_ = true; return nullptr; }
        auto* result = pipeline.get();
        pipelines_.emplace(key, std::move(pipeline));
        return result;
    }

    bool Draw(plume::RenderCommandList* commands, plume::RenderPipeline* pipeline,
        const FrameIdentity& identity, plume::RenderTexture* depth,
        const plume::RenderBufferReference (&constants)[3],
        plume::RenderDescriptorSet* const* sets, uint32_t setCount,
        const plume::RenderViewport& viewport, const plume::RenderRect& scissor,
        bool indexed, uint32_t count, int32_t baseVertex, uint64_t drawOrdinal,
        std::vector<std::shared_ptr<BatchUse>>& batchUses) {
        if (!ready_ || failedFrame_ || !commands || !pipeline || !depth || !count ||
            identity.renderFrame != frame_ || identity.geometryEpoch != epoch_ ||
            !identity.colorAllocation || !identity.depthAllocation ||
            !identity.width || !identity.height || identity.width > 7680 || identity.height > 4320 ||
            setCount != (vulkan_ ? 5u : 4u)) return false;
        auto frameIt = std::find_if(frames_.begin(), frames_.end(), [&](const auto& value) {
            return value->identity == identity;
        });
        std::shared_ptr<FrameLease> frame;
        const bool firstDrawForFrame = frameIt == frames_.end();
        if (firstDrawForFrame) {
            frame = std::make_shared<FrameLease>();
            frame->identity = identity;
            frame->texture = device_->createTexture(plume::RenderTextureDesc::Texture2D(identity.width,
                identity.height, 1, plume::RenderFormat::R8_UNORM, plume::RenderTextureFlag::RENDER_TARGET));
            if (frame->texture) {
                const plume::RenderTexture* color[] = {frame->texture.get()};
                frame->clearFramebuffer = device_->createFramebuffer(plume::RenderFramebufferDesc(color, 1));
            }
            if (!frame->texture || !frame->clearFramebuffer) {
                error_ = "FSR alpha target allocation failed"; failedFrame_ = true; return false;
            }
            frames_.push_back(frame);
        } else frame = *frameIt;
        auto useIt = std::find_if(batchUses.begin(), batchUses.end(), [&](const auto& value) {
            return value->lease == frame && value->borrowedDepth == depth;
        });
        std::shared_ptr<BatchUse> use;
        if (useIt == batchUses.end()) {
            use = std::make_shared<BatchUse>();
            use->lease = frame; use->borrowedDepth = depth;
            const plume::RenderTexture* color[] = {frame->texture.get()};
            use->drawFramebuffer = device_->createFramebuffer(plume::RenderFramebufferDesc(color, 1, depth, true));
            if (!use->drawFramebuffer) {
                error_ = "FSR alpha depth framebuffer allocation failed"; failedFrame_ = true; return false;
            }
            batchUses.push_back(use);
        } else use = *useIt;
        // Complete all fallible allocations and register this batch's lease
        // before recording a command that refers to the texture.
        // The destination is loaded for MAX accumulation. Even when its
        // layout stays COLOR_WRITE, the next render pass (including one after
        // Flush) must see preceding color-attachment writes.
        commands->barriers(plume::RenderBarrierStage::GRAPHICS,
            plume::RenderTextureBarrier(frame->texture.get(), plume::RenderTextureLayout::COLOR_WRITE));
        frame->layout = plume::RenderTextureLayout::COLOR_WRITE;
        if (firstDrawForFrame) {
            commands->setFramebuffer(frame->clearFramebuffer.get());
            commands->clearColor(0, plume::RenderColor(0, 0, 0, 0));
            // clearColor writes through a separate framebuffer. Its zeroes
            // must be visible to the first MAX load in drawFramebuffer.
            commands->barriers(plume::RenderBarrierStage::GRAPHICS,
                plume::RenderTextureBarrier(frame->texture.get(), plume::RenderTextureLayout::COLOR_WRITE));
        }
        commands->setFramebuffer(use->drawFramebuffer.get());
        commands->setViewports(&viewport, 1); commands->setScissors(&scissor, 1);
        commands->setGraphicsPipelineLayout(layout_.get()); commands->setPipeline(pipeline);
        if (vulkan_) {
            uint64_t addresses[3];
            for (unsigned i = 0; i < 3; ++i)
                addresses[i] = constants[i].ref->getDeviceAddress() + constants[i].offset;
            commands->setGraphicsPushConstants(0, addresses);
        } else for (unsigned i = 0; i < 3; ++i) commands->setGraphicsRootDescriptor(constants[i], i);
        for (uint32_t i = 0; i < setCount; ++i) commands->setGraphicsDescriptorSet(sets[i], i);
        if (indexed) commands->drawIndexedInstanced(count, 1, 0, baseVertex, 0);
        else commands->drawInstanced(count, 1, uint32_t(baseVertex), 0);
        ++frame->auditedDraws;
        frame->lastDrawOrdinal = drawOrdinal;
        return true;
    }

    FrameView ViewFor(const FrameIdentity& identity) const {
        if (failedFrame_) return {};
        for (const auto& frame : frames_)
            if (frame->identity == identity && frame->auditedDraws)
                return {Coverage::PartialCoverage, identity, frame, frame->texture.get(), frame->auditedDraws};
        return {};
    }
};

} // namespace gpu::fsr_alpha
#endif
