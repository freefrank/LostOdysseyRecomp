//
// plume deko3d back end (banjo-nx, 2026-10-17): command lists, the queue and the swapchain (plume_deko3d.cpp has the
// device). docs/deko3d-renderer.md section 4 maps each plume call to deko3d.
//
#include "plume_deko3d.h"

#include <switch.h>

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace plume {
    DkImageFormat toDkFormat(RenderFormat format);

    void deko3dLog(const char *fmt, ...); // plume_deko3d.cpp
#define logf deko3dLog

    static constexpr uint32_t alignUp(uint32_t v, uint32_t a) { return (v + a - 1) / a * a; }

    // The push constants' UBO binding in every stage, and the address window's storage binding (tools/deko3d_glsl.py).
    static constexpr uint32_t PushUboSlot = 5;
    static constexpr uint32_t TexSizeUboSlot = 3;  // banjo-nx (2026-10-19, txq)
    static constexpr uint32_t AddressWindowSlot = 15;
    static constexpr uint32_t CommandBlockSize = 1u << 20;
    static constexpr uint32_t TransientBlockSize = 256u << 10;
    static const DkStage GraphicsStages[3] = {DkStage_Vertex, DkStage_Geometry, DkStage_Fragment};
    // Debug builds: the first operations of each kind, logged (what the frame is made of).
    static std::atomic<uint32_t> g_traceClears{0}, g_traceCopies{0}, g_traceTargets{0};

    // ---- Deko3DCommandList

    static void addMemoryCallback(void *user, DkCmdBuf, size_t minimum) {
        static_cast<Deko3DCommandList *>(user)->addMemory(minimum);
    }

    Deko3DCommandList::Deko3DCommandList(Deko3DCommandQueue *queue) : queue(queue), device(queue->device) {
        DkCmdBufMaker m;
        dkCmdBufMakerDefaults(&m, device->dk);
        m.userData = this;
        m.cbAddMem = addMemoryCallback;
        cmd = dkCmdBufCreate(&m);
    }

    Deko3DCommandList::~Deko3DCommandList() {
        if (device->queue)
            dkQueueWaitIdle(device->queue);
        if (cmd)
            dkCmdBufDestroy(cmd);
        for (auto &m : memory)
            device->destroyBlock(m.first);
        for (auto &t : transient)
            device->destroyBlock(t.first);
    }

    void Deko3DCommandList::addMemory(size_t minimum) {
        const uint32_t want = alignUp(uint32_t(std::max<size_t>(minimum, CommandBlockSize)), DK_MEMBLOCK_ALIGNMENT);
        while (memoryUsed < memory.size() && memory[memoryUsed].second < want)
            memoryUsed++;
        if (memoryUsed == memory.size()) {
            DkMemBlock block = device->makeBlock(want, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, "command memory");
            memory.push_back({block, want});
            GetDeko3DStats().commandBytes += want;
        }
        dkCmdBufAddMemory(cmd, memory[memoryUsed].first, 0, memory[memoryUsed].second);
        memoryUsed++;
    }

    bool Deko3DCommandList::transientAlloc(uint32_t size, uint32_t alignment, void **cpu, DkGpuAddr *gpu) {
        if (size > TransientBlockSize)
            return false;
        for (;;) {
            if (transientBlock < transient.size()) {
                const uint32_t at = alignUp(transientUsed, alignment);
                if (at + size <= transient[transientBlock].second) {
                    *cpu = static_cast<uint8_t *>(dkMemBlockGetCpuAddr(transient[transientBlock].first)) + at;
                    *gpu = dkMemBlockGetGpuAddr(transient[transientBlock].first) + at;
                    transientUsed = at + size;
                    return true;
                }
                transientBlock++;
                transientUsed = 0;
                continue;
            }
            DkMemBlock block = device->makeBlock(TransientBlockSize, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, "transient data");
            transient.push_back({block, TransientBlockSize});
        }
    }

    void Deko3DCommandList::begin() {
        // The application waited for this list's last submission (as Vulkan requires before a reset): its command and
        // transient memory is free again. dkCmdBufClear rewinds into the memory fed last, so the first block is fed
        // explicitly (SwitchWakerHD: deko3d 0.5.0's clear keeps the old position).
        dkCmdBufClear(cmd);
        pendingBarrier = false;  // banjo-nx (2026-10-19): nothing held over from the list before
        pendingBarrierMode = DkBarrier_None;
        pendingBarrierInvalidate = 0;
        memoryUsed = 0;
        addMemory(0);
        transientBlock = 0;
        transientUsed = 0;
        open = true;
        list = 0;

        graphicsPipeline = nullptr;
        computePipeline = nullptr;
        boundStateHash = 0;
        boundShaders[0] = boundShaders[1] = boundShaders[2] = nullptr;
        std::fill(std::begin(boundGroups), std::end(boundGroups), 0);
        graphicsLayout = computeLayout = nullptr;
        boundImageSet = boundSamplerSet = 0;
        boundImageCount = boundSamplerCount = 0;
        graphicsUboValid = graphicsSsboValid = 0;
        computeUboValid = computeSsboValid = 0;
        computeUboDirty = computeSsboDirty = true;
        computePushSize = 0;
        memset(uboBoundStages, 0, sizeof(uboBoundStages));
        memset(ssboBoundStages, 0, sizeof(ssboBoundStages));
        framebuffer = nullptr;
        viewportCount = scissorCount = 0;
        indexAddress = 0;
        boundIndexFormat = DkIdxFormat_Uint16;
        boundTexCount[0] = boundTexCount[1] = boundTexCount[2] = 0;
        sharedBase = 0;
        sharedSize = 0;
        sharedCpu = nullptr;
        sharedWords = nullptr;  // banjo-nx (2026-10-19)
        sharedWordsSize = 0;
        descriptorEpoch = 0;
        // NaN: the first setBlendConstant of the list always writes it.
        blendConstant[0] = blendConstant[1] = blendConstant[2] = blendConstant[3] = __builtin_nanf("");

        // The push constants' UBO, bound once for every graphics stage. The vertex fetch data's binding (15) is each
        // draw's own range (Deko3DBindFetchData).
        graphicsUbo[PushUboSlot] = {device->pushAddr, Deko3DDevice::PushSize};
        graphicsUboValid |= 1u << PushUboSlot;
        // banjo-nx (2026-10-19, GPU, txq): the texture size buffer, for the shaders whose usage has UBO bit 3.
        graphicsUbo[TexSizeUboSlot] = {device->texSizeAddr, Deko3DDevice::TexSizeSize};
        graphicsUboValid |= 1u << TexSizeUboSlot;
        boundImageSetObj = nullptr;
        texSizeValid = 0;
        // LostOdysseyRecomp (2026-10-27): every descriptor set's descriptors are in the device's two heaps, bound once.
        dkCmdBufBindImageDescriptorSet(cmd, device->imageHeapAddr, Deko3DDevice::ImageHeapCount);
        dkCmdBufBindSamplerDescriptorSet(cmd, device->samplerHeapAddr, Deko3DDevice::SamplerHeapCount);
        boundImageSet = device->imageHeapAddr;
        boundImageCount = Deko3DDevice::ImageHeapCount;
        boundSamplerSet = device->samplerHeapAddr;
        boundSamplerCount = Deko3DDevice::SamplerHeapCount;
        std::fill(std::begin(graphicsSets), std::end(graphicsSets), nullptr);
        // Whatever the GPU holds of descriptors and shader code from before is dropped.
        dkCmdBufBarrier(cmd, DkBarrier_None, DkInvalidateFlags_Descriptors | DkInvalidateFlags_Shader);
        descriptorEpoch = currentEpoch();
    }

    // Descriptors written and shader code loaded so far, as one number (flushDescriptorInvalidate).
    uint64_t Deko3DCommandList::currentEpoch() const {
        return device->descriptorEpoch.load(std::memory_order_acquire) ^ (device->codeEpoch.load(std::memory_order_acquire) << 40);
    }

    void Deko3DCommandList::end() {
        flushBarriers();
        list = dkCmdBufFinishList(cmd);
        open = false;
        // banjo-nx (2026-10-19, render-thread proposal 13): this list's draws into the totals.
        Deko3DStats &st = GetDeko3DStats();
        st.draws.store(st.draws.load(std::memory_order_relaxed) + statDraws, std::memory_order_relaxed);
        st.windowDraws.store(st.windowDraws.load(std::memory_order_relaxed) + statWindowDraws, std::memory_order_relaxed);
        statDraws = statWindowDraws = 0;
    }

    void Deko3DCommandList::flushDescriptorInvalidate() {
        // Descriptors the CPU wrote since this list last invalidated, or shader code loaded since: the GPU's caches of
        // them are dropped before the next draw or dispatch.
        const uint64_t epoch = currentEpoch();
        if (epoch != descriptorEpoch) {
            flushBarriers();
            dkCmdBufBarrier(cmd, DkBarrier_None, DkInvalidateFlags_Descriptors | DkInvalidateFlags_Shader);
            descriptorEpoch = epoch;
        }
    }

    void Deko3DCommandList::barriers(RenderBarrierStages stages, const RenderBufferBarrier *bufferBarriers, uint32_t bufferBarriersCount, const RenderTextureBarrier *textureBarriers, uint32_t textureBarriersCount) {
        // Image layouts do not exist in deko3d: a barrier is an ordering of the GPU's work and an invalidation of its
        // caches. Rendering and compute before sampling: Fragments / Primitives with the texture cache dropped
        // (SwitchWakerHD's choices); the copy engine involved: Full (it waits for the copy engine too).
        uint32_t invalidate = 0;
        DkBarrier mode = DkBarrier_None;
        bool copy = (stages & RenderBarrierStage::COPY) != 0;
        for (uint32_t i = 0; i < textureBarriersCount; i++) {
            switch (textureBarriers[i].layout) {
            case RenderTextureLayout::SHADER_READ:
            case RenderTextureLayout::DEPTH_READ:
            case RenderTextureLayout::GENERAL:
                invalidate |= DkInvalidateFlags_Image;
                mode = std::max(mode, DkBarrier_Fragments);
                break;
            case RenderTextureLayout::COPY_SOURCE:
            case RenderTextureLayout::COPY_DEST:
            case RenderTextureLayout::RESOLVE_SOURCE:
            case RenderTextureLayout::RESOLVE_DEST:
                copy = true;
                break;
            case RenderTextureLayout::PRESENT:
            case RenderTextureLayout::COLOR_WRITE:
            case RenderTextureLayout::DEPTH_WRITE:
                mode = std::max(mode, DkBarrier_Fragments);
                // banjo-nx (2026-10-19, Deko3DOptions::leanBarriers): written next, not read: no invalidation.
                if (!Deko3DGetOptions().leanBarriers)
                    invalidate |= DkInvalidateFlags_Image;
                break;
            default:
                mode = std::max(mode, DkBarrier_Fragments);
                invalidate |= DkInvalidateFlags_Image;
                break;
            }
        }
        if (bufferBarriersCount) {
            invalidate |= DkInvalidateFlags_Shader | DkInvalidateFlags_Image;
            mode = std::max(mode, DkBarrier_Primitives);
        }
        if (stages & RenderBarrierStage::COMPUTE)
            mode = std::max(mode, DkBarrier_Primitives);
        if (copy)
            mode = DkBarrier_Full;
        if (mode == DkBarrier_None && textureBarriersCount == 0 && bufferBarriersCount == 0)
            mode = DkBarrier_Primitives;
        if (Deko3DGetOptions().mergeBarriers) {
            // banjo-nx (2026-10-19): held until the next command that does work (flushBarriers), merged with any
            // barrier recorded before then.
            pendingBarrierMode = pendingBarrier ? std::max(pendingBarrierMode, mode) : mode;
            pendingBarrierInvalidate |= invalidate;
            pendingBarrier = true;
            return;
        }
        emitBarrier(mode, invalidate);
    }

    void Deko3DCommandList::emitBarrier(DkBarrier mode, uint32_t invalidate) {
        {
            // banjo-nx (2026-10-19): counted for the renderer's report (a load and a store: diagnostics).
            Deko3DStats &st = GetDeko3DStats();
            auto bump = [](std::atomic<uint64_t> &c) { c.store(c.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed); };
            bump(mode == DkBarrier_Full ? st.barriersFull : mode == DkBarrier_Fragments ? st.barriersFragments : st.barriersPrimitives);
            if (invalidate & DkInvalidateFlags_Image)
                bump(st.barriersImageInvalidate);
        }
        dkCmdBufBarrier(cmd, mode, invalidate);
        pendingBarrier = false;
        pendingBarrierMode = DkBarrier_None;
        pendingBarrierInvalidate = 0;
    }

    void Deko3DCommandList::dispatch(uint32_t x, uint32_t y, uint32_t z) {
        if (!computePipeline || !computePipeline->valid)
            return;
        flushDescriptorInvalidate();
        // Compute jobs read their uniform data when they run, so the push constants go into fresh transient memory
        // per dispatch (not the graphics stages' pipelined UBO).
        if (computePushSize) {
            void *cpu;
            DkGpuAddr gpu;
            if (transientAlloc(DK_UNIFORM_BUF_ALIGNMENT, DK_UNIFORM_BUF_ALIGNMENT, &cpu, &gpu)) {
                memset(cpu, 0, DK_UNIFORM_BUF_ALIGNMENT);
                memcpy(cpu, computePush, computePushSize);
                computeUbo[PushUboSlot] = {gpu, DK_UNIFORM_BUF_ALIGNMENT};
                computeUboValid |= 1u << PushUboSlot;
                computeUboDirty = true;
            }
        }
        if (computeUboDirty) {
            for (uint32_t s = 0; s < 16; s++) {
                if (((computePipeline->uboMask & computeUboValid) >> s) & 1u)
                    dkCmdBufBindUniformBuffer(cmd, DkStage_Compute, s, computeUbo[s].addr, computeUbo[s].size);
            }
            computeUboDirty = false;
        }
        if (computeSsboDirty) {
            for (uint32_t s = 0; s < 16; s++) {
                if (((computePipeline->ssboMask & computeSsboValid) >> s) & 1u)
                    dkCmdBufBindStorageBuffer(cmd, DkStage_Compute, s, computeSsbo[s].addr, computeSsbo[s].size);
            }
            computeSsboDirty = false;
        }
        flushBarriers();
        dkCmdBufDispatchCompute(cmd, x, y, z);
    }

    void Deko3DCommandList::traceRays(uint32_t, uint32_t, uint32_t, RenderBufferReference, const RenderShaderBindingGroupsInfo &) {}

    // The graphics stages' constant and storage buffers each shader reads, bound for the stages that read them when
    // they changed.
    void Deko3DCommandList::bindGraphicsBuffers() {
        const Deko3DGraphicsPipeline *p = graphicsPipeline;
        if ((p->ssboMask[0] | p->ssboMask[1] | p->ssboMask[2]) & (1u << AddressWindowSlot))
            ++statWindowDraws;
        for (int st = 0; st < 3; st++) {
            const uint32_t stageBit = 1u << GraphicsStages[st];
            uint32_t ubo = p->uboMask[st] & graphicsUboValid;
            while (ubo) {
                const uint32_t s = uint32_t(__builtin_ctz(ubo));
                ubo &= ubo - 1;
                if (!(uboBoundStages[s] & stageBit)) {
                    dkCmdBufBindUniformBuffer(cmd, GraphicsStages[st], s, graphicsUbo[s].addr, graphicsUbo[s].size);
                    uboBoundStages[s] |= stageBit;
                }
            }
            uint32_t ssbo = p->ssboMask[st] & graphicsSsboValid;
            while (ssbo) {
                const uint32_t s = uint32_t(__builtin_ctz(ssbo));
                ssbo &= ssbo - 1;
                if (!(ssboBoundStages[s] & stageBit)) {
                    dkCmdBufBindStorageBuffer(cmd, GraphicsStages[st], s, graphicsSsbo[s].addr, graphicsSsbo[s].size);
                    ssboBoundStages[s] |= stageBit;
                }
            }
        }
    }

    // Bound textures: each graphics stage's handles from the shared constants the draw has bound (UBO binding 2), read
    // where the CPU wrote them, bound when they differ from the last ones.
    static constexpr uint32_t SharedUboSlot = 2;

    void Deko3DCommandList::bindTextures() {
        const Deko3DGraphicsPipeline *p = graphicsPipeline;
        // LostOdysseyRecomp (2026-10-27): shaders translated from SPIR-V: each combined binding's handle from the
        // descriptor sets bound now (image id in the image heap) and its sampler, fixed or chosen by a word of a bound
        // uniform buffer (Xenos shaders: XeShared.xeSamplerIndex[i].x indexes the sampler palette).
        if (!p->textureSources[0].empty() || !p->textureSources[1].empty() || !p->textureSources[2].empty()) {
            for (int st = 0; st < 3; st++) {
                const auto &sources = p->textureSources[st];
                if (sources.empty())
                    continue;
                DkResHandle handles[32];
                const uint32_t n = std::min<uint32_t>(uint32_t(sources.size()), 32);
                bool same = n == boundTexCount[st];
                for (uint32_t k = 0; k < n; k++) {
                    const Deko3DSpirvTexture &src = sources[k];
                    uint32_t image = 0, sampler = 0;
                    const Deko3DDescriptorSet *is = src.imageSet < 8 ? graphicsSets[src.imageSet] : nullptr;
                    if (is && is->imageHeapBase != UINT32_MAX && src.imageIndex >= is->imageFirst && src.imageIndex - is->imageFirst < is->imageCount)
                        image = is->imageHeapBase + (src.imageIndex - is->imageFirst);
                    if (src.samplerFrom != Deko3DSpirvTexture::SamplerFrom::None) {
                        uint32_t index = src.samplerIndex;
                        if (src.samplerFrom == Deko3DSpirvTexture::SamplerFrom::UniformWord) {
                            uint32_t word = 0;
                            const uint32_t slot = src.uniformSlot;
                            if (slot < 16 && (graphicsUboValid & (1u << slot))) {
                                const DkGpuAddr addr = graphicsUbo[slot].addr + src.uniformOffset;
                                DkGpuAddr base = 0;
                                uint32_t size = 0;
                                const uint8_t *cpu = nullptr;
                                if (device->findBuffer(addr, base, size, cpu) && cpu && addr + 4 <= base + size)
                                    memcpy(&word, cpu + (addr - base), 4);
                            }
                            index += word;
                        }
                        const Deko3DDescriptorSet *ss = src.samplerSet < 8 ? graphicsSets[src.samplerSet] : nullptr;
                        if (ss && ss->samplerHeapBase != UINT32_MAX && index >= ss->samplerFirst && index - ss->samplerFirst < ss->samplerCount)
                            sampler = ss->samplerHeapBase + (index - ss->samplerFirst);
                    }
                    handles[k] = dkMakeTextureHandle(image, sampler);
                    same = same && handles[k] == boundTex[st][k];
                }
                if (same)
                    continue;
                dkCmdBufBindTextures(cmd, GraphicsStages[st], 0, handles, n);
                memcpy(boundTex[st], handles, n * sizeof(DkResHandle));
                boundTexCount[st] = n;
            }
            return;
        }
        if (!(graphicsUboValid & (1u << SharedUboSlot)))
            return;
        const DkGpuAddr addr = graphicsUbo[SharedUboSlot].addr;
        // banjo-nx (2026-10-19, render-thread proposal 5): pushed shared constants: the renderer's copy.
        if (sharedWords) {
            sharedCpu = sharedWords;
            sharedBase = addr;
            sharedSize = sharedWordsSize;
        } else if (!sharedCpu || addr < sharedBase || addr >= sharedBase + sharedSize) {
            if (!device->findBuffer(addr, sharedBase, sharedSize, sharedCpu)) {
                sharedCpu = nullptr;
                static std::atomic<uint32_t> logged{0};
                if (logged.fetch_add(1) < 8)
                    logf("bound textures: no buffer holds the shared constants at 0x%" PRIx64, addr);
                return;
            }
        }
        const uint8_t *words = sharedCpu + (addr - sharedBase);
        const uint32_t limit = uint32_t(sharedBase + sharedSize - addr);
        for (int st = 0; st < 3; st++) {
            const auto &bindings = p->texBindings[st];
            if (bindings.empty())
                continue;
            DkResHandle handles[32];
            const uint32_t n = std::min<uint32_t>(uint32_t(bindings.size()), 32);
            bool same = n == boundTexCount[st];
            for (uint32_t k = 0; k < n; k++) {
                const Deko3DShaderUsage::TexBinding &b = bindings[k];
                uint32_t texture = 0, sampler = 0;
                if (b.textureWord + 4u <= limit)
                    memcpy(&texture, words + b.textureWord, 4);
                if (b.samplerWord != Deko3DShaderUsage::SamplerNone && b.samplerWord + 4u <= limit)
                    memcpy(&sampler, words + b.samplerWord, 4);
                handles[k] = (texture & 0xFFFFFu) | (sampler & 0xFFF00000u);
                same = same && handles[k] == boundTex[st][k];
            }
            // banjo-nx (2026-10-19, GPU, txq): the sizes of the bindings the shader reads from the texture size
            // buffer (entry k for the pixel stage, 32 + k for the vertex stage), updated where they changed. Also when
            // the handles are the same: a descriptor slot may hold another texture now.
            if (p->txqMask[st]) {
                const uint32_t base = st == 2 ? 0u : 32u;
                for (uint32_t mask = p->txqMask[st]; mask;) {
                    const uint32_t k = uint32_t(__builtin_ctz(mask));
                    mask &= mask - 1;
                    if (k >= n || k >= 32)
                        continue;
                    const uint32_t id = handles[k] & 0xFFFFFu;
                    uint32_t size[4] = {0, 0, 0, 0};
                    if (boundImageSetObj && id < boundImageSetObj->imageSizes.size()) {
                        const auto &dims = boundImageSetObj->imageSizes[id];
                        size[0] = dims[0];
                        size[1] = dims[1];
                        size[2] = dims[2];
                    }
                    const uint32_t e = base + k;
                    if (((texSizeValid >> e) & 1) && !memcmp(texSizeShadow[e], size, sizeof(size)))
                        continue;
                    dkCmdBufPushConstants(cmd, device->texSizeAddr, Deko3DDevice::TexSizeSize, e * 16, 16, size);
                    memcpy(texSizeShadow[e], size, sizeof(size));
                    texSizeValid |= uint64_t(1) << e;
                }
            }
            if (same)
                continue;
            dkCmdBufBindTextures(cmd, GraphicsStages[st], 0, handles, n);
            memcpy(boundTex[st], handles, n * sizeof(DkResHandle));
            boundTexCount[st] = n;
        }
    }

    // LostOdysseyRecomp (2026-10-27): storage windows that start at an address the push constants hold (the
    // renderer's own shaders read the shared constants through such addresses): the buffer holding the address, from
    // there to its end.
    void Deko3DCommandList::bindPushWindows() {
        const Deko3DGraphicsPipeline *p = graphicsPipeline;
        for (int st = 0; st < 3; st++) {
            for (const auto &w : p->pushWindows[st]) {
                if (w.slot >= 16 || w.pushOffset + 8 > sizeof(graphicsPushShadow))
                    continue;
                uint64_t address = 0;
                memcpy(&address, graphicsPushShadow + w.pushOffset, 8);
                DkGpuAddr base = 0;
                uint32_t size = 0;
                const uint8_t *cpu = nullptr;
                DkBufExtents e{device->zeroAddr, Deko3DDevice::ZeroSize};
                if (address && device->findBuffer(address, base, size, cpu) && address < base + size)
                    e = {address, uint32_t(base + size - address)};
                if (!(graphicsSsboValid & (1u << w.slot)) || graphicsSsbo[w.slot].addr != e.addr || graphicsSsbo[w.slot].size != e.size) {
                    graphicsSsbo[w.slot] = e;
                    graphicsSsboValid |= 1u << w.slot;
                    ssboBoundStages[w.slot] = 0;
                }
            }
        }
    }

    void Deko3DCommandList::drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex, uint32_t startInstance) {
        if (!graphicsPipeline || !graphicsPipeline->valid)
            return;
        flushDescriptorInvalidate();
        if (graphicsPipeline->hasPushWindows)
            bindPushWindows();
        bindGraphicsBuffers();
        if (graphicsPipeline->boundTextures)
            bindTextures();
        flushBarriers();
        dkCmdBufDraw(cmd, graphicsPipeline->primitive, vertexCount, instanceCount, startVertex, startInstance);
        ++statDraws;
    }

    void Deko3DCommandList::drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t startIndex, int32_t baseVertex, uint32_t startInstance) {
        if (!graphicsPipeline || !graphicsPipeline->valid || !indexAddress)
            return;
        flushDescriptorInvalidate();
        if (graphicsPipeline->hasPushWindows)
            bindPushWindows();
        bindGraphicsBuffers();
        if (graphicsPipeline->boundTextures)
            bindTextures();
        flushBarriers();
        dkCmdBufDrawIndexed(cmd, graphicsPipeline->primitive, indexCount, instanceCount, startIndex, baseVertex, startInstance);
        ++statDraws;
    }

    void Deko3DCommandList::setPipeline(const RenderPipeline *pipeline) {
        const Deko3DPipeline *base = static_cast<const Deko3DPipeline *>(pipeline);
        if (base == nullptr)
            return;
        if (base->type == Deko3DPipeline::Type::Compute) {
            const Deko3DComputePipeline *p = static_cast<const Deko3DComputePipeline *>(base);
            if (p != computePipeline && p->valid) {
                dkCmdBufBindShaders(cmd, DkStageFlag_Compute, &p->shader, 1);
                computeUboDirty = computeSsboDirty = true;
            }
            computePipeline = p;
            return;
        }
        const Deko3DGraphicsPipeline *p = static_cast<const Deko3DGraphicsPipeline *>(base);
        if (p == graphicsPipeline)
            return;
        graphicsPipeline = p;
        if (!p->valid)
            return;
        // banjo-nx (2026-10-19): a load and a store (a lost count is possible between two recording threads), not
        // an exclusive read-modify-write loop: the A57 has no LSE atomics.
        auto bump = [](std::atomic<uint64_t> &c) { c.store(c.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed); };
        bump(GetDeko3DStats().pipelineBinds);
        // Shaders: rebound when any stage differs (absent stages are disabled by the bind).
        const DkShader *const *want = p->stageShaders;  // banjo-nx: kept at creation
        if (want[0] != boundShaders[0] || want[1] != boundShaders[1] || want[2] != boundShaders[2]) {
            dkCmdBufBindShaders(cmd, DkStageFlag_GraphicsMask, p->shaders, p->shaderCount);
            boundShaders[0] = want[0];
            boundShaders[1] = want[1];
            boundShaders[2] = want[2];
            // A new program in a stage reads its constant buffers from the stage's bindings, which stay; nothing to
            // rebind.
        }
        if (p->stateHash != boundStateHash) {
            bump(GetDeko3DStats().stateBinds);
            // banjo-nx (2026-10-19, CPU proposal 13): only the groups whose state differs from what is bound (a group
            // the GPU already holds is a redundant command). The stencil values and the depth bias are left as they
            // are by a pipeline that does not set them (no stencil, a dynamic bias), as before; the dynamic setters
            // make their groups unknown, so the next pipeline that sets them does.
            auto changed = [&](uint32_t g) {
                if (boundGroups[g] == p->groupHash[g])
                    return false;
                boundGroups[g] = p->groupHash[g];
                return true;
            };
            if (changed(0))
                dkCmdBufBindRasterizerState(cmd, &p->rasterizer);
            if (changed(1))
                dkCmdBufBindMultisampleState(cmd, &p->multisample);
            if (changed(2))
                dkCmdBufBindColorState(cmd, &p->color);
            if (changed(3))
                dkCmdBufBindColorWriteState(cmd, &p->colorWrite);
            if (p->blendCount && changed(4)) {
                dkCmdBufBindBlendStates(cmd, 0, p->blend, p->blendCount);
                // deko3d 0.5.0's FuncAlphaDst (see blendWords): the six registers of each target again.
                for (uint32_t i = 0; i < p->blendCount; i++)
                    dkCmdBufReplayCmds(cmd, p->blendWords[i], 7);
            }
            if (changed(5))
                dkCmdBufBindDepthStencilState(cmd, &p->depthStencil);
            if (p->stencilEnabled && changed(6))
                dkCmdBufSetStencil(cmd, DkFace_FrontAndBack, p->stencilWriteMask, p->stencilReference, p->stencilReadMask);
            if (!p->dynamicDepthBias && changed(7))
                dkCmdBufSetDepthBias(cmd, p->depthBias, p->depthBiasClamp, p->slopeScaledDepthBias);
            // Always, also with none: deko3d makes the attributes after the last one given constants and disables the
            // buffers after the last one, so a pipeline without vertex input leaves no earlier attribute fetching from
            // a buffer that may be gone. (banjo-nx: with the same attributes and buffers as bound, nothing to do.)
            if (changed(8)) {
                dkCmdBufBindVtxAttribState(cmd, p->attribs, p->attribCount);
                dkCmdBufBindVtxBufferState(cmd, p->vtxBuffers, p->vtxBufferCount);
            }
            boundStateHash = p->stateHash;
        }
    }

    void Deko3DCommandList::setComputePipelineLayout(const RenderPipelineLayout *pipelineLayout) {
        computeLayout = static_cast<const Deko3DPipelineLayout *>(pipelineLayout);
    }

    void Deko3DCommandList::setComputePushConstants(uint32_t rangeIndex, const void *data, uint32_t offset, uint32_t size) {
        if (!computeLayout || rangeIndex >= computeLayout->pushRanges.size())
            return;
        const RenderPushConstantRange &r = computeLayout->pushRanges[rangeIndex];
        if (size == 0)
            size = r.size;
        const uint32_t at = r.offset + offset;
        if (at + size > sizeof(computePush))
            return;
        memcpy(computePush + at, data, size);
        computePushSize = std::max(computePushSize, at + size);
    }

    // A descriptor set's tables and buffers into the bindings: the image and sampler tables are deko3d's descriptor
    // sets (one of each is bound at a time, for every stage), the buffers go to UBO / SSBO slots by the layout.
    static void bindSetTables(Deko3DCommandList &cl, const Deko3DDescriptorSet *set) {
        if (set->imageBlock) {
            const DkGpuAddr addr = dkMemBlockGetGpuAddr(set->imageBlock);
            if (addr != cl.boundImageSet || set->imageCount != cl.boundImageCount) {
                dkCmdBufBindImageDescriptorSet(cl.cmd, addr, set->imageCount);
                cl.boundImageSet = addr;
                cl.boundImageCount = set->imageCount;
            }
            cl.boundImageSetObj = set;  // banjo-nx (2026-10-19, txq): its sizes
        }
        if (set->samplerBlock) {
            const DkGpuAddr addr = dkMemBlockGetGpuAddr(set->samplerBlock);
            if (addr != cl.boundSamplerSet || set->samplerCount != cl.boundSamplerCount) {
                dkCmdBufBindSamplerDescriptorSet(cl.cmd, addr, set->samplerCount);
                cl.boundSamplerSet = addr;
                cl.boundSamplerCount = set->samplerCount;
            }
        }
    }

    static bool isConstantRange(RenderDescriptorRangeType t) {
        return t == RenderDescriptorRangeType::CONSTANT_BUFFER || t == RenderDescriptorRangeType::DYNAMIC_CONSTANT_BUFFER;
    }

    static bool isStorageRange(RenderDescriptorRangeType t) {
        switch (t) {
        case RenderDescriptorRangeType::STRUCTURED_BUFFER:
        case RenderDescriptorRangeType::READ_WRITE_STRUCTURED_BUFFER:
        case RenderDescriptorRangeType::BYTE_ADDRESS_BUFFER:
        case RenderDescriptorRangeType::READ_WRITE_BYTE_ADDRESS_BUFFER:
        case RenderDescriptorRangeType::FORMATTED_BUFFER:
        case RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER:
            return true;
        default:
            return false;
        }
    }

    static void bindSetBuffers(const Deko3DPipelineLayout *layout, uint32_t setIndex, const Deko3DDescriptorSet *set,
                               const uint32_t *dynamicOffsets, uint32_t dynamicOffsetsCount,
                               DkBufExtents *ubo, uint32_t &uboValid, uint32_t *uboBound,
                               DkBufExtents *ssbo, uint32_t &ssboValid, uint32_t *ssboBound) {
        if (!layout || setIndex >= layout->sets.size() || set->buffers.empty())
            return;
        uint32_t dynamicIndex = 0;
        for (const Deko3DPipelineLayout::Range &r : layout->sets[setIndex].ranges) {
            const bool constant = isConstantRange(r.type);
            const bool storage = isStorageRange(r.type);
            if (!constant && !storage)
                continue;
            for (uint32_t i = 0; i < r.count; i++) {
                const uint32_t d = r.descriptorIndex + i;
                const uint32_t slot = r.slot + i;
                if (d >= set->buffers.size() || slot >= 16)
                    break;
                DkBufExtents e = set->buffers[d];
                if (r.type == RenderDescriptorRangeType::DYNAMIC_CONSTANT_BUFFER && dynamicIndex < dynamicOffsetsCount)
                    e.addr += dynamicOffsets[dynamicIndex++];
                DkBufExtents *dst = constant ? ubo : ssbo;
                uint32_t &valid = constant ? uboValid : ssboValid;
                uint32_t *bound = constant ? uboBound : ssboBound;
                if (!(valid & (1u << slot)) || dst[slot].addr != e.addr || dst[slot].size != e.size) {
                    dst[slot] = e;
                    valid |= 1u << slot;
                    if (bound)
                        bound[slot] = 0;
                }
            }
        }
    }

    void Deko3DCommandList::setComputeDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
        const Deko3DDescriptorSet *set = static_cast<const Deko3DDescriptorSet *>(descriptorSet);
        if (!set)
            return;
        bindSetTables(*this, set);
        const uint32_t uboBefore = computeUboValid, ssboBefore = computeSsboValid;
        DkBufExtents uboOld[16], ssboOld[16];
        memcpy(uboOld, computeUbo, sizeof(uboOld));
        memcpy(ssboOld, computeSsbo, sizeof(ssboOld));
        bindSetBuffers(computeLayout, setIndex, set, nullptr, 0, computeUbo, computeUboValid, nullptr, computeSsbo, computeSsboValid, nullptr);
        if (computeUboValid != uboBefore || memcmp(uboOld, computeUbo, sizeof(uboOld)) != 0)
            computeUboDirty = true;
        if (computeSsboValid != ssboBefore || memcmp(ssboOld, computeSsbo, sizeof(ssboOld)) != 0)
            computeSsboDirty = true;
    }

    void Deko3DCommandList::setGraphicsPipelineLayout(const RenderPipelineLayout *pipelineLayout) {
        graphicsLayout = static_cast<const Deko3DPipelineLayout *>(pipelineLayout);
    }

    void Deko3DCommandList::setGraphicsPushConstants(uint32_t rangeIndex, const void *data, uint32_t offset, uint32_t size) {
        if (!graphicsLayout || rangeIndex >= graphicsLayout->pushRanges.size())
            return;
        const RenderPushConstantRange &r = graphicsLayout->pushRanges[rangeIndex];
        if (size == 0)
            size = r.size;
        const uint32_t at = r.offset + offset;
        if (at + size > Deko3DDevice::PushSize)
            return;
        // Pipelined: draws before this one keep the old values (deko3d's push constants).
        dkCmdBufPushConstants(cmd, device->pushAddr, Deko3DDevice::PushSize, at, alignUp(size, 4), data);
        memcpy(graphicsPushShadow + at, data, size);  // LostOdysseyRecomp (2026-10-27): bindPushWindows
    }

    void Deko3DCommandList::setGraphicsDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) {
        const Deko3DDescriptorSet *set = static_cast<const Deko3DDescriptorSet *>(descriptorSet);
        if (!set)
            return;
        if (setIndex < 8)
            graphicsSets[setIndex] = set;  // LostOdysseyRecomp (2026-10-27)
        bindSetTables(*this, set);
        bindSetBuffers(graphicsLayout, setIndex, set, nullptr, 0, graphicsUbo, graphicsUboValid, uboBoundStages, graphicsSsbo, graphicsSsboValid, ssboBoundStages);
    }

    void Deko3DCommandList::setGraphicsDescriptorSetDynamic(RenderDescriptorSet *descriptorSet, uint32_t setIndex, const uint32_t *dynamicOffsets, uint32_t dynamicOffsetsCount) {
        const Deko3DDescriptorSet *set = static_cast<const Deko3DDescriptorSet *>(descriptorSet);
        if (!set)
            return;
        if (setIndex < 8)
            graphicsSets[setIndex] = set;  // LostOdysseyRecomp (2026-10-27)
        bindSetTables(*this, set);
        bindSetBuffers(graphicsLayout, setIndex, set, dynamicOffsets, dynamicOffsetsCount, graphicsUbo, graphicsUboValid, uboBoundStages, graphicsSsbo, graphicsSsboValid, ssboBoundStages);
    }

    void Deko3DCommandList::setGraphicsRootDescriptor(RenderBufferReference, uint32_t) {}
    void Deko3DCommandList::setRaytracingPipelineLayout(const RenderPipelineLayout *) {}
    void Deko3DCommandList::setRaytracingPushConstants(uint32_t, const void *, uint32_t, uint32_t) {}
    void Deko3DCommandList::setRaytracingDescriptorSet(RenderDescriptorSet *, uint32_t) {}

    void Deko3DCommandList::setIndexBuffer(const RenderIndexBufferView *view) {
        if (!view || !view->buffer.ref) {
            indexAddress = 0;
            return;
        }
        const Deko3DBuffer *b = static_cast<const Deko3DBuffer *>(view->buffer.ref);
        indexFormat = view->format == RenderFormat::R32_UINT ? DkIdxFormat_Uint32 : view->format == RenderFormat::R8_UINT ? DkIdxFormat_Uint8 : DkIdxFormat_Uint16;
        const DkGpuAddr addr = b->gpu + view->buffer.offset;
        if (addr != indexAddress || indexFormat != boundIndexFormat) {
            dkCmdBufBindIdxBuffer(cmd, indexFormat, addr);
            indexAddress = addr;
            boundIndexFormat = indexFormat;
        }
    }

    void Deko3DCommandList::setVertexBuffers(uint32_t startSlot, const RenderVertexBufferView *views, uint32_t viewCount, const RenderInputSlot *inputSlots) {
        // The strides are the pipeline's (as the Vulkan back end, which ignores inputSlots); a missing buffer reads the
        // device's zero buffer, as Vulkan's null buffer.
        (void)inputSlots;
        DkBufExtents extents[16];
        const uint32_t count = std::min<uint32_t>(viewCount, 16 - std::min<uint32_t>(startSlot, 16));
        for (uint32_t i = 0; i < count; i++) {
            const Deko3DBuffer *b = static_cast<const Deko3DBuffer *>(views[i].buffer.ref);
            extents[i] = (b && b->block) ? DkBufExtents{b->gpu + views[i].buffer.offset, std::max<uint32_t>(views[i].size, 4)}
                                         : DkBufExtents{device->zeroAddr, Deko3DDevice::ZeroSize};
        }
        if (count)
            dkCmdBufBindVtxBuffers(cmd, startSlot, extents, count);
    }

    // ViewportTransform registers (method 0x280 + 8 * viewport): ScaleY at +1, TranslateY at +4.
    static uint32_t floatBits(float f) {
        uint32_t u;
        memcpy(&u, &f, sizeof(u));
        return u;
    }

    void Deko3DCommandList::setViewports(const RenderViewport *vps, uint32_t count) {
        count = std::min<uint32_t>(count, 16);
        bool flipped = false;
        for (uint32_t i = 0; i < count; i++) {
            const RenderViewport &v = vps[i];
            DkViewport &d = viewports[i];
            // A negative height is a flipped viewport (D3D's and Vulkan's); deko3d derives its clip rectangle from the
            // height, so it gets the positive one and the transform's y is written after (below).
            d.x = v.x;
            d.width = v.width;
            d.y = v.height < 0.0f ? v.y + v.height : v.y;
            d.height = std::fabs(v.height);
            d.near = v.minDepth;
            d.far = v.maxDepth;
            flipped = flipped || v.height < 0.0f;
        }
        viewportCount = count;
        dkCmdBufSetViewports(cmd, 0, viewports, count);
        if (flipped) {
            for (uint32_t i = 0; i < count; i++) {
                if (vps[i].height >= 0.0f)
                    continue;
                // deko3d's transform with the upper-left origin: scaleY = -h/2, translateY = y + h/2 (y and h of the
                // positive viewport). The flipped one mirrors it: +h/2 around the same centre.
                const float half = 0.5f * viewports[i].height;
                const uint32_t words[2] = {(1u << 29) | (1u << 16) | (0x281u + 8u * i), floatBits(half)};
                dkCmdBufReplayCmds(cmd, words, 2);
            }
        }
    }

    void Deko3DCommandList::setScissors(const RenderRect *rects, uint32_t count) {
        count = std::min<uint32_t>(count, 16);
        for (uint32_t i = 0; i < count; i++) {
            const RenderRect &r = rects[i];
            DkScissor &s = scissors[i];
            s.x = uint32_t(std::max(r.left, 0));
            s.y = uint32_t(std::max(r.top, 0));
            s.width = uint32_t(std::max(r.right - std::max(r.left, 0), 0));
            s.height = uint32_t(std::max(r.bottom - std::max(r.top, 0), 0));
        }
        scissorCount = count;
        dkCmdBufSetScissors(cmd, 0, scissors, count);
    }

    void Deko3DCommandList::restoreScissors() {
        if (scissorCount)
            dkCmdBufSetScissors(cmd, 0, scissors, scissorCount);
        else {
            const DkScissor all = {0, 0, 16384, 16384};
            dkCmdBufSetScissors(cmd, 0, &all, 1);
        }
    }

    // The framebuffer sRGB register (Maxwell 0x56E) deko3d 0.5.0 never writes (SwitchWakerHD).
    static void writeFramebufferSrgb(DkCmdBuf cmd, bool srgb) {
        const uint32_t words[2] = {(1u << 29) | (1u << 16) | 0x56Eu, srgb ? 1u : 0u};
        dkCmdBufReplayCmds(cmd, words, 2);
    }

    void Deko3DCommandList::setFramebuffer(const RenderFramebuffer *fb) {
        // Bound every time: an application can free a framebuffer and make another at the same address.
        const Deko3DFramebuffer *f = static_cast<const Deko3DFramebuffer *>(fb);
        framebuffer = f;
        if (!f)
            return;
#if defined(PLUME_DEKO3D_GUARDS)
        if (g_traceTargets.fetch_add(1) < 40)
            logf("trace targets: %u colour (%ux%u, format %d%s), depth %s", f->colorCount, f->width, f->height,
                 f->colorCount ? int(f->colors[0].format ? f->colors[0].format : f->colors[0].pImage ? 0 : -1) : -1,
                 f->colorCount && f->srgb[0] ? ", sRGB" : "", f->hasDepth ? "yes" : "no");
#endif
        const DkImageView *colors[8];
        for (uint32_t i = 0; i < f->colorCount; i++)
            colors[i] = &f->colors[i];
        flushBarriers();
        dkCmdBufBindRenderTargets(cmd, colors, f->colorCount, f->hasDepth ? &f->depth : nullptr);
        writeFramebufferSrgb(cmd, f->colorCount && f->srgb[0]);
    }

    void Deko3DCommandList::setDepthBias(float depthBias, float depthBiasClamp, float slopeScaledDepthBias) {
        dkCmdBufSetDepthBias(cmd, depthBias, depthBiasClamp, slopeScaledDepthBias);
        boundGroups[7] = 0;  // banjo-nx (2026-10-19): unknown for the next pipeline that sets a bias
    }

    void Deko3DCommandList::setStencilReference(uint32_t reference) {
        const Deko3DGraphicsPipeline *p = graphicsPipeline;
        dkCmdBufSetStencil(cmd, DkFace_FrontAndBack, p ? p->stencilWriteMask : 0xFF, uint8_t(reference), p ? p->stencilReadMask : 0xFF);
        boundGroups[6] = 0;  // banjo-nx (2026-10-19): unknown for the next pipeline that sets the stencil values
    }

    void Deko3DCommandList::clearColor(uint32_t attachmentIndex, RenderColor color, const RenderRect *rects, uint32_t rectsCount) {
#if defined(PLUME_DEKO3D_GUARDS)
        if (g_traceClears.fetch_add(1) < 40) {
            const Deko3DFramebuffer *f = framebuffer;
            logf("trace clear: target %u of %u (%ux%u, image format %d) to (%.3f %.3f %.3f %.3f), %u rects%s", attachmentIndex,
                 f ? f->colorCount : 0, f ? f->width : 0, f ? f->height : 0,
                 f && attachmentIndex < f->colorCount ? int(f->colors[attachmentIndex].pImage ? 0 : -1) : -2, color.r, color.g,
                 color.b, color.a, rectsCount, rectsCount ? "" : " (whole target)");
        }
#endif
        // deko3d's clears go through scissor 0 (it enables the clear's scissor flag): a rectangle is a scissor.
        if (rectsCount == 0) {
            const DkScissor all = {0, 0, 16384, 16384};
            dkCmdBufSetScissors(cmd, 0, &all, 1);
            flushBarriers();
            dkCmdBufClearColorFloat(cmd, attachmentIndex, DkColorMask_RGBA, color.r, color.g, color.b, color.a);
        }
        else {
            for (uint32_t i = 0; i < rectsCount; i++) {
                const RenderRect &r = rects[i];
                const DkScissor s = {uint32_t(std::max(r.left, 0)), uint32_t(std::max(r.top, 0)),
                                     uint32_t(std::max(r.right - r.left, 0)), uint32_t(std::max(r.bottom - r.top, 0))};
                dkCmdBufSetScissors(cmd, 0, &s, 1);
                flushBarriers();
                dkCmdBufClearColorFloat(cmd, attachmentIndex, DkColorMask_RGBA, color.r, color.g, color.b, color.a);
            }
        }
        restoreScissors();
    }

    void Deko3DCommandList::clearDepthStencil(bool clearDepth, bool clearStencil, float depthValue, uint32_t stencilValue, const RenderRect *rects, uint32_t rectsCount) {
        if (rectsCount == 0) {
            const DkScissor all = {0, 0, 16384, 16384};
            dkCmdBufSetScissors(cmd, 0, &all, 1);
            flushBarriers();
            dkCmdBufClearDepthStencil(cmd, clearDepth, depthValue, clearStencil ? 0xFF : 0, uint8_t(stencilValue));
        }
        else {
            for (uint32_t i = 0; i < rectsCount; i++) {
                const RenderRect &r = rects[i];
                const DkScissor s = {uint32_t(std::max(r.left, 0)), uint32_t(std::max(r.top, 0)),
                                     uint32_t(std::max(r.right - r.left, 0)), uint32_t(std::max(r.bottom - r.top, 0))};
                dkCmdBufSetScissors(cmd, 0, &s, 1);
                flushBarriers();
                dkCmdBufClearDepthStencil(cmd, clearDepth, depthValue, clearStencil ? 0xFF : 0, uint8_t(stencilValue));
            }
        }
        restoreScissors();
    }

    void Deko3DCommandList::copyBufferRegion(RenderBufferReference dst, RenderBufferReference src, uint64_t size) {
        const Deko3DBuffer *d = static_cast<const Deko3DBuffer *>(dst.ref);
        const Deko3DBuffer *s = static_cast<const Deko3DBuffer *>(src.ref);
        if (!d || !s || size == 0)
            return;
        flushBarriers();
        GetDeko3DStats().copyBuffer.store(GetDeko3DStats().copyBuffer.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);  // banjo-nx (2026-10-19)
        dkCmdBufCopyBuffer(cmd, s->gpu + src.offset, d->gpu + dst.offset, uint32_t(size));
    }

    static DkImageView subresourceView(const Deko3DTexture *t, uint32_t mip, uint32_t layer) {
        DkImageView v = t->defaultView();
        v.mipLevelOffset = uint8_t(mip);
        v.mipLevelCount = 1;
        if (t->type == DkImageType_2DArray || t->type == DkImageType_Cubemap || t->type == DkImageType_CubemapArray ||
            t->type == DkImageType_1DArray || t->type == DkImageType_2DMSArray) {
            v.layerOffset = uint16_t(layer);
            v.layerCount = 1;
        }
        return v;
    }

    static uint32_t mipDim(uint32_t d, uint32_t mip) { return std::max<uint32_t>(d >> mip, 1); }

    // Bytes per block of a plume format, and its block width (1 for uncompressed).
    static uint32_t blockBytes(RenderFormat f) {
        switch (f) {
        case RenderFormat::BC1_TYPELESS: case RenderFormat::BC1_UNORM: case RenderFormat::BC1_UNORM_SRGB:
        case RenderFormat::BC4_TYPELESS: case RenderFormat::BC4_UNORM: case RenderFormat::BC4_SNORM:
            return 8;
        case RenderFormat::BC2_TYPELESS: case RenderFormat::BC2_UNORM: case RenderFormat::BC2_UNORM_SRGB:
        case RenderFormat::BC3_TYPELESS: case RenderFormat::BC3_UNORM: case RenderFormat::BC3_UNORM_SRGB:
        case RenderFormat::BC5_TYPELESS: case RenderFormat::BC5_UNORM: case RenderFormat::BC5_SNORM:
        case RenderFormat::BC6H_TYPELESS: case RenderFormat::BC6H_UF16: case RenderFormat::BC6H_SF16:
        case RenderFormat::BC7_TYPELESS: case RenderFormat::BC7_UNORM: case RenderFormat::BC7_UNORM_SRGB:
            return 16;
        default:
            return RenderFormatSize(f);
        }
    }

    // LostOdysseyRecomp (2026-10-28): a copy rectangle cut to both images' level sizes. The copy and 2D engines write
    // whatever they are given, so a rectangle past the end of the destination wrote into the image after it in the
    // image heap: with the scene targets one or two rows taller than the textures resolved from them (dynamic
    // resolution's 442-row target and 432-row textures), blocks and bands of other targets were overwritten. Vulkan
    // requires copies in bounds; this makes deko3d's the same. False when nothing is left to copy.
    // The first cuts, for the log (each one a copy that would have written out of bounds).
    static void noteClip(uint32_t sx, uint32_t sy, uint32_t w, uint32_t h, uint32_t sw, uint32_t sh, uint32_t dx, uint32_t dy,
                         uint32_t dw, uint32_t dh) {
        static std::atomic<uint32_t> logged{0};
        if (logged.fetch_add(1) < 16)
            logf("copy cut to bounds: %ux%u from (%u, %u) of %ux%u to (%u, %u) of %ux%u", w, h, sx, sy, sw, sh, dx, dy, dw, dh);
    }
    static bool clipCopy(const Deko3DTexture *src, uint32_t srcMip, DkImageRect &s, const Deko3DTexture *dst, uint32_t dstMip,
                         DkImageRect &d) {
        const uint32_t sw = mipDim(src->desc.width, srcMip), sh = mipDim(src->desc.height, srcMip);
        const uint32_t dw = mipDim(dst->desc.width, dstMip), dh = mipDim(dst->desc.height, dstMip);
        if (s.x >= sw || s.y >= sh || d.x >= dw || d.y >= dh)
            return false;
        const uint32_t w = std::min({s.width, sw - s.x, dw - d.x});
        const uint32_t h = std::min({s.height, sh - s.y, dh - d.y});
        if (!w || !h)
            return false;
        if (w != s.width || h != s.height)
            noteClip(s.x, s.y, s.width, s.height, sw, sh, d.x, d.y, dw, dh);
        s.width = d.width = w;
        s.height = d.height = h;
        return true;
    }
    static bool clipToImage(const Deko3DTexture *t, uint32_t mip, DkImageRect &r) {
        const uint32_t w = mipDim(t->desc.width, mip), h = mipDim(t->desc.height, mip);
        if (r.x >= w || r.y >= h)
            return false;
        if (r.width > w - r.x || r.height > h - r.y)
            noteClip(r.x, r.y, r.width, r.height, w, h, r.x, r.y, w, h);
        r.width = std::min(r.width, w - r.x);
        r.height = std::min(r.height, h - r.y);
        return r.width && r.height;
    }

    void Deko3DCommandList::copyTextureRegion(const RenderTextureCopyLocation &dstLoc, const RenderTextureCopyLocation &srcLoc, uint32_t dstX, uint32_t dstY, uint32_t dstZ, const RenderBox *srcBox) {
#if defined(PLUME_DEKO3D_GUARDS)
        if (g_traceCopies.fetch_add(1) < 60) {
            auto describe = [](const RenderTextureCopyLocation &l, char *out, size_t n) {
                if (l.type == RenderTextureCopyType::PLACED_FOOTPRINT)
                    snprintf(out, n, "buffer (%ux%u, format %d, row %u texels)", l.placedFootprint.width, l.placedFootprint.height,
                             int(l.placedFootprint.format), l.placedFootprint.rowWidth);
                else {
                    const Deko3DTexture *t = static_cast<const Deko3DTexture *>(l.texture);
                    snprintf(out, n, "texture %p (%ux%u, format %d, mip %u layer %u)", (const void *)t, t ? t->desc.width : 0,
                             t ? t->desc.height : 0, t ? int(t->desc.format) : -1, l.subresource.mipLevel, l.subresource.arrayIndex);
                }
            };
            char a[160], b[160];
            describe(srcLoc, a, sizeof(a));
            describe(dstLoc, b, sizeof(b));
            logf("trace copy: %s -> %s at (%u, %u, %u)%s", a, b, dstX, dstY, dstZ, srcBox ? " (box)" : "");
        }
#endif
        if (dstLoc.type == RenderTextureCopyType::SUBRESOURCE && srcLoc.type == RenderTextureCopyType::PLACED_FOOTPRINT) {
            const Deko3DTexture *t = static_cast<const Deko3DTexture *>(dstLoc.texture);
            const Deko3DBuffer *b = static_cast<const Deko3DBuffer *>(srcLoc.buffer);
            if (!t || !t->valid || !b)
                return;
            const auto &fp = srcLoc.placedFootprint;
            const uint32_t bw = RenderFormatBlockWidth(t->desc.format);
            const uint32_t rowBytes = (fp.rowWidth + bw - 1) / bw * blockBytes(t->desc.format);
            const uint32_t rows = (fp.height + bw - 1) / bw;
            const DkCopyBuf src = {b->gpu + fp.offset, rowBytes, rowBytes * rows};
            DkImageView v = subresourceView(t, dstLoc.subresource.mipLevel, dstLoc.subresource.arrayIndex);
            const bool layered = t->type == DkImageType_3D;
            DkImageRect rect = {dstX, dstY, layered ? dstZ : 0, fp.width, fp.height, std::max<uint32_t>(fp.depth, 1)};
            if (!clipToImage(t, dstLoc.subresource.mipLevel, rect))
                return;
            flushBarriers();
            GetDeko3DStats().copyUpload.store(GetDeko3DStats().copyUpload.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);  // banjo-nx (2026-10-19)
            dkCmdBufCopyBufferToImage(cmd, &src, &v, &rect, 0);
            invalidateZcullAfterCopy(t);  // banjo-nx (2026-10-18): an upload into a depth target, the same as a copy
        }
        else if (dstLoc.type == RenderTextureCopyType::PLACED_FOOTPRINT && srcLoc.type == RenderTextureCopyType::SUBRESOURCE) {
            const Deko3DTexture *t = static_cast<const Deko3DTexture *>(srcLoc.texture);
            const Deko3DBuffer *b = static_cast<const Deko3DBuffer *>(dstLoc.buffer);
            if (!t || !t->valid || !b)
                return;
            const auto &fp = dstLoc.placedFootprint;
            const uint32_t bw = RenderFormatBlockWidth(t->desc.format);
            const uint32_t rowBytes = (fp.rowWidth + bw - 1) / bw * blockBytes(t->desc.format);
            const uint32_t rows = (fp.height + bw - 1) / bw;
            const DkCopyBuf dst = {b->gpu + fp.offset, rowBytes, rowBytes * rows};
            DkImageView v = subresourceView(t, srcLoc.subresource.mipLevel, srcLoc.subresource.arrayIndex);
            DkImageRect rect;
            if (srcBox)
                rect = {uint32_t(srcBox->left), uint32_t(srcBox->top), uint32_t(srcBox->front), uint32_t(srcBox->right - srcBox->left),
                        uint32_t(srcBox->bottom - srcBox->top), uint32_t(std::max(srcBox->back - srcBox->front, 1))};
            else
                rect = {0, 0, 0, fp.width, fp.height, std::max<uint32_t>(fp.depth, 1)};
            if (!clipToImage(t, srcLoc.subresource.mipLevel, rect))
                return;
            // The copy engine finishes, and its writes leave the GPU's L2, before the CPU reads them (after a fence).
            flushBarriers();
            dkCmdBufBarrier(cmd, DkBarrier_Full, DkInvalidateFlags_Image);
            flushBarriers();
            GetDeko3DStats().copyReadback.store(GetDeko3DStats().copyReadback.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);  // banjo-nx (2026-10-19)
            dkCmdBufCopyImageToBuffer(cmd, &v, &rect, &dst, 0);
            flushBarriers();
            dkCmdBufBarrier(cmd, DkBarrier_Full, DkInvalidateFlags_L2Cache);
        }
        else {
            const Deko3DTexture *dt = static_cast<const Deko3DTexture *>(dstLoc.texture);
            const Deko3DTexture *st = static_cast<const Deko3DTexture *>(srcLoc.texture);
            if (!dt || !st || !dt->valid || !st->valid)
                return;
            DkImageView dv = subresourceView(dt, dstLoc.subresource.mipLevel, dstLoc.subresource.arrayIndex);
            DkImageView sv = subresourceView(st, srcLoc.subresource.mipLevel, srcLoc.subresource.arrayIndex);
            DkImageRect srect;
            if (srcBox)
                srect = {uint32_t(srcBox->left), uint32_t(srcBox->top), uint32_t(srcBox->front), uint32_t(srcBox->right - srcBox->left),
                         uint32_t(srcBox->bottom - srcBox->top), uint32_t(std::max(srcBox->back - srcBox->front, 1))};
            else
                srect = {0, 0, 0, mipDim(st->desc.width, srcLoc.subresource.mipLevel), mipDim(st->desc.height, srcLoc.subresource.mipLevel),
                         st->type == DkImageType_3D ? mipDim(st->desc.depth, srcLoc.subresource.mipLevel) : 1};
            DkImageRect drect = {dstX, dstY, dt->type == DkImageType_3D ? dstZ : 0, srect.width, srect.height, srect.depth};
            if (!clipCopy(st, srcLoc.subresource.mipLevel, srect, dt, dstLoc.subresource.mipLevel, drect))
                return;
            flushBarriers();
            GetDeko3DStats().copyImage.store(GetDeko3DStats().copyImage.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);  // banjo-nx (2026-10-19)
            dkCmdBufCopyImage(cmd, &sv, &srect, &dv, &drect, 0);
            invalidateZcullAfterCopy(dt);
        }
    }

    void Deko3DCommandList::invalidateZcullAfterCopy(const Deko3DTexture *dst) {
        // banjo-nx (2026-10-18): deko3d drops the Zcull data only when another depth target is bound; a copy into the
        // bound one (the copy engine does not update Zcull) leaves it describing the old depth, which would cull draws
        // the new depth passes.
        if (Deko3DGetOptions().zcull && (dst->desc.flags & RenderTextureFlag::DEPTH_TARGET))
            flushBarriers();
            dkCmdBufBarrier(cmd, DkBarrier_None, DkInvalidateFlags_Zcull);
    }

    void Deko3DCommandList::copyBuffer(const RenderBuffer *dst, const RenderBuffer *src) {
        const Deko3DBuffer *d = static_cast<const Deko3DBuffer *>(dst);
        const Deko3DBuffer *s = static_cast<const Deko3DBuffer *>(src);
        if (!d || !s)
            return;
        flushBarriers();
        GetDeko3DStats().copyBuffer.store(GetDeko3DStats().copyBuffer.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);  // banjo-nx (2026-10-19)
        dkCmdBufCopyBuffer(cmd, s->gpu, d->gpu, uint32_t(std::min(d->desc.size, s->desc.size)));
    }

    void Deko3DCommandList::copyTexture(const RenderTexture *dst, const RenderTexture *src) {
        const Deko3DTexture *dt = static_cast<const Deko3DTexture *>(dst);
        const Deko3DTexture *st = static_cast<const Deko3DTexture *>(src);
        if (!dt || !st || !dt->valid || !st->valid)
            return;
        const uint32_t levels = std::min(std::max<uint32_t>(dt->desc.mipLevels, 1), std::max<uint32_t>(st->desc.mipLevels, 1));
        const uint32_t layers = std::max<uint32_t>(std::min(dt->desc.arraySize, st->desc.arraySize), 1);
        for (uint32_t m = 0; m < levels; m++) {
            for (uint32_t l = 0; l < layers; l++) {
                DkImageView dv = subresourceView(dt, m, l);
                DkImageView sv = subresourceView(st, m, l);
                DkImageRect r = {0, 0, 0, mipDim(st->desc.width, m), mipDim(st->desc.height, m),
                                 st->type == DkImageType_3D ? mipDim(st->desc.depth, m) : 1};
                DkImageRect rd = r;
                if (!clipCopy(st, m, r, dt, m, rd))
                    continue;
                flushBarriers();
                GetDeko3DStats().copyImage.store(GetDeko3DStats().copyImage.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);  // banjo-nx (2026-10-19)
                dkCmdBufCopyImage(cmd, &sv, &r, &dv, &rd, 0);
            }
        }
        invalidateZcullAfterCopy(dt);
    }

    void Deko3DCommandList::resolveTexture(const RenderTexture *dst, const RenderTexture *src) {
        const Deko3DTexture *dt = static_cast<const Deko3DTexture *>(dst);
        const Deko3DTexture *st = static_cast<const Deko3DTexture *>(src);
        if (!dt || !st || !dt->valid || !st->valid)
            return;
        if (dt->desc.width != st->desc.width || dt->desc.height != st->desc.height) {
            // (sizes differ: the region path, cut to both)
            resolveTextureRegion(dst, 0, 0, src, nullptr, RenderResolveMode());
            return;
        }
        DkImageView dv = dt->defaultView();
        DkImageView sv = st->defaultView();
        flushBarriers();
        GetDeko3DStats().copyResolve.store(GetDeko3DStats().copyResolve.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);  // banjo-nx (2026-10-19)
        dkCmdBufResolveImage(cmd, &sv, &dv);
        invalidateZcullAfterCopy(dt);
    }

    void Deko3DCommandList::resolveTextureRegion(const RenderTexture *dst, uint32_t dstX, uint32_t dstY, const RenderTexture *src, const RenderRect *srcRect, RenderResolveMode) {
        const Deko3DTexture *dt = static_cast<const Deko3DTexture *>(dst);
        const Deko3DTexture *st = static_cast<const Deko3DTexture *>(src);
        if (!dt || !st || !dt->valid || !st->valid)
            return;
        const bool whole = dstX == 0 && dstY == 0 && dt->desc.width == st->desc.width && dt->desc.height == st->desc.height &&
                           (!srcRect || (srcRect->left == 0 && srcRect->top == 0 && uint32_t(srcRect->right) == st->desc.width &&
                                         uint32_t(srcRect->bottom) == st->desc.height));
        if (whole) {
            resolveTexture(dst, src);
            return;
        }
        // A region: the 2D engine's blit of the multisampled source (it averages the samples it covers).
        DkImageView dv = dt->defaultView();
        DkImageView sv = st->defaultView();
        const RenderRect r = srcRect ? *srcRect : RenderRect(0, 0, int32_t(st->desc.width), int32_t(st->desc.height));
        if (r.left < 0 || r.top < 0 || r.right <= r.left || r.bottom <= r.top)
            return;
        DkImageRect srect = {uint32_t(r.left), uint32_t(r.top), 0, uint32_t(r.right - r.left), uint32_t(r.bottom - r.top), 1};
        DkImageRect drect = {dstX, dstY, 0, srect.width, srect.height, 1};
        if (!clipCopy(st, 0, srect, dt, 0, drect))
            return;
        flushBarriers();
        GetDeko3DStats().copyBlit.store(GetDeko3DStats().copyBlit.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);  // banjo-nx (2026-10-19)
        dkCmdBufBlitImage(cmd, &sv, &srect, &dv, &drect, DkBlitFlag_FilterLinear, 0);
        invalidateZcullAfterCopy(dt);
    }

    void Deko3DCommandList::buildBottomLevelAS(const RenderAccelerationStructure *, RenderBufferReference, const RenderBottomLevelASBuildInfo &) {}
    void Deko3DCommandList::buildTopLevelAS(const RenderAccelerationStructure *, RenderBufferReference, RenderBufferReference, const RenderTopLevelASBuildInfo &) {}
    void Deko3DCommandList::discardTexture(const RenderTexture *) {}
    void Deko3DCommandList::resetQueryPool(const RenderQueryPool *, uint32_t, uint32_t) {}

    void Deko3DCommandList::writeTimestamp(const RenderQueryPool *queryPool, uint32_t queryIndex) {
        const Deko3DQueryPool *pool = static_cast<const Deko3DQueryPool *>(queryPool);
        if (!pool || !pool->block || queryIndex >= pool->results.size())
            return;
        // The timestamp when the work before it has finished (bottom of the pipe), as Vulkan's.
        flushBarriers();
        dkCmdBufReportCounter(cmd, DkCounter_Timestamp, dkMemBlockGetGpuAddr(pool->block) + uint64_t(queryIndex) * 16);
    }

    void Deko3DCommandList::setBlendConstant(const float rgba[4]) {
        if (memcmp(blendConstant, rgba, sizeof(blendConstant)) == 0)
            return;
        memcpy(blendConstant, rgba, sizeof(blendConstant));
        dkCmdBufSetBlendConst(cmd, rgba[0], rgba[1], rgba[2], rgba[3]);
    }

    bool Deko3DBindMemExport(RenderCommandList *commandList, const RenderBuffer *buffer) {
        Deko3DCommandList *cl = static_cast<Deko3DCommandList *>(commandList);
        const Deko3DBuffer *b = static_cast<const Deko3DBuffer *>(buffer);
        if (!cl || !b || !b->block)
            return false;
        constexpr uint32_t slot = 14;
        const DkBufExtents e = {b->gpu, uint32_t(std::min<uint64_t>(b->desc.size, 0xFFFFF000u))};
        if (!(cl->graphicsSsboValid & (1u << slot)) || cl->graphicsSsbo[slot].addr != e.addr || cl->graphicsSsbo[slot].size != e.size) {
            cl->graphicsSsbo[slot] = e;
            cl->graphicsSsboValid |= 1u << slot;
            cl->ssboBoundStages[slot] = 0;
        }
        return true;
    }

    bool Deko3DBindFetchData(RenderCommandList *commandList, uint64_t address, uint32_t size) {
        Deko3DCommandList *cl = static_cast<Deko3DCommandList *>(commandList);
        if (!cl || !address)
            return false;
        const DkBufExtents e = {address, std::max<uint32_t>(alignUp(std::min(size, 0xFFFFF000u), 16), 16)};
        if (!(cl->graphicsSsboValid & (1u << AddressWindowSlot)) || cl->graphicsSsbo[AddressWindowSlot].addr != e.addr ||
            cl->graphicsSsbo[AddressWindowSlot].size != e.size) {
            cl->graphicsSsbo[AddressWindowSlot] = e;
            cl->graphicsSsboValid |= 1u << AddressWindowSlot;
            cl->ssboBoundStages[AddressWindowSlot] = 0;
        }
        return true;
    }

    bool Deko3DBindUniform(RenderCommandList *commandList, uint32_t slot, uint64_t address, uint32_t size) {
        Deko3DCommandList *cl = static_cast<Deko3DCommandList *>(commandList);
        if (!cl || !address || slot >= 16)
            return false;
        const DkBufExtents e = {address, size};
        if (!(cl->graphicsUboValid & (1u << slot)) || cl->graphicsUbo[slot].addr != e.addr || cl->graphicsUbo[slot].size != e.size) {
            cl->graphicsUbo[slot] = e;
            cl->graphicsUboValid |= 1u << slot;
            cl->uboBoundStages[slot] = 0;
        }
        return true;
    }

    bool Deko3DPushUniform(RenderCommandList *commandList, uint64_t address, uint32_t uboSize, uint32_t offset,
                           const void *data, uint32_t size) {
        Deko3DCommandList *cl = static_cast<Deko3DCommandList *>(commandList);
        if (!cl || !address || !size || offset + size > uboSize)
            return false;
        // Pipelined: the draws recorded before it read the old values (as setGraphicsPushConstants' updates). In pieces
        // of at most 1 KB: Ryujinx stages one update packet in a 512-word buffer and throws on a longer one (emulator
        // run 60: "Destination is too short" in ConstantBufferUpdater.Update, the GPU thread gone); the console takes
        // either, and consecutive pieces cost a few command words each.
        constexpr uint32_t kPiece = 1024;
        const uint8_t *bytes = static_cast<const uint8_t *>(data);
        for (uint32_t done = 0; done < size;) {
            const uint32_t n = std::min(kPiece, size - done);
            dkCmdBufPushConstants(cl->cmd, address, uboSize, offset + done, alignUp(n, 4), bytes + done);
            done += n;
        }
        return true;
    }

    bool Deko3DSetSharedWords(RenderCommandList *commandList, const void *cpu, uint32_t size) {
        Deko3DCommandList *cl = static_cast<Deko3DCommandList *>(commandList);
        if (!cl)
            return false;
        cl->sharedWords = static_cast<const uint8_t *>(cpu);
        cl->sharedWordsSize = size;
        if (!cpu)
            cl->sharedCpu = nullptr;  // looked up again where the bound buffer is
        return true;
    }

    bool Deko3DSetBlendConstant(RenderCommandList *commandList, const float rgba[4]) {
        if (!commandList)
            return false;
        static_cast<Deko3DCommandList *>(commandList)->setBlendConstant(rgba);
        return true;
    }

    // ---- Deko3DCommandQueue

    // A GPU self-test of the frame capture's path (debug builds): a render target cleared to a known colour, copied to a
    // buffer by the copy engine, read back by the CPU; with and without hardware compression. Logs the first pixel.
    static void readbackSelfTest(Deko3DDevice *device) {
        DkCmdBufMaker cm;
        dkCmdBufMakerDefaults(&cm, device->dk);
        DkCmdBuf cmd = dkCmdBufCreate(&cm);
        DkMemBlock cmdMem = device->makeBlock(64u << 10, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, "self-test commands");
        DkMemBlock out = device->makeBlock(64u << 10, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, "self-test readback");
        for (int compressed = 0; compressed < 2; compressed++) {
            DkImageLayoutMaker m;
            dkImageLayoutMakerDefaults(&m, device->dk);
            m.flags = DkImageFlags_UsageRender | DkImageFlags_Usage2DEngine | (compressed ? DkImageFlags_HwCompression : 0);
            m.format = DkImageFormat_RGBA8_Unorm;
            m.dimensions[0] = 64;
            m.dimensions[1] = 64;
            DkImageLayout layout;
            dkImageLayoutInitialize(&layout, &m);
            const uint32_t size = uint32_t(dkImageLayoutGetSize(&layout));
            DkMemBlock imgMem = device->makeBlock(alignUp(size, 1u << 16), DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image, "self-test image");
            DkImage image;
            dkImageInitialize(&image, &layout, imgMem, 0);
            memset(dkMemBlockGetCpuAddr(out), 0x11, 64 * 64 * 4);
            dkCmdBufClear(cmd);
            dkCmdBufAddMemory(cmd, cmdMem, 0, 64u << 10);
            DkImageView view;
            dkImageViewDefaults(&view, &image);
            dkCmdBufBindRenderTarget(cmd, &view, nullptr);
            const DkScissor all = {0, 0, 64, 64};
            dkCmdBufSetScissors(cmd, 0, &all, 1);
            dkCmdBufClearColorFloat(cmd, 0, DkColorMask_RGBA, 0.25f, 0.5f, 0.75f, 1.0f);
            dkCmdBufBarrier(cmd, DkBarrier_Full, DkInvalidateFlags_Image);
            const DkImageRect rect = {0, 0, 0, 64, 64, 1};
            const DkCopyBuf dst = {dkMemBlockGetGpuAddr(out), 64 * 4, 64 * 64 * 4};
            GetDeko3DStats().copyReadback.store(GetDeko3DStats().copyReadback.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);  // banjo-nx (2026-10-19)
            dkCmdBufCopyImageToBuffer(cmd, &view, &rect, &dst, 0);
            dkCmdBufBarrier(cmd, DkBarrier_Full, DkInvalidateFlags_L2Cache);
            DkFence fence{};
            dkCmdBufSignalFence(cmd, &fence, true);
            dkQueueSubmitCommands(device->queue, dkCmdBufFinishList(cmd));
            dkQueueFlush(device->queue);
            const DkResult r = dkFenceWait(&fence, 2'000'000'000ll);
            const uint8_t *px = static_cast<const uint8_t *>(dkMemBlockGetCpuAddr(out));
            logf("self-test (%s render target): clear (0.25, 0.5, 0.75, 1) read back as %u %u %u %u (expected 64 128 191 255; "
                 "fence %d)", compressed ? "compressed" : "plain", px[0], px[1], px[2], px[3], int(r));
            dkQueueWaitIdle(device->queue);
            device->destroyBlock(imgMem);
        }
        dkCmdBufDestroy(cmd);
        device->destroyBlock(cmdMem);
        device->destroyBlock(out);
    }

    Deko3DCommandQueue::Deko3DCommandQueue(Deko3DDevice *device, RenderCommandListType type) : device(device), type(type) {
#if defined(PLUME_DEKO3D_GUARDS)
        readbackSelfTest(device);
#endif
    }

    std::unique_ptr<RenderCommandList> Deko3DCommandQueue::createCommandList() {
        return std::make_unique<Deko3DCommandList>(this);
    }

    std::unique_ptr<RenderSwapChain> Deko3DCommandQueue::createSwapChain(const RenderSwapChainDesc &desc) {
        auto swapchain = std::make_unique<Deko3DSwapChain>(this, desc);
        return swapchain;
    }

    void Deko3DCommandQueue::executeCommandLists(const RenderCommandList **commandLists, uint32_t count, RenderCommandSemaphore **, uint32_t, RenderCommandSemaphore **, uint32_t, RenderCommandFence *signalFence) {
        // One queue: its order already serialises what semaphores order in Vulkan (acquires and presents are queue
        // operations in deko3d).
        for (uint32_t i = 0; i < count; i++) {
            const Deko3DCommandList *cl = static_cast<const Deko3DCommandList *>(commandLists[i]);
            if (cl && cl->list)
                dkQueueSubmitCommands(device->queue, cl->list);
        }
        if (signalFence) {
            Deko3DCommandFence *f = static_cast<Deko3DCommandFence *>(signalFence);
            // flush: the GPU's caches are written out before the fence signals, so the CPU reads what the GPU wrote
            // (readbacks, timestamps) once it has waited for it.
            dkQueueSignalFence(device->queue, &f->fence, true);
            f->pending = true;
        }
        device->noteSubmission();
        dkQueueFlush(device->queue);
        device->checkGuards();
        device->collectRetired();
    }

    void Deko3DCommandQueue::waitForCommandFence(RenderCommandFence *fence) {
        Deko3DCommandFence *f = static_cast<Deko3DCommandFence *>(fence);
        if (!f || !f->pending)
            return;
        // Until it signals, or the queue has failed: memory the submission uses (command memory with its compute
        // launch descriptors, uploads) is reused once this returns, so giving up on a late fence made the GPU run
        // garbage (Ryujinx run 16: shaders decoded from data, then a lost device). Logged at 2, 4, 8, 16 ... s.
        for (int waited = 1;; waited++) {
            const DkResult r = dkFenceWait(&f->fence, 2'000'000'000ll);
            if (r == DkResult_Success)
                break;
            const bool failed = dkQueueIsInErrorState(device->queue);
            if (failed || (waited & (waited - 1)) == 0)
                logf("a fence has not signalled after %d s (result %d)%s", waited * 2, int(r),
                     failed ? "; the queue is in an error state: not waited for" : "");
            if (failed)
                break;
        }
        f->pending = false;
    }

    // ---- Deko3DSwapChain

    // sa2-nx (2026-10-20, 1.0.3.1): Deko3DSetSwapChainSize's size (width << 32 | height; 0 = the window's).
    static std::atomic<uint64_t> g_wantedSwapChainSize{0};

    void Deko3DSetSwapChainSize(uint32_t width, uint32_t height) {
        g_wantedSwapChainSize.store(width && height ? (uint64_t(width) << 32 | height) : 0, std::memory_order_relaxed);
    }

    Deko3DSwapChain::Deko3DSwapChain(Deko3DCommandQueue *queue, const RenderSwapChainDesc &desc) : queue(queue), device(queue->device), desc(desc) {
        resize();
    }

    Deko3DSwapChain::~Deko3DSwapChain() { destroy(); }

    void Deko3DSwapChain::destroy() {
        if (!swapchain)
            return;
        dkQueueWaitIdle(device->queue);
        dkSwapchainDestroy(swapchain);
        swapchain = nullptr;
        textures.clear();
        for (DkMemBlock b : blocks)
            device->destroyBlock(b);
        blocks.clear();
    }

    bool Deko3DSwapChain::resize() {
        // The window's size, read before the old swapchain lets go of the window (afterwards it reads 1x1: the first
        // emulator runs). An implausible size keeps the current one (1280x720 at first); the same size keeps the
        // swapchain.
        uint32_t w = 0, h = 0;
        NWindow *window = desc.renderWindow ? desc.renderWindow : nwindowGetDefault();
        // sa2-nx (2026-10-20, 1.0.3.1): a size set by Deko3DSetSwapChainSize (the console's display: 1080p docked)
        // comes first; the window takes it below, once the old swapchain has let go of its buffers.
        const uint64_t wanted = g_wantedSwapChainSize.load(std::memory_order_relaxed);
        if (wanted) {
            w = uint32_t(wanted >> 32);
            h = uint32_t(wanted);
        } else if (nwindowGetDimensions(window, &w, &h) != 0 || w < 64 || h < 64) {
            w = width ? width : 1280;
            h = height ? height : 720;
        }
        if (swapchain && w == width && h == height)
            return true;
        destroy();
        if (wanted) {
            uint32_t cw = 0, ch = 0;
            if (nwindowGetDimensions(window, &cw, &ch) != 0 || cw != w || ch != h) {
                const Result rc = nwindowSetDimensions(window, w, h);
                logf("window: %ux%u -> %ux%u (%s)", cw, ch, w, h, R_SUCCEEDED(rc) ? "set" : "failed");
            }
        }
        width = w;
        height = h;
        DkImageFormat format = toDkFormat(desc.format);
        if (format == DkImageFormat_None)
            format = DkImageFormat_RGBA8_Unorm;
        const uint32_t count = std::max<uint32_t>(desc.textureCount, 2);
        std::vector<const DkImage *> images;
        for (uint32_t i = 0; i < count; i++) {
            auto t = std::make_unique<Deko3DTexture>(device);
            t->desc = RenderTextureDesc::ColorTarget(w, h, desc.format);
            t->format = format;
            t->type = DkImageType_2D;
            t->ownsMemory = false;
            DkImageLayoutMaker m;
            dkImageLayoutMakerDefaults(&m, device->dk);
            m.flags = DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_HwCompression | DkImageFlags_Usage2DEngine;
            m.format = format;
            m.dimensions[0] = w;
            m.dimensions[1] = h;
            dkImageLayoutInitialize(&t->layout, &m);
            const uint32_t size = uint32_t(dkImageLayoutGetSize(&t->layout));
            const uint32_t align = dkImageLayoutGetAlignment(&t->layout);
            DkMemBlock block = device->makeBlock(alignUp(size, std::max<uint32_t>(align, DK_MEMBLOCK_ALIGNMENT)), DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image, "swapchain image");
            blocks.push_back(block);
            dkImageInitialize(&t->image, &t->layout, block, 0);
            t->valid = true;
            images.push_back(&t->image);
            textures.push_back(std::move(t));
        }
        DkSwapchainMaker sm;
        dkSwapchainMakerDefaults(&sm, device->dk, window, images.data(), count);
        swapchain = dkSwapchainCreate(&sm);
        if (!swapchain) {
            logf("dkSwapchainCreate failed");
            return false;
        }
        dkSwapchainSetSwapInterval(swapchain, vsync ? 1 : 0);
        logf("swapchain: %u images of %ux%u (format %d), swap interval %u", count, w, h, int(format), vsync ? 1u : 0u);
        return true;
    }

    bool Deko3DSwapChain::needsResize() const {
        if (swapchain == nullptr)
            return true;
        // sa2-nx (2026-10-20, 1.0.3.1): another size wanted (docking or undocking).
        const uint64_t wanted = g_wantedSwapChainSize.load(std::memory_order_relaxed);
        return wanted && (uint32_t(wanted >> 32) != width || uint32_t(wanted) != height);
    }

    void Deko3DSwapChain::setVsyncEnabled(bool vsyncEnabled) {
        vsync = vsyncEnabled;
        if (swapchain)
            dkSwapchainSetSwapInterval(swapchain, vsync ? 1 : 0);
    }

    RenderTexture *Deko3DSwapChain::getTexture(uint32_t textureIndex) {
        return textureIndex < textures.size() ? textures[textureIndex].get() : nullptr;
    }

    bool Deko3DSwapChain::acquireTexture(RenderCommandSemaphore *, uint32_t *textureIndex) {
        if (!swapchain)
            return false;
        // A queue operation: the queue waits for the image before the commands submitted after this.
        const int slot = dkQueueAcquireImage(device->queue, swapchain);
        if (slot < 0)
            return false;
        *textureIndex = uint32_t(slot);
        return true;
    }

    bool Deko3DSwapChain::present(uint32_t textureIndex, RenderCommandSemaphore **, uint32_t) {
        if (!swapchain)
            return false;
        dkQueuePresentImage(device->queue, swapchain, int(textureIndex));
        return true;
    }

    void Deko3DSwapChain::wait() {}
};
