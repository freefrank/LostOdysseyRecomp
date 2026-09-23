#pragma once
#include "temporal_gpu_timing.h"
#include "motion_vector.h"
#include "motion_frame.h"
#include "pipeline_cache.h"
#include "shader/motion_replay_hlsl.h"
#include "shader/dxc_compiler.h"
#include <bit>
#include <cstdint>
#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#ifdef LO_GPU_PLUME
#include <plume_render_interface.h>
#include <plume_render_interface_builders.h>

namespace gpu::temporal {
struct MotionReplayConstants {
    uint32_t previousVS[1024]{};
    uint32_t previousShared[52]{};
    float extent[4]{};
    uint32_t metadata[4]{}; // tag, asuint(current Jx), asuint(current Jy), exactStationary bit 0
};
static_assert(sizeof(MotionReplayConstants) == 4336);
static_assert(offsetof(MotionReplayConstants, extent) == 4304);
static_assert(offsetof(MotionReplayConstants, metadata) == 4320);
inline MotionReplayConstants MakeMotionReplayConstants(const DrawTemporalTracker::Match& match,
    uint32_t width, uint32_t height, float jx, float jy) {
    MotionReplayConstants c{};
    if (match.previous) {
        std::memcpy(c.previousVS, match.previous->vsConstants.data(), sizeof(c.previousVS));
        std::memcpy(c.previousShared, match.previous->shared.data(), sizeof(c.previousShared));
        c.metadata[0] = match.tag;
        c.metadata[3] = match.exactStationary && match.previous->raster.width == width &&
            match.previous->raster.height == height ? 1u : 0u;
    }
    c.extent[0] = float(width); c.extent[1] = float(height);
    c.metadata[1] = std::bit_cast<uint32_t>(jx); c.metadata[2] = std::bit_cast<uint32_t>(jy);
    return c;
}

// Actual geometry replay: original translated VS runs with current and previous
// inputs; original PS retains its alpha/discard coverage. No CPU per-pixel work.
class MotionReplayGPU {
    using Key = gpu::pipeline_cache::Key;
    using KeyHash = gpu::pipeline_cache::KeyHash;
    struct Image {
        std::unique_ptr<plume::RenderTexture> texture;
        plume::RenderTextureLayout layout = plume::RenderTextureLayout::UNKNOWN;
    } velocity_, depths_, tags_, reactive_;
    struct Module {
        std::unique_ptr<plume::RenderShader> shader;
        std::future<xenos::CompiledShader> compilation;
        std::string source, error;
        MotionConstantUsage constantUsage;
    };
    std::unordered_map<uint64_t, Module> vertexModules_, pixelModules_;
    std::unordered_map<Key, std::unique_ptr<plume::RenderPipeline>, KeyHash> pipelines_;
    plume::RenderDevice* device_ = nullptr;
    bool vulkan_ = false, initialized_ = false, cleared_ = false, finalized_ = false, aborted_ = false;
    bool pendingThisFrame_ = false, injectPreparePending_ = false;
    uint64_t frame_ = ~0ull, epoch_ = 0, allocation_ = 0, serial_ = 0, targetGeneration_ = 0;
    uint32_t width_ = 0, height_ = 0;
    const plume::RenderTexture* boundDepth_ = nullptr;
    std::unique_ptr<plume::RenderPipelineLayout> layout_, maskLayout_;
    std::unique_ptr<plume::RenderShader> maskVS_, maskPS_;
    std::unique_ptr<plume::RenderPipeline> maskPipeline_;
    std::unique_ptr<plume::RenderFramebuffer> clearFramebuffer_, drawFramebuffer_;
    struct Retired {
        uint64_t serial = 0;
        // Guest depth is not owned. The framebuffer's stencil view must die first.
        const plume::RenderTexture* externalDepth = nullptr;
        std::unique_ptr<plume::RenderFramebuffer> framebuffer;
        std::unique_ptr<plume::RenderTexture> texture;
    };
    std::vector<Retired> retired_;
    struct MaskBatch {
        uint64_t serial = 0, generation = 0;
        std::unique_ptr<plume::RenderBuffer> flags, constants;
        std::unique_ptr<plume::RenderDescriptorSet> set;
        std::unique_ptr<plume::RenderFramebuffer> framebuffer;
    };
    std::vector<std::unique_ptr<MaskBatch>> pending_, free_;
    static constexpr size_t kMaxBatches = 16;
    std::string error_;
    uint32_t drawCount_ = 0, failedDraws_ = 0;
    GpuPassTimer<128> drawTimer_;
    GpuPassTimer<> maskTimer_;
    uint64_t maskBatchAllocations_ = 0;
    uint32_t compilingModules_ = 0;
    static constexpr uint32_t kMaxCompilingModules = 2;
    static constexpr const char* kMaskShader = R"HLSL(
Texture2D<float2> motionDepth : register(t0);
Texture2D<uint> motionTag : register(t1);
Texture2D<float> sceneDepth : register(t2);
ByteAddressBuffer matchFlags : register(t3);
#ifdef __spirv__
[[vk::binding(4,0)]]
#endif
cbuffer Params : register(b0) { uint tagCount; uint3 reserved; };
float4 vertex(uint id : SV_VertexID) : SV_Position {
    float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2,-2) + float2(-1,1), 0, 1);
}
float pixel(float4 p : SV_Position) : SV_Target {
    int3 at = int3(p.xy,0);
    uint tag = motionTag.Load(at);
    if (tag == 0 || tag >= tagCount || matchFlags.Load(tag * 4) == 0) return 1;
    float2 d = motionDepth.Load(at); float visible = sceneDepth.Load(at);
    if (!all(isfinite(d)) || !isfinite(visible) || d.x <= 0 || d.y <= 0 || d.x > 1 || d.y > 1 || visible <= 0 || visible > 1) return 1;
    // A draw overwritten/occluded by later unsupported geometry must not supply
    // the visible pixel's motion. Keep a tight host-depth rounding tolerance.
    return abs(d.x-visible) <= max(2e-7, abs(visible)*2e-5) ? 0 : 1;
}
)HLSL";
    static void Transition(plume::RenderCommandList* commands, Image& image, plume::RenderTextureLayout to) {
        if (image.layout != to) {
            commands->barriers(plume::RenderBarrierStage::ALL, plume::RenderTextureBarrier(image.texture.get(), to));
            image.layout = to;
        }
    }
    void Retire(std::unique_ptr<plume::RenderFramebuffer>& p, const plume::RenderTexture* externalDepth = nullptr) {
        if (p) retired_.push_back({serial_, externalDepth, std::move(p), {}});
    }
    bool Allocate(Image& image, plume::RenderFormat format) {
        if (image.texture) retired_.push_back({serial_, nullptr, {}, std::move(image.texture)});
        image.layout = plume::RenderTextureLayout::UNKNOWN;
        image.texture = device_->createTexture(plume::RenderTextureDesc::Texture2D(width_, height_, 1, format, plume::RenderTextureFlag::RENDER_TARGET));
        return bool(image.texture);
    }
    void MaskSet(plume::RenderDescriptorSetBuilder& b) const {
        b.begin(); b.addTexture(0); b.addTexture(1); b.addTexture(2); b.addByteAddressBuffer(3);
        if (vulkan_) b.addConstantBuffer(4);
        b.end();
    }
    Module& GetModule(bool pixel, uint64_t hash, const uint32_t* guestWords, uint32_t count, bool wait) {
        auto& cache = pixel ? pixelModules_ : vertexModules_;
        auto [it, inserted] = cache.try_emplace(hash);
        auto& m = it->second;
        if (inserted) {
            // Endian conversion is identical to Renderer::GetShader.
            xenos::TranslatedShader translated;
            if (count && guestWords) {
                std::vector<uint32_t> words(count);
                for (uint32_t i = 0; i < count; ++i) {
                    const uint32_t w = guestWords[i];
                    words[i] = (w >> 24) | ((w >> 8) & 0xff00u) | ((w << 8) & 0xff0000u) | (w << 24);
                }
                translated = xenos::TranslateShader(words.data(), count, pixel);
            } else if (!pixel) { m.error = "Missing vertex microcode"; return m; }
            if(!pixel && translated.errors.empty()) m.constantUsage = ParseMotionConstantUsage(translated.hlsl,translated.usesRelativeConstants);
            m.source = pixel ? xenos::motion_replay::Pixel(count ? &translated : nullptr) : xenos::motion_replay::Vertex(translated);
            if (m.source.empty()) { m.error = "Unsupported replay program: " + translated.errors; return m; }
        }
        if (m.shader || !m.error.empty()) return m;
        const char* profile = pixel ? "ps_6_0" : "vs_6_0";
        const auto format = vulkan_ ? xenos::ShaderBinaryFormat::Spirv : xenos::ShaderBinaryFormat::Dxil;
        if (!m.compilation.valid() && (wait || compilingModules_ < kMaxCompilingModules)) {
            auto source = m.source;
            m.compilation = std::async(std::launch::async, [source = std::move(source), profile, format] {
                return xenos::CompileCachedHlsl(source, "main", profile, format);
            });
            ++compilingModules_;
        }
        if (!m.compilation.valid() || (!wait && m.compilation.wait_for(std::chrono::seconds(0)) != std::future_status::ready))
            return m;
        auto compiled = m.compilation.get();
        --compilingModules_;
        m.source.clear();
        if (!compiled.ok) { m.error = compiled.errors; return m; }
        m.shader = device_->createShader(compiled.bytecode.data(), compiled.bytecode.size(), "main",
            vulkan_ ? plume::RenderShaderFormat::SPIRV : plume::RenderShaderFormat::DXIL);
        if (!m.shader) m.error = "Replay shader module allocation failed";
        return m;
    }
public:
    const MotionConstantUsage* ConstantUsage(uint64_t vsHash) const {
        const auto it=vertexModules_.find(vsHash);
        return it==vertexModules_.end()?nullptr:&it->second.constantUsage;
    }
    bool Init(plume::RenderDevice* device, const plume::RenderDescriptorSetBuilder* originalSets, uint32_t setCount) {
        if (!device || !originalSets || (setCount != 4 && setCount != 5)) return false;
        device_ = device; vulkan_ = device->getCapabilities().shaderFormat == plume::RenderShaderFormat::SPIRV;
        plume::RenderPipelineLayoutBuilder b; b.begin(false, false);
        if (vulkan_) b.addPushConstant(0, 0, 4 * sizeof(uint64_t), plume::RenderShaderStageFlag::VERTEX | plume::RenderShaderStageFlag::PIXEL);
        else for (unsigned i = 0; i < 4; ++i) b.addRootDescriptor(i, 0, plume::RenderRootDescriptorType::CONSTANT_BUFFER);
        for (uint32_t i = 0; i < setCount; ++i) b.addDescriptorSet(originalSets[i]);
        b.end(); layout_ = b.create(device);
        plume::RenderDescriptorSetBuilder sb; MaskSet(sb);
        b.begin(false, false);
        if (!vulkan_) b.addPushConstant(0, 0, 16, plume::RenderShaderStageFlag::PIXEL);
        b.addDescriptorSet(sb); b.end(); maskLayout_ = b.create(device);
        auto v = xenos::CompileCachedHlsl(kMaskShader, "vertex", "vs_6_0", vulkan_ ? xenos::ShaderBinaryFormat::Spirv : xenos::ShaderBinaryFormat::Dxil);
        auto p = xenos::CompileCachedHlsl(kMaskShader, "pixel", "ps_6_0", vulkan_ ? xenos::ShaderBinaryFormat::Spirv : xenos::ShaderBinaryFormat::Dxil);
        if (!v.ok || !p.ok || !layout_ || !maskLayout_) { error_ = v.errors + p.errors; return false; }
        const auto format = vulkan_ ? plume::RenderShaderFormat::SPIRV : plume::RenderShaderFormat::DXIL;
        maskVS_ = device->createShader(v.bytecode.data(), v.bytecode.size(), "vertex", format);
        maskPS_ = device->createShader(p.bytecode.data(), p.bytecode.size(), "pixel", format);
        if (!maskVS_ || !maskPS_) return false;
        plume::RenderGraphicsPipelineDesc d{};
        d.pipelineLayout = maskLayout_.get(); d.vertexShader = maskVS_.get(); d.pixelShader = maskPS_.get();
        d.renderTargetCount = 1; d.renderTargetFormat[0] = plume::RenderFormat::R8_UNORM;
        d.renderTargetBlend[0] = plume::RenderBlendDesc::Copy();
        maskPipeline_ = device->createGraphicsPipeline(d);
        initialized_ = bool(maskPipeline_); return initialized_;
    }
    void EnableGpuTiming(bool enabled) {drawTimer_.Enable(enabled);maskTimer_.Enable(enabled);}
    // Called before EVERY renderer submission, including mid-frame flush/abort.
    void SealTimings(plume::RenderCommandList* commands) {drawTimer_.Seal(commands);}
    const GpuPassTimingStats& DrawTiming() const {return drawTimer_.Stats();}
    const GpuPassTimingStats& MaskTiming() const {return maskTimer_.Stats();}
    uint64_t MaskBatchAllocations() const {return maskBatchAllocations_;}
    bool Ready() const { return initialized_; }
    bool UsableThisFrame() const { return initialized_ && !aborted_ && !finalized_; }
    bool PipelinePendingThisFrame() const { return pendingThisFrame_; }
    bool SceneReadyThisFrame() const { return cleared_ && !aborted_; }
    bool AbortedThisFrame() const { return aborted_; }
    const std::string& LastError() const { return error_; }
    enum class PipelinePrepareStatus : uint8_t { Ready, Pending, Failed };
    struct SceneDrawPrepare {
        PipelinePrepareStatus status = PipelinePrepareStatus::Failed;
        plume::RenderPipeline* pipeline = nullptr;
        bool sceneReady = false;
    };
    // Deterministic test inject: next wait=false prepare reports Pending without racing compiles.
    void InjectNextPreparePending() { injectPreparePending_ = true; }
    void BeginFrame(uint64_t frame, uint64_t epoch) {
        if (frame_ == frame && epoch_ == epoch) return;
        frame_ = frame; epoch_ = epoch; cleared_ = finalized_ = aborted_ = pendingThisFrame_ = false;
        injectPreparePending_ = false;
        drawCount_ = failedDraws_ = 0;
    }
    void AbortFrame(std::string_view reason = {}) {
        aborted_ = true;
        if (!reason.empty()) error_ = std::string(reason);
    }
    // Whole-frame fallback is used for unsupported visibility writes (stencil,
    // PS depth, unknown scene view), never fabricated stationary vectors.
    bool BeginScene(plume::RenderCommandList* commands, uint64_t allocation, plume::RenderTexture* depth,
        uint32_t width, uint32_t height) {
        if (!initialized_ || aborted_ || finalized_ || !commands || !depth || !width || !height || width > 7680 || height > 4320) return false;
        if (cleared_) return allocation_ == allocation && width_ == width && height_ == height && boundDepth_ == depth;
        if (width_ != width || height_ != height) {
            ++targetGeneration_;
            free_.clear(); // release cached framebuffers before retiring their attachments
            Retire(drawFramebuffer_, boundDepth_); Retire(clearFramebuffer_);
            width_ = width; height_ = height;
            if (!Allocate(velocity_, plume::RenderFormat::R16G16_FLOAT) || !Allocate(depths_, plume::RenderFormat::R32G32_FLOAT) ||
                !Allocate(tags_, plume::RenderFormat::R32_UINT) || !Allocate(reactive_, plume::RenderFormat::R8_UNORM)) {
                aborted_ = true; width_ = height_ = 0; error_ = "MV target allocation failed"; return false;
            }
            boundDepth_ = nullptr;
        }
        const plume::RenderTexture* colors[] = {velocity_.texture.get(), depths_.texture.get(), tags_.texture.get()};
        if (!clearFramebuffer_) clearFramebuffer_ = device_->createFramebuffer(plume::RenderFramebufferDesc(colors, 3));
        if (boundDepth_ != depth || allocation_ != allocation || !drawFramebuffer_) {
            Retire(drawFramebuffer_, boundDepth_);
            // Keep DEPTH_WRITE layout as in the guest pass, but the replay PSO
            // disables depth/stencil writes. No per-draw depth transition or wait.
            drawFramebuffer_ = device_->createFramebuffer(plume::RenderFramebufferDesc(colors, 3, depth, false));
            boundDepth_ = depth;
        }
        allocation_ = allocation;
        if (!clearFramebuffer_ || !drawFramebuffer_) { aborted_ = true; return false; }
        Transition(commands, velocity_, plume::RenderTextureLayout::COLOR_WRITE);
        Transition(commands, depths_, plume::RenderTextureLayout::COLOR_WRITE);
        Transition(commands, tags_, plume::RenderTextureLayout::COLOR_WRITE);
        commands->setFramebuffer(clearFramebuffer_.get());
        for (unsigned i = 0; i < 3; ++i) commands->clearColor(i, plume::RenderColor(0,0,0,0));
        ++serial_; cleared_ = true; return true;
    }
    plume::RenderPipeline* PreparePipeline(const Key& key, plume::RenderGraphicsPipelineDesc desc,
        const uint32_t* vsWords, uint32_t vsCount, const uint32_t* psWords, uint32_t psCount, bool wait = false,
        PipelinePrepareStatus* status = nullptr) {
        const auto setStatus = [&](PipelinePrepareStatus value) { if (status) *status = value; };
        // One-shot fixture inject: wait=false reports Pending even for a warm cache.
        // pendingThisFrame_ stays set for the rest of the frame; Ready must not clear it.
        if (injectPreparePending_ && !wait) {
            injectPreparePending_ = false;
            pendingThisFrame_ = true;
            setStatus(PipelinePrepareStatus::Pending);
            return nullptr;
        }
        auto found = pipelines_.find(key);
        if (found != pipelines_.end()) {
            if (!found->second) { setStatus(PipelinePrepareStatus::Failed); return nullptr; }
            setStatus(PipelinePrepareStatus::Ready);
            return found->second.get();
        }
        if (desc.geometryShader || desc.stencilEnabled || !desc.depthEnabled) {
            ++failedDraws_; setStatus(PipelinePrepareStatus::Failed); return nullptr;
        }
        auto& vs = GetModule(false, key.vs, vsWords, vsCount, wait);
        auto& ps = GetModule(true, key.ps, psWords, psCount, wait);
        if (!vs.shader || !ps.shader) {
            if (vs.error.empty() && ps.error.empty()) {
                pendingThisFrame_ = true;
                setStatus(PipelinePrepareStatus::Pending);
                return nullptr;
            }
            ++failedDraws_;
            error_ = vs.error + ps.error;
            setStatus(PipelinePrepareStatus::Failed);
            return nullptr;
        }
        desc.pipelineLayout = layout_.get(); desc.vertexShader = vs.shader.get(); desc.pixelShader = ps.shader.get();
        desc.depthWriteEnabled = false; desc.depthFunction = plume::RenderComparisonFunction::EQUAL;
        desc.stencilWriteMask = 0; desc.logicOpEnabled = false;
        desc.renderTargetCount = 3;
        desc.renderTargetFormat[0] = plume::RenderFormat::R16G16_FLOAT;
        desc.renderTargetFormat[1] = plume::RenderFormat::R32G32_FLOAT;
        desc.renderTargetFormat[2] = plume::RenderFormat::R32_UINT;
        for (unsigned i = 0; i < 3; ++i) desc.renderTargetBlend[i] = plume::RenderBlendDesc::Copy();
        auto pipeline = device_->createGraphicsPipeline(desc);
        auto* result = pipeline.get();
        if (!result) { ++failedDraws_; error_ = "MV pipeline allocation failed"; setStatus(PipelinePrepareStatus::Failed); return nullptr; }
        pipelines_.emplace(key, std::move(pipeline));
        setStatus(PipelinePrepareStatus::Ready);
        return result;
    }
    SceneDrawPrepare PrepareSceneDraw(plume::RenderCommandList* commands, uint64_t allocation, plume::RenderTexture* depth,
        uint32_t width, uint32_t height, const Key& key, plume::RenderGraphicsPipelineDesc desc,
        const uint32_t* vsWords, uint32_t vsCount, const uint32_t* psWords, uint32_t psCount, bool wait = false) {
        SceneDrawPrepare prepared;
        prepared.sceneReady = BeginScene(commands, allocation, depth, width, height);
        if (!prepared.sceneReady) return prepared;
        prepared.pipeline = PreparePipeline(key, desc, vsWords, vsCount, psWords, psCount, wait, &prepared.status);
        return prepared;
    }
    bool Draw(plume::RenderCommandList* commands, plume::RenderPipeline* pipeline,
        const plume::RenderBufferReference (&constants)[4], plume::RenderDescriptorSet* const* sets, uint32_t setCount,
        const plume::RenderViewport& viewport, const plume::RenderRect& scissor, bool indexed, uint32_t count, int32_t baseVertex) {
        if (!cleared_ || aborted_ || finalized_ || !pipeline || !commands) return false;
        const bool timed = drawTimer_.Begin(device_, commands);
        commands->setFramebuffer(drawFramebuffer_.get()); commands->setViewports(&viewport, 1); commands->setScissors(&scissor, 1);
        commands->setGraphicsPipelineLayout(layout_.get()); commands->setPipeline(pipeline);
        if (vulkan_) {
            uint64_t addresses[4]; for (unsigned i = 0; i < 4; ++i) addresses[i] = constants[i].ref->getDeviceAddress() + constants[i].offset;
            commands->setGraphicsPushConstants(0, addresses);
        } else for (unsigned i = 0; i < 4; ++i) commands->setGraphicsRootDescriptor(constants[i], i);
        for (uint32_t i = 0; i < setCount; ++i) commands->setGraphicsDescriptorSet(sets[i], i);
        if (indexed) commands->drawIndexedInstanced(count, 1, 0, baseVertex, 0);
        else commands->drawInstanced(count, 1, uint32_t(baseVertex), 0);
        ++drawCount_; ++serial_;
        if (timed) drawTimer_.End(commands, serial_);
        return true;
    }
    MotionFrameView Finish(plume::RenderCommandList* commands, plume::RenderTexture* currentDepth,
        const std::vector<uint32_t>& validity, bool resetInitialization = false) {
        MotionFrameView result;
        if (!cleared_ || aborted_ || finalized_ || !currentDepth || !commands || validity.empty() || validity.size() > DrawTemporalTracker::kMaxDraws + 1) return result;
        if (pendingThisFrame_) { finalized_ = true; return result; }
        finalized_ = true;
        if (pending_.size() >= kMaxBatches) { error_ = "MV in-flight batch bound exceeded"; aborted_ = true; return result; }
        std::unique_ptr<MaskBatch> batch;
        if (!free_.empty()) { batch = std::move(free_.back()); free_.pop_back(); }
        else {
            batch = std::make_unique<MaskBatch>(); ++maskBatchAllocations_;
            batch->flags = device_->createBuffer(plume::RenderBufferDesc::UploadBuffer((DrawTemporalTracker::kMaxDraws + 1) * 4, plume::RenderBufferFlag::STORAGE));
            if (vulkan_) batch->constants = device_->createBuffer(plume::RenderBufferDesc::UploadBuffer(16, plume::RenderBufferFlag::CONSTANT));
            plume::RenderDescriptorSetBuilder sb; MaskSet(sb); batch->set = sb.create(device_);
        }
        if (!batch->flags || !batch->set || (vulkan_ && !batch->constants)) { aborted_ = true; return result; }
        if (batch->generation != targetGeneration_) batch->framebuffer.reset();
        batch->generation = targetGeneration_;
        if (!batch->framebuffer) {
            const plume::RenderTexture* attachments[] = {reactive_.texture.get()};
            batch->framebuffer = device_->createFramebuffer(plume::RenderFramebufferDesc(attachments, 1));
        }
        if (!batch->framebuffer) { aborted_ = true; return result; }
        void* mapped = batch->flags->map(); if (!mapped) { aborted_ = true; return result; }
        std::memcpy(mapped, validity.data(), validity.size() * 4); batch->flags->unmap();
        uint32_t c[4] = {uint32_t(validity.size()),0,0,0};
        if (vulkan_) {
            mapped = batch->constants->map(); if (!mapped) { aborted_ = true; return result; }
            std::memcpy(mapped, c, sizeof(c)); batch->constants->unmap();
            batch->set->setBuffer(4, batch->constants.get(), sizeof(c));
        }
        const bool timed = maskTimer_.Begin(device_, commands);
        Transition(commands, velocity_, plume::RenderTextureLayout::SHADER_READ);
        Transition(commands, depths_, plume::RenderTextureLayout::SHADER_READ);
        Transition(commands, tags_, plume::RenderTextureLayout::SHADER_READ);
        Transition(commands, reactive_, plume::RenderTextureLayout::COLOR_WRITE);
        batch->set->setTexture(0, depths_.texture.get(), plume::RenderTextureLayout::SHADER_READ);
        batch->set->setTexture(1, tags_.texture.get(), plume::RenderTextureLayout::SHADER_READ);
        batch->set->setTexture(2, currentDepth, plume::RenderTextureLayout::SHADER_READ);
        batch->set->setBuffer(3, batch->flags.get(), validity.size() * 4);
        commands->setFramebuffer(batch->framebuffer.get());
        plume::RenderViewport vp(0,0,float(width_),float(height_)); plume::RenderRect sc(0,0,width_,height_);
        commands->setViewports(&vp,1); commands->setScissors(&sc,1);
        commands->setGraphicsPipelineLayout(maskLayout_.get()); commands->setPipeline(maskPipeline_.get());
        if (!vulkan_) commands->setGraphicsPushConstants(0, c);
        commands->setGraphicsDescriptorSet(batch->set.get(),0); commands->drawInstanced(3,1,0,0);
        Transition(commands, reactive_, plume::RenderTextureLayout::SHADER_READ);
        batch->serial = ++serial_;
        if (timed) maskTimer_.End(commands, serial_);
        pending_.push_back(std::move(batch));
        return {velocity_.texture.get(), depths_.texture.get(), reactive_.texture.get(), frame_, epoch_, allocation_, width_, height_, true,
            resetInitialization ? MotionState::ResetInitialization : MotionState::Tracked};
    }
    // The slot fence has already proved this depth's last use. Drop every
    // framebuffer that still holds its stencil view before the texture dies.
    // Other in-flight replay textures and unrelated depth bindings stay.
    void ReleaseDepthAfterGpuCompletion(const plume::RenderTexture* depth) {
        if (!depth) return;
        if (boundDepth_ == depth) { drawFramebuffer_.reset(); boundDepth_ = nullptr; }
        for (auto& entry : retired_) if (entry.externalDepth == depth) {
            entry.framebuffer.reset();
            entry.externalDepth = nullptr;
        }
    }
    // Full renderer and presentation drain already succeeded. Retire every
    // completed object, then drop the current draw framebuffer. Pipelines stay.
    void ReleaseDepthBindingsAfterGpuDrain() {
        ReleaseCompletedThrough(RecordedSerial());
        drawFramebuffer_.reset();
        boundDepth_ = nullptr;
    }
    bool ReferencesExternalDepth(const plume::RenderTexture* depth) const {
        if (!depth) return false;
        if (boundDepth_ == depth && drawFramebuffer_) return true;
        for (const auto& entry : retired_) if (entry.externalDepth == depth && entry.framebuffer) return true;
        return false;
    }
    bool BoundDepthIs(const plume::RenderTexture* depth) const { return depth && boundDepth_ == depth && drawFramebuffer_; }
    uint32_t RetiredDepthFramebufferCount(const plume::RenderTexture* depth) const {
        uint32_t count = 0;
        if (!depth) return 0;
        for (const auto& entry : retired_) if (entry.externalDepth == depth && entry.framebuffer) ++count;
        return count;
    }
    uint32_t PipelineCount() const { return uint32_t(pipelines_.size()); }
    uint32_t RetiredOwnedTextureCount() const {
        uint32_t count = 0;
        for (const auto& entry : retired_) if (entry.texture) ++count;
        return count;
    }
    // A later TAA/diagnostic read may be in another submission than Finish().
    // Include it before that submission is stamped by the renderer.
    void RecordConsumerUse() { ++serial_; }
    uint64_t RecordedSerial() const { return serial_; }
    uint32_t DrawCount() const { return drawCount_; }
    uint32_t FailedDraws() const { return failedDraws_; }
    size_t PendingCount() const { return pending_.size(); }
    size_t BatchCount() const { return pending_.size() + free_.size(); }
    void ReleaseCompletedThrough(uint64_t completed) {
        drawTimer_.ReleaseCompletedThrough(completed);maskTimer_.ReleaseCompletedThrough(completed);
        for (auto it = pending_.begin(); it != pending_.end();) {
            if ((*it)->serial <= completed) {
                if ((*it)->generation != targetGeneration_) (*it)->framebuffer.reset();
                free_.push_back(std::move(*it)); it = pending_.erase(it); }
            else ++it;
        }
        // Destroy all completed framebuffers before freeing their attachment resources.
        for (auto& r : retired_) if (r.serial <= completed) r.framebuffer.reset();
        std::erase_if(retired_, [completed](const Retired& r) { return r.serial <= completed; });
    }
};
}
#endif
