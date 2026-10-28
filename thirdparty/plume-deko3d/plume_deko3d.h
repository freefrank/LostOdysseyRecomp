//
// plume deko3d back end (banjo-nx, 2026-10-17): plume's render interface on deko3d, the Switch's own GPU API, for
// Renut's renderer (docs/deko3d-renderer.md). MIT, as plume. deko3d is zlib (fincs, devkitPro); the shaders are compiled
// by uam (thirdparty/uam: zlib, Mesa's files MIT, SwitchWakerHD's patches MPL-2.0).
//
// Shaders come as RenderShaderFormat::DEKO3D: a Deko3DShaderBlob header, then GLSL that uam takes (tools/deko3d_glsl.py
// wrote it, with its "// banjo-dk" line of bindings), then optionally the DKSH of the default variant. A pipeline's
// specialization constants select the variant: the same GLSL with the constants defined first, compiled once by uam
// (the device's compile thread) unless a DKSH cache has it.
//
#pragma once

#include "plume_render_interface.h"
#include "plume_deko3d_api.h"

#include <deko3d.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace plume {
    // RenderShaderFormat::DEKO3D's container.
    struct Deko3DShaderBlob {
        static constexpr uint32_t Magic = 0x334B4442; // "BDK3"
        uint32_t magic = Magic;
        uint32_t stage = 0;     // DkStage
        uint32_t glslSize = 0;  // bytes of GLSL after this header
        uint32_t dkshSize = 0;  // bytes of DKSH after the GLSL (the default variant), or 0
    };


    // Counters for the renderer's reports.
    struct Deko3DStats {
        std::atomic<uint64_t> shaderCompiles{0}, shaderCompileNs{0}, shaderCacheHits{0}, shaderCompileFailures{0}, shaderTranslateNs{0}, translations{0}, translationHits{0};
        std::atomic<uint64_t> imageBytes{0}, bufferBytes{0}, codeBytes{0}, commandBytes{0};
        std::atomic<uint64_t> descriptorWrites{0}, pipelineBinds{0}, stateBinds{0}, draws{0};
        std::atomic<uint64_t> windowDraws{0}; // draws whose shaders read fetch data from memory (storage binding 15)
        // banjo-nx (2026-10-19): the barriers recorded, by what they wait for (DkBarrier_Primitives, _Fragments, _Full:
        // the 3D pipe drained, the copy engine as well), and those that invalidate the texture cache.
        std::atomic<uint64_t> barriersPrimitives{0}, barriersFragments{0}, barriersFull{0}, barriersImageInvalidate{0};
        // banjo-nx (2026-10-19): copy-engine and 2D-engine work, by kind (each fenced by Full barriers in Renut):
        // buffer copies, buffer-to-image (texture uploads), image-to-buffer (readbacks), image copies, resolves, blits.
        std::atomic<uint64_t> copyBuffer{0}, copyUpload{0}, copyReadback{0}, copyImage{0}, copyResolve{0}, copyBlit{0};
    };
    // banjo-nx (2026-10-19, render-thread proposal 13): read inline (no call per draw).
    extern Deko3DStats g_deko3dStats;
    inline Deko3DStats &GetDeko3DStats() { return g_deko3dStats; }

    // Where the back end's messages go (stderr until set: the application's log, so they reach it on the console and in
    // an emulator alike).
    void SetDeko3DLogCallback(void (*callback)(const char *message));

    struct Deko3DDevice;
    struct Deko3DCommandQueue;

    struct Deko3DBuffer : RenderBuffer {
        Deko3DDevice *device = nullptr;
        RenderBufferDesc desc;
        DkMemBlock block = nullptr;
        DkGpuAddr gpu = 0;
        uint8_t *cpu = nullptr;
        bool cpuCached = false;

        Deko3DBuffer(Deko3DDevice *device, const RenderBufferDesc &desc);
        ~Deko3DBuffer() override;
        void *map(uint32_t subresource, const RenderRange *readRange) override;
        void unmap(uint32_t subresource, const RenderRange *writtenRange) override;
        std::unique_ptr<RenderBufferFormattedView> createBufferFormattedView(RenderFormat format) override;
        void setName(const std::string &name) override;
        uint64_t getDeviceAddress() const override;
    };

    struct Deko3DBufferFormattedView : RenderBufferFormattedView {
        RenderFormat format = RenderFormat::UNKNOWN;
        Deko3DBuffer *buffer = nullptr;
    };

    // A piece of the device's image heap.
    struct Deko3DImageAlloc {
        DkMemBlock block = nullptr;
        uint32_t offset = 0;
        uint32_t size = 0;
        int32_t chunk = -1;     // -1: its own block (swapchain images)
    };

    struct Deko3DTexture : RenderTexture {
        Deko3DDevice *device = nullptr;
        RenderTextureDesc desc;
        DkImageLayout layout{};
        DkImage image{};
        DkImageFormat format = DkImageFormat_None;
        DkImageType type = DkImageType_2D;
        Deko3DImageAlloc alloc;
        bool ownsMemory = true;
        bool ownsBlock = false;  // LostOdysseyRecomp (2026-10-28): its own memory block (compressed render targets)
        bool valid = false;

        Deko3DTexture(Deko3DDevice *device, const RenderTextureDesc &desc);
        Deko3DTexture(Deko3DDevice *device) : device(device) {} // swapchain images
        ~Deko3DTexture() override;
        std::unique_ptr<RenderTextureView> createTextureView(const RenderTextureViewDesc &desc) const override;
        void setName(const std::string &name) override;
        // The view of the whole texture as plume's default (no view given).
        DkImageView defaultView() const;
    };

    struct Deko3DTextureView : RenderTextureView {
        const Deko3DTexture *texture = nullptr;
        DkImageView view{};
        RenderTextureViewDesc desc;
    };

    struct Deko3DSampler : RenderSampler {
        DkSamplerDescriptor descriptor{};
        RenderSamplerDesc desc;
    };

    // The bindings a shader uses (its banjo-dk line).
    struct Deko3DShaderUsage {
        uint32_t uboMask = 0;
        uint32_t ssboMask = 0;
        bool textures = false;
        // Bound textures (the game shaders' texb): binding k's handle is made from two words of the shared constants
        // (UBO binding 2): the texture word's low 20 bits and the sampler word's high 12 (SamplerNone: sampler 0).
        struct TexBinding {
            uint16_t textureWord = 0, samplerWord = 0;
        };
        static constexpr uint16_t SamplerNone = 0xFFFF;
        std::vector<TexBinding> texBindings;
        // LostOdysseyRecomp (2026-10-27): shaders translated from SPIR-V (plume_deko3d_spirv.h): where each combined
        // binding's texture and sampler come from (the descriptor sets bound at the draw, and a uniform word).
        std::vector<Deko3DSpirvTexture> textureSources;
        // Storage windows at an address the push constants hold (Deko3DSpirvResult::pushWindows).
        std::vector<Deko3DSpirvResult::PushWindow> pushWindows;
        // Vertex shaders: the original location of each attribute the shader reads, in its attribute order.
        std::vector<uint32_t> vertexInputs;
        // banjo-nx (2026-10-18): the user varying locations (below 32) the shader declares: a pixel shader's inputs, a
        // vertex shader's outputs (specialization constant 905, createGraphicsPipeline).
        uint32_t inputMask = 0;
        uint32_t outputMask = 0;
        // A vertex shader's outputs it writes as zero (its "// banjo-vszero=0x..." line): constant 906 of the pixel shader.
        uint32_t zeroOutputMask = 0;
        // banjo-nx (2026-10-19, GPU): the texture bindings whose level-0 size the shader reads from the texture size
        // buffer (UBO binding 3, entry k of the pixel stage, 32 + k of the vertex stage) instead of a texture query
        // (the banjo-dk line's txq=; tools/deko3d_glsl_opt.py, pass txq).
        uint32_t txqMask = 0;
    };

    struct Deko3DShader : RenderShader {
        Deko3DDevice *device = nullptr;
        DkStage stage = DkStage_Vertex;
        std::string glsl;
        std::vector<uint8_t> defaultDksh;
        Deko3DShaderUsage usage;
        // Specialization constant ids the GLSL defines (SPIRV_CROSS_CONSTANT_ID_n), so a variant key holds only those.
        std::vector<uint32_t> specIds;
        std::string name;
        // LostOdysseyRecomp (2026-10-27): a SPIR-V shader keeps its words; its GLSL depends on the pipeline layout (the
        // deko3d bindings), so it is translated per layout when a pipeline uses it (Deko3DDevice::translated).
        std::vector<uint32_t> spirv;
        Deko3DSpirvStage spirvStage = Deko3DSpirvStage::Vertex;
        mutable std::mutex translateMutex;
        mutable std::map<const void *, std::unique_ptr<RenderShader>> translations; // by pipeline layout

        void setName(const std::string &name) override { this->name = name; }
    };

    // A loaded variant: DKSH in the device's code memory.
    struct Deko3DShaderVariant {
        DkShader shader{};
        bool valid = false;
    };

    struct Deko3DPipelineLayout : RenderPipelineLayout {
        struct Range {
            RenderDescriptorRangeType type = RenderDescriptorRangeType::UNKNOWN;
            uint32_t binding = 0;
            uint32_t count = 0;
            uint32_t descriptorIndex = 0; // first descriptor of the range in the set's contiguous numbering
            uint32_t slot = 0;           // deko3d UBO / SSBO binding of the first descriptor
        };
        struct Set {
            std::vector<Range> ranges;
        };
        std::vector<Set> sets;
        std::vector<RenderPushConstantRange> pushRanges;
        uint32_t pushSize = 0;
    };

    struct Deko3DDescriptorSet : RenderDescriptorSet {
        struct Range {
            RenderDescriptorRangeType type = RenderDescriptorRangeType::UNKNOWN;
            uint32_t binding = 0;
            uint32_t first = 0;  // first descriptor index
            uint32_t count = 0;
        };
        Deko3DDevice *device = nullptr;
        std::vector<Range> ranges;
        // Image and sampler descriptors: a block each (32 bytes a descriptor), the deko3d descriptor sets that bind
        // when this set is bound. Index i of the image table is image id i (and sampler id i).
        DkMemBlock imageBlock = nullptr, samplerBlock = nullptr;
        uint32_t imageCount = 0, samplerCount = 0;
        // LostOdysseyRecomp (2026-10-27): the descriptors live in the device's image and sampler heaps (bound once a
        // list), so any number of sets can be bound together: their first ids there (UINT32_MAX: none).
        uint32_t imageHeapBase = UINT32_MAX, samplerHeapBase = UINT32_MAX;
        uint32_t imageFirst = 0, samplerFirst = 0; // descriptor index of the first image / sampler descriptor
        // Buffers (constant and storage): address and size per descriptor index.
        std::vector<DkBufExtents> buffers;
        // banjo-nx (2026-10-19, GPU): per image id, the level-0 size of the view written there as a texture query
        // reads it (width, height, depth or layers), for the texture size buffer (txq).
        std::vector<std::array<uint32_t, 3>> imageSizes;

        Deko3DDescriptorSet(Deko3DDevice *device, const RenderDescriptorSetDesc &desc);
        ~Deko3DDescriptorSet() override;
        void setBuffer(uint32_t descriptorIndex, const RenderBuffer *buffer, uint64_t bufferSize, const RenderBufferStructuredView *bufferStructuredView, const RenderBufferFormattedView *bufferFormattedView) override;
        void setTexture(uint32_t descriptorIndex, const RenderTexture *texture, RenderTextureLayout textureLayout, const RenderTextureView *textureView) override;
        void setSampler(uint32_t descriptorIndex, const RenderSampler *sampler) override;
        void setAccelerationStructure(uint32_t descriptorIndex, const RenderAccelerationStructure *accelerationStructure) override;
        const Range *rangeOf(uint32_t descriptorIndex) const;
    };

    struct Deko3DPipeline : RenderPipeline {
        enum class Type { Graphics, Compute };
        Type type = Type::Graphics;
        Deko3DDevice *device = nullptr;
        const Deko3DPipelineLayout *layout = nullptr;
        std::string name;
        void setName(const std::string &name) override { this->name = name; }
        RenderPipelineProgram getProgram(const std::string &) const override { return {}; }
    };

    struct Deko3DGraphicsPipeline : Deko3DPipeline {
        const DkShader *shaders[3] = {};  // vertex, geometry, fragment (null when absent)
        uint32_t stageMask = 0;
        uint32_t shaderCount = 0;
        bool valid = false;
        DkPrimitive primitive = DkPrimitive_Triangles;
        DkRasterizerState rasterizer{};
        DkMultisampleState multisample{};
        DkColorState color{};
        DkColorWriteState colorWrite{};
        DkBlendState blend[8]{};
        uint32_t blendWords[8][7]{}; // the IndependentBlend registers written again (deko3d 0.5.0's FuncAlphaDst)
        uint32_t blendCount = 0;
        DkDepthStencilState depthStencil{};
        uint8_t stencilReadMask = 0xFF, stencilWriteMask = 0xFF, stencilReference = 0;
        bool stencilEnabled = false;
        bool dynamicStencilReference = false;
        float depthBias = 0.0f, depthBiasClamp = 0.0f, slopeScaledDepthBias = 0.0f;
        bool dynamicDepthBias = false;
        DkVtxAttribState attribs[32]{};
        uint32_t attribCount = 0;
        DkVtxBufferState vtxBuffers[16]{};
        uint32_t vtxBufferCount = 0;
        bool usesBlendConstant = false;
        // UBO / SSBO slots each stage reads (vertex, geometry, fragment).
        uint32_t uboMask[3] = {}, ssboMask[3] = {};
        std::vector<Deko3DShaderUsage::TexBinding> texBindings[3];
        std::vector<Deko3DSpirvTexture> textureSources[3];  // LostOdysseyRecomp (2026-10-27)
        std::vector<Deko3DSpirvResult::PushWindow> pushWindows[3];
        bool hasPushWindows = false;
        uint32_t txqMask[3] = {};  // banjo-nx (2026-10-19): Deko3DShaderUsage::txqMask of each stage
        bool boundTextures = false;
        bool textures = false;
        uint64_t stateHash = 0; // equal hashes: the same fixed-function state (binds skipped)
        // banjo-nx (2026-10-19, CPU proposal 13): the shader of each stage (vertex, geometry, fragment; null when
        // absent), so a bind does not ask deko3d, and a hash of each group of the fixed-function state, so a bind
        // after a pipeline of other state sends only the groups that differ (kStateGroups, Deko3DCommandList).
        const DkShader *stageShaders[3] = {};
        uint64_t groupHash[9] = {};
    };

    struct Deko3DComputePipeline : Deko3DPipeline {
        const DkShader *shader = nullptr;
        bool valid = false;
        uint32_t uboMask = 0, ssboMask = 0;
    };

    struct Deko3DCommandFence : RenderCommandFence {
        DkFence fence{};
        bool pending = false;
    };

    struct Deko3DCommandSemaphore : RenderCommandSemaphore {
    };

    struct Deko3DFramebuffer : RenderFramebuffer {
        DkImageView colors[8]{};
        uint32_t colorCount = 0;
        DkImageView depth{};
        bool hasDepth = false;
        uint32_t width = 0, height = 0;
        bool srgb[8] = {};
        uint32_t getWidth() const override { return width; }
        uint32_t getHeight() const override { return height; }
    };

    struct Deko3DQueryPool : RenderQueryPool {
        Deko3DDevice *device = nullptr;
        DkMemBlock block = nullptr;
        std::vector<uint64_t> results;
        Deko3DQueryPool(Deko3DDevice *device, uint32_t count);
        ~Deko3DQueryPool() override;
        void queryResults() override;
        bool queryResults(uint32_t count);
        bool partialResults() const { return true; }
        const uint64_t *getResults() const override { return results.data(); }
        uint32_t getCount() const override { return uint32_t(results.size()); }
    };

    struct Deko3DSwapChain;

    struct Deko3DCommandList : RenderCommandList {
        Deko3DCommandQueue *queue = nullptr;
        Deko3DDevice *device = nullptr;
        DkCmdBuf cmd = nullptr;
        DkCmdList list = 0;
        bool open = false;
        // Command memory: blocks reused from the start at each begin() (the application waits for the list's last
        // submission before it begins it again, as Vulkan requires).
        std::vector<std::pair<DkMemBlock, uint32_t>> memory;
        size_t memoryUsed = 0;
        // Transient data (compute push constants): a linear area per list, reset at begin().
        std::vector<std::pair<DkMemBlock, uint32_t>> transient;
        size_t transientBlock = 0;
        uint32_t transientUsed = 0;

        // What is bound (to skip binds that change nothing).
        const Deko3DGraphicsPipeline *graphicsPipeline = nullptr;
        const Deko3DComputePipeline *computePipeline = nullptr;
        uint64_t boundStateHash = 0;
        const DkShader *boundShaders[3] = {};
        // banjo-nx (2026-10-19): the groups of fixed-function state as last bound (Deko3DGraphicsPipeline::groupHash;
        // 0: unknown). The dynamic depth bias and stencil reference make theirs unknown.
        uint64_t boundGroups[9] = {};
        const Deko3DPipelineLayout *graphicsLayout = nullptr, *computeLayout = nullptr;
        DkGpuAddr boundImageSet = 0, boundSamplerSet = 0;
        uint32_t boundImageCount = 0, boundSamplerCount = 0;
        // banjo-nx (2026-10-19, GPU): the descriptor set whose image table is bound (its sizes, Deko3DDescriptorSet::
        // imageSizes), and what the texture size buffer holds after this list's updates so far (valid bit per entry).
        const struct Deko3DDescriptorSet *boundImageSetObj = nullptr;
        uint32_t texSizeShadow[64][4] = {};
        uint64_t texSizeValid = 0;
        // LostOdysseyRecomp (2026-10-27): the graphics descriptor set bound at each set index (texture handles).
        const struct Deko3DDescriptorSet *graphicsSets[8] = {};
        // The graphics push constants as last set (push constant address windows read them at each draw).
        uint8_t graphicsPushShadow[256] = {};
        void bindPushWindows();
        DkBufExtents graphicsUbo[16]{}, graphicsSsbo[16]{};
        uint32_t graphicsUboValid = 0, graphicsSsboValid = 0;
        uint32_t uboBoundStages[16] = {}, ssboBoundStages[16] = {}; // DkStage bits a slot was bound for
        DkBufExtents computeUbo[16]{}, computeSsbo[16]{};
        uint32_t computeUboValid = 0, computeSsboValid = 0;
        bool computeUboDirty = true, computeSsboDirty = true;
        uint8_t computePush[256]{};
        uint32_t computePushSize = 0;
        DkIdxFormat indexFormat = DkIdxFormat_Uint16, boundIndexFormat = DkIdxFormat_Uint16;
        uint32_t indexSize = 2;
        DkGpuAddr indexAddress = 0;
        bool descriptorsInvalidated = false;
        uint64_t descriptorEpoch = 0;
        const Deko3DFramebuffer *framebuffer = nullptr;
        // Bound textures: the handles last bound per graphics stage, and the buffer the shared constants were last
        // found in (GPU range and CPU address).
        DkResHandle boundTex[3][32]{};
        uint32_t boundTexCount[3] = {};
        DkGpuAddr sharedBase = 0;
        uint32_t sharedSize = 0;
        const uint8_t *sharedCpu = nullptr;
        // banjo-nx (2026-10-19, render-thread proposal 5, Deko3DSetSharedWords): the CPU's copy of the shared constants
        // the next draws read, when the renderer pushes them (its uniform buffer holds them only on the GPU's side).
        const uint8_t *sharedWords = nullptr;
        uint32_t sharedWordsSize = 0;
        DkViewport viewports[16]{};
        uint32_t viewportCount = 0;
        DkScissor scissors[16]{};
        uint32_t scissorCount = 0;
        float blendConstant[4] = {};

        Deko3DCommandList(Deko3DCommandQueue *queue);
        ~Deko3DCommandList() override;
        void begin() override;
        void end() override;
        void barriers(RenderBarrierStages stages, const RenderBufferBarrier *bufferBarriers, uint32_t bufferBarriersCount, const RenderTextureBarrier *textureBarriers, uint32_t textureBarriersCount) override;
        void dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) override;
        void traceRays(uint32_t width, uint32_t height, uint32_t depth, RenderBufferReference shaderBindingTable, const RenderShaderBindingGroupsInfo &shaderBindingGroupsInfo) override;
        void drawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) override;
        void drawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) override;
        void setPipeline(const RenderPipeline *pipeline) override;
        void setComputePipelineLayout(const RenderPipelineLayout *pipelineLayout) override;
        void setComputePushConstants(uint32_t rangeIndex, const void *data, uint32_t offset, uint32_t size) override;
        void setComputeDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) override;
        void setGraphicsPipelineLayout(const RenderPipelineLayout *pipelineLayout) override;
        void setGraphicsPushConstants(uint32_t rangeIndex, const void *data, uint32_t offset, uint32_t size) override;
        void setGraphicsDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) override;
        void setGraphicsDescriptorSetDynamic(RenderDescriptorSet *descriptorSet, uint32_t setIndex, const uint32_t *dynamicOffsets, uint32_t dynamicOffsetsCount) override;
        void setGraphicsRootDescriptor(RenderBufferReference bufferReference, uint32_t rootDescriptorIndex) override;
        void setRaytracingPipelineLayout(const RenderPipelineLayout *pipelineLayout) override;
        void setRaytracingPushConstants(uint32_t rangeIndex, const void *data, uint32_t offset, uint32_t size) override;
        void setRaytracingDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) override;
        void setIndexBuffer(const RenderIndexBufferView *view) override;
        void setVertexBuffers(uint32_t startSlot, const RenderVertexBufferView *views, uint32_t viewCount, const RenderInputSlot *inputSlots) override;
        void setViewports(const RenderViewport *viewports, uint32_t count) override;
        void setScissors(const RenderRect *scissorRects, uint32_t count) override;
        void setFramebuffer(const RenderFramebuffer *framebuffer) override;
        void setDepthBias(float depthBias, float depthBiasClamp, float slopeScaledDepthBias) override;
        void setStencilReference(uint32_t reference);
        void clearColor(uint32_t attachmentIndex, RenderColor colorValue, const RenderRect *clearRects, uint32_t clearRectsCount) override;
        void clearDepthStencil(bool clearDepth, bool clearStencil, float depthValue, uint32_t stencilValue, const RenderRect *clearRects, uint32_t clearRectsCount) override;
        void copyBufferRegion(RenderBufferReference dstBuffer, RenderBufferReference srcBuffer, uint64_t size) override;
        void copyTextureRegion(const RenderTextureCopyLocation &dstLocation, const RenderTextureCopyLocation &srcLocation, uint32_t dstX, uint32_t dstY, uint32_t dstZ, const RenderBox *srcBox) override;
        void copyBuffer(const RenderBuffer *dstBuffer, const RenderBuffer *srcBuffer) override;
        void copyTexture(const RenderTexture *dstTexture, const RenderTexture *srcTexture) override;
        void resolveTexture(const RenderTexture *dstTexture, const RenderTexture *srcTexture) override;
        void resolveTextureRegion(const RenderTexture *dstTexture, uint32_t dstX, uint32_t dstY, const RenderTexture *srcTexture, const RenderRect *srcRect, RenderResolveMode resolveMode) override;
        void buildBottomLevelAS(const RenderAccelerationStructure *dstAccelerationStructure, RenderBufferReference scratchBuffer, const RenderBottomLevelASBuildInfo &buildInfo) override;
        void buildTopLevelAS(const RenderAccelerationStructure *dstAccelerationStructure, RenderBufferReference scratchBuffer, RenderBufferReference instancesBuffer, const RenderTopLevelASBuildInfo &buildInfo) override;
        void discardTexture(const RenderTexture *texture) override;
        void resetQueryPool(const RenderQueryPool *queryPool, uint32_t queryFirstIndex, uint32_t queryCount) override;
        void writeTimestamp(const RenderQueryPool *queryPool, uint32_t queryIndex) override;

        // banjo-nx: the blend constant (BLEND_FACTOR), which plume has no call for.
        void setBlendConstant(const float rgba[4]);

        // internal
        void addMemory(size_t minimum);
        bool transientAlloc(uint32_t size, uint32_t alignment, void **cpu, DkGpuAddr *gpu);
        void flushDescriptorInvalidate();
        // banjo-nx (2026-10-19, Deko3DOptions::mergeBarriers): the barrier recorded but not yet emitted.
        DkBarrier pendingBarrierMode = DkBarrier_None;
        uint32_t pendingBarrierInvalidate = 0;
        bool pendingBarrier = false;
        // banjo-nx (2026-10-19, render-thread proposal 13): per-draw counters of this list, added to the totals at end()
        // (no atomic read-modify-write per draw: the A57 has no LSE).
        uint64_t statDraws = 0, statWindowDraws = 0;
        void emitBarrier(DkBarrier mode, uint32_t invalidate);
        void flushBarriers() {
            if (pendingBarrier)
                emitBarrier(pendingBarrierMode, pendingBarrierInvalidate);
        }
        void invalidateZcullAfterCopy(const Deko3DTexture *dst);
        uint64_t currentEpoch() const;
        void bindGraphicsBuffers();
        void bindTextures();
        void restoreScissors();
    };

    struct Deko3DCommandQueue : RenderCommandQueue {
        Deko3DDevice *device = nullptr;
        RenderCommandListType type = RenderCommandListType::UNKNOWN;

        Deko3DCommandQueue(Deko3DDevice *device, RenderCommandListType type);
        std::unique_ptr<RenderCommandList> createCommandList() override;
        std::unique_ptr<RenderSwapChain> createSwapChain(const RenderSwapChainDesc &desc) override;
        void executeCommandLists(const RenderCommandList **commandLists, uint32_t commandListCount, RenderCommandSemaphore **waitSemaphores, uint32_t waitSemaphoreCount, RenderCommandSemaphore **signalSemaphores, uint32_t signalSemaphoreCount, RenderCommandFence *signalFence) override;
        void waitForCommandFence(RenderCommandFence *fence) override;
    };

    struct Deko3DSwapChain : RenderSwapChain {
        Deko3DCommandQueue *queue = nullptr;
        Deko3DDevice *device = nullptr;
        RenderSwapChainDesc desc;
        DkSwapchain swapchain = nullptr;
        std::vector<std::unique_ptr<Deko3DTexture>> textures;
        std::vector<DkMemBlock> blocks;
        uint32_t width = 0, height = 0;
        bool vsync = true;

        Deko3DSwapChain(Deko3DCommandQueue *queue, const RenderSwapChainDesc &desc);
        ~Deko3DSwapChain() override;
        bool present(uint32_t textureIndex, RenderCommandSemaphore **waitSemaphores, uint32_t waitSemaphoreCount) override;
        void wait() override;
        bool resize() override;
        bool needsResize() const override;
        void setVsyncEnabled(bool vsyncEnabled) override;
        bool isVsyncEnabled() const override { return vsync; }
        uint32_t getWidth() const override { return width; }
        uint32_t getHeight() const override { return height; }
        RenderTexture *getTexture(uint32_t textureIndex) override;
        uint32_t getTextureCount() const override { return uint32_t(textures.size()); }
        bool acquireTexture(RenderCommandSemaphore *signalSemaphore, uint32_t *textureIndex) override;
        RenderWindow getWindow() const override { return desc.renderWindow; }
        bool isEmpty() const override { return swapchain == nullptr; }
        uint32_t getRefreshRate() const override { return 60; }
        void destroy();
    };

    struct Deko3DDevice : RenderDevice {
        DkDevice dk = nullptr;
        DkQueue queue = nullptr;
        RenderDeviceCapabilities capabilities;
        RenderDeviceDescription description;

        // Image heap: chunks with first-fit free lists; frees wait for the GPU (see retire()).
        struct Chunk {
            DkMemBlock block = nullptr;
            uint32_t size = 0;
            std::vector<std::pair<uint32_t, uint32_t>> freeList; // offset, size, sorted by offset
        };
        std::mutex heapMutex;
        std::vector<Chunk> chunks;

        // Shader code: blocks, bump allocated.
        std::mutex codeMutex;
        std::vector<std::pair<DkMemBlock, uint32_t>> codeBlocks; // block, used
        std::atomic<uint64_t> codeEpoch{1}; // advances with each code load (command lists invalidate the shader cache)
        std::unordered_map<uint64_t, std::unique_ptr<Deko3DShaderVariant>> variants; // by variant source hash
        std::mutex variantMutex;

        // The uam worker: one thread (uam is not reentrant), 8 MB of stack.
        std::thread compileThread;
        std::mutex compileMutex;
        std::condition_variable compileWake, compileDone;
        struct CompileJob {
            DkStage stage;
            const std::string *source;
            std::vector<uint8_t> *dksh;
            std::string *log;
            bool done = false;
            bool ok = false;
        };
        std::vector<CompileJob *> compileQueue;
        // banjo-nx (2026-10-18): the uam thread's handle, the priorities of the threads waiting for a compile, and the
        // priority last set (applyCompilePriority).
        uint32_t compileThreadHandle = 0;
        std::vector<int32_t> compileWaiters;
        int32_t compileAppliedPriority = -1;
        bool compileStop = false;
        bool compilerReady = false;

        // Retired memory: freed once the queue has passed the submission that was current when it was retired.
        std::mutex retireMutex;
        struct Retired {
            uint64_t serial;
            Deko3DImageAlloc image;
            DkMemBlock block;
        };
        std::vector<Retired> retired;
        std::atomic<uint64_t> submitSerial{1};
        std::atomic<uint64_t> completedSerial{0};
        static constexpr uint32_t SerialFences = 16;
        DkFence serialFences[SerialFences]{};
        uint64_t serialFenceValue[SerialFences] = {};

        // LostOdysseyRecomp (2026-10-27): the image and sampler descriptor heaps every descriptor set takes its
        // descriptors from (32 bytes each), with first-fit free lists; frees wait for the GPU like memory (retire).
        static constexpr uint32_t ImageHeapCount = 65536, SamplerHeapCount = 4096;
        DkMemBlock imageHeap = nullptr, samplerHeap = nullptr;
        DkGpuAddr imageHeapAddr = 0, samplerHeapAddr = 0;
        std::mutex descriptorHeapMutex;
        std::vector<std::pair<uint32_t, uint32_t>> imageHeapFree, samplerHeapFree; // first, count (sorted by first)
        struct RetiredDescriptors {
            uint64_t serial;
            bool sampler;
            uint32_t first, count;
        };
        std::vector<RetiredDescriptors> retiredDescriptors;
        uint32_t heapAlloc(bool sampler, uint32_t count);
        void heapRetire(bool sampler, uint32_t first, uint32_t count);

        // Descriptors written by the CPU since the GPU's descriptor cache was last invalidated (by a command list).
        std::atomic<uint64_t> descriptorEpoch{1};

        // The push constant UBO (graphics stages; dkCmdBufPushConstants updates it in command order).
        DkMemBlock pushBlock = nullptr;
        DkGpuAddr pushAddr = 0;
        static constexpr uint32_t PushSize = 256;
        // banjo-nx (2026-10-19, GPU): the texture size buffer (UBO binding 3: 64 uvec4, pixel bindings then vertex),
        // updated in command order like the push constants.
        DkMemBlock texSizeBlock = nullptr;
        DkGpuAddr texSizeAddr = 0;
        static constexpr uint32_t TexSizeSize = 64 * 16;
        // Zeros for missing vertex buffers (Vulkan's null buffer).
        DkMemBlock zeroBlock = nullptr;
        DkGpuAddr zeroAddr = 0;
        static constexpr uint32_t ZeroSize = 4096;
        // Scratch (local) memory per warp the queue was made with; a shader that needs more is refused (logged).
        uint32_t perWarpScratch = 0;

        // Every buffer by its GPU address (bound textures read the shared constants' words through it).
        std::mutex bufferMutex;
        std::map<DkGpuAddr, std::pair<uint32_t, const uint8_t *>> bufferRanges; // GPU address -> size, CPU address
        bool findBuffer(DkGpuAddr address, DkGpuAddr &base, uint32_t &size, const uint8_t *&cpu);

        Deko3DDevice();
        ~Deko3DDevice() override;
        bool initialize();

        std::unique_ptr<RenderDescriptorSet> createDescriptorSet(const RenderDescriptorSetDesc &desc) override;
        std::unique_ptr<RenderShader> createShader(const void *data, uint64_t size, const char *entryPointName, RenderShaderFormat format) override;
        std::unique_ptr<RenderSampler> createSampler(const RenderSamplerDesc &desc) override;
        std::unique_ptr<RenderPipeline> createComputePipeline(const RenderComputePipelineDesc &desc) override;
        std::unique_ptr<RenderPipeline> createGraphicsPipeline(const RenderGraphicsPipelineDesc &desc) override;
        std::unique_ptr<RenderPipeline> createRaytracingPipeline(const RenderRaytracingPipelineDesc &desc, const RenderPipeline *previousPipeline) override;
        std::unique_ptr<RenderCommandQueue> createCommandQueue(RenderCommandListType type) override;
        std::unique_ptr<RenderBuffer> createBuffer(const RenderBufferDesc &desc) override;
        std::unique_ptr<RenderTexture> createTexture(const RenderTextureDesc &desc) override;
        std::unique_ptr<RenderAccelerationStructure> createAccelerationStructure(const RenderAccelerationStructureDesc &desc) override;
        std::unique_ptr<RenderPool> createPool(const RenderPoolDesc &desc) override;
        std::unique_ptr<RenderPipelineLayout> createPipelineLayout(const RenderPipelineLayoutDesc &desc) override;
        std::unique_ptr<RenderCommandFence> createCommandFence() override;
        std::unique_ptr<RenderCommandSemaphore> createCommandSemaphore() override;
        std::unique_ptr<RenderFramebuffer> createFramebuffer(const RenderFramebufferDesc &desc) override;
        std::unique_ptr<RenderQueryPool> createQueryPool(uint32_t queryCount) override;
        void setBottomLevelASBuildInfo(RenderBottomLevelASBuildInfo &buildInfo, const RenderBottomLevelASMesh *meshes, uint32_t meshCount, bool preferFastBuild, bool preferFastTrace) override;
        void setTopLevelASBuildInfo(RenderTopLevelASBuildInfo &buildInfo, const RenderTopLevelASInstance *instances, uint32_t instanceCount, bool preferFastBuild, bool preferFastTrace) override;
        void setShaderBindingTableInfo(RenderShaderBindingTableInfo &tableInfo, const RenderShaderBindingGroups &groups, const RenderPipeline *pipeline, RenderDescriptorSet **descriptorSets, uint32_t descriptorSetCount) override;
        const RenderDeviceCapabilities &getCapabilities() const override { return capabilities; }
        const RenderDeviceDescription &getDescription() const override { return description; }
        RenderSampleCounts getSampleCountsSupported(RenderFormat format) const override;
        bool beginCapture() override { return false; }
        bool endCapture() override { return false; }

        // internal
        DkMemBlock makeBlock(uint32_t size, uint32_t flags, const char *what);
        void destroyBlock(DkMemBlock block);
        // PLUME_DEKO3D_GUARDS (debug builds): every block's storage has guard bytes after it, checked here.
        void checkGuards();
        struct GuardedBlock {
            DkMemBlock block;
            uint8_t *storage;
            uint32_t size;
            const char *what;
        };
        std::mutex guardMutex;
        std::vector<GuardedBlock> guarded;
        bool imageAlloc(uint32_t size, uint32_t alignment, Deko3DImageAlloc &out);
        void retire(const Deko3DImageAlloc &image, DkMemBlock block);
        void collectRetired();
        void noteSubmission(); // after a submission: a fence for its serial
        const DkShader *loadVariant(const Deko3DShader *shader, const RenderSpecConstant *specConstants, uint32_t specConstantsCount);
        // LostOdysseyRecomp (2026-10-27): a SPIR-V shader's translation for this layout (null when it fails, logged);
        // a GLSL shader itself.
        const Deko3DShader *translated(const Deko3DShader *shader, const Deko3DPipelineLayout *layout);
        bool loadByKey(const Deko3DShader *shader, uint64_t key, const DkShader *&out);
        bool translationText(const Deko3DShader *shader);
        bool compile(DkStage stage, const std::string &source, std::vector<uint8_t> &dksh, std::string &log);
        bool loadCode(const std::vector<uint8_t> &dksh, DkShader &shader, const char *what);
        void compileWorker();
        void applyCompilePriority();
    };

    struct Deko3DInterface : RenderInterface {
        RenderInterfaceCapabilities capabilities;
        std::vector<std::string> deviceNames;

        Deko3DInterface();
        std::unique_ptr<RenderDevice> createDevice(const std::string &preferredDeviceName) override;
        const std::vector<std::string> &getDeviceNames() const override { return deviceNames; }
        const RenderInterfaceCapabilities &getCapabilities() const override { return capabilities; }
    };

    std::unique_ptr<RenderInterface> CreateDeko3DInterface();

    // banjo-nx: the blend constant on a deko3d command list (false when the list is not one).
    bool Deko3DSetBlendConstant(RenderCommandList *commandList, const float rgba[4]);
    // banjo-nx: the memexport buffer of the next draws (storage binding 14 of the graphics stages).
    bool Deko3DBindMemExport(RenderCommandList *commandList, const RenderBuffer *buffer);
    // banjo-nx: the vertex fetch data of the next draws (storage binding 15 of the graphics stages): size bytes from
    // address, which the shaders index from 0 (shader_common.h, g_DkAddressWindow).
    bool Deko3DBindFetchData(RenderCommandList *commandList, uint64_t address, uint32_t size);
    // banjo-nx (2026-10-19, render-thread proposals 2 and 5): a uniform buffer the renderer keeps up to date with
    // pipelined updates (Deko3DPushUniform: dkCmdBufPushConstants; the draws before an update keep the old values), bound
    // at 'slot' for the graphics stages that read it; the update of 'size' bytes at 'offset' of the 'uboSize'-byte
    // buffer at 'address'; and the CPU's copy of the shared constants bound textures are read from (bindTextures), for
    // a shared uniform buffer the renderer pushes (nullptr: read where the bound buffer's memory is, as before).
    bool Deko3DBindUniform(RenderCommandList *commandList, uint32_t slot, uint64_t address, uint32_t size);
    bool Deko3DPushUniform(RenderCommandList *commandList, uint64_t address, uint32_t uboSize, uint32_t offset,
                           const void *data, uint32_t size);
    bool Deko3DSetSharedWords(RenderCommandList *commandList, const void *cpu, uint32_t size);
    // banjo-nx: the queue lost (dkQueueIsInErrorState).
    bool Deko3DDeviceLost(RenderDevice *device);
    // sa2-nx (2026-10-20, 1.0.3.1): the size the swapchain should have, the console display's (1920x1080 docked,
    // 1280x720 handheld); 0 x 0 = the window's, as before. A swapchain of another size asks for a resize
    // (needsResize), which gives the window that size while no buffer is registered (nwindowSetDimensions) and makes
    // the images again. Any thread.
    void Deko3DSetSwapChainSize(uint32_t width, uint32_t height);
    // banjo-nx (2026-10-19, latency batch 3): a copy of what a command fence's last submit signals, taken on the thread
    // that submits (the fence itself is re-armed by the next submit of its slot), and whether the GPU has signalled it
    // (dkFenceWait with no wait): any thread may test its own copy.
    struct Deko3DFenceCopy {
        DkFence fence{};
        bool valid = false;  // a submit's signal to test
        bool done = false;   // already waited for (nothing pending)
    };
    Deko3DFenceCopy Deko3DCopyFence(const RenderCommandFence *fence);
    bool Deko3DFenceCopyDone(Deko3DFenceCopy &copy);
    // banjo-nx (2026-10-20, input latency): waits until the copy's signal arrives (or the queue fails); true when it did.
    bool Deko3DFenceCopyWait(Deko3DFenceCopy &copy, RenderDevice *device);

    // banjo-nx (2026-10-18, GPU bandwidth): options read when the device and its textures are made; set them before
    // CreateDeko3DInterface. depthCompression: depth/stencil targets get hardware compression (as colour targets do).
    // zcull: the queue keeps its Zcull context (hierarchical depth culling; SwitchWakerHD saw depth-biased decals dropped in
    // 4x8 tiles with it).
    struct Deko3DOptions {
        bool depthCompression = false;
        bool zcull = false;
        // LostOdysseyRecomp (2026-10-28): colour targets with hardware compression (on, as banjo-nx ships them).
        bool colorCompression = true;
        // A pipeline's vertex shader gets specialization constant 905 (when it declares it), the locations its pixel
        // shader reads of the ones it writes (none without a pixel shader; not with a geometry shader): banjo-nx's
        // tools/deko3d_glsl_opt.py, pass vsout, drops the other outputs and what computes them.
        bool vsOutputs = false;
        // uam's banjo-nx codegen passes (uam_api.h setBanjoFlags): 1 PRMT byte moves, 2 storage buffer offsets in the
        // load, 4 shift distribution, 8 + 16 the input interpolation cache where it pays.
        unsigned uamFlags = 447;
        // The uam thread's core (-1: any) and priority (Horizon: a higher number is a lower priority; 0: unchanged).
        // A thread that waits for a compile lends its priority to the uam thread while it waits.
        int compileCore = -1;
        int compilePriority = 0;
        // banjo-nx (2026-10-19, GPU): barriers recorded one after another with no work between them are one barrier
        // (the strongest wait and every invalidation of them), emitted right before the next command that does GPU
        // work: the same ordering, one drain instead of several.
        bool mergeBarriers = false;
        // banjo-nx (2026-10-19, GPU): a transition into a layout that is only written next (colour, depth, present)
        // waits for the work before it but keeps the texture cache: only a read after a write needs it invalidated,
        // and that read's own transition does it.
        bool leanBarriers = false;
    };
    void Deko3DSetOptions(const Deko3DOptions &options);
    // banjo-nx (2026-10-19, render-thread proposal 13): read inline (barriers() reads it per call).
    extern Deko3DOptions g_deko3dOptions;
    inline const Deko3DOptions &Deko3DGetOptions() { return g_deko3dOptions; }
};
