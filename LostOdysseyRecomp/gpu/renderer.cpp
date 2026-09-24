#include "taa_collection.h"
#include "taa_binding_producer.h"
#include "temporal_evidence.h"
#include "scene_aa_provenance.h"
#include "scene_copy_promotion_policy.h"
#include "scene_copy_promotion_shaders.h"
#include "bloom_prefilter.h"
#include <stdafx.h>
#include "renderer.h"
#include <os/user_paths.h>
#include "render_resolution.h"
#include "movie_clear.h"
#include "video.h"
#include "command_processor.h"
#include "shader_source_capture.h"
#include "position_evidence_collection.h"
#include "geometry_prepare.h"
#include "vertex_cache.h"
#include "texture_descriptor_cache.h"
#include "texture_key.h"
#include "depth_format.h"
#include "depth_clear_layout.h"
#include "polygon_offset.h"
#include "pipeline_cache.h"
#include "texture_layout.h"
#include "temporal_scene.h"
#include "temporal_jitter.h"
#include "temporal_history.h"
#include "sr_scene_input_policy.h"
#include "temporal_lifecycle.h"
#include "motion_options.h"
#include "motion_replay_gpu.h"
#include "fsr_alpha_replay_gpu.h"
#include "fsr_alpha_postprocess_gpu.h"
#include "fsr_alpha_propagation_gpu.h"
#include "presentation.h"
#include <settings/config.h>
#include "shader/xenos_translator.h"
#include "shader/dxc_compiler.h"
#include "shader/cache.h"
#include "shader/binary_cache.h"
#include "shader/preparation_queue.h"
#include "shader/retry_state.h"
#include "shader/startup_cache.h"
#include "shader/portable_shader_pack.h"
#include "shader/portable_shader_contract.h"
#include "shader/resource_scan.h"
#include "shader/source_store.h"
#include "shader/resource_xex.h"
#include "shader/resource_variants.h"
#include <kernel/io/file_system.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <os/shader_log.h>
#include "color_qualification.h"
#include "render_timing.h"
#include "render_batch_policy.h"
#include "render_arena_policy.h"
#include "resolve_copy_policy.h"
#include <os/log_file.h>
#include <os/startup_diagnostics.h>
#include <os/capture_archive.h>
#include <version.h>

#ifdef LO_GPU_PLUME
#include <plume_render_interface.h>
#include <plume_render_interface_builders.h>
#include "dlss_ngx.h"
#include "temporal_upscaler.h"
#include "fsr_projection.h"
#include "dlss_evaluate_capture.h"
#include "draw_timing.h"
#endif

#include <algorithm>
#include <atomic>
#include <thread>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <future>
#include <set>
#include <tuple>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <string_view>
#include "gpu/frame_plan.h"

namespace gpu::renderer
{
    namespace binding = gpu::taa_collection::binding;
    namespace {
        std::mutex framePlanMutex;
        frame_plan::FramePlan committedPlan{};
        std::mutex catalogMutex;
        std::unordered_map<uint64_t, frame_plan::SurfaceRole> catalogRoles;
        uint64_t CatalogKey(uint32_t surfaceInfo, uint32_t colorInfo)
        {
            return (uint64_t(colorInfo & 0xFFF) << 32) | (surfaceInfo & 0x3FFF);
        }
        resolution::TargetRole ResolveCatalogRole(uint32_t base, uint32_t pitch)
        {
            std::lock_guard lock(catalogMutex);
            const auto found = catalogRoles.find((uint64_t(base) << 32) | pitch);
            if (found == catalogRoles.end()) return resolution::TargetRole::Unknown;
            return found->second == frame_plan::SurfaceRole::Scene ? resolution::TargetRole::Scene
                : found->second == frame_plan::SurfaceRole::Fixed ? resolution::TargetRole::Fixed
                : resolution::TargetRole::Unknown;
        }
        std::mutex captureMutex;
        bool captureBusy = false, capturePending = false, fsrCaptureBusy = false;
        std::wstring captureStatus;
        std::future<os::CaptureArchiveResult> captureArchive;

        // Called under captureMutex. Polling never waits on compression.
        void UpdateCaptureArchive(bool wait = false)
        {
            if (!captureArchive.valid() || (!wait &&
                captureArchive.wait_for(std::chrono::seconds(0)) != std::future_status::ready)) return;
            const auto result = captureArchive.get();
            if (result.saved)
            {
                LOG_INFO("render capture archive saved: {}", FileSystem::PathUtf8(result.archive));
                captureStatus = L"归档已保存 / Archive saved: " + result.archive.wstring();
                if (result.cleanupError)
                {
                    LOG_WARNING("render capture source cleanup failed: {}: {}",
                        FileSystem::PathUtf8(result.directory), result.cleanupError.message());
                    captureStatus += L" (原始目录清理失败 / Source cleanup failed)";
                }
            }
            else
            {
                LOG_WARNING("render capture archive failed: {}: {}",
                    FileSystem::PathUtf8(result.directory), result.error.message());
                captureStatus = L"归档失败，原始文件保留 / Archive failed: " + result.directory.wstring();
            }
            captureBusy = false;
        }
    }
    void RequestDebugCapture()
    {
        std::lock_guard lock(captureMutex);
        UpdateCaptureArchive();
        if (captureBusy || fsrCaptureBusy) return;
#ifdef LO_GPU_PLUME
        captureBusy = capturePending = true;
        captureStatus = L"等待下一完整帧 / Waiting for next frame";
#else
        captureStatus = L"当前构建不支持渲染捕获 / Renderer unavailable";
#endif
    }
    std::wstring DebugCaptureStatus() { std::lock_guard lock(captureMutex); UpdateCaptureArchive(); return captureStatus; }
    void WaitDebugCaptureArchive()
    {
        std::lock_guard lock(captureMutex);
        UpdateCaptureArchive(true);
    }
    bool DebugCaptureBusy() { std::lock_guard lock(captureMutex); UpdateCaptureArchive(); return captureBusy; }
#ifdef LO_GPU_PLUME
    using namespace plume;

    namespace
    {
        // ---- register indices ---------------------------------------------
        constexpr uint32_t REG_RB_SURFACE_INFO = 0x2000;
        constexpr uint32_t REG_RB_COLOR_INFO = 0x2001;
        constexpr uint32_t REG_RB_DEPTH_INFO = 0x2002;
        constexpr uint32_t REG_RB_DEPTH_CLEAR = 0x200B;
        constexpr uint32_t REG_RB_COLOR_CLEAR = 0x200C;
        constexpr uint32_t REG_PA_SC_WINDOW_OFFSET = 0x2080;
        constexpr uint32_t REG_PA_SC_WINDOW_SCISSOR_TL = 0x2081;
        constexpr uint32_t REG_PA_SC_WINDOW_SCISSOR_BR = 0x2082;
        constexpr uint32_t REG_VGT_INDX_OFFSET = 0x2102;
        constexpr uint32_t REG_RB_COLOR_MASK = 0x2104;
        constexpr uint32_t REG_RB_ALPHA_REF = 0x210E;
        constexpr uint32_t REG_RB_STENCILREFMASK_BF = 0x210C;
        constexpr uint32_t REG_RB_STENCILREFMASK = 0x210D;
        constexpr uint32_t REG_PA_CL_VPORT_XSCALE = 0x210F;
        constexpr uint32_t REG_RB_DEPTHCONTROL = 0x2200;
        constexpr uint32_t REG_RB_BLENDCONTROL0 = 0x2201;
        constexpr uint32_t REG_RB_COLORCONTROL = 0x2202;
        constexpr uint32_t REG_PA_SU_SC_MODE_CNTL = 0x2205;
        constexpr uint32_t REG_PA_CL_VTE_CNTL = 0x2206;
        constexpr uint32_t REG_RB_MODECONTROL = 0x2208;
        constexpr uint32_t REG_PA_SU_VTX_CNTL = 0x2302;
        constexpr uint32_t REG_RB_COPY_CONTROL = 0x2318;
        constexpr uint32_t REG_RB_COPY_DEST_BASE = 0x2319;
        constexpr uint32_t REG_RB_COPY_DEST_PITCH = 0x231A;
        constexpr uint32_t REG_RB_COPY_DEST_INFO = 0x231B;
        constexpr uint32_t REG_ALU_CONSTANTS = 0x4000;
        constexpr uint32_t REG_FETCH_CONSTANTS = 0x4800;
        constexpr uint32_t REG_BOOL_CONSTANTS = 0x4900;
        constexpr uint32_t REG_LOOP_CONSTANTS = 0x4908;

        constexpr uint32_t kUploadRingSize = 96u << 20;
        constexpr uint32_t kUploadHeadroom = 24u << 20;     // per-draw slack checked before a draw records anything
        constexpr uint32_t kReadbackSize = 128u << 20;
        constexpr uint32_t kVertexFetchSlots = 96;
        constexpr uint32_t kTextureSlots = 32;

        uint32_t Reg(uint32_t index) { return g_commandProcessor.ReadRegister(index); }
        float RegF(uint32_t index) { uint32_t v = Reg(index); float f; memcpy(&f, &v, 4); return f; }
        uint8_t* Phys(uint32_t physicalAddress) { return static_cast<uint8_t*>(g_memory.Translate(0xA0000000u + (physicalAddress & 0x1FFFFFFF))); }

        struct HotCaptureEnvironment
        {
            bool geometryCaptureEnabled = false;
            bool dumpResolveDirConfigured = false;
            std::string geometryCaptureDir;
            std::string dumpResolveDir;
            uint32_t dumpDrawFrame = 0;
            uint32_t dumpResolveFrame = 0;
        };

        const HotCaptureEnvironment& GetHotCaptureEnvironment()
        {
            // These are launch-time diagnostics. Runtime capture requests keep
            // using captureFrame/debugCaptureDir and take precedence below.
            static const HotCaptureEnvironment environment = []
            {
                HotCaptureEnvironment result;
                if (const char* value = getenv("LO_GEOMETRY_CAPTURE_DIR"))
                {
                    result.geometryCaptureEnabled = true;
                    result.geometryCaptureDir = value;
                }
                if (const char* value = getenv("LO_DUMP_RESOLVE_DIR"))
                {
                    result.dumpResolveDirConfigured = true;
                    result.dumpResolveDir = value;
                }
                if (const char* value = getenv("LO_DUMP_DRAW_SEQ"))
                    result.dumpDrawFrame = strtoul(value, nullptr, 10);
                if (const char* value = getenv("LO_DUMP_RESOLVE_SEQ"))
                    result.dumpResolveFrame = strtoul(value, nullptr, 10);
                return result;
            }();
            return environment;
        }

        uint32_t GpuSwap(uint32_t value, uint32_t endian)
        {
            switch (endian & 3)
            {
            case 1: return ((value & 0xFF00FF00u) >> 8) | ((value & 0x00FF00FFu) << 8);
            case 2: return ByteSwap(value);
            case 3: return (value >> 16) | (value << 16);
            default: return value;
            }
        }

        void SwapBuffer(uint32_t* data, size_t dwords, uint32_t endian)
        {
            if ((endian & 3) == 0)
                return;
            for (size_t i = 0; i < dwords; i++)
                data[i] = GpuSwap(data[i], endian);
        }

        float HalfToFloat(uint16_t h)
        {
            uint32_t sign = (h >> 15) & 1, exp = (h >> 10) & 0x1F, mant = h & 0x3FF;
            uint32_t bits;
            if (exp == 0)
            {
                if (mant == 0) bits = sign << 31;
                else { float f = float(mant) / 1024.0f * std::ldexp(1.0f, -14); memcpy(&bits, &f, 4); bits |= sign << 31; }
            }
            else if (exp == 31) bits = (sign << 31) | 0x7F800000 | (mant << 13);
            else bits = (sign << 31) | ((exp + 112) << 23) | (mant << 13);
            float f; memcpy(&f, &bits, 4); return f;
        }

        uint16_t FloatToHalf(float f)
        {
            uint32_t bits; memcpy(&bits, &f, 4);
            uint32_t sign = (bits >> 16) & 0x8000;
            int32_t exp = int32_t((bits >> 23) & 0xFF) - 112;
            uint32_t mant = (bits >> 13) & 0x3FF;
            if (((bits >> 23) & 0xFF) == 0xFF) return uint16_t(sign | 0x7C00 | (mant ? 0x200 : 0));
            if (exp <= 0) return uint16_t(sign);
            if (exp >= 31) return uint16_t(sign | 0x7C00);
            return uint16_t(sign | (uint32_t(exp) << 10) | mant);
        }

        uint64_t Fnv1a(const void* data, size_t size)
        {
            uint64_t h = 0xcbf29ce484222325ull;
            auto* p = static_cast<const uint8_t*>(data);
            for (size_t i = 0; i < size; i++) { h ^= p[i]; h *= 0x100000001b3ull; }
            return h;
        }

        // ---- host resources -------------------------------------------------
        struct HostTexture
        {
            uint64_t allocationSerial = 0; // Render-target identity, never a recycled host pointer.
            binding::Producer bindingProducer;
            uint32_t bindingWidth = 0, bindingHeight = 0; // Actual uploaded allocation extent, when block-padded.
            scene_aa::Provenance aaProvenance;
            uint32_t aaValidWidth=0,aaValidHeight=0; // Proven scene domain; allocation may contain padding.
            std::unique_ptr<RenderTexture> texture;
            RenderTextureLayout layout = RenderTextureLayout::UNKNOWN;
            RenderFormat format = RenderFormat::UNKNOWN;
            uint32_t width = 0, height = 0;
            uint32_t guestWidth = 0, guestHeight = 0;
            resolution::Size resolutionSize{};
            uint32_t ScaleX(uint32_t value) const { return resolution::ScaleX(value, resolutionSize.width); }
            uint32_t ScaleY(uint32_t value) const { return resolution::Scale(value, resolutionSize.height); }
            uint32_t depthMsaa = 0;
            // Guest-memory footprint and a sampled hash of it, so a texture the
            // title streams in after we first uploaded it is noticed and re-read.
            uint32_t guestAddress = 0, guestBytes = 0;
            uint64_t guestHash = 0;
            uint64_t checkedFrame = ~0ull;
            uint64_t clearedFrame = ~0ull;   // LO_CLEAR_RT debugging
            uint64_t sdrProducerFrame = ~0ull;
            uint32_t qualifiedSdrWidth = 0;
            uint32_t qualifiedSdrHeight = 0;
        };

        struct RenderTargetKey
        {
            uint32_t base, format, pitch, height;
            bool depth;
            bool operator==(const RenderTargetKey& o) const { return base == o.base && format == o.format && pitch == o.pitch && height == o.height && depth == o.depth; }
        };
        struct RenderTargetKeyHash { size_t operator()(const RenderTargetKey& k) const { return k.base * 1000003u ^ k.format * 8191u ^ k.pitch * 131u ^ k.height ^ (k.depth ? 0x9E3779B9u : 0); } };

        using TextureKey = gpu::texture_cache::Key;
        using TextureKeyHash = gpu::texture_cache::KeyHash;

        struct Shader
        {
            std::unique_ptr<RenderShader> shader;
            xenos::TranslatedShader info;
            bool valid = false;
            position_evidence::Summary position;
            bool positionReady = false;
            xenos::retry::State retry;
        };

        using PipelineKey = gpu::pipeline_cache::Key;
        using PipelineKeyHash = gpu::pipeline_cache::KeyHash;

        struct Renderer
        {
            RenderDevice* device = nullptr;
            bool vulkan = false;
            xenos::ShaderBinaryFormat binaryFormat = xenos::ShaderBinaryFormat::Dxil;
            RenderShaderFormat renderFormat = RenderShaderFormat::DXIL;
            uint64_t constantAddresses[3]{};
            std::unique_ptr<RenderDescriptorSet> staticSamplerSet;
            RenderCommandQueue* queue = nullptr;
            static constexpr uint32_t kGpuSlots = 2;
            static_assert(kGpuSlots == gpu::render_arena::kGpuSlots, "slot count must match arena policy");
            using TextureSetCache = texture_descriptors::BatchCache<RenderTexture, RenderDescriptorSet, kTextureSlots>;
#if defined(LO_GPU_PLUME)
            struct FsrAlphaCapture {
                fsr_alpha::FrameView view;
                std::unique_ptr<RenderBuffer> buffer;
                std::filesystem::path directory;
                uint32_t rowPitch = 0;
                uint64_t submissionSerial = 0;
            };
            struct FsrAlphaEquality {
                std::array<std::unique_ptr<RenderBuffer>, 4> buffers;
                std::filesystem::path directory;
                uint32_t width = 0, height = 0, colorBpp = 0, colorPitch = 0, depthPitch = 0;
                RenderFormat colorFormat = RenderFormat::UNKNOWN, depthFormat = RenderFormat::UNKNOWN;
                uint64_t frame = 0, drawOrdinal = 0, colorAllocation = 0, depthAllocation = 0;
                uint64_t submissionSerial = 0;
                bool replayRecorded = false;
                std::string reason;
            };
            struct FsrAlphaBridgeDiagnostic {
                struct Snapshot {
                    std::string label;
                    std::unique_ptr<RenderBuffer> buffer;
                    uint32_t width = 0, height = 0, rowPitch = 0, bytesPerPixel = 0;
                    RenderFormat format = RenderFormat::UNKNOWN;
                };
                std::string fields;
                uint64_t frame = 0, submissionSerial = 0, writeOrdinal = 0;
                uint32_t eventIndex = 0;
                fsr_alpha::FrameView raw;
                std::shared_ptr<fsr_alpha::MaskLease> mask;
                fsr_alpha::MaskRect rect{};
                uint32_t rawWidth = 0, rawHeight = 0, resolvedWidth = 0, resolvedHeight = 0;
                uint32_t rawPitch = 0, resolvedPitch = 0;
                std::array<std::unique_ptr<RenderBuffer>, 2> buffers;
                std::vector<Snapshot> snapshots;
                std::vector<std::string> captureFailures;
            };
#endif
            struct GpuSlot {
                std::unique_ptr<RenderCommandList> list;
                // The prefix is list. An NGX recording error must never place
                // vendor commands in the guest continuation batch.
                std::unique_ptr<RenderCommandList> srIsolated;
                std::unique_ptr<RenderCommandList> srContinuation;
                std::unique_ptr<RenderCommandFence> fence;
                std::unique_ptr<RenderQueryPool> timingQueries;
                draw_timing::Probe drawProbe;
                std::unique_ptr<RenderBuffer> uploadRing;
                uint8_t* uploadMapped = nullptr;
                uint64_t uploadOffset = 0;
                uint64_t arenaOffset = 0;
                std::vector<std::unique_ptr<RenderDescriptorSet>> setPools[4];
                uint32_t setPoolUsed[4] = {};
                TextureSetCache textureSetCache[3];
                std::vector<std::unique_ptr<HostTexture>> retiredTextures;
                // Reused only after this slot's fence completes. Each draw gets
                // a distinct output while commands in the slot remain in flight.
                std::vector<std::unique_ptr<HostTexture>> bloomPrefilterTextures;
                size_t bloomPrefilterUsed = 0;
                uint64_t temporalSerial = 0, hdrTemporalSerial = 0, motionSerial = 0;
#if defined(LO_RENDERER_P2_EMBEDDED_TEST)
                uint64_t srUseId = 0;
#else
                SrUseToken srUseId{};
#endif
                uint64_t srSubmissionSerial = 0;
                bool srPrefixClosed = false, srIsolatedAccepted = false, srContinuationOpen = false;
                bool submitted = false;
                std::vector<std::shared_ptr<dlss::EvaluateCapture>> evaluateCaptures;
#if defined(LO_GPU_PLUME)
                // Every submitted producer batch owns its raw alpha target and
                // read-only depth framebuffer until this slot's fence completes.
                std::vector<std::shared_ptr<fsr_alpha::BatchUse>> fsrAlphaBatches;
                std::vector<std::shared_ptr<FsrAlphaCapture>> fsrAlphaCaptures;
                std::vector<std::shared_ptr<FsrAlphaEquality>> fsrAlphaEqualities;
                std::vector<std::shared_ptr<fsr_alpha::MaskLease>> fsrAlphaBridgeUses;
                std::vector<std::shared_ptr<FsrAlphaBridgeDiagnostic>> fsrAlphaBridgeDiagnostics;
#endif
                // Copied at srApplied. Flush must not read activePlan; resolution
                // changes replace that plan before they submit.
                struct DlssSubmitMark {
                    bool pending = false;
                    frame_plan::FramePlan plan{};
                    uint64_t renderFrame = 0;
                } dlssSubmit;
            };
            GpuSlot gpuSlots[kGpuSlots];
            uint32_t gpuSlot = 0;
            GpuSlot& Gpu() { return gpuSlots[gpuSlot]; }
            void BindGpuSlot()
            {
                auto& g = Gpu();
                // A successful isolated SR list leaves the ordinary renderer in
                // the continuation.  Rebinding after an upload-ring Flush must
                // therefore resume that open list instead of recording into the
                // already-ended prefix.
                commandList = g.srContinuationOpen ? g.srContinuation.get() : g.list.get();
                fence = g.fence.get();
                timingQueries = g.timingQueries.get();
                uploadRing = g.uploadRing.get();
                uploadMapped = g.uploadMapped;
            }
            // Non-owning aliases to gpuSlots[gpuSlot], rebound in BindGpuSlot.
            RenderCommandList* commandList = nullptr;
            RenderCommandFence* fence = nullptr;
            RenderQueryPool* timingQueries = nullptr;
            RenderBuffer* uploadRing = nullptr;
            uint8_t* uploadMapped = nullptr;
            render_timing::GpuBatches gpuTiming;
            bool timingInitialized = false;
            bool listOpen = false;

            // Vertex buffers live in a persistent arena keyed by (address, size,
            // endian). A fetch constant may describe a multi-megabyte buffer for a
            // draw that touches a few hundred vertices, so copying per draw is
            // hopeless; instead each buffer is uploaded once and re-validated with
            // exact sampled guest bytes when it is referenced again.
            std::unique_ptr<RenderBuffer> vertexArena;
            uint8_t* arenaMapped = nullptr;
            using VertexEntry = geometry_prepare::VertexEntry;
            std::vector<uint32_t> indexScratch, primitiveScratch;
            geometry_prepare::VertexCache vertexCache;
            geometry_prepare::IndexCache indexCache;
            uint64_t indexCacheHits = 0, indexCacheMisses = 0;
            std::array<uint64_t, kGpuSlots> motionArenaGeneration{};
            void ResetSlotArena(uint32_t i)
            {
                ++motionArenaGeneration[i];
                gpuSlots[i].arenaOffset = 0;
                for (auto it = vertexCache.begin(); it != vertexCache.end(); )
                {
                    if (it->second.slot == uint8_t(i))
                        it = vertexCache.erase(it);
                    else
                        ++it;
                }
            }
            uint32_t vertexUploads = 0, vertexRevalidations = 0;
            size_t vertexBytesUploaded = 0;
            std::unique_ptr<RenderBuffer> readback;

            std::unique_ptr<RenderPipelineLayout> pipelineLayout;
            RenderDescriptorSetBuilder setBuilders[5];
            const bool descriptorReuse = [] {
                const char* value = getenv("LO_DESCRIPTOR_REUSE");
                return !value || std::string_view(value) != "0";
            }();
            uint32_t descriptorHits = 0, descriptorMisses = 0;
            uint32_t vfetchDescriptorBase = 0, samplerDescriptorBase = 0;
            std::unique_ptr<RenderDescriptorSet> staticSet0;     // ring buffers + sampler palette
            std::unique_ptr<RenderDescriptorSet> staticDummySets[3]; // unused 2D / 3D / cube banks
            std::map<uint64_t, uint32_t> samplerPalette;         // sampler key -> palette index
            static constexpr uint32_t kSamplerPalette = 64;

            std::unique_ptr<RenderBuffer> dummyBuffer;
            HostTexture dummyTexture2D, dummyTexture3D, dummyTextureCube;
            std::unique_ptr<RenderShader> rectListGs;

            std::unordered_map<uint64_t, Shader> shaders[2];
            std::unordered_map<PipelineKey, std::unique_ptr<RenderPipeline>, PipelineKeyHash> pipelines;
            // Full state recipes are portable; driver blobs and object pointers
            // are never persisted. Render maps stay on the command thread.
            static constexpr uint32_t kPipelineRecipeVersion = 1;
            std::unordered_set<PipelineKey, PipelineKeyHash> pipelineRecipes, preparedPipelineKeys, usedPreparedPipelineKeys;
            std::future<gpu::pipeline_cache::WriteResult> pipelineWrite;
            bool pipelineCacheEnabled = false, pipelineRecipesDirty = false;
            uint64_t preparedPipelineHits = 0, runtimePipelineCreates = 0;
            std::unordered_map<RenderTargetKey, std::unique_ptr<HostTexture>, RenderTargetKeyHash> renderTargets;
            std::unordered_map<TextureKey, std::unique_ptr<HostTexture>, TextureKeyHash> textures;

            // Resolve results kept on the GPU, keyed by guest physical address.
            // The game samples them through fetch constants pointing at the
            // same address, and the presenter copies the frontbuffer from here.
            struct ResolvedSurface
            {
                std::unique_ptr<HostTexture> tex;
                std::unordered_map<uint64_t, std::unique_ptr<HostTexture>> fetchViews;
                uint32_t destFormat = 0, destPitch = 0;
                bool swapRedBlue = false;
                uint64_t frame = 0;
                uint64_t writeOrdinal = 0;
                uint32_t writeX = 0, writeY = 0, writeWidth = 0, writeHeight = 0;
                uint64_t sdrWriteOrdinal = 0;
                frame_plan::FramePlan sourcePlan{};
                bool sourcePlanValid = false;

            };
            // Keyed by destination address, one entry per destination format: the
            // title resolves the HDR scene to a scratch buffer as FP16 and then the
            // very same EDRAM tiles, reinterpreted as fixed 2_10_10_10, to the same
            // address. A single entry per address let the second resolve evict the
            // first, so the composite's fetch of the FP16 surface missed and fell
            // back to guest memory.
            std::unordered_map<uint32_t, std::vector<ResolvedSurface>> resolved;
            uint64_t resolveWriteOrdinal = 0;
            uint64_t nextTargetAllocation = 0;
            resolve_copy::ConsecutiveCopies consecutiveResolveCopies;
            const bool resolveCopyReuse = [] {
                const char* value = getenv("LO_RESOLVE_COPY_REUSE");
                return !value || std::string_view(value) != "0";
            }();
            uint32_t resolveCopiesRecorded = 0, resolveCopiesSkipped = 0;
            std::array<uint64_t, 2> bindingRecordedFrame{~0ull, ~0ull};
            temporal::SceneObservation temporalScene;
            std::shared_ptr<taa_collection::SparseDepthGPU> sparseCollector;
            temporal::DrawTemporalTracker drawTemporalTracker;
            double mvTrackCpuMs = 0;
            uint64_t mvScratchBytes = 0;
            const temporal::MotionOptions motionOptions = temporal::MotionOptions::Environment();
            std::unique_ptr<temporal::MotionReplayGPU> motionReplay;
#if defined(LO_GPU_PLUME)
            const bool fsrAlphaReplayEnabled = [] {
                const char* value = getenv("LO_FSR_ALPHA_REPLAY");
                return !value || std::string_view(value) != "0";
            }();
            std::unique_ptr<fsr_alpha::ReplayGPU> fsrAlphaReplay;
            const bool fsrAlphaBridgeEnabled = [] {
                const char* value = getenv("LO_FSR_ALPHA_BRIDGE");
                return !value || std::string_view(value) != "0";
            }();
            std::unique_ptr<fsr_alpha::PropagationGPU> fsrAlphaBridge;
            std::unique_ptr<fsr_alpha::PostprocessGPU> fsrAlphaPostprocess;
            std::vector<fsr_alpha::FrameView> fsrAlphaRawViews;
            uint32_t fsrAlphaBridgePairCount = 0, fsrAlphaBridgeTraceCount = 0;
            bool fsrAlphaInitFailed = false;
            const uint64_t fsrAlphaCaptureFrame = [] {
                const char* value = getenv("LO_FSR_ALPHA_CAPTURE_FRAME");
                if (!value || !*value) return ~0ull;
                char* end = nullptr;
                const auto result = std::strtoull(value, &end, 10);
                return end && *end == '\0' ? result : ~0ull;
            }();
            const std::filesystem::path fsrAlphaCaptureDir = [] {
                const char* value = getenv("LO_FSR_ALPHA_CAPTURE_DIR");
                return value && *value ? std::filesystem::path(value) : std::filesystem::path{};
            }();
            uint64_t fsrAlphaCaptureQueuedFrame = ~0ull;
            const uint64_t fsrAlphaCompareFrame = [] {
                const char* value = getenv("LO_FSR_ALPHA_COMPARE_FRAME");
                if (!value || !*value) return ~0ull;
                char* end = nullptr;
                const auto result = std::strtoull(value, &end, 10);
                return end && *end == '\0' ? result : ~0ull;
            }();
            uint64_t fsrAlphaCompareQueuedFrame = ~0ull;
#endif
            bool motionInitFailed = false;
            temporal::MotionFrameView motionView;
            uint64_t motionFinalizedFrame = ~0ull;
            std::array<uint64_t, 8> motionFailureKeys{};
            uint32_t motionFailureKeyCount = 0;
            uint64_t motionFirstFailureFrame = ~0ull;
            uint64_t motionFailureSuppressed = 0;
            std::array<uint32_t, 8> motionFinishStates{};
            uint32_t motionFinishDistinct = 0;
            uint64_t motionFinishSuppressed = 0;
            bool temporalInputProbe = false;
            bool dlssSrRequested = false;
            uint64_t dlssDisableReportedEpoch = ~0ull;
            struct DlssFrameFeedback {
                bool submitted = false;
                frame_plan::FramePlan submittedPlan{};
                uint64_t submittedFrame = 0;
                uint64_t submissionSerial = 0;
                bool hasFallback = false;
                frame_plan::DlssEffectReason fallback = frame_plan::DlssEffectReason::NoEligibleScene;
                frame_plan::FramePlan fallbackPlan{};
                uint64_t fallbackFrame = 0;
                bool submitRejected = false;
                frame_plan::FramePlan submitRejectedPlan{};
                uint64_t submitRejectedFrame = 0;
                uint64_t publishedFrame = ~0ull;
                bool publishedSubmitted = false;
                frame_plan::FramePlan publishedPlan{};
            } dlssFrame;
            static bool SameDlssIdentity(const frame_plan::FramePlan& a, const frame_plan::FramePlan& b)
            {
                return a.deviceEpoch == b.deviceEpoch && a.requestSignature == b.requestSignature &&
                    a.geometryEpoch == b.geometryEpoch;
            }
            struct DlssSceneInputSelection {
                enum class Kind : uint8_t { Ignored, Pending, Incomplete, UnknownColor, Selected };
                Kind kind = Kind::Ignored;
                temporal::TemporalFrameInputs inputs{};
            };
            void NoteDlssFrameFallback(frame_plan::DlssEffectReason reason)
            {
                // First note wins only for the same plan. A later identity on
                // this frame replaces it; an earlier identity must not hide it.
                if (dlssFrame.submitted && dlssFrame.submittedFrame == frame &&
                    SameDlssIdentity(dlssFrame.submittedPlan, activePlan)) return;
                if (dlssFrame.hasFallback && dlssFrame.fallbackFrame == frame &&
                    SameDlssIdentity(dlssFrame.fallbackPlan, activePlan)) return;
                dlssFrame.hasFallback = true;
                dlssFrame.fallback = reason;
                dlssFrame.fallbackPlan = activePlan;
                dlssFrame.fallbackFrame = frame;
                if (evaluatePage && evaluatePage->frame == frame)
                    evaluatePage->fallbackReason = dlss::capture::FrameFallbackName(reason);
            }
            void NoteDlssSubmitted(const frame_plan::FramePlan& plan, uint64_t renderFrame, uint64_t serial)
            {
                if (!serial) return;
                if (dlssFrame.submitted && dlssFrame.submittedFrame == renderFrame &&
                    SameDlssIdentity(dlssFrame.submittedPlan, plan)) return;
                dlssFrame.submitted = true;
                dlssFrame.submittedPlan = plan;
                dlssFrame.submittedFrame = renderFrame;
                dlssFrame.submissionSerial = serial;
            }
            void ClearDlssSubmit(GpuSlot& slot, bool rejected)
            {
                if (rejected && slot.dlssSubmit.pending) {
                    dlssFrame.submitRejected = true;
                    dlssFrame.submitRejectedPlan = slot.dlssSubmit.plan;
                    dlssFrame.submitRejectedFrame = slot.dlssSubmit.renderFrame;
                }
                slot.dlssSubmit = {};
            }
            void ArmDlssSubmit()
            {
                Gpu().dlssSubmit.pending = true;
                Gpu().dlssSubmit.plan = activePlan;
                Gpu().dlssSubmit.renderFrame = frame;
            }
            // Shared with the renderer fixture. DrawImpl keeps the original logs.
            DlssSceneInputSelection SelectDlssSceneCopyInputs(const temporal::TemporalFrameInputs& inputs)
            {
                DlssSceneInputSelection selected;
                const bool recoverablePending = motionReplay && motionReplay->PipelinePendingThisFrame()
                    && !motionReplay->AbortedThisFrame() && !drawTemporalTracker.Failed();
                if (recoverablePending) {
                    NoteDlssFrameFallback(frame_plan::DlssEffectReason::MotionPipelinePending);
                    selected.kind = DlssSceneInputSelection::Kind::Pending;
                    return selected;
                }
                if (!inputs.CompleteForConsumer()) {
                    selected.kind = DlssSceneInputSelection::Kind::Incomplete;
                    return selected;
                }
                if (dlssSrRequested && inputs.colorEncoding == temporal::ColorEncoding::Unknown) {
                    NoteDlssFrameFallback(frame_plan::DlssEffectReason::UnknownColorEncoding);
                    selected.kind = DlssSceneInputSelection::Kind::UnknownColor;
                    return selected;
                }
                if (dlssSrRequested) {
                    selected.kind = DlssSceneInputSelection::Kind::Selected;
                    selected.inputs = inputs;
                    return selected;
                }
                return selected;
            }
            void PublishDlssFrameOutcome()
            {
                const bool confirmed = dlssFrame.submitted && dlssFrame.submittedFrame == frame &&
                    dlssFrame.submissionSerial && SameDlssIdentity(dlssFrame.submittedPlan, activePlan);
                if (dlssFrame.publishedSubmitted && dlssFrame.publishedFrame == frame &&
                    SameDlssIdentity(dlssFrame.publishedPlan, activePlan) && confirmed) return;
                const bool rejected = dlssFrame.submitRejected && dlssFrame.submitRejectedFrame == frame &&
                    SameDlssIdentity(dlssFrame.submitRejectedPlan, activePlan);
                frame_plan::DlssExecutionObservation observation;
                observation.renderFrame = frame;
                if (confirmed) {
                    observation.plan = dlssFrame.submittedPlan;
                    observation.submissionSerial = dlssFrame.submissionSerial;
                    observation.outcome = frame_plan::DlssExecutionOutcome::Submitted;
                    observation.reason = frame_plan::DlssEffectReason::None;
                } else if (rejected) {
                    return;
                } else if (dlssFrame.hasFallback && dlssFrame.fallbackFrame == frame &&
                    SameDlssIdentity(dlssFrame.fallbackPlan, activePlan)) {
                    observation.plan = dlssFrame.fallbackPlan;
                    observation.outcome = frame_plan::DlssExecutionOutcome::Fallback;
                    observation.reason = dlssFrame.fallback;
                } else {
                    for (const auto& slot : gpuSlots)
                        if (slot.dlssSubmit.pending && slot.dlssSubmit.renderFrame == frame &&
                            SameDlssIdentity(slot.dlssSubmit.plan, activePlan)) return;
                    if (!upscaling::IsSrConsumer(activePlan.consumer) || !activePlan.cpuSerial) return;
                    observation.plan = activePlan;
                    observation.outcome = frame_plan::DlssExecutionOutcome::Fallback;
                    observation.reason = frame_plan::DlssEffectReason::NoEligibleScene;
                }
                observation.actualProvider = observation.plan.requestedUpscaler;
#if defined(LO_RENDERER_P2_EMBEDDED_TEST)
                frame_plan::ReportDlssExecution(observation);
#else
                frame_plan::ReportUpscalerExecution(observation);
#endif
                dlssFrame.publishedFrame = frame;
                dlssFrame.publishedPlan = observation.plan;
                dlssFrame.publishedSubmitted = observation.outcome == frame_plan::DlssExecutionOutcome::Submitted;
            }
            void FinishMotion(temporal::HistoryOwner* history) {
                if (!motionOptions.enabled || !history || motionFinalizedFrame == frame) return;
                motionFinalizedFrame = frame;
                render_batch::CpuTimer<> timer(motionOptions.timing);
                const auto& flags = drawTemporalTracker.FinalizeFrame();
                timer.AddTo(mvTrackCpuMs);
                if (motionReplay && !drawTemporalTracker.Failed()) {
                    motionView = motionReplay->Finish(commandList, history->CurrentDepth(), flags, history->ResetInitializationRequired());
                    if (motionView.ready) history->RecordExternalRead();
                }
                if (motionOptions.log) {
                    const auto& st = drawTemporalTracker.Stats();
                    const bool trackerFailed = drawTemporalTracker.Failed();
                    const bool aborted = motionReplay && motionReplay->AbortedThisFrame();
                    const bool pending = motionReplay && motionReplay->PipelinePendingThisFrame();
                    const bool cleared = motionReplay && motionReplay->ClearedThisFrame();
                    const bool replayFinalized = motionReplay && motionReplay->FinalizedThisFrame();
                    const char* emptyReason = trackerFailed ? "tracker_failed" : motionInitFailed ? "motion_init_failed" :
                        !motionReplay ? "no_replay" : aborted ? "replay_aborted" : pending ? "pipeline_pending" :
                        !cleared ? "scene_not_cleared" : !motionView.ready ? "finish_no_view" : "none";
                    const uint32_t state = uint32_t(trackerFailed) | (uint32_t(motionInitFailed) << 1) |
                        (uint32_t(aborted) << 2) | (uint32_t(pending) << 3) |
                        (uint32_t(cleared) << 4) | (uint32_t(motionView.ready) << 5) |
                        (uint32_t(motionReplay && motionReplay->ResourceFailedThisFrame()) << 6) |
                        (uint32_t(replayFinalized) << 7);
                    const bool seen = std::find(motionFinishStates.begin(),
                        motionFinishStates.begin() + motionFinishDistinct, state) !=
                        motionFinishStates.begin() + motionFinishDistinct;
                    const bool firstState = !seen && motionFinishDistinct < motionFinishStates.size();
                    if (firstState) motionFinishStates[motionFinishDistinct++] = state;
                    if (firstState || frame % 120 == 0) {
                        LOG_INFO("mv: frame={} epoch={} tracker_failed={} tracker_finalized={} tracked={} matched={} unmatched={} overflow={} late={} replay_draws={} replay_failed={} replay_cleared={} replay_finalized={} aborted={} resource_failed={} pending_pipeline={} pending_batches={} ready={} empty_reason={} replay_error={} suppressed_draw={} suppressed_finish={}",
                            frame, temporalEpoch, trackerFailed, drawTemporalTracker.Finalized(),
                            st.trackedCurrentDraws, st.matchedPreviousDraws, st.unmatchedDraws,
                            st.overflowDraws, st.lateDraws, motionReplay ? motionReplay->DrawCount() : 0,
                            motionReplay ? motionReplay->FailedDraws() : 0, cleared, replayFinalized, aborted,
                            motionReplay && motionReplay->ResourceFailedThisFrame(), pending,
                            motionReplay ? motionReplay->PendingCount() : 0, motionView.ready, emptyReason,
                            motionReplay ? motionReplay->LastError() : std::string{},
                            motionFailureSuppressed, motionFinishSuppressed);
                    } else ++motionFinishSuppressed;
                }
            }
            std::unique_ptr<temporal::HistoryOwner> temporalHistory;
#if defined(LO_GPU_PLUME)
            dlss::Controller* dlssController = nullptr;
            TemporalUpscaler* temporalUpscaler = nullptr;
#endif
            // Opt-in candidate, controlled through the local diagnostic file.
            // Separate owner: HDR is accumulated before bloom and tone mapping.
            std::unique_ptr<temporal::HistoryOwner> hdrTemporalHistory;
            RenderTexture* hdrTemporalOutput = nullptr;
            temporal::SceneResolve hdrTemporalSource{};
            bool hdrTonemapApplied = false, hdrTemporalInitFailed = false;
            uint32_t hdrTemporalLogs = 0;
            bool temporalExperiment=false,temporalAllowHistory=false,temporalJitter=false,temporalStableGrid=false;
            bool temporalForced=false,temporalForcedHistory=false,temporalForcedJitter=false,temporalForcedStable=false;
            bool temporalInitFailed=false;
            uint64_t temporalSupportedFrame=~0ull;
            uint32_t temporalJitterDraws=0,temporalJitterMisses=0,temporalJitterUnknowns=0;
            uint64_t temporalEpoch=1,temporalFramesLogged=0,temporalSubmittedFrame=~0ull;
            temporal::JitterSample actualRasterJitter{}, frameRasterJitter{};
            std::chrono::steady_clock::time_point srFrameTime{};
            uint64_t srTimedFrame = ~0ull;
            float srFrameDeltaMilliseconds = 0;
            bool srTimeReset = true;
            bool actualRasterJitterCaptured=false;
            uint64_t appliedPlanEpoch=~0ull;
            resolution::Size internalSize{}, requestedInternalSize{};
            frame_plan::FramePlan activePlan{};
            // Allocation failures are recorded by the GPU and can be observed
            // after later plan markers have already been committed.
            std::unordered_set<uint64_t> failedPlanEpochs;
            std::chrono::steady_clock::time_point temporalFrameTime=std::chrono::steady_clock::now();
            uint64_t temporalGapResetFrame=~0ull;
            std::unique_ptr<gpu::Presentation> sceneProcessor;
            // A single scene-copy promotion is deliberately renderer-owned. The
            // render-target map keeps the active allocation; parkedLow keeps the
            // alternate grid alive across ordinary mid-frame Flush calls.
            struct SceneCopyPromotion {
                RenderTargetKey key{};
                uint64_t frame = ~0ull, epoch = 0;
                uint64_t sourceAllocation = 0;
                HostTexture* active = nullptr;
                std::unique_ptr<HostTexture> parkedLow;
                std::unique_ptr<HostTexture> preparedPromoted;
                std::unique_ptr<HostTexture> scratch;
                std::unique_ptr<HostTexture> composite;
                temporal::TemporalFrameInputs inputs{};
                SrDispatchOptions srOptions{};
                std::shared_ptr<fsr_alpha::MaskLease> fsrMaskLease;
                RenderDescriptorSet* fallbackSet = nullptr;
                RenderDescriptorSet* rgbSet = nullptr;
                uint64_t fallbackConstants = UINT64_MAX, rgbConstants = UINT64_MAX;
                bool prepared = false, activeMapping = false, srApplied = false;
            } sceneCopyPromotion;
            SrDispatchOptions frameSrOptions{};
            uint64_t sceneCopyPromotionFrame = ~0ull;
            uint64_t srReconfigureFrame = ~0ull;
            std::unique_ptr<RenderShader> sceneCopyPromotionPs, sceneCopyPromotionRgbPs;
            std::map<uint32_t, std::unique_ptr<RenderPipeline>> sceneCopyPromotionPipelines, sceneCopyPromotionRgbPipelines;
            std::unique_ptr<RenderTexture> sceneAAOutput;
            uint32_t sceneAAWidth=0,sceneAAHeight=0,sceneAAMode=0;
            bool sceneAAEnabled=false,sceneAABusy=false,activeSpatialAA=false;
            uint64_t sceneAAConfigFrame=~0ull,sceneAAAppliedFrame=~0ull,sceneAAAllocation=0;

            // The entry at `base` whose destination format the fetch can read.
            ResolvedSurface* FindResolved(uint32_t base, uint32_t fetchFormat)
            {
                auto it = resolved.find(base);
                if (it == resolved.end())
                    return nullptr;
                for (ResolvedSurface& rs : it->second)
                    if (rs.tex && ResolveFormatMatches(fetchFormat, rs.destFormat))
                        return &rs;
                return nullptr;
            }

            // The entry at `base` written most recently - what the presenter and the
            // debug dumps want, since they ask for a surface, not a format.
            ResolvedSurface* NewestResolved(uint32_t base)
            {
                auto it = resolved.find(base);
                if (it == resolved.end())
                    return nullptr;
                ResolvedSurface* best = nullptr;
                for (ResolvedSurface& rs : it->second)
                    if (rs.tex && (!best || rs.frame >= best->frame))
                        best = &rs;
                return best;
            }

            ResolvedSurface& ResolvedSlot(uint32_t base, uint32_t destFormat)
            {
                std::vector<ResolvedSurface>& list = resolved[base];
                for (ResolvedSurface& rs : list)
                    if (rs.destFormat == destFormat)
                        return rs;
                list.emplace_back();
                list.back().destFormat = destFormat;
                return list.back();
            }

            void DropResolved(uint32_t base, uint32_t destFormat)
            {
                auto it = resolved.find(base);
                if (it == resolved.end())
                    return;
                for (auto rs = it->second.begin(); rs != it->second.end(); ++rs)
                    if (rs->destFormat == destFormat) { it->second.erase(rs); break; }
                if (it->second.empty())
                    resolved.erase(it);
            }
            bool resolveReadback = false; // LO_RESOLVE_READBACK=1: legacy CPU write-back into guest memory
            bool textureRevalidate = true; // LO_TEXTURE_STATIC=1 disables re-hashing cached textures
            uint32_t textureReuploads = 0;
            uint32_t dummyBindings = 0;

            // Separate opt-in diagnostics: ordinary frame timing must not pay
            // for a clock read on every vertex-cache operation.
            const bool vertexTimingEnabled = [] {
                const char* value = getenv("LO_VERTEX_TIMING");
                return value && std::string_view(value) == "1";
            }();
            struct VertexStage
            {
                double totalMs = 0, maxMs = 0;
                uint64_t calls = 0, bytes = 0;
                uint32_t maxAddress = 0;
                size_t maxBytes = 0;
            };
            struct VertexStageTimer
            {
                VertexStage& stage;
                bool enabled;
                uint32_t address;
                size_t bytes;
                render_batch::CpuTimer<> timer;
                VertexStageTimer(VertexStage& stage, bool enabled, uint32_t address, size_t bytes)
                    : stage(stage), enabled(enabled), address(address), bytes(bytes), timer(enabled) {}
                ~VertexStageTimer()
                {
                    if (!enabled) return;
                    double ms = 0;
                    timer.AddTo(ms);
                    stage.totalMs += ms;
                    ++stage.calls;
                    stage.bytes += bytes;
                    if (ms > stage.maxMs)
                    {
                        stage.maxMs = ms;
                        stage.maxAddress = address;
                        stage.maxBytes = bytes;
                    }
                }
            };
            struct VertexTiming
            {
                VertexStage find, match, erase, capture, copy, insert;
                size_t initialSize = 0, initialBuckets = 0;
                uint64_t initialEvictions = 0;
                uint32_t rehashes = 0;
            } vertexTiming;

            // Both diagnostic consumers need CPU segments; ordinary play does not.
            const bool cpuTimingEnabled = getenv("LO_GPU_STATS") != nullptr || render_timing::Enabled();
            uint32_t descriptorBatchLimit = 500;
            uint32_t descriptorSplits = 0, uploadSplits = 0, arenaSplits = 0;
            struct ScopedTimer
            {
                double& acc;
                render_batch::CpuTimer<> timer;
                ScopedTimer(double& value, bool enabled) : acc(value), timer(enabled) {}
                ~ScopedTimer() { timer.AddTo(acc); }
            };
            double tDraw = 0, tShader = 0, tPipeline = 0, tTexture = 0, tResolve = 0, tFlush = 0;
            double tConst = 0, tSets = 0, tVertex = 0, tBind = 0, tIndex = 0, tRecord = 0;
            double tRt = 0, tTaa = 0, tNestedFlush = 0;
            double tShaderLookup = 0, tPipelineLookup = 0, tSceneCopy = 0;
            uint32_t nShader = 0, nPipeline = 0, nTexture = 0, nResolve = 0;
            size_t texBytes = 0;
            void ResetTimers() { tDraw = tShader = tPipeline = tTexture = tResolve = tFlush = 0; tConst = tSets = tVertex = tBind = tIndex = tRecord = 0; tRt = tTaa = tNestedFlush = 0; tShaderLookup = tPipelineLookup = tSceneCopy = 0; nShader = nPipeline = nTexture = nResolve = 0; texBytes = 0; }
            std::map<uint64_t, std::unique_ptr<RenderSampler>> samplers;
            std::map<std::pair<const RenderTexture*, const RenderTexture*>, std::unique_ptr<RenderFramebuffer>> framebuffers;

            std::string shaderCacheDir;
            uint32_t drawsThisFrame = 0;
            // Draws that never reached the command list, by reason. A pass that
            // is missing from the image usually shows up here rather than as a
            // warning, because every one of these paths returns silently.
            struct DropStats
            {
                uint32_t mode, modeMask, shader, pitch, pipeline, upload, index, scissor, primMask, vfetchSkips;
                bool Any() const { return mode || shader || pitch || pipeline || upload || index || scissor || vfetchSkips; }
            } drops{};

            // Legacy mode census for relative-constant VS candidates. Mode 4
            // can still have a zero colour mask; sceneDraws only counts the
            // full-width 7e3 heuristic, not every visible material pass.
            struct SkinStats { uint32_t depthDraws, colorDraws, colorIndices, sceneDraws, sceneIndices; } skin{};
            // Relative addressing is a candidate marker, not proof of skinning.
            // Keep the actual target and output state so shadow/mask passes do
            // not get mistaken for the character's visible material pass.
            using RelativeDrawKey = std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint32_t, uint32_t>;
            struct RelativeDrawStats { uint32_t draws = 0, indices = 0; };
            std::map<RelativeDrawKey, RelativeDrawStats> relativeDraws;
            uint32_t frame = 0;
            uint64_t collectionFrame = ~0ull;
            uint32_t captureFrame = 0;
            uint64_t captureRequest = 0;
            std::string debugCaptureDir;
            std::filesystem::path debugCaptureRoot;
            present_capture::Ticket captureTicket{};
            present_capture::GuestImage captureGuest{};
            bool capturePrepared = false;
            uint32_t captureAttemptFrame = 0;
            uint32_t captureAttemptSwap = 0;
            static constexpr uint32_t debugCaptureFrameCount = 3;
            uint32_t debugCaptureFirstFrame = 0, debugCaptureCompleted = 0;
            std::shared_ptr<dlss::capture::Page> evaluatePage;
            bool evaluatePagePrepared = false;
            bool evaluatePageExported = false;
            bool evaluatePageExportOk = true;
            // A separate, bounded three-frame FSR diagnostic uses the same
            // checked-submit/readback ownership without enabling F1 draw traces.
            std::vector<std::shared_ptr<dlss::capture::Page>> fsrCapturePages;
            std::filesystem::path fsrCaptureRoot;
            uint64_t fsrCaptureRequestSeen = 0;
            uint32_t fsrCaptureFirstFrame = 0;
            std::ofstream debugTrace;
            uint32_t debugDraw = 0;
            std::vector<uint32_t> debugRegisters;
            std::set<uint64_t> debugShaders;
            std::shared_ptr<shader_source_capture::Capture> debugShaderSources;
            // Narrow P2 provenance evidence. This is created only with the existing
            // three-frame capture and is not a general frame logging facility.
            std::ofstream p2Evidence;

            void BeginDebugCapture()
            {
                std::lock_guard lock(captureMutex);
                if (!capturePending) return;
                capturePending = false;
                try
                {
                    if (debugCaptureRoot.empty())
                    {
                        const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
                        debugCaptureRoot = std::filesystem::absolute(std::filesystem::path("captures") / fmt::format("render-{}-f{}", stamp, frame));
                        debugCaptureFirstFrame = frame;
                        debugCaptureCompleted = 0;
                        debugShaders.clear();
                        debugShaderSources = std::make_shared<shader_source_capture::Capture>();
                        std::filesystem::create_directories(debugCaptureRoot / "shaders");
                    }
                    if (frame != debugCaptureFirstFrame + debugCaptureCompleted)
                        throw std::runtime_error("Capture frame sequence is not consecutive");
                    const auto path = debugCaptureRoot / fmt::format("frame-{:02}-f{}", debugCaptureCompleted + 1, frame);
                    std::filesystem::create_directories(path);
                    debugCaptureDir = path.string();
                    debugTrace.open(path / "render-state.txt", std::ios::trunc);
                    if (!debugTrace) throw std::runtime_error("Cannot write render-state.txt");
                    p2Evidence.open(path / "p2-oracle.jsonl", std::ios::trunc);
                    if (!p2Evidence) throw std::runtime_error("Cannot write p2-oracle.jsonl");
                    debugTrace << "Frame " << frame << "\nRegister index/value pairs are hex. First draw is a full register snapshot; later draws contain changes.\n";
                    // Newline-delimited JSON. Each subsequent line is either a resolve
                    // event or one of the two PS draw records requested by the oracle.
                    p2Evidence << "{\"schema\":\"lostodyssey.p2-oracle-evidence.v1\",\"event\":\"capture\",\"renderer_frame\":" << frame
                               << ",\"trigger\":\"F1_or_LO_DEBUG_CAPTURE_SWAP\",\"encoding_claim\":\"unknown\"}\n";
                    const auto& description = device->getDescription();
                    debugTrace << fmt::format("GPU: {} driver_raw={}\n", description.name, description.driverVersion);
                    const auto config = settings::GetConfig();
                    debugTrace << fmt::format("Source version: {}\nConfigured graphics: output={}x{} internal_resolution={} window_mode={} AA={} scaling_quality={} frame_rate={}\n",
                        lo_version::Source, config.width, config.height, config.internalResolution,
                        uint32_t(config.windowMode), config.antialiasing, config.scalingQuality, config.frameRate);
                    debugTrace << "AA IDs: 0 Off, 1 FXAA, 2 SMAA, 3 experimental TAA. Configured mode does not prove per-draw application; inspect surfaces and draw state.\n";
                    debugTrace << "Shared translated shaders: ../shaders/<hash>.hlsl. Exact R32 depth is in .bin; no duplicate .f32 file.\n";
                    debugTrace << "Original VS/PS microcode: ../shaders/source/{vs,ps}_<hash>.bin; source manifest reports missing programs, limits and unavailable HLSL. Written by the background archive worker.\n";
                    debugDraw = 0;
                    debugRegisters.clear();
                    resolveSeq = 0;
                    captureFrame = frame;
                    evaluatePage = std::make_shared<dlss::capture::Page>();
                    evaluatePage->number = debugCaptureCompleted + 1;
                    evaluatePage->frame = frame;
                    evaluatePage->gapResetBeforeInputs = temporalGapResetFrame == frame;
                    evaluatePagePrepared = false;
                    evaluatePageExported = false;
                    evaluatePageExportOk = true;
                    captureStatus = L"正在截取 / Capturing " + std::to_wstring(debugCaptureCompleted + 1) + L"/3: " + path.wstring();
                    LOG_INFO("render capture started: {}", FileSystem::PathUtf8(std::filesystem::path(debugCaptureDir)));
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("render capture: {}", e.what());
                    debugCaptureDir.clear();
                    debugTrace.close();
                    debugTrace.clear();
                    p2Evidence.close();
                    p2Evidence.clear();
                    captureFrame = 0;
                    evaluatePage.reset();
                    captureBusy = false;
                    captureStatus = L"捕获失败，已有文件保留 / Capture failed, files retained: " + debugCaptureRoot.wstring();
                    // A failure opening frame 2/3 must not leave the retained
                    // sequence looking permanently in progress.
                    try
                    {
                        if (!debugCaptureRoot.empty())
                        {
                            std::ofstream manifest(debugCaptureRoot / "capture-info.txt");
                            manifest << "requested_frames=" << debugCaptureFrameCount
                                << "\ncompleted_frames=" << debugCaptureCompleted
                                << "\nfirst_frame=" << debugCaptureFirstFrame
                                << "\nlast_attempted_frame=" << frame << "\nstatus=incomplete\n"
                                << "shader_sources_status=check_shaders/source/manifest.json_if_background_save_succeeded\n";
                        }
                    }
                    catch (...) {} // The output directory itself may be unwritable.
                    if (debugShaderSources && !debugCaptureRoot.empty())
                    {
                        try
                        {
                            auto sources = std::move(debugShaderSources);
                            captureArchive = os::StartCaptureArchive(debugCaptureRoot, [sources](const std::filesystem::path& directory) {
                                sources->Write(directory);
                                // Preserve partial capture data without publishing a complete ZIP.
                                throw std::system_error(std::make_error_code(std::errc::io_error));
                            });
                            captureBusy = true;
                            captureStatus = L"后台保存不完整捕获 / Saving incomplete capture in background";
                        }
                        catch (const std::exception& error) { LOG_ERROR("render capture shader source save could not start: {}", error.what()); }
                    }
                    debugCaptureRoot.clear();
                    debugCaptureCompleted = 0;
                }
            }
            uint64_t psTraceRequest = 0, psTraceHash = 0;
            uint32_t psTraceRemaining = 0, psTraceFirst = 0, psTraceCount = 0, psTraceDraws = 0;

            // Opt-in temporal readback. Copies are queued at the selected resolve,
            // then mapped only after the normal end-of-frame fence. Unlike full
            // capture, this does not insert a wait between shadow production and use.
            struct ResolveTraceTarget { uint32_t address, occurrence, seen = 0; };
            struct ResolveTraceCopy
            {
                uint32_t address, occurrence, width, height, bpp, pitch, offset;
                RenderFormat format;
            };
            uint64_t taaDiagnosticSerial = 0;
            int taaDiagnosticAA = -1, taaDiagnosticJitter = -1, taaDiagnosticHistory = -1, taaDiagnosticBloom = -1;
            int taaDiagnosticHDR = 0;
            int taaDiagnosticMaterials = 1;
            const char* taaLiveDirectory = getenv("LO_TAA_LIVE_DIR");
            const temporal::LiveOptions taaMainOptions = temporal::MainTaaOptions();
            temporal::LiveOptions taaLiveOptions;
            bool taaLiveApplied = false;
            const temporal::LiveOptions& ActiveTaaOptions() const {
                return taaLiveApplied ? taaLiveOptions : taaMainOptions;
            }
            std::string taaLiveError;
            std::chrono::steady_clock::time_point taaLivePoll{};
            uint64_t taaLiveResolvedFrame = 0;
            uint32_t taaLiveWidth = 0, taaLiveHeight = 0;
            double taaLiveJitterX = 0, taaLiveJitterY = 0;
            bool taaLiveHistoryReused = false, taaLiveMotionReady = false, taaLiveMotionConsumed = false;
            RenderFormat taaLiveSourceFormat = RenderFormat::UNKNOWN, taaLiveHistoryFormat = RenderFormat::UNKNOWN, taaLiveOutputFormat = RenderFormat::UNKNOWN;
            void PollTaaLive()
            {
                if (!taaLiveDirectory || !*taaLiveDirectory) return;
                const auto now = std::chrono::steady_clock::now();
                if (now < taaLivePoll) return;
                taaLivePoll = now + std::chrono::milliseconds(250);
                const auto directory = std::filesystem::path(taaLiveDirectory);
                std::ifstream request(directory / "control.txt");
                temporal::LiveOptions next;
                if (request && temporal::ReadLiveOptions(request, next, taaLiveError)) {
                    if (!taaLiveApplied || next.serial != taaLiveOptions.serial) {
                        taaLiveOptions = next; taaLiveApplied = true;
                        taaDiagnosticAA = next.aa; taaDiagnosticJitter = next.jitter; taaDiagnosticHistory = next.history;
                        taaDiagnosticBloom = next.bloom; taaDiagnosticHDR = next.hdr; taaDiagnosticMaterials = next.materials;
                        if (temporalHistory) temporalHistory->Reset();
                        if (hdrTemporalHistory) hdrTemporalHistory->Reset();
                        temporalSupportedFrame = ~0ull; ++temporalEpoch;
                        taaLiveResolvedFrame = 0; taaLiveHistoryReused = false;
                        taaLiveSourceFormat = taaLiveHistoryFormat = taaLiveOutputFormat = RenderFormat::UNKNOWN;
                        LOG_INFO("renderer: live TAA controls applied serial={} frame={}", next.serial, frame);
                    }
                }
                const auto temporary = directory / "state.json.tmp", destination = directory / "state.json";
                std::ofstream state(temporary, std::ios::trunc);
                if (!state) return;
                const auto taaTiming = temporalHistory ? temporalHistory->ResolveTiming() : temporal::GpuPassTimingStats{};
                const auto replayTiming = motionReplay ? motionReplay->DrawTiming() : temporal::GpuPassTimingStats{};
                const auto maskTiming = motionReplay ? motionReplay->MaskTiming() : temporal::GpuPassTimingStats{};
                state << fmt::format("{{\"applied_serial\":{},\"frame\":{},\"resolved_frame\":{},\"width\":{},\"height\":{},"
                    "\"history_reused\":{},\"motion_ready\":{},\"motion_consumed\":{},\"aa\":{},\"jitter_x\":{},\"jitter_y\":{},\"acceptance\":{},"
                    "\"history_fp16\":{},\"source_format\":{},\"history_format\":{},\"output_format\":{},"
                    "\"gpu_timing\":{},\"taa_samples\":{},\"taa_total_ms\":{},\"taa_last_ms\":{},"
                    "\"replay_samples\":{},\"replay_total_ms\":{},\"replay_last_ms\":{},"
                    "\"mask_samples\":{},\"mask_total_ms\":{},\"mask_last_ms\":{},"
                    "\"backend\":{},\"source_version\":{},\"request_error\":{}}}",
                    taaLiveApplied ? taaLiveOptions.serial : 0, frame, taaLiveResolvedFrame, taaLiveWidth, taaLiveHeight,
                    taaLiveHistoryReused, taaLiveMotionReady, taaLiveMotionConsumed, taaDiagnosticAA,
                    taaLiveJitterX, taaLiveJitterY, taaLiveApplied ? taaLiveOptions.acceptance : 0,
                    taaLiveHistoryFormat == RenderFormat::R16G16B16A16_FLOAT,
                    uint32_t(taaLiveSourceFormat), uint32_t(taaLiveHistoryFormat), uint32_t(taaLiveOutputFormat),
                    taaLiveApplied && taaLiveOptions.gpu_timing,
                    taaTiming.samples, taaTiming.totalMilliseconds, taaTiming.lastMilliseconds,
                    replayTiming.samples, replayTiming.totalMilliseconds, replayTiming.lastMilliseconds,
                    maskTiming.samples, maskTiming.totalMilliseconds, maskTiming.lastMilliseconds,
                    temporal::LiveJsonString(vulkan ? "Vulkan" : "D3D12"),
                    temporal::LiveJsonString(lo_version::Source), temporal::LiveJsonString(taaLiveError));
                state.close();
                if (state.fail()) return;
                std::error_code error; std::filesystem::rename(temporary, destination, error);
            }
            void PollTaaDiagnostic()
            {
                static const char* path = getenv("LO_TAA_DIAGNOSTIC_REQUEST");
                if (!path) return;
                std::ifstream input(path);
                std::string serialText, extra;
                int aa, jitter, history, bloom, hdr = 0, materials = 1;
                if (!(input >> serialText >> aa >> jitter >> history >> bloom) ||
                    serialText.empty() || serialText.find_first_not_of("0123456789") != std::string::npos) return;
                // The optional sixth field keeps existing five-field requests valid.
                if (input >> extra) {
                    if (extra != "0" && extra != "1") return;
                    hdr = extra == "1";
                    if (input >> extra) {
                        if (extra != "0" && extra != "1") return;
                        materials = extra == "1";
                        if (input >> extra) return;
                    }
                }
                uint64_t serial = 0;
                for (char digit : serialText) {
                    if (serial > (UINT64_MAX - uint64_t(digit - '0')) / 10) return;
                    serial = serial * 10 + uint64_t(digit - '0');
                }
                if (!serial || serial == taaDiagnosticSerial || (aa != -1 && aa != 0 && aa != 3) ||
                    jitter < -1 || jitter > 1 || history < -1 || history > 1 || bloom < -1 || bloom > 1) return;
                taaDiagnosticSerial = serial;
                taaDiagnosticAA = aa; taaDiagnosticJitter = jitter; taaDiagnosticHistory = history; taaDiagnosticBloom = bloom;
                taaDiagnosticHDR = hdr;
                taaDiagnosticMaterials = materials;
                if (temporalHistory) temporalHistory->Reset();
                if (hdrTemporalHistory) hdrTemporalHistory->Reset();
                temporalSupportedFrame = ~0ull; ++temporalEpoch;
                LOG_INFO("renderer: TAA diagnostic serial={} frame={} aa={} jitter={} history={} bloom={} hdr={} materials={}",
                    serial, frame, aa, jitter, history, bloom, hdr, materials);
            }
            uint64_t resolveTraceSerial = 0, resolveTraceFirstFrame = ~0ull;
            uint32_t resolveTraceRemaining = 0, resolveTraceBytes = 0;
            std::vector<ResolveTraceTarget> resolveTraceTargets;
            std::vector<ResolveTraceCopy> resolveTraceCopies;
            std::unique_ptr<RenderBuffer> resolveTraceBuffer;

            void FinishResolveTraceFrame()
            {
                static const char* requestPath = getenv("LO_RESOLVE_TRACE_REQUEST");
                if (!requestPath) return;
                if (!resolveTraceCopies.empty())
                {
                    // Flush submits asynchronously. Recycle completed slots before
                    // mapping their copies; this also consumes/reset-tracks Vulkan
                    // fences so a later Begin cannot wait the same fence twice.
                    if (!WaitForReadback()) return;
                    const auto directory = std::filesystem::path(requestPath).parent_path();
                    const auto* data = static_cast<const char*>(resolveTraceBuffer->map());
                    for (const auto& copy : resolveTraceCopies)
                    {
                        const auto path = directory / fmt::format("trace_f{}_a{:x}_n{}_{}x{}_fmt{}.bin",
                            frame, copy.address, copy.occurrence, copy.width, copy.height, uint32_t(copy.format));
                        std::ofstream out(path, std::ios::binary);
                        for (uint32_t y = 0; y < copy.height; ++y)
                            out.write(data + copy.offset + size_t(y) * copy.pitch, size_t(copy.width) * copy.bpp);
                        out.close();
                        if (out.fail()) LOG_ERROR("renderer: resolve trace write failed: {}", path.string());
                    }
                    resolveTraceBuffer->unmap();
                }
                if (resolveTraceRemaining)
                {
                    LOG_INFO("renderer: resolve trace f{} copies={}", frame, resolveTraceCopies.size());
                    --resolveTraceRemaining;
                }
                resolveTraceCopies.clear();
                resolveTraceBytes = 0;
                for (auto& target : resolveTraceTargets) target.seen = 0;
                // serial frames [address_hex occurrence_decimal] (one to four pairs).
                uint64_t serial = 0;
                uint32_t frames = 0, address = 0, occurrence = 0;
                std::ifstream in(requestPath);
                if (!(in >> serial >> frames) || !serial || serial == resolveTraceSerial || frames > 600) return;
                std::vector<ResolveTraceTarget> targets;
                while (in >> std::hex >> address >> std::dec >> occurrence)
                {
                    if (!occurrence || occurrence > 32 || targets.size() == 4) return;
                    targets.push_back({address, occurrence});
                }
                if (targets.empty()) return;
                resolveTraceSerial = serial;
                resolveTraceFirstFrame = frame + 1;
                resolveTraceRemaining = frames;
                resolveTraceTargets = std::move(targets);
                LOG_INFO("renderer: resolve trace request {} next-frame={} frames={} targets={}",
                    serial, frame + 1, frames, resolveTraceTargets.size());
            }

            void QueueResolveTrace(RenderTexture* texture, RenderFormat format, uint32_t width, uint32_t height, RenderTextureLayout previousLayout, uint32_t address)
            {
                if (!resolveTraceRemaining) return;
                for (auto& target : resolveTraceTargets)
                {
                    if (target.address != address || ++target.seen != target.occurrence) continue;
                    const uint32_t bpp = format == RenderFormat::R8G8B8A8_UNORM ? 4 :
                        (format == RenderFormat::R16G16B16A16_FLOAT || format == RenderFormat::R32G32_FLOAT) ? 8 :
                        (format == RenderFormat::R32_FLOAT || format == RenderFormat::R16G16_FLOAT) ? 4 :
                        format == RenderFormat::R8_UNORM ? 1 : 0;
                    if (!bpp) continue;
                    const uint32_t pitch = (width * bpp + 255) & ~255u;
                    const uint32_t offset = (resolveTraceBytes + 511) & ~511u;
                    constexpr uint32_t capacity = 192u << 20; // 4K HDR source/output + R32 depth + RGBA8 output.
                    if (uint64_t(offset) + uint64_t(pitch) * height > capacity) continue;
                    if (!resolveTraceBuffer) resolveTraceBuffer = device->createBuffer(RenderBufferDesc::ReadbackBuffer(capacity));
                    if (!resolveTraceBuffer) continue;
                    commandList->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(texture,RenderTextureLayout::COPY_SOURCE));
                    commandList->copyTextureRegion(
                        RenderTextureCopyLocation::PlacedFootprint(resolveTraceBuffer.get(), format,
                            width, height, 1, pitch / bpp, offset),
                        RenderTextureCopyLocation::Subresource(texture, 0));
                    commandList->barriers(RenderBarrierStage::ALL,RenderTextureBarrier(texture,previousLayout));
                    resolveTraceCopies.push_back({address, target.occurrence, width, height, bpp, pitch, offset, format});
                    resolveTraceBytes = offset + pitch * height;
                }
            }

            void QueueResolveTrace(HostTexture& tex,uint32_t address) {
                QueueResolveTrace(tex.texture.get(),tex.format,tex.width,tex.height,tex.layout,address);
            }

            // Separate from full capture: no resource readbacks or GPU waits.
            // File: serial frames shader_hex first_constant constant_count.
            void PollPsTraceRequest()
            {
                if (psTraceRemaining)
                {
                    SHADER_LOG_INFO("pixel-constants", RendererByteFnv, "renderer: ps trace end f{} ps={:016x} draws={} truncated={}",
                        frame, psTraceHash, psTraceDraws, psTraceDraws > 64);
                    --psTraceRemaining;
                }
                psTraceDraws = 0;
                static const char* path = getenv("LO_PS_TRACE_REQUEST");
                if (!path) return;
                uint64_t serial = 0, hash = 0;
                uint32_t frames = 0, first = 0, count = 0;
                std::ifstream in(path);
                if (!(in >> serial >> frames >> std::hex >> hash >> std::dec >> first >> count) ||
                    !serial || serial == psTraceRequest || frames > 600 || first >= 256 ||
                    !count || count > 16 || count > 256 - first)
                    return;
                psTraceRequest = serial;
                psTraceHash = hash;
                psTraceRemaining = frames;
                psTraceFirst = first;
                psTraceCount = count;
                SHADER_LOG_INFO("pixel-constants", RendererByteFnv, "renderer: ps trace request {} next-frame={} frames={} ps={:016x} constants={}+{}",
                    serial, frame + 1, frames, hash, first, count);
            }

            // An opt-in request file contains a changing nonzero integer. Poll
            // only at a frame boundary, so every diagnostic sees the same frame.
            void PollCaptureRequest()
            {
                BeginDebugCapture();
                if (!debugCaptureDir.empty()) return;
                static const char* path = getenv("LO_CAPTURE_REQUEST");
                if (!path) return;
                {
                    std::lock_guard lock(captureMutex);
                    if (fsrCaptureBusy) return;
                }
                uint64_t request = 0;
                std::ifstream in(path);
                if (in >> request && request && request != captureRequest)
                {
                    captureRequest = request;
                    captureFrame = frame;
                    LOG_INFO("renderer: capture request {} at frame {}", request, frame);
                }
            }

            void FinishFsrCapture()
            {
                if (fsrCapturePages.empty()) return;
                const bool gpuComplete = WaitForGpu();
                bool ok = gpuComplete && fsrCapturePages.size() == 3;
                try {
                    std::filesystem::create_directories(fsrCaptureRoot);
                    for (size_t i = 0; i < fsrCapturePages.size(); ++i) {
                        auto& page = *fsrCapturePages[i];
                        const auto dir = fsrCaptureRoot / fmt::format("frame-{:02}-f{}", i + 1, page.frame);
                        std::filesystem::create_directories(dir);
                        const bool exported = page.closed && page.ExportFsr(dir);
                        ok = exported && page.saved > 0 && ok;
                    }
                    std::ofstream manifest(fsrCaptureRoot / "capture-info.json", std::ios::trunc);
                    manifest << "{\"schema\":1,\"trigger\":\"LO_FSR_CAPTURE_REQUEST\",\"request\":"
                             << fsrCaptureRequestSeen << ",\"first_renderer_frame\":" << fsrCaptureFirstFrame
                             << ",\"frames\":[";
                    for (size_t i = 0; i < fsrCapturePages.size(); ++i)
                        manifest << (i ? "," : "") << fsrCapturePages[i]->frame;
                    manifest << "],\"gpu_complete\":" << (gpuComplete ? "true" : "false")
                             << ",\"complete\":" << (ok ? "true" : "false") << "}\n";
                    manifest.close(); ok = !manifest.fail() && ok;
                } catch (const std::exception& e) {
                    ok = false;
                    LOG_ERROR("FSR-only capture export failed: {}", e.what());
                }
                LOG_INFO("FSR-only capture {}: {}", ok ? "saved" : "incomplete",
                    FileSystem::PathUtf8(fsrCaptureRoot));
                if (evaluatePage && !fsrCapturePages.empty() && evaluatePage == fsrCapturePages.back())
                    evaluatePage.reset();
                fsrCapturePages.clear(); fsrCaptureRoot.clear();
                std::lock_guard lock(captureMutex);
                fsrCaptureBusy = false;
            }

            void PollFsrCaptureRequest()
            {
                if (fsrCapturePages.size() == 3 && fsrCapturePages.back()->closed)
                    FinishFsrCapture();
                if (fsrCapturePages.empty()) {
                    static const char* path = getenv("LO_FSR_CAPTURE_REQUEST");
                    if (!path || !*path || !debugCaptureDir.empty() || captureFrame == frame) return;
                    uint64_t request = 0;
                    std::ifstream in(path);
                    if (!(in >> request) || !request || request == fsrCaptureRequestSeen) return;
                    {
                        std::lock_guard lock(captureMutex);
                        if (captureBusy || fsrCaptureBusy) return;
                        fsrCaptureBusy = true;
                    }
                    try {
                        const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
                        fsrCaptureRoot = std::filesystem::absolute(std::filesystem::path("captures") /
                            fmt::format("fsr-motion-{}-f{}", stamp, frame));
                    } catch (const std::exception& e) {
                        std::lock_guard lock(captureMutex);
                        fsrCaptureBusy = false;
                        LOG_ERROR("FSR-only capture start failed: {}", e.what());
                        return;
                    }
                    fsrCaptureRequestSeen = request;
                    fsrCaptureFirstFrame = frame;
                    LOG_INFO("FSR-only capture request {} starting frame={} root={}", request, frame,
                        FileSystem::PathUtf8(fsrCaptureRoot));
                }
                if (fsrCapturePages.size() >= 3) return;
                if (frame != fsrCaptureFirstFrame + fsrCapturePages.size()) {
                    LOG_WARNING("FSR-only capture frame gap: expected={} actual={}",
                        fsrCaptureFirstFrame + fsrCapturePages.size(), frame);
                    FinishFsrCapture();
                    return;
                }
                if (evaluatePage && evaluatePage->frame == frame) return;
                evaluatePage = std::make_shared<dlss::capture::Page>();
                evaluatePage->number = uint32_t(fsrCapturePages.size() + 1);
                evaluatePage->frame = frame;
                evaluatePage->gapResetBeforeInputs = temporalGapResetFrame == frame;
                fsrCapturePages.push_back(evaluatePage);
            }

            uint32_t TraceFrame() const
            {
                static const uint32_t configured = getenv("LO_DRAW_TRACE") ? strtoul(getenv("LO_DRAW_TRACE"), nullptr, 10) : 0;
                return captureFrame ? captureFrame : configured;
            }
            std::set<uint32_t> loggedFormats;

            struct SharedConstants
            {
                uint32_t bools[8];
                uint32_t loops[32];
                float ndcScale[4];
                float ndcOffset[4];
                float halfPixel[2];
                uint32_t vtxFmt;
                uint32_t flags;
                float alphaTest[4];
                float colorMax[4];
                uint32_t transfer[4];
                uint32_t vfetchOffset[96];
                uint32_t samplerIndex[32];
                uint32_t textureInfo[32];
                uint32_t textureSize[32]; // packed guest width/height; physical resolves may be larger.
            };

            static_assert(offsetof(SharedConstants,ndcScale)==160);
            static_assert(offsetof(SharedConstants,transfer)==240);
            static_assert(offsetof(SharedConstants,vfetchOffset)==256);

            void WriteP2ResolveEvent(const char* kind, const HostTexture& source, uint32_t destBase,
                const ResolvedSurface& destination, const char* operation)
            {
                if (!p2Evidence.is_open() || !destination.tex) return;
                const auto& dest = *destination.tex;
                // Both renderer copy paths name subresource zero explicitly. Do not
                // infer image layout or a color transform from this bookkeeping.
                p2Evidence << "{\"schema\":\"lostodyssey.p2-oracle-evidence.v1\",\"event\":\"resolve\",\"kind\":\"" << kind
                    << "\",\"renderer_frame\":" << frame << ",\"operation\":\"" << operation
                    << "\",\"source\":{\"allocation\":" << source.allocationSerial
                    << ",\"host_format\":" << uint32_t(source.format)
                    << ",\"extent\":[" << source.width << ',' << source.height << "]"
                    << ",\"subresource\":0}"
                    << ",\"destination\":{\"guest_base\":" << destBase
                    << ",\"guest_format\":" << destination.destFormat
                    << ",\"guest_pitch\":" << destination.destPitch
                    << ",\"allocation\":" << dest.allocationSerial
                    << ",\"host_format\":" << uint32_t(dest.format)
                    << ",\"extent\":[" << dest.width << ',' << dest.height << "]"
                    << ",\"subresource\":0,\"write_version\":" << destination.writeOrdinal
                    << ",\"write_ordinal\":" << destination.writeOrdinal
                    << ",\"rect\":[" << destination.writeX << ',' << destination.writeY << ','
                    << destination.writeWidth << ',' << destination.writeHeight << "]}}\n";
            }
            static_assert(offsetof(SharedConstants,samplerIndex)==640);
            static_assert(offsetof(SharedConstants,textureInfo)==768);
            static_assert(offsetof(SharedConstants,textureSize)==896);

            struct UploadedConstants
            {
                uint32_t vs[256 * 4]{};
                uint32_t ps[256 * 4]{};
                SharedConstants shared{};
                uint64_t vsOffset = UINT64_MAX;
                uint64_t psOffset = UINT64_MAX;
                uint64_t sharedOffset = UINT64_MAX;
            };
            UploadedConstants uploadedConstants[kGpuSlots];

            uint64_t UploadUnchanged(int bank, const void* data, size_t size)
            {
                auto& before = uploadedConstants[gpuSlot];
                uint64_t lastOffset = bank == 0 ? before.vsOffset : bank == 1 ? before.psOffset : before.sharedOffset;
                const void* last = bank == 0 ? static_cast<const void*>(before.vs) :
                    bank == 1 ? static_cast<const void*>(before.ps) : static_cast<const void*>(&before.shared);
                if (lastOffset != UINT64_MAX && std::memcmp(last, data, size) == 0)
                    return lastOffset;
                const uint64_t offset = Upload(data, size);
                auto& after = uploadedConstants[gpuSlot];
                if (offset != UINT64_MAX)
                {
                    if (bank == 0) { std::memcpy(after.vs, data, size); after.vsOffset = offset; }
                    else if (bank == 1) { std::memcpy(after.ps, data, size); after.psOffset = offset; }
                    else { std::memcpy(&after.shared, data, size); after.sharedOffset = offset; }
                }
                return offset;
            }

            // ---- lifecycle -----------------------------------------------------
            xenos::cache::Identity cacheIdentity;
            bool initializationModuleFailure = false;
            bool InitFailure(const char* stage, uint64_t bytes = 0, int slot = -1) noexcept
            {
                try {
                    LOG_ERROR("renderer init failed: backend={} stage={} bytes={} slot={}",
                        vulkan ? "Vulkan" : "D3D12", stage, bytes, slot);
                    os::diagnostics::LogHostMemory("renderer initialization failure");
                } catch (...) {
                    char line[384];
                    const int size = std::snprintf(line, sizeof(line),
                        "[error] renderer init failed: backend=%s stage=%s bytes=%llu slot=%d\n",
                        vulkan ? "Vulkan" : "D3D12", stage, static_cast<unsigned long long>(bytes), slot);
                    if (size > 0) os::logger::EmergencyWrite(line, size_t(size) < sizeof(line) ? size_t(size) : sizeof(line) - 1);
                }
                return false;
            }
            bool Init()
            {
                device = video::GetDevice();
                queue = video::GetQueue();
                vulkan = video::IsVulkan();
#if defined(LO_GPU_PLUME)
                dlssController = vulkan ? video::GetDlssController() : nullptr;
#if !defined(LO_RENDERER_P2_EMBEDDED_TEST)
                temporalUpscaler = vulkan ? video::GetTemporalUpscaler() : nullptr;
#endif
#endif
                const char* batchOverride = getenv("LO_VK_DESCRIPTOR_BATCH_LIMIT");
                descriptorBatchLimit = render_batch::DescriptorLimit(vulkan, batchOverride ? batchOverride : "");
                LOG_INFO("renderer: descriptor reuse={} backend={} limit={} gpu_slots={} (LO_DESCRIPTOR_REUSE=0 disables reuse)",
                    descriptorReuse, vulkan ? "Vulkan" : "D3D12", descriptorBatchLimit, kGpuSlots);
                binaryFormat = vulkan ? xenos::ShaderBinaryFormat::Spirv : xenos::ShaderBinaryFormat::Dxil;
                renderFormat = vulkan ? RenderShaderFormat::SPIRV : RenderShaderFormat::DXIL;
                if (!device || !queue)
                    return InitFailure("device_or_queue");
                cacheIdentity = xenos::cache::MakeIdentity(vulkan ? backend::Backend::Vulkan : backend::Backend::D3D12, xenos::DxcIdentity());

                // Optional collection resources are prepared before the game loop.
                // Enabling collection later never compiles or maps on a draw; an
                // unprepared collector waits for the next renderer startup.
                try {
                    if(taa_collection::Enabled()&&!vulkan) {
                        auto collector=std::make_shared<taa_collection::SparseDepthGPU>();
                        if(collector->Prepare(device))sparseCollector=std::move(collector);
                        else LOG_WARNING("renderer: sparse GPU collection unavailable; VS/PS and summary collection remain available");
                    } else if(taa_collection::Enabled()&&vulkan) {
                        LOG_INFO("renderer: Vulkan sparse GPU collection skipped: coherent nonblocking readback unavailable; VS/PS and summary collection remain available");
                    }
                } catch(const std::exception& error) {
                    sparseCollector.reset();
                    LOG_WARNING("renderer: optional sparse GPU collection preparation failed: {}",error.what());
                }
                try {
                    positionEvidence=std::make_unique<position_evidence::Collection>([](std::span<const uint8_t> raw) {
                        std::vector<uint32_t> swapped(raw.size()/sizeof(uint32_t));
                        std::memcpy(swapped.data(),raw.data(),raw.size());
                        for(auto& word:swapped)word=ByteSwap(word);
                        const auto diagnostic=xenos::TranslateShader(swapped.data(),uint32_t(swapped.size()),false);
                        return position_evidence::Analyze(diagnostic.hlsl);
                    });
                } catch(const std::exception& error) {
                    positionEvidence.reset();
                    LOG_WARNING("renderer: optional position evidence collection unavailable: {}",error.what());
                }

                for (uint32_t i = 0; i < kGpuSlots; ++i) {
                    auto& g = gpuSlots[i];
                    g.list = queue->createCommandList();
                    if (!g.list) return InitFailure("command_list.create", 0, i);
                    if (vulkan && dlssController) {
                        g.srIsolated = queue->createCommandList();
                        g.srContinuation = queue->createCommandList();
                        if (!g.srIsolated || !g.srContinuation)
                            return InitFailure("dlss.command_list.create", 0, i);
                    }
                    g.fence = device->createCommandFence();
                    if (!g.fence) return InitFailure("command_fence.create", 0, i);
                    g.uploadRing = device->createBuffer(RenderBufferDesc::UploadBuffer(kUploadRingSize, vulkan ? RenderBufferFlag::DEVICE_ADDRESSABLE | RenderBufferFlag::INDEX | RenderBufferFlag::STORAGE : RenderBufferFlag::NONE));
                    if (!g.uploadRing) return InitFailure("upload_ring.create", kUploadRingSize, i);
                    g.uploadMapped = static_cast<uint8_t*>(g.uploadRing->map());
                    if (!g.uploadMapped) return InitFailure("upload_ring.map", kUploadRingSize, i);
                }
                BindGpuSlot();
                vertexArena = device->createBuffer(RenderBufferDesc::UploadBuffer(gpu::render_arena::kVertexArenaSize, RenderBufferFlag::STORAGE));
                if (!vertexArena) return InitFailure("vertex_arena.create", gpu::render_arena::kVertexArenaSize);
                arenaMapped = static_cast<uint8_t*>(vertexArena->map());
                if (!arenaMapped) return InitFailure("vertex_arena.map", gpu::render_arena::kVertexArenaSize);
                readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(kReadbackSize));
                if (!readback) return InitFailure("readback.create", kReadbackSize);
                resolveReadback = getenv("LO_RESOLVE_READBACK") != nullptr;
                textureRevalidate = getenv("LO_TEXTURE_STATIC") == nullptr;
                auto enabled=[](const char* key){const char* value=getenv(key);return value&&strcmp(value,"1")==0;};
                bloomPrefilterEnabled = !enabled("LO_DISABLE_BLOOM_PREFILTER");
                temporalExperiment = enabled("LO_TEMPORAL_EXPERIMENT");
                temporalAllowHistory = enabled("LO_TEMPORAL_CAMERA_HISTORY");
                temporalJitter = enabled("LO_TEMPORAL_JITTER_EXPERIMENT");
                temporalStableGrid = enabled("LO_TEMPORAL_STABLE_GRID");
                temporalForced=temporalExperiment;temporalForcedHistory=temporalAllowHistory;
                temporalForcedJitter=temporalJitter;temporalForcedStable=temporalStableGrid;
                // The legacy CPU resolve path writes guest tiled memory and has
                // no GPU scene-depth/resolve provenance; keep its existing AA path.
                const char* sceneOverride=getenv("LO_SCENE_AA_EXPERIMENT");
                sceneAAEnabled = (!sceneOverride||strcmp(sceneOverride,"0")!=0)&&!resolveReadback;
                if(sceneAAEnabled) {
                    sceneProcessor=std::make_unique<gpu::Presentation>();
                    if(!sceneProcessor->Init(device)) return InitFailure("scene_presentation.init");
                }
                if(temporalExperiment) {
                    temporalHistory=std::make_unique<temporal::HistoryOwner>();
                    if(!temporalHistory->Init(device,sparseCollector)) {temporalHistory.reset();temporalExperiment=false;return InitFailure("temporal_history.init");}
                    else LOG_INFO("renderer: temporal pre-UI experiment enabled, camera_history={} jitter={} stable_grid={} (known scene VS only; no object motion vectors)",temporalAllowHistory,temporalJitter,temporalStableGrid);
                }
                dummyBuffer = device->createBuffer(RenderBufferDesc::DefaultBuffer(256));
                if (!dummyBuffer) return InitFailure("dummy_buffer.create", 256);

                // Layout: root CBVs b0 (VS constants) b1 (shared) b2 (PS constants) in space0;
                // set0 = vertex fetch buffers t0-95 + samplers s0-31 (space0),
                // set1..3 = 2D / 3D / cube textures t0-31 (space1..3).
                RenderPipelineLayoutBuilder layout;
                layout.begin(false, false);
                if (vulkan) layout.addPushConstant(0, 0, sizeof(constantAddresses), RenderShaderStageFlag::VERTEX | RenderShaderStageFlag::PIXEL);
                else {
                    layout.addRootDescriptor(0, 0, RenderRootDescriptorType::CONSTANT_BUFFER);
                    layout.addRootDescriptor(1, 0, RenderRootDescriptorType::CONSTANT_BUFFER);
                    layout.addRootDescriptor(2, 0, RenderRootDescriptorType::CONSTANT_BUFFER);
                }
                setBuilders[0].begin();
                vfetchDescriptorBase = setBuilders[0].addByteAddressBuffer(0, vulkan ? 1 : kVertexFetchSlots);
                if (!vulkan) samplerDescriptorBase = setBuilders[0].addSampler(0, kSamplerPalette);
                setBuilders[0].end();
                for (int i = 1; i < 4; i++)
                {
                    setBuilders[i].begin();
                    if(vulkan) for(uint32_t slot=0;slot<kTextureSlots;++slot) setBuilders[i].addTexture(slot);
                    else setBuilders[i].addTexture(0, kTextureSlots);
                    setBuilders[i].end();
                }
                if(vulkan) {
                    setBuilders[4].begin();samplerDescriptorBase=setBuilders[4].addSampler(0,kSamplerPalette);setBuilders[4].end();
                    staticSamplerSet=setBuilders[4].create(device);
                    if (!staticSamplerSet) return InitFailure("static_sampler_set.create");
                }
                for (int i = 0; i < (vulkan?5:4); i++)
                    layout.addDescriptorSet(setBuilders[i]);
                layout.end();
                pipelineLayout = layout.create(device);
                if (!pipelineLayout) return InitFailure("pipeline_layout.create");

                staticSet0 = setBuilders[0].create(device);
                if (!staticSet0) return InitFailure("vertex_fetch_set.create");
                for (uint32_t i = 0; i < (vulkan?1:kVertexFetchSlots); i++)
                    staticSet0->setBuffer(vfetchDescriptorBase + i, vertexArena.get(), gpu::render_arena::kVertexArenaSize);
                RenderSampler* defaultSampler = GetSampler(0x2 | (0x2 << 2) | (0x1 << 4)); // linear, wrap
                if (!defaultSampler) return InitFailure("default_sampler.create");
                for (uint32_t i = 0; i < kSamplerPalette; i++)
                    (vulkan?staticSamplerSet.get():staticSet0.get())->setSampler(samplerDescriptorBase + i, defaultSampler);

                CreateDummyTexture(dummyTexture2D, RenderTextureDimension::TEXTURE_2D, 0);
                CreateDummyTexture(dummyTexture3D, RenderTextureDimension::TEXTURE_3D, 0);
                CreateDummyTexture(dummyTextureCube, RenderTextureDimension::TEXTURE_2D, RenderTextureFlag::CUBE);
                HostTexture* dummyBanks[] = { &dummyTexture2D, &dummyTexture3D, &dummyTextureCube };
                for (int bank = 0; bank < 3; ++bank)
                {
                    staticDummySets[bank] = setBuilders[bank + 1].create(device);
                    if (!staticDummySets[bank]) return InitFailure("dummy_texture_set.create", 0, bank);
                    if (!dummyBanks[bank]->texture) return InitFailure("dummy_texture.create", 0, bank);
                    for (uint32_t slot = 0; slot < kTextureSlots; ++slot)
                        staticDummySets[bank]->setTexture(slot, dummyBanks[bank]->texture.get(), RenderTextureLayout::SHADER_READ);
                }

                if (const char* dir = getenv("LO_SHADER_CACHE_DIR"))
                    shaderCacheDir = dir;
                else
                    shaderCacheDir = os::user_paths::UsePortableLayout() ? "cache/shaders" : (os::user_paths::DataDir() / "cache/shaders").string();
                if (!shaderCacheDir.empty()) {
                    std::error_code ec;
                    std::filesystem::create_directories(shaderCacheDir, ec);
                    if (ec) {
                        LOG_WARNING("renderer: shader cache unavailable: {}", ec.message());
                        shaderCacheDir.clear();
                    } else LOG_INFO("renderer: shader cache {}", shaderCacheDir);
                }

                CompileRectListGs();
                CompileBlitShaders();
                CompileSceneCopyPromotionShaders();
                CompileTransferShader();
                if (!rectListGs) return InitFailure("rect_list_shader.create");
                if (!blitVs || !blitPs) return InitFailure("blit_shader.create");
                if (!transferPs) return InitFailure("transfer_shader.create");
                PrepareKnownShaders();
                if (initializationModuleFailure) return InitFailure("known_shaders.prepare");
                PrepareKnownPipelines();
                taa_collection::SetDevice(vulkan, device->getDescription().name, device->getDescription().driverVersion);
                const auto dxcStats = xenos::GetDxcStatistics();
                LOG_INFO("renderer: startup DXC actual calls {}, succeeded {}, deterministic rejections {}, infrastructure failures {}",
                    dxcStats.calls, dxcStats.succeeded, dxcStats.rejected, dxcStats.infrastructureFailed);
                LOG_INFO("renderer: initialised");
                return true;
            }

            void CreateDummyTexture(HostTexture& tex, RenderTextureDimension dim, RenderTextureFlags flags)
            {
                const uint32_t slices = (flags & RenderTextureFlag::CUBE) ? 6u : 1u;
                RenderTextureDesc desc = RenderTextureDesc::Texture(dim, 1, 1, 1, 1, slices, RenderFormat::R8G8B8A8_UNORM, flags);
                tex.texture = device->createTexture(desc);
                tex.format = RenderFormat::R8G8B8A8_UNORM;
                tex.width = tex.height = 1;
                tex.layout = RenderTextureLayout::UNKNOWN;
                if (!tex.texture)
                    return;
                // Texture memory starts undefined: a slot that falls back to the dummy
                // would otherwise sample garbage. Give it opaque black.
                const uint8_t black[4] = { 0, 0, 0, 255 };
                if (!Begin()) return;
                uint64_t offset = Upload(black, sizeof(black), 512);
                if (offset == UINT64_MAX)
                    return;
                Transition(tex, RenderTextureLayout::COPY_DEST, RenderBarrierStage::COPY);
                for (uint32_t slice = 0; slice < slices; slice++)
                    commandList->copyTextureRegion(
                        RenderTextureCopyLocation::Subresource(tex.texture.get(), 0, slice),
                        RenderTextureCopyLocation::PlacedFootprint(uploadRing, RenderFormat::R8G8B8A8_UNORM, 1, 1, 1, 64, offset));
                Transition(tex, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
            }

            // Full-screen blit that reinterprets one render target into another
            // format. SV_Position is the same pixel in both, so a Load() needs no
            // constants and the viewport alone selects the rectangle.
            std::unique_ptr<RenderShader> blitVs, blitPs;
            std::map<uint32_t, std::unique_ptr<RenderPipeline>> blitPipelines;
            bool bloomPrefilterEnabled = true;
            uint32_t bloomPrefilterLogs = 0;
            std::unique_ptr<RenderShader> bloomPrefilterPs;
            std::unique_ptr<RenderPipeline> bloomPrefilterPipeline;

            // Reinterprets one EDRAM class as another: pack the source value into the
            // guest's 32-bit word, then unpack it the way the new class reads it.
            std::unique_ptr<RenderShader> transferPs;
            std::map<uint32_t, std::unique_ptr<RenderPipeline>> transferPipelines;

            void CompileTransferShader()
            {
                const char* psSrc =
                    "Texture2D<float4> src : register(t0, space1);\n"
                    "#ifdef __spirv__\n"
                    "struct XePushConstants { uint64_t vs; uint64_t sharedAddress; uint64_t ps; };\n"
                    "[[vk::push_constant]] ConstantBuffer<XePushConstants> xePush;\n"
                    "#define xeTransfer vk::RawBufferLoad<uint4>(xePush.sharedAddress + 240)\n"
                    "#else\n"
                    "cbuffer XeShared : register(b1, space0) {\n"
                    "  uint4 pad0[2]; uint4 pad1[8]; float4 pad2; float4 pad3; float4 pad4;\n"
                    "  float4 pad5; float4 xeColorMax; uint4 xeTransfer;\n"
                    "};\n"
                    "#endif\n"
                    "float Float7e3To32(uint f10) {\n"
                    "  f10 &= 0x3FFu; if (f10 == 0u) return 0.0;\n"
                    "  uint mantissa = f10 & 0x7Fu, exponent = f10 >> 7;\n"
                    "  if (exponent == 0u) { uint lz = firstbithigh(mantissa); uint shift = 7u - lz;\n"
                    "    exponent = uint(int(1) - int(shift)); mantissa = (mantissa << shift) & 0x7Fu; }\n"
                    "  return asfloat(((exponent + 124u) << 23) | (mantissa << 16));\n"
                    "}\n"
                    "uint Float32To7e3(float f) {\n"
                    "  if (!(f > 0.0)) return 0u;\n"
                    "  uint u = asuint(f);\n"
                    "  if (u >= 0x41FF73FFu) return 0x3FFu;\n"
                    "  if (u < 0x3E800000u) { uint shift = min(125u - (u >> 23), 24u);\n"
                    "    u = (0x800000u | (u & 0x7FFFFFu)) >> shift; }\n"
                    "  else { u += 0xC2000000u; }\n"
                    "  return ((u + 0x7FFFu + ((u >> 16) & 1u)) >> 16) & 0x3FFu;\n"
                    "}\n"
                    "uint PackGuest(float4 v, uint cls) {\n"
                    "  if (cls == 0u) { uint4 c = uint4(saturate(v) * 255.0 + 0.5);\n"
                    "    return c.r | (c.g << 8) | (c.b << 16) | (c.a << 24); }\n"
                    "  uint a = uint(saturate(v.a) * 3.0 + 0.5);\n"
                    "  uint3 c;\n"
                    "  if (cls == 1u) c = uint3(saturate(v.rgb) * 1023.0 + 0.5);\n"
                    "  else c = uint3(Float32To7e3(v.r), Float32To7e3(v.g), Float32To7e3(v.b));\n"
                    "  return c.r | (c.g << 10) | (c.b << 20) | (a << 30);\n"
                    "}\n"
                    "float4 UnpackGuest(uint w, uint cls) {\n"
                    "  if (cls == 0u) return float4(uint4(w, w >> 8, w >> 16, w >> 24) & 0xFFu) * (1.0 / 255.0);\n"
                    "  float alpha = float(w >> 30) * (1.0 / 3.0);\n"
                    "  uint3 c = uint3(w, w >> 10, w >> 20) & 0x3FFu;\n"
                    "  if (cls == 1u) return float4(float3(c) * (1.0 / 1023.0), alpha);\n"
                    "  return float4(Float7e3To32(c.x), Float7e3To32(c.y), Float7e3To32(c.z), alpha);\n"
                    "}\n"
                    "float4 main(float4 pos : SV_Position) : SV_Target {\n"
                    "  float4 v = src.Load(int3(int2(pos.x * asfloat(xeTransfer.z), pos.y * asfloat(xeTransfer.w)), 0));\n"
                    "  return UnpackGuest(PackGuest(v, xeTransfer.x), xeTransfer.y);\n"
                    "}\n";
                xenos::CompiledShader f = xenos::CompileCachedHlsl(psSrc, "main", "ps_6_0", binaryFormat);
                if (!f.ok)
                {
                    SHADER_LOG_WARNING("compile-failed", None, "renderer: transfer shader compilation failed: {}", f.errors);
                    return;
                }
                transferPs = device->createShader(f.bytecode.data(), f.bytecode.size(), "main", renderFormat);
            }

            RenderPipeline* GetTransferPipeline(RenderFormat targetFormat)
            {
                auto it = transferPipelines.find(uint32_t(targetFormat));
                if (it != transferPipelines.end())
                    return it->second.get();
                if (!blitVs || !transferPs)
                    return nullptr;
                RenderGraphicsPipelineDesc desc;
                desc.pipelineLayout = pipelineLayout.get();
                desc.vertexShader = blitVs.get();
                desc.pixelShader = transferPs.get();
                desc.depthEnabled = false;
                desc.depthWriteEnabled = false;
                desc.depthFunction = RenderComparisonFunction::ALWAYS;
                desc.depthTargetFormat = RenderFormat::UNKNOWN;
                desc.renderTargetFormat[0] = targetFormat;
                desc.renderTargetBlend[0] = RenderBlendDesc::Copy();
                desc.renderTargetCount = 1;
                desc.renderTargetBlend[0].renderTargetWriteMask = 0xF;
                desc.cullMode = RenderCullMode::NONE;
                desc.primitiveTopology = RenderPrimitiveTopology::TRIANGLE_LIST;
                auto pipeline = device->createGraphicsPipeline(desc);
                RenderPipeline* result = pipeline.get();
                transferPipelines.emplace(uint32_t(targetFormat), std::move(pipeline));
                return result;
            }

            void TransferRegion(HostTexture& src, HostTexture& dst, uint32_t srcClass, uint32_t dstClass)
            {
                dst.sdrProducerFrame = ~0ull;
                consecutiveResolveCopies.Invalidate();
                // Only the 32-bit classes share a word layout; wider ones are left alone.
                if (srcClass > kClass7e3 || dstClass > kClass7e3)
                    return;
                RenderPipeline* pipeline = GetTransferPipeline(dst.format);
                if (!pipeline)
                    return;
                SharedConstants transferConstants{};
                transferConstants.transfer[0] = srcClass;
                transferConstants.transfer[1] = dstClass;
                transferConstants.transfer[2] = std::bit_cast<uint32_t>(float(src.resolutionSize.width) / dst.resolutionSize.width);
                transferConstants.transfer[3] = std::bit_cast<uint32_t>(float(src.resolutionSize.height) / dst.resolutionSize.height);
                uint64_t offset = Upload(&transferConstants, sizeof(transferConstants));
                if (offset == UINT64_MAX)
                    return;
                const uint32_t w = dst.ScaleX(std::min(src.guestWidth, dst.guestWidth));
                const uint32_t h = dst.ScaleY(std::min(src.guestHeight, dst.guestHeight));
                if (!Begin()) return;
                Transition(src, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
                Transition(dst, RenderTextureLayout::COLOR_WRITE, RenderBarrierStage::GRAPHICS);
                RenderDescriptorSet* set1 = AcquireSet(1);
                set1->setTexture(0, src.texture.get(), RenderTextureLayout::SHADER_READ);
                commandList->setFramebuffer(GetFramebuffer(&dst, nullptr));
                RenderViewport viewport(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h));
                commandList->setViewports(&viewport, 1);
                RenderRect scissor{ 0, 0, int32_t(w), int32_t(h) };
                commandList->setScissors(&scissor, 1);
                commandList->setPipeline(pipeline);
                commandList->setGraphicsPipelineLayout(pipelineLayout.get());
                SetConstantBuffer(offset, 0);
                SetConstantBuffer(offset, 1);
                SetConstantBuffer(offset, 2);
                commandList->setGraphicsDescriptorSet(staticSet0.get(), 0);
                commandList->setGraphicsDescriptorSet(set1, 1);
                commandList->setGraphicsDescriptorSet(AcquireSet(2), 2);
                commandList->setGraphicsDescriptorSet(AcquireSet(3), 3);
                if(vulkan) commandList->setGraphicsDescriptorSet(staticSamplerSet.get(),4);
                commandList->drawInstanced(3, 1, 0, 0);
#if defined(LO_GPU_PLUME)
                HandleFsrAlphaRgbWriter(dst, "tile_owner_transfer");
#endif
                if (taa_collection::Enabled())
                    dst.bindingProducer.Copy(src.bindingProducer, taa_collection::ConsentEpoch(), frame,
                        w == dst.width && h == dst.height);
                transfers++;
            }
            uint32_t transfers = 0;

            void CompileBlitShaders()
            {
                const char* vsSrc =
                    "void main(uint id : SV_VertexID, out float4 pos : SV_Position)\n"
                    "{\n"
                    "    float2 uv = float2((id << 1) & 2, id & 2);\n"
                    "    pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);\n"
                    "}\n";
                const char* psSrc =
                    "Texture2D<float4> src : register(t0, space1);\n"
                    "float4 main(float4 pos : SV_Position) : SV_Target\n"
                    "{\n"
                    "    return src.Load(int3(int2(pos.xy), 0));\n"
                    "}\n";
                xenos::CompiledShader v = xenos::CompileCachedHlsl(vsSrc, "main", "vs_6_0", binaryFormat);
                xenos::CompiledShader f = xenos::CompileCachedHlsl(psSrc, "main", "ps_6_0", binaryFormat);
                if (!v.ok || !f.ok)
                {
                    SHADER_LOG_WARNING("compile-failed", None, "renderer: blit shader compilation failed: {}{}", v.errors, f.errors);
                    return;
                }
                blitVs = device->createShader(v.bytecode.data(), v.bytecode.size(), "main", renderFormat);
                blitPs = device->createShader(f.bytecode.data(), f.bytecode.size(), "main", renderFormat);
                if (bloomPrefilterEnabled || getenv("LO_TAA_DIAGNOSTIC_REQUEST")) {
                    auto bloom = xenos::CompileCachedHlsl(bloom_prefilter::PixelShader, "main", "ps_6_0", binaryFormat);
                    if (bloom.ok)
                        bloomPrefilterPs = device->createShader(bloom.bytecode.data(), bloom.bytecode.size(), "main", renderFormat);
                    else LOG_WARNING("renderer: bloom prefilter compilation failed: {}", bloom.errors);
                }
            }

            void CompileSceneCopyPromotionShaders()
            {
                auto rgbaCompiled = xenos::CompileCachedHlsl(scene_copy_promotion::RgbaShader, "main", "ps_6_0", binaryFormat);
                auto rgbCompiled = xenos::CompileCachedHlsl(scene_copy_promotion::RgbShader, "main", "ps_6_0", binaryFormat);
                if (!rgbaCompiled.ok || !rgbCompiled.ok) {
                    SHADER_LOG_WARNING("compile-failed", None, "renderer: scene-copy promotion shader compilation failed: {}{}", rgbaCompiled.errors, rgbCompiled.errors);
                    return;
                }
                sceneCopyPromotionPs = device->createShader(rgbaCompiled.bytecode.data(), rgbaCompiled.bytecode.size(), "main", renderFormat);
                sceneCopyPromotionRgbPs = device->createShader(rgbCompiled.bytecode.data(), rgbCompiled.bytecode.size(), "main", renderFormat);
            }

            RenderPipeline* GetSceneCopyPromotionPipeline(RenderFormat format, bool rgb)
            {
                auto& cache = rgb ? sceneCopyPromotionRgbPipelines : sceneCopyPromotionPipelines;
                auto found = cache.find(uint32_t(format));
                if (found != cache.end()) return found->second.get();
                auto* ps = rgb ? sceneCopyPromotionRgbPs.get() : sceneCopyPromotionPs.get();
                if (!blitVs || !ps) return nullptr;
                RenderGraphicsPipelineDesc desc;
                desc.pipelineLayout = pipelineLayout.get(); desc.vertexShader = blitVs.get(); desc.pixelShader = ps;
                desc.depthEnabled = false; desc.depthWriteEnabled = false; desc.depthFunction = RenderComparisonFunction::ALWAYS;
                desc.depthTargetFormat = RenderFormat::UNKNOWN; desc.renderTargetFormat[0] = format;
                desc.renderTargetBlend[0] = RenderBlendDesc::Copy(); desc.renderTargetBlend[0].renderTargetWriteMask = 0xF;
                desc.renderTargetCount = 1; desc.cullMode = RenderCullMode::NONE; desc.primitiveTopology = RenderPrimitiveTopology::TRIANGLE_LIST;
                auto pipeline = device->createGraphicsPipeline(desc);
                if (!pipeline) return nullptr;
                auto* result = pipeline.get(); cache.emplace(uint32_t(format), std::move(pipeline)); return result;
            }

            std::unique_ptr<HostTexture> CreatePromotedTarget(const HostTexture& source, resolution::Size output)
            {
                auto target = std::make_unique<HostTexture>();
                target->allocationSerial = ++nextTargetAllocation; target->format = source.format;
                target->guestWidth = source.guestWidth; target->guestHeight = source.guestHeight;
                target->resolutionSize = output;
                target->width = std::max(1u, target->ScaleX(target->guestWidth));
                target->height = std::max(1u, target->ScaleY(target->guestHeight));
                target->texture = device->createTexture(RenderTextureDesc::Texture2D(target->width, target->height, 1,
                    target->format, RenderTextureFlag::RENDER_TARGET));
                target->layout = RenderTextureLayout::UNKNOWN;
                if (!target->texture) return nullptr;
                return target;
            }

            std::unique_ptr<HostTexture> CreateSceneCopyScratch(const HostTexture& source, resolution::Size output)
            {
                // Provider output has no guest pitch padding. FSR's encoded
                // output is R8 even when the SDR guest destination uses FP16;
                // the RGB-only composite samples it into that original format.
                auto scratch = std::make_unique<HostTexture>();
                scratch->allocationSerial = ++nextTargetAllocation;
                scratch->format = activePlan.requestedUpscaler == upscaling::Upscaler::Fsr ?
                    RenderFormat::R8G8B8A8_UNORM : source.format;
                scratch->guestWidth = scratch->width = std::max(1u, output.width);
                scratch->guestHeight = scratch->height = std::max(1u, output.height);
                scratch->resolutionSize = output;
                scratch->texture = device->createTexture(RenderTextureDesc::Texture2D(scratch->width, scratch->height, 1,
                    scratch->format, RenderTextureFlag::STORAGE));
                scratch->layout = RenderTextureLayout::UNKNOWN;
                if (!scratch->texture) return nullptr;
                return scratch;
            }

            bool PrepareSceneCopyDestination(const RenderTargetKey& key, HostTexture& color,
                const temporal::TemporalFrameInputs& inputs)
            {
                const auto reject = [&] {
                    NoteDlssFrameFallback(frame_plan::DlssEffectReason::PromotionUnavailable);
                    return false;
                };
                if (sceneCopyPromotion.activeMapping || sceneCopyPromotionFrame == frame || Gpu().srPrefixClosed ||
                    !sceneCopyPromotionPs || !sceneCopyPromotionRgbPs ||
                    inputs.colorEncoding == temporal::ColorEncoding::Unknown || !inputs.CompleteForConsumer()) return reject();
                const resolution::Size output{activePlan.output.width, activePlan.output.height};
                if (!output.width || !output.height) return reject();
                // VS/PS/shared constants, texture descriptors and indices for
                // this guest draw already belong to Gpu(). Never Flush here.
                if (!scene_copy_promotion::CanAppendConstants(Gpu().uploadOffset,
                        kUploadRingSize, sizeof(SharedConstants))) return reject();
                auto promoted = CreatePromotedTarget(color, output);
                auto scratch = CreateSceneCopyScratch(color, output);
                auto composite = CreatePromotedTarget(color, output);
                if (!promoted || !scratch || !composite) return reject();
                const auto scale = [&](const HostTexture& src) {
                    SharedConstants constants{};
                    constants.transfer[0] = std::bit_cast<uint32_t>(float(src.width) / float(promoted->width));
                    constants.transfer[1] = std::bit_cast<uint32_t>(float(src.height) / float(promoted->height));
                    return Upload(&constants, sizeof(constants));
                };
                const uint64_t fallbackConstants = scale(color);
                const uint64_t rgbConstants = scale(*promoted);
                if (fallbackConstants == UINT64_MAX || rgbConstants == UINT64_MAX) return reject();
                auto* fallbackSet = AcquireSet(1);
                auto* rgbSet = AcquireSet(1);
                if (!fallbackSet || !rgbSet) return reject();
                fallbackSet->setTexture(0, color.texture.get(), RenderTextureLayout::SHADER_READ);
                fallbackSet->setTexture(1, dummyTexture2D.texture.get(), RenderTextureLayout::SHADER_READ);
                rgbSet->setTexture(0, promoted->texture.get(), RenderTextureLayout::SHADER_READ);
                rgbSet->setTexture(1, scratch->texture.get(), RenderTextureLayout::SHADER_READ);
                sceneCopyPromotion = {};
                sceneCopyPromotion.key = key; sceneCopyPromotion.frame = frame; sceneCopyPromotion.epoch = activePlan.geometryEpoch;
                sceneCopyPromotion.sourceAllocation = color.allocationSerial;
                sceneCopyPromotion.inputs = inputs; sceneCopyPromotion.scratch = std::move(scratch); sceneCopyPromotion.composite = std::move(composite);
                sceneCopyPromotion.srOptions = frameSrOptions;
                sceneCopyPromotion.fallbackSet = fallbackSet; sceneCopyPromotion.rgbSet = rgbSet;
                sceneCopyPromotion.fallbackConstants = fallbackConstants; sceneCopyPromotion.rgbConstants = rgbConstants;
                // parkedLow is installed by Activate after the map identity check.
                sceneCopyPromotion.preparedPromoted = std::move(promoted);
                sceneCopyPromotion.prepared = true;
                return true;
            }

            void SetPromotionConstants(uint64_t offset)
            {
                if (vulkan) {
                    const uint64_t base = uploadRing->getDeviceAddress();
                    constantAddresses[0] = base + offset; constantAddresses[1] = base + offset; constantAddresses[2] = base + offset;
                    commandList->setGraphicsPushConstants(0, constantAddresses);
                } else {
                    SetConstantBuffer(offset, 0); SetConstantBuffer(offset, 1); SetConstantBuffer(offset, 2);
                }
            }

            bool DrawPromotionResample(HostTexture& destination, RenderDescriptorSet* set, uint64_t constants, bool rgb)
            {
                destination.sdrProducerFrame = ~0ull;
                auto* pipeline = GetSceneCopyPromotionPipeline(destination.format, rgb);
                if (!pipeline || !set) return false;
                Transition(destination, RenderTextureLayout::COLOR_WRITE, RenderBarrierStage::GRAPHICS);
                commandList->setFramebuffer(GetFramebuffer(&destination, nullptr));
                RenderViewport viewport(0.0f, 0.0f, float(destination.width), float(destination.height));
                RenderRect scissor{0, 0, int32_t(destination.width), int32_t(destination.height)};
                commandList->setViewports(viewport); commandList->setScissors(scissor);
                commandList->setPipeline(pipeline); commandList->setGraphicsPipelineLayout(pipelineLayout.get());
                SetPromotionConstants(constants); commandList->setGraphicsDescriptorSet(staticSet0.get(), 0);
                commandList->setGraphicsDescriptorSet(set, 1); commandList->setGraphicsDescriptorSet(AcquireSet(2), 2);
                commandList->setGraphicsDescriptorSet(AcquireSet(3), 3); if (vulkan) commandList->setGraphicsDescriptorSet(staticSamplerSet.get(), 4);
                commandList->drawInstanced(3, 1, 0, 0);
#if defined(LO_GPU_PLUME)
                HandleFsrAlphaRgbWriter(destination, "promotion_resample_rgb");
#endif
                return true;
            }

            bool ActivateSceneCopyDestination(HostTexture*& color, HostTexture*& rasterTarget,
                RenderViewport& rasterViewport, RenderRect& physicalScissor,
                const RenderViewport& guestViewport, const RenderRect& guestScissor)
            {
                auto& promotion = sceneCopyPromotion;
                const auto reject = [&] {
                    NoteDlssFrameFallback(frame_plan::DlssEffectReason::PromotionUnavailable);
                    return false;
                };
                if (!promotion.prepared || promotion.activeMapping || !promotion.preparedPromoted) return reject();
                auto it = renderTargets.find(promotion.key);
                if (it == renderTargets.end() || it->second.get() != color ||
                    color->allocationSerial != promotion.sourceAllocation) return reject();
                promotion.parkedLow = std::move(it->second);
                it->second = std::move(promotion.preparedPromoted);
                promotion.active = it->second.get(); promotion.activeMapping = true;
                Transition(*promotion.parkedLow, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
                if (!DrawPromotionResample(*promotion.active, promotion.fallbackSet, promotion.fallbackConstants, false)) {
                    it->second = std::move(promotion.parkedLow); promotion.active = nullptr; promotion.activeMapping = false;
                    return reject();
                }
                sceneCopyPromotionFrame = frame;
                const bool rasterWasColor = rasterTarget == color;
                color = promotion.active;
                if (rasterWasColor) rasterTarget = color;
                rasterViewport = guestViewport;
                rasterViewport.x *= double(rasterTarget->resolutionSize.width) / 1280.0;
                rasterViewport.y *= double(rasterTarget->resolutionSize.height) / 720.0;
                rasterViewport.width *= double(rasterTarget->resolutionSize.width) / 1280.0;
                rasterViewport.height *= double(rasterTarget->resolutionSize.height) / 720.0;
                physicalScissor.left = int32_t(color->ScaleX(uint32_t(guestScissor.left)));
                physicalScissor.right = int32_t(color->ScaleX(uint32_t(guestScissor.right)));
                physicalScissor.top = int32_t(color->ScaleY(uint32_t(guestScissor.top)));
                physicalScissor.bottom = int32_t(color->ScaleY(uint32_t(guestScissor.bottom)));
                return true;
            }

            bool RestoreSceneCopyDestination(const char* reason)
            {
                auto& promotion = sceneCopyPromotion;
                if (!promotion.activeMapping) return true;
                const auto fail = [&] {
                    // Losing the RGBA restore makes the next incompatible draw
                    // unsafe. Suppress this plan instead of reusing a stale map.
                    FailCurrentPlan(frame_plan::FailureReason::InvalidInput);
                    return false;
                };
                if (!promotion.active || !promotion.parkedLow) return fail();
                auto it = renderTargets.find(promotion.key);
                if (it == renderTargets.end() || it->second.get() != promotion.active) return fail();
                if (!Begin()) return false;
                SharedConstants constants{};
                constants.transfer[0] = std::bit_cast<uint32_t>(float(promotion.active->width) / float(promotion.parkedLow->width));
                constants.transfer[1] = std::bit_cast<uint32_t>(float(promotion.active->height) / float(promotion.parkedLow->height));
                // Upload may rotate slots. Acquire descriptors and record
                // barriers only afterwards; callers have not borrowed targets.
                const uint64_t offset = Upload(&constants, sizeof(constants));
                if (offset == UINT64_MAX) return fail();
                auto* restoreSet = AcquireSet(1);
                if (!restoreSet) return fail();
                restoreSet->setTexture(0, promotion.active->texture.get(), RenderTextureLayout::SHADER_READ);
                restoreSet->setTexture(1, dummyTexture2D.texture.get(), RenderTextureLayout::SHADER_READ);
                Transition(*promotion.active, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
                if (!DrawPromotionResample(*promotion.parkedLow, restoreSet, offset, false)) return fail();
                auto retired = std::move(it->second);
                it->second = std::move(promotion.parkedLow);
                if (retired) Gpu().retiredTextures.push_back(std::move(retired));
                if (promotion.composite) Gpu().retiredTextures.push_back(std::move(promotion.composite));
                if (promotion.scratch) Gpu().retiredTextures.push_back(std::move(promotion.scratch));
                static const char* loggedReason = nullptr;
                static uint32_t loggedRepeats = 0;
                const bool reasonChanged = !loggedReason || std::strcmp(loggedReason, reason) != 0;
                if (reasonChanged && loggedReason && loggedRepeats)
                    LOG_INFO("renderer: restored scene-copy destination frame={} reason={} repeats={}", frame, loggedReason, loggedRepeats);
                if (frame_plan::EmitSparseRepeat(loggedRepeats, reasonChanged)) {
                    if (reasonChanged)
                        LOG_INFO("renderer: restored scene-copy destination frame={} reason={}", frame, reason);
                    else
                        LOG_INFO("renderer: restored scene-copy destination frame={} reason={} repeats={}", frame, reason, loggedRepeats);
                }
                if (reasonChanged) loggedReason = reason;
                promotion = {};
                return true;
            }

            bool PreparePromotionAccess(const RenderTargetKey& key, uint32_t height, bool needsDepthStencil)
            {
                const auto& promotion = sceneCopyPromotion;
                if (!promotion.activeMapping) return true;
                if (!promotion.active) return RestoreSceneCopyDestination("invalid_mapping");
                height = std::clamp<uint32_t>((height + 31) & ~31u, 32, 2048);
                if (!scene_copy_promotion::MustRestore(key == promotion.key, promotion.frame == frame,
                        promotion.epoch == activePlan.geometryEpoch, needsDepthStencil,
                        height, promotion.active->guestHeight)) return true;
                return RestoreSceneCopyDestination("target_depth_extent_or_epoch");
            }

#if defined(LO_GPU_PLUME)
            // Keep the renderer route identical in the asset-free Vulkan fixture;
            // only the vendor's session and isolated command recording are substituted.
            void CaptureFsrAlphaAtSceneCopy()
            {
                if (!fsrAlphaReplayEnabled || fsrAlphaCaptureDir.empty() ||
                    frame != fsrAlphaCaptureFrame || fsrAlphaCaptureQueuedFrame == frame ||
                    fsrAlphaRawViews.empty()) return;
                size_t recorded = 0;
                for (const auto& view : fsrAlphaRawViews) {
                    if (!view || view.identity.renderFrame != frame || recorded == 4) continue;
                    const uint64_t pitch = (uint64_t(view.identity.width) + 255u) & ~255ull;
                    const uint64_t bytes = pitch * view.identity.height;
                    if (bytes == 0 || bytes > (128u << 20)) continue;
                    auto capture = std::make_shared<FsrAlphaCapture>();
                    capture->view = view;
                    capture->directory = fsrAlphaCaptureDir;
                    capture->rowPitch = uint32_t(pitch);
                    capture->buffer = device->createBuffer(RenderBufferDesc::ReadbackBuffer(bytes));
                    if (!capture->buffer) continue;
                    commandList->barriers(RenderBarrierStage::COPY,
                        RenderTextureBarrier(view.texture, RenderTextureLayout::COPY_SOURCE));
                    view.lease->layout = RenderTextureLayout::COPY_SOURCE;
                    commandList->copyTextureRegion(
                        RenderTextureCopyLocation::PlacedFootprint(capture->buffer.get(), RenderFormat::R8_UNORM,
                            view.identity.width, view.identity.height, 1, capture->rowPitch, 0),
                        RenderTextureCopyLocation::Subresource(view.texture, 0));
                    commandList->barriers(RenderBarrierStage::GRAPHICS,
                        RenderTextureBarrier(view.texture, RenderTextureLayout::COLOR_WRITE));
                    view.lease->layout = RenderTextureLayout::COLOR_WRITE;
                    Gpu().fsrAlphaCaptures.push_back(std::move(capture));
                    ++recorded;
                }
                if (recorded) fsrAlphaCaptureQueuedFrame = frame;
            }

            std::shared_ptr<FsrAlphaEquality> PrepareFsrAlphaEquality(
                const HostTexture& color, const HostTexture& depth, uint64_t drawOrdinal)
            {
                if (frame != fsrAlphaCompareFrame || fsrAlphaCaptureDir.empty() ||
                    fsrAlphaCompareQueuedFrame == frame) return {};
                fsrAlphaCompareQueuedFrame = frame;
                auto capture = std::make_shared<FsrAlphaEquality>();
                capture->directory = fsrAlphaCaptureDir;
                capture->frame = frame;
                capture->drawOrdinal = drawOrdinal;
                capture->colorAllocation = color.allocationSerial;
                capture->depthAllocation = depth.allocationSerial;
                capture->width = color.width;
                capture->height = color.height;
                capture->colorFormat = color.format;
                capture->depthFormat = depth.format;
                capture->colorBpp = color.format == RenderFormat::R8G8B8A8_UNORM ? 4u :
                    color.format == RenderFormat::R16G16B16A16_FLOAT ? 8u : 0u;
                if (!capture->colorBpp || depth.format != RenderFormat::D32_FLOAT_S8_UINT ||
                    color.width != depth.width || color.height != depth.height)
                    capture->reason = "unsupported_color_or_depth_format_or_extent";
                else {
                    const uint64_t colorPitch = (uint64_t(color.width) * capture->colorBpp + 255u) & ~255ull;
                    const uint64_t depthPitch = (uint64_t(depth.width) * 4u + 255u) & ~255ull;
                    if (!color.width || !color.height || colorPitch * color.height > (128u << 20) ||
                        depthPitch * depth.height > (128u << 20))
                        capture->reason = "readback_extent_exceeds_limit";
                    else {
                        capture->colorPitch = uint32_t(colorPitch);
                        capture->depthPitch = uint32_t(depthPitch);
                        for (size_t i = 0; i < capture->buffers.size(); ++i) {
                            const uint64_t bytes = (i < 2 ? colorPitch : depthPitch) * color.height;
                            capture->buffers[i] = device->createBuffer(RenderBufferDesc::ReadbackBuffer(bytes));
                            if (!capture->buffers[i]) { capture->reason = "readback_allocation_failed"; break; }
                        }
                    }
                }
                // The slot owns every buffer before a command may reference it.
                Gpu().fsrAlphaEqualities.push_back(capture);
                return capture;
            }

            void RecordFsrAlphaEqualitySnapshot(const std::shared_ptr<FsrAlphaEquality>& capture,
                HostTexture& color, HostTexture& depth, bool after)
            {
                if (!capture || !capture->reason.empty()) return;
                const auto colorLayout = color.layout;
                const auto depthLayout = depth.layout;
                Transition(color, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
                Transition(depth, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
                const size_t index = after ? 1 : 0;
                commandList->copyTextureRegion(
                    RenderTextureCopyLocation::PlacedFootprint(capture->buffers[index].get(),
                        color.format, color.width, color.height, 1,
                        capture->colorPitch / capture->colorBpp, 0),
                    RenderTextureCopyLocation::Subresource(color.texture.get(), 0));
                commandList->copyTextureRegion(
                    RenderTextureCopyLocation::PlacedFootprint(capture->buffers[index + 2].get(),
                        RenderFormat::R32_FLOAT, depth.width, depth.height, 1,
                        capture->depthPitch / 4, 0),
                    RenderTextureCopyLocation::Subresource(depth.texture.get(), 0));
                Transition(color, colorLayout, RenderBarrierStage::GRAPHICS);
                Transition(depth, depthLayout, RenderBarrierStage::GRAPHICS);
            }

            bool FsrAlphaBridgeTraceEnabled() const {
                return fsrAlphaBridge && frame == fsrAlphaCaptureFrame && !fsrAlphaCaptureDir.empty();
            }
            void TraceFsrAlphaBridge(std::shared_ptr<FsrAlphaBridgeDiagnostic> event) {
                if (!FsrAlphaBridgeTraceEnabled() || fsrAlphaBridgeTraceCount >= 128) return;
                event->frame = frame;
                event->eventIndex = fsrAlphaBridgeTraceCount;
                Gpu().fsrAlphaBridgeDiagnostics.push_back(std::move(event));
                ++fsrAlphaBridgeTraceCount;
            }
            bool QueueFsrAlphaSnapshot(const std::shared_ptr<FsrAlphaBridgeDiagnostic>& event,
                const char* label, RenderTexture* texture, RenderFormat format,
                uint32_t width, uint32_t height, uint32_t bytesPerPixel,
                RenderTextureLayout originalLayout) {
                if (!event) return false;
                const auto fail = [&](const char* reason) {
                    event->captureFailures.push_back(fmt::format("{}:{}", label, reason));
                    return false;
                };
                if (!texture || !width || !height || !bytesPerPixel)
                    return fail("missing_image_or_extent");
                if (event->snapshots.size() >= 16) return fail("snapshot_limit");
                const uint64_t rowBytes = uint64_t(width) * bytesPerPixel;
                const uint64_t rowPitch = (rowBytes + 255u) & ~255ull;
                if (rowPitch * height > (128u << 20)) return fail("extent_limit");
                FsrAlphaBridgeDiagnostic::Snapshot snapshot{};
                snapshot.label = label; snapshot.width = width; snapshot.height = height;
                snapshot.rowPitch = uint32_t(rowPitch); snapshot.bytesPerPixel = bytesPerPixel;
                snapshot.format = format;
                snapshot.buffer = device->createBuffer(RenderBufferDesc::ReadbackBuffer(rowPitch * height));
                if (!snapshot.buffer) return fail("buffer_allocation_failed");
                // Register the readback before any GPU command can reference it.
                auto* buffer = snapshot.buffer.get();
                event->snapshots.push_back(std::move(snapshot));
                commandList->barriers(RenderBarrierStage::COPY,
                    RenderTextureBarrier(texture, RenderTextureLayout::COPY_SOURCE));
                commandList->copyTextureRegion(
                    RenderTextureCopyLocation::PlacedFootprint(buffer, format, width, height, 1,
                        uint32_t(rowPitch / bytesPerPixel), 0),
                    RenderTextureCopyLocation::Subresource(texture, 0));
                commandList->barriers(RenderBarrierStage::GRAPHICS,
                    RenderTextureBarrier(texture, originalLayout));
                return true;
            }
            void HandleFsrAlphaRgbWriter(HostTexture& color, const char* writer,
                uint64_t drawOrdinal = 0, uint64_t vsHash = 0, uint64_t psHash = 0,
                uint64_t blend = 0, uint32_t colorMask = 7, bool retainsRaw = false) {
                if (!fsrAlphaBridge || activePlan.requestedUpscaler != upscaling::Upscaler::Fsr) return;
                fsrAlphaBridge->InvalidateClearBackground(color.allocationSerial, writer);
                const bool retained = retainsRaw && fsrAlphaBridge->HasRawSource(color.allocationSerial);
                if (retained ? !fsrAlphaBridge->HasCurrentSource(color.allocationSerial) :
                    !fsrAlphaBridge->InvalidateRawSource(color.allocationSerial)) return;
                if (!FsrAlphaBridgeTraceEnabled()) return;
                auto event = std::make_shared<FsrAlphaBridgeDiagnostic>();
                event->fields = fmt::format(
                    "\"kind\":\"source_writer\",\"status\":\"{}\","
                    "\"writer\":\"{}\",\"source_allocation\":{},\"draw_ordinal\":{},"
                    "\"vs_hash\":\"{:016x}\",\"ps_hash\":\"{:016x}\","
                    "\"blend\":{},\"color_mask\":{}",
                    retained ? "retained_audited_local_blend" : "source_written_after_raw",
                    writer, color.allocationSerial, drawOrdinal, vsHash, psHash, blend, colorMask);
                TraceFsrAlphaBridge(std::move(event));
            }
            void RecordFsrAlphaBridgeResolve(HostTexture& source, uint32_t address,
                const ResolvedSurface& destination, const char* operation) {
                if (!fsrAlphaBridge || activePlan.requestedUpscaler != upscaling::Upscaler::Fsr) return;
                const fsr_alpha::MaskRect rect{destination.writeX, destination.writeY,
                    destination.writeWidth, destination.writeHeight};
                const auto result = fsrAlphaBridge->RecordResolve(commandList, frame, temporalEpoch,
                    source.allocationSerial,
                    source.width, source.height, address, destination.destFormat,
                    destination.tex->allocationSerial, destination.writeOrdinal,
                    destination.tex->width, destination.tex->height, rect, operation,
                    Gpu().fsrAlphaBridgeUses);
                if (!FsrAlphaBridgeTraceEnabled() || !fsrAlphaBridge->HasEvidence() ||
                    fsrAlphaBridgeTraceCount >= 128) return;
                auto event = std::make_shared<FsrAlphaBridgeDiagnostic>();
                event->writeOrdinal = destination.writeOrdinal;
                event->fields = fmt::format(
                    "\"kind\":\"resolve\",\"operation\":\"{}\",\"status\":\"{}\","
                    "\"source_allocation\":{},\"source_extent\":[{},{}],\"raw_revision\":{},"
                    "\"raw_epoch\":{},\"raw_depth_allocation\":{},"
                    "\"source_revision\":{},\"source_stage\":{},"
                    "\"destination_address\":{},\"destination_format\":{},\"destination_allocation\":{},"
                    "\"write_ordinal\":{},\"resolve_rect\":[{},{},{},{}],\"resolved_extent\":[{},{}],"
                    "\"mask_valid_rect\":[{},{},{},{}]",
                    operation, result.reason, source.allocationSerial, source.width, source.height,
                    result.rawAtResolve.auditedDraws,
                    result.rawAtResolve.identity.geometryEpoch, result.rawAtResolve.identity.depthAllocation,
                    result.version.sourceRevision, uint32_t(result.version.sourceStage),
                    address, destination.destFormat,
                    destination.tex->allocationSerial, destination.writeOrdinal,
                    rect.x, rect.y, rect.width, rect.height,
                    destination.tex->width, destination.tex->height,
                    result.version.validRect.x, result.version.validRect.y,
                    result.version.validRect.width, result.version.validRect.height);
                if (result.copied && result.rawAtResolve && fsrAlphaBridgePairCount < 8) {
                    event->raw = result.rawAtResolve;
                    event->mask = result.version.mask;
                    event->rect = rect;
                    event->rawWidth = source.width; event->rawHeight = source.height;
                    event->resolvedWidth = destination.tex->width;
                    event->resolvedHeight = destination.tex->height;
                    const uint64_t rawPitch = (uint64_t(source.width) + 255u) & ~255ull;
                    const uint64_t resolvedPitch = (uint64_t(destination.tex->width) + 255u) & ~255ull;
                    if (rawPitch * source.height > (128u << 20) ||
                        resolvedPitch * destination.tex->height > (128u << 20))
                        event->fields += ",\"pair_capture\":\"extent_exceeds_limit\"";
                    else {
                        event->rawPitch = uint32_t(rawPitch);
                        event->resolvedPitch = uint32_t(resolvedPitch);
                        event->buffers[0] = device->createBuffer(
                            RenderBufferDesc::ReadbackBuffer(rawPitch * source.height));
                        event->buffers[1] = device->createBuffer(
                            RenderBufferDesc::ReadbackBuffer(resolvedPitch * destination.tex->height));
                        if (!event->buffers[0] || !event->buffers[1])
                            event->fields += ",\"pair_capture\":\"buffer_allocation_failed\"";
                        else {
                            // Retain both sources and buffers in this GPU slot before copies.
                            TraceFsrAlphaBridge(event);
                            ++fsrAlphaBridgePairCount;
                            commandList->barriers(RenderBarrierStage::COPY,
                                RenderTextureBarrier(event->raw.texture, RenderTextureLayout::COPY_SOURCE));
                            event->raw.lease->layout = RenderTextureLayout::COPY_SOURCE;
                            commandList->barriers(RenderBarrierStage::COPY,
                                RenderTextureBarrier(event->mask->texture.get(), RenderTextureLayout::COPY_SOURCE));
                            event->mask->layout = RenderTextureLayout::COPY_SOURCE;
                            commandList->copyTextureRegion(
                                RenderTextureCopyLocation::PlacedFootprint(event->buffers[0].get(), RenderFormat::R8_UNORM,
                                    event->rawWidth, event->rawHeight, 1, event->rawPitch, 0),
                                RenderTextureCopyLocation::Subresource(event->raw.texture, 0));
                            commandList->copyTextureRegion(
                                RenderTextureCopyLocation::PlacedFootprint(event->buffers[1].get(), RenderFormat::R8_UNORM,
                                    event->resolvedWidth, event->resolvedHeight, 1, event->resolvedPitch, 0),
                                RenderTextureCopyLocation::Subresource(event->mask->texture.get(), 0));
                            commandList->barriers(RenderBarrierStage::GRAPHICS,
                                RenderTextureBarrier(event->raw.texture, RenderTextureLayout::COLOR_WRITE));
                            event->raw.lease->layout = RenderTextureLayout::COLOR_WRITE;
                            commandList->barriers(RenderBarrierStage::GRAPHICS,
                                RenderTextureBarrier(event->mask->texture.get(), RenderTextureLayout::SHADER_READ));
                            event->mask->layout = RenderTextureLayout::SHADER_READ;
                            return;
                        }
                    }
                }
                TraceFsrAlphaBridge(std::move(event));
            }
            template<class SrController>
            bool RecordSceneCopyDlssUsing(SrController& controller, HostTexture*& color, HostTexture*& rasterTarget)
            {
                auto& promotion = sceneCopyPromotion;
                if (!promotion.activeMapping || !vulkan || !Gpu().srIsolated || !Gpu().srContinuation) return false;
                if (activePlan.requestedUpscaler == upscaling::Upscaler::Fsr)
                    CaptureFsrAlphaAtSceneCopy();
                dlss::SrConfig config{};
                config.renderExtent = {promotion.inputs.color.width, promotion.inputs.color.height};
                config.outputExtent = {activePlan.output.width, activePlan.output.height};
                config.quality = activePlan.dlssQuality; config.deviceEpoch = activePlan.deviceEpoch;
                config.depthInverted = promotion.inputs.depthConvention == temporal::DepthConvention::Reversed;
                config.colorSpace = promotion.inputs.colorEncoding == temporal::ColorEncoding::Sdr ?
                    dlss::SrColorSpace::DisplayEncoded : dlss::SrColorSpace::Linear;
                std::shared_ptr<dlss::EvaluateCapture> evidence;
                if (activePlan.requestedUpscaler == upscaling::Upscaler::Dlss && evaluatePage && evaluatePage->frame == frame && promotion.inputs.color.texture && promotion.scratch) {
                    try {
                        evidence = evaluatePage->NewAttempt(*device, promotion.inputs, config,
                            *static_cast<plume::VulkanTexture*>(promotion.inputs.color.texture),
                            *static_cast<plume::VulkanTexture*>(promotion.scratch->texture.get()),
                            temporalScene.Color().ordinal, drawsThisFrame);
                    } catch (const std::exception&) { evaluatePage->captureFailed = true; }
                } else if (activePlan.requestedUpscaler == upscaling::Upscaler::Fsr && evaluatePage &&
                    evaluatePage->frame == frame && temporalHistory && promotion.inputs.color.texture &&
                    promotion.inputs.depth.texture && promotion.inputs.motion.texture &&
                    promotion.inputs.motionInvalidity.texture && promotion.scratch) {
                    try {
                        evidence = evaluatePage->NewFsrAttempt(*device, promotion.inputs, config,
                            *static_cast<plume::VulkanTexture*>(promotion.inputs.color.texture),
                            *static_cast<plume::VulkanTexture*>(promotion.inputs.depth.texture),
                            *static_cast<plume::VulkanTexture*>(promotion.inputs.motion.texture),
                            *static_cast<plume::VulkanTexture*>(promotion.inputs.motionInvalidity.texture),
                            *static_cast<plume::VulkanTexture*>(promotion.scratch->texture.get()),
                            temporalHistory->CurrentCameraForCapture(), temporalHistory->PreviousCameraForCapture(),
                            temporalScene.Color().ordinal, drawsThisFrame);
                    } catch (const std::exception&) { evaluatePage->captureFailed = true; }
                }
                // A fresh controller has no feature to recreate. EnsureSession
                // establishes that cold state; RecordIsolated reports a genuine
                // configuration change as NeedsReconfigure after it exists.
#if defined(LO_RENDERER_P2_EMBEDDED_TEST)
                const bool sessionReady = controller.EnsureSession(*static_cast<plume::VulkanDevice*>(device)) == dlss::SrStatus::Executable;
#else
                const auto prepared = controller.Prepare(*static_cast<plume::VulkanDevice*>(device),
                    {activePlan, promotion.inputs, promotion.srOptions});
                const bool sessionReady = prepared.status == SrResultStatus::Ready;
                if (prepared.status == SrResultStatus::InputUnavailable) {
                    NoteDlssFrameFallback(frame_plan::DlssEffectReason::UnsupportedProjection);
                    return false;
                }
                if (prepared.status == SrResultStatus::NeedsReconfigure) {
                    srReconfigureFrame = frame;
                    NoteDlssFrameFallback(frame_plan::DlssEffectReason::FeatureReconfigurePending);
                    return false;
                }
#endif
                if (!sessionReady) {
                    if (evidence) { evidence->stage = "ensure_session"; evidence->reason = "session_unavailable"; }
                    DisableDlssRequest(frame_plan::FailureReason::DlssUnavailable);
                    return false;
                }
                // The promoted destination first received the old RGBA contents,
                // then the original guest copy (including its destination-dependent
                // alpha blend). The composite below preserves that post-copy alpha;
                // it never reads SR scratch alpha. A's
                // fixture contract requires every NGX image to enter GENERAL with
                // an ALL-stage barrier, regardless of its preceding renderer use.
                std::vector<RenderTextureBarrier> barriers;
                for (auto* image : {promotion.inputs.color.texture, promotion.inputs.depth.texture,
                                    promotion.inputs.motion.texture, promotion.inputs.motionInvalidity.texture})
                    if (image) barriers.emplace_back(image, activePlan.requestedUpscaler == upscaling::Upscaler::Fsr ?
                        RenderTextureLayout::SHADER_READ : RenderTextureLayout::GENERAL);
                barriers.emplace_back(promotion.scratch->texture.get(), activePlan.requestedUpscaler == upscaling::Upscaler::Fsr ?
                    RenderTextureLayout::COPY_DEST : RenderTextureLayout::GENERAL);
                // The scene-copy input only borrows this image. Hold the lease in
                // the consuming slot before its first prefix command; the producer
                // slot continues to own any earlier fetch/crop commands.
                if (activePlan.requestedUpscaler == upscaling::Upscaler::Fsr &&
                    fsr_alpha::RetainSceneCopyMaskForBatch(promotion.inputs, promotion.fsrMaskLease,
                        Gpu().fsrAlphaBridgeUses)) {
                    barriers.emplace_back(promotion.fsrMaskLease->texture.get(), RenderTextureLayout::SHADER_READ);
                    promotion.fsrMaskLease->layout = RenderTextureLayout::SHADER_READ;
                } else promotion.inputs.fsrMask = {};
                commandList->barriers(RenderBarrierStage::ALL, barriers);
                std::shared_ptr<FsrAlphaBridgeDiagnostic> handoffTrace;
                if (activePlan.requestedUpscaler == upscaling::Upscaler::Fsr &&
                    FsrAlphaBridgeTraceEnabled() && fsrAlphaBridgeTraceCount < 128) {
                    handoffTrace = std::make_shared<FsrAlphaBridgeDiagnostic>();
                    handoffTrace->fields = "\"kind\":\"sr_mask_handoff\",\"status\":\"before_record\"";
                    TraceFsrAlphaBridge(handoffTrace); // Own readbacks before recording their copies.
                    const auto& depth = promotion.inputs.depth;
                    if (depth.Complete() && depth.x == 0 && depth.y == 0 &&
                        depth.width == depth.allocation.width && depth.height == depth.allocation.height)
                        QueueFsrAlphaSnapshot(handoffTrace, "sdk-depth-before", depth.texture,
                            RenderFormat::R32_FLOAT, depth.width, depth.height, 4, RenderTextureLayout::SHADER_READ);
                }
                Gpu().drawProbe.End(commandList);
                if (!video::EndGpuCommands(commandList)) {
                    if (evidence) { evidence->stage = "prefix_end"; evidence->reason = "prefix_end_failed"; }
                    listOpen = false; return false;
                }
                listOpen = false;
                Gpu().srPrefixClosed = true;
#if defined(LO_RENDERER_P2_EMBEDDED_TEST)
                auto attempt = controller.RecordIsolated(*static_cast<plume::VulkanCommandList*>(Gpu().srIsolated.get()), config,
                    promotion.inputs, *static_cast<plume::VulkanTexture*>(promotion.scratch->texture.get()), evidence.get());
                const bool accepted = attempt.status == dlss::SrStatus::Executable;
                const bool deviceLost = attempt.status == dlss::SrStatus::DeviceLost;
                const bool needsReconfigure = attempt.status == dlss::SrStatus::NeedsReconfigure;
                Gpu().srUseId = attempt.useId;
#else
                auto attempt = controller.RecordIsolated(*static_cast<plume::VulkanCommandList*>(Gpu().srIsolated.get()),
                    {activePlan, promotion.inputs, promotion.srOptions},
                    *static_cast<plume::VulkanTexture*>(promotion.scratch->texture.get()), evidence.get());
                const bool accepted = attempt.status == SrResultStatus::Ready;
                const bool deviceLost = attempt.status == SrResultStatus::DeviceLost;
                const bool needsReconfigure = attempt.status == SrResultStatus::NeedsReconfigure;
                Gpu().srUseId = attempt.token;
#endif
                if (handoffTrace) {
                    const auto& p = promotion.inputs.fsrMask.provenance;
                    handoffTrace->fields = fmt::format(
                        "\"kind\":\"sr_mask_handoff\",\"status\":\"{}\","
                        "\"temporal_epoch\":{},\"geometry_epoch\":{},\"device_epoch\":{},"
                        "\"color_ordinal\":{},\"source_allocation\":{},\"source_write_ordinal\":{},"
                        "\"captured_color_image\":{},\"mask_image\":{},\"consumer_lease_held\":{},"
                        "\"requested_rcas_enabled\":{},\"requested_rcas_strength\":{},"
                        "\"sdk_capture_present\":{}",
                        accepted ? "accepted" : "rejected", promotion.inputs.temporalEpoch,
                        activePlan.geometryEpoch, activePlan.deviceEpoch, promotion.inputs.colorOrdinal,
                        p.sourceAllocation, p.sourceWriteOrdinal, uintptr_t(promotion.inputs.color.texture),
                        uintptr_t(promotion.inputs.fsrMask.sceneContribution.texture),
                        promotion.fsrMaskLease && promotion.inputs.fsrMask.sceneContribution.texture ? 1 : 0,
                        promotion.srOptions.fsrSharpening ? 1 : 0, promotion.srOptions.fsrSharpness,
                        evidence ? 1 : 0);
                    if (evidence) handoffTrace->fields += fmt::format(
                        ",\"sdk_mask_bound\":{},\"sdk_mask_rejection\":{},\"sdk_reactive_format\":{},"
                        "\"sdk_reactive_cap\":{},\"sdk_rcas_enabled\":{},\"sdk_rcas_strength\":{}",
                        evidence->fsrDispatch.maskBound ? 1 : 0, evidence->fsrDispatch.maskRejection,
                        uint32_t(evidence->fsrDispatch.reactiveFormat), evidence->fsrDispatch.reactiveCap,
                        evidence->fsrDispatch.rcasEnabled ? 1 : 0, evidence->fsrDispatch.rcasStrength);
                }
                if (evidence) {
                    if (evidence->evaluated) evaluatePage->Called(evidence);
                    else evaluatePage->CancelReservation(evidence);
                    evidence->isolatedAccepted = accepted;
                    Gpu().evaluateCaptures.push_back(evidence);
                }
                if (deviceLost) {
                    if (Gpu().srUseId) {
#if defined(LO_RENDERER_P2_EMBEDDED_TEST)
                        controller.OnBatchDiscarded(Gpu().srUseId);
#else
                        controller.OnDiscarded(Gpu().srUseId);
#endif
                    }
                    Gpu().srUseId = {};
                    video::StopGpuWork(attempt.rawVkResult.value_or(VK_ERROR_DEVICE_LOST));
                    return false;
                }
                if (!accepted && Gpu().srUseId) {
#if defined(LO_RENDERER_P2_EMBEDDED_TEST)
                    controller.OnBatchDiscarded(Gpu().srUseId);
#else
                    controller.OnDiscarded(Gpu().srUseId);
#endif
                    Gpu().srUseId = {};
                }
                Gpu().srIsolatedAccepted = accepted;
                commandList = Gpu().srContinuation.get(); if (!video::BeginGpuCommands(commandList)) return false; listOpen = true; Gpu().srContinuationOpen = true;
                // Continuation owns the normal post-NGX layouts. It may record the
                // RGB-only combine only after a successfully closed isolated list.
                std::vector<RenderTextureBarrier> continuationBarriers;
                for (auto* image : {promotion.inputs.color.texture, promotion.inputs.depth.texture,
                                    promotion.inputs.motion.texture, promotion.inputs.motionInvalidity.texture,
                                    promotion.scratch->texture.get()})
                    if (image) continuationBarriers.emplace_back(image, RenderTextureLayout::SHADER_READ);
                commandList->barriers(RenderBarrierStage::GRAPHICS, continuationBarriers);
                if (accepted && handoffTrace) {
                    const auto& depth = promotion.inputs.depth;
                    if (depth.Complete() && depth.x == 0 && depth.y == 0 &&
                        depth.width == depth.allocation.width && depth.height == depth.allocation.height)
                        QueueFsrAlphaSnapshot(handoffTrace, "sdk-depth-after", depth.texture,
                            RenderFormat::R32_FLOAT, depth.width, depth.height, 4, RenderTextureLayout::SHADER_READ);
                }
                if (!accepted) {
                    if (needsReconfigure) {
                        srReconfigureFrame = frame; // Retry only after a drained next-frame boundary.
                        NoteDlssFrameFallback(frame_plan::DlssEffectReason::FeatureReconfigurePending);
                    } else
                        DisableDlssRequest(frame_plan::FailureReason::DlssUnavailable);
                    return false;
                }
                Transition(*promotion.active, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
                if (!DrawPromotionResample(*promotion.composite, promotion.rgbSet, promotion.rgbConstants, true)) {
                    if (evidence) evidence->reason = "composite_failed";
                    DisableDlssRequest(frame_plan::FailureReason::InvalidInput);
                    return false;
                }
                auto it = renderTargets.find(promotion.key);
                if (it == renderTargets.end() || it->second.get() != promotion.active) {
                    if (evidence) { evidence->compositeSucceeded = true; evidence->reason = "composite_target_missing"; }
                    return false;
                }
                auto fallback = std::move(it->second); it->second = std::move(promotion.composite);
                promotion.active = it->second.get(); color = promotion.active;
                if (rasterTarget == fallback.get()) rasterTarget = color;
                Gpu().retiredTextures.push_back(std::move(fallback));
                promotion.srApplied = true;
                if (activePlan.requestedUpscaler == upscaling::Upscaler::Fsr && (promotion.inputs.resetHistory || frame % 120 == 0)) {
                    const auto projection = fsr::DeriveProjection(promotion.inputs.cameraViewProjection,
                        double(promotion.inputs.color.width) / promotion.inputs.color.height);
                    if (projection) LOG_INFO("renderer: FSR recorded frame={} input={}x{} output={}x{} quality={} near={} fov={} dt_ms={} jitter=({}, {}) input_reset={} unit_scale=1_uncalibrated",
                        promotion.inputs.renderFrameId, promotion.inputs.color.width, promotion.inputs.color.height,
                        activePlan.output.width, activePlan.output.height, uint32_t(activePlan.fsrQuality), projection->nearDistance,
                        projection->verticalFovRadians, promotion.inputs.frameTimeDeltaMilliseconds,
                        promotion.inputs.jitter.pixelX, promotion.inputs.jitter.pixelY, promotion.inputs.resetHistory);
                }
                if (evidence) { evidence->compositeSucceeded = true; evidence->adopted = true; }
                ArmDlssSubmit();
                return true;
            }
            bool RecordSceneCopyDlss(HostTexture*& color, HostTexture*& rasterTarget)
            {
#if defined(LO_RENDERER_P2_EMBEDDED_TEST)
                return dlssController && RecordSceneCopyDlssUsing(*dlssController, color, rasterTarget);
#else
                return temporalUpscaler && RecordSceneCopyDlssUsing(*temporalUpscaler, color, rasterTarget);
#endif
            }

#endif

            HostTexture* PrefilterBloom(HostTexture& src, RenderTexture* hdrSource = nullptr)
            {
                if (!bloomPrefilterPs || !blitVs) return nullptr;
                if (!bloomPrefilterPipeline) {
                    RenderGraphicsPipelineDesc desc;
                    desc.pipelineLayout = pipelineLayout.get();
                    desc.vertexShader = blitVs.get();
                    desc.pixelShader = bloomPrefilterPs.get();
                    desc.depthEnabled = false;
                    desc.depthWriteEnabled = false;
                    desc.depthFunction = RenderComparisonFunction::ALWAYS;
                    desc.depthTargetFormat = RenderFormat::UNKNOWN;
                    desc.renderTargetFormat[0] = RenderFormat::R16G16B16A16_FLOAT;
                    desc.renderTargetBlend[0] = RenderBlendDesc::Copy();
                    desc.renderTargetBlend[0].renderTargetWriteMask = 0xF;
                    desc.renderTargetCount = 1;
                    desc.cullMode = RenderCullMode::NONE;
                    desc.primitiveTopology = RenderPrimitiveTopology::TRIANGLE_LIST;
                    bloomPrefilterPipeline = device->createGraphicsPipeline(desc);
                    if (!bloomPrefilterPipeline) return nullptr;
                }
                if (!Begin()) return nullptr;
                auto& slot = Gpu();
                if (slot.bloomPrefilterUsed == slot.bloomPrefilterTextures.size()) {
                    auto dst = std::make_unique<HostTexture>();
                    dst->width = dst->guestWidth = 1280;
                    dst->height = dst->guestHeight = 720;
                    dst->format = RenderFormat::R16G16B16A16_FLOAT;
                    dst->texture = device->createTexture(RenderTextureDesc::Texture2D(1280, 720, 1,
                        dst->format, RenderTextureFlag::RENDER_TARGET));
                    if (!dst->texture) return nullptr;
                    slot.bloomPrefilterTextures.push_back(std::move(dst));
                }
                HostTexture& dst = *slot.bloomPrefilterTextures[slot.bloomPrefilterUsed++];
                Transition(src, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
                Transition(dst, RenderTextureLayout::COLOR_WRITE, RenderBarrierStage::GRAPHICS);
                auto* set = AcquireSet(1); // Does not submit or change GPU slots.
                set->setTexture(0, hdrSource ? hdrSource : src.texture.get(), RenderTextureLayout::SHADER_READ);
                commandList->setFramebuffer(GetFramebuffer(&dst, nullptr));
                RenderViewport viewport(0.0f, 0.0f, 1280.0f, 720.0f);
                RenderRect scissor{0, 0, 1280, 720};
                commandList->setViewports(&viewport, 1);
                commandList->setScissors(&scissor, 1);
                commandList->setPipeline(bloomPrefilterPipeline.get());
                commandList->setGraphicsPipelineLayout(pipelineLayout.get());
                SetConstantBuffer(0, 0);
                SetConstantBuffer(0, 1);
                SetConstantBuffer(0, 2);
                commandList->setGraphicsDescriptorSet(staticSet0.get(), 0);
                commandList->setGraphicsDescriptorSet(set, 1);
                commandList->setGraphicsDescriptorSet(AcquireSet(2), 2);
                commandList->setGraphicsDescriptorSet(AcquireSet(3), 3);
                if (vulkan) commandList->setGraphicsDescriptorSet(staticSamplerSet.get(), 4);
                commandList->drawInstanced(3, 1, 0, 0);
#if defined(LO_GPU_PLUME)
                HandleFsrAlphaRgbWriter(dst, "bloom_prefilter");
#endif
                Transition(dst, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
                return &dst;
            }

            RenderPipeline* GetBlitPipeline(RenderFormat targetFormat)
            {
                auto it = blitPipelines.find(uint32_t(targetFormat));
                if (it != blitPipelines.end())
                    return it->second.get();
                if (!blitVs || !blitPs)
                    return nullptr;
                RenderGraphicsPipelineDesc desc;
                desc.pipelineLayout = pipelineLayout.get();
                desc.vertexShader = blitVs.get();
                desc.pixelShader = blitPs.get();
                desc.depthEnabled = false;
                desc.depthWriteEnabled = false;
                desc.depthFunction = RenderComparisonFunction::ALWAYS;
                desc.depthTargetFormat = RenderFormat::UNKNOWN;
                desc.renderTargetFormat[0] = targetFormat;
                desc.renderTargetBlend[0] = RenderBlendDesc::Copy();
                desc.renderTargetCount = 1;
                desc.renderTargetBlend[0].renderTargetWriteMask = 0xF;
                desc.cullMode = RenderCullMode::NONE;
                desc.primitiveTopology = RenderPrimitiveTopology::TRIANGLE_LIST;
                auto pipeline = device->createGraphicsPipeline(desc);
                RenderPipeline* result = pipeline.get();
                blitPipelines.emplace(uint32_t(targetFormat), std::move(pipeline));
                return result;
            }

            // Copies rect (x0,y0,w,h) from src into dst at the same position,
            // converting formats along the way.
            bool BlitRegion(HostTexture& src, HostTexture& dst, uint32_t x0, uint32_t y0, uint32_t w, uint32_t h)
            {
                dst.sdrProducerFrame = ~0ull;
                consecutiveResolveCopies.Invalidate();
                RenderPipeline* pipeline = GetBlitPipeline(dst.format);
                if (!pipeline)
                    return false;
                if (!Begin()) return false;
                Transition(src, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
                Transition(dst, RenderTextureLayout::COLOR_WRITE, RenderBarrierStage::GRAPHICS);
                RenderDescriptorSet* set1 = AcquireSet(1);
                set1->setTexture(0, src.texture.get(), RenderTextureLayout::SHADER_READ);
                commandList->setFramebuffer(GetFramebuffer(&dst, nullptr));
                RenderViewport viewport(static_cast<float>(x0), static_cast<float>(y0), static_cast<float>(w), static_cast<float>(h));
                commandList->setViewports(&viewport, 1);
                RenderRect scissor{ int32_t(x0), int32_t(y0), int32_t(x0 + w), int32_t(y0 + h) };
                commandList->setScissors(&scissor, 1);
                commandList->setPipeline(pipeline);
                commandList->setGraphicsPipelineLayout(pipelineLayout.get());
                SetConstantBuffer(0, 0);
                SetConstantBuffer(0, 1);
                SetConstantBuffer(0, 2);
                commandList->setGraphicsDescriptorSet(staticSet0.get(), 0);
                commandList->setGraphicsDescriptorSet(set1, 1);
                commandList->setGraphicsDescriptorSet(AcquireSet(2), 2);
                commandList->setGraphicsDescriptorSet(AcquireSet(3), 3);
                if(vulkan) commandList->setGraphicsDescriptorSet(staticSamplerSet.get(),4);
                commandList->drawInstanced(3, 1, 0, 0);
#if defined(LO_GPU_PLUME)
                HandleFsrAlphaRgbWriter(dst, "blit_region");
#endif
                return true;
            }

            void CompileRectListGs()
            {
                std::string src = R"HLSL(
struct V { float4 pos : SV_Position; float4 t[16] : TEXCOORD0; };
[maxvertexcount(4)]
void main(triangle V input[3], inout TriangleStream<V> stream)
{
    // The rectangle's right-angle corner is the vertex opposite the longest
    // edge; rotate it to the front (keeps the winding) and synthesise the
    // fourth vertex as the sum of its neighbours minus the corner.
    float2 p0 = input[0].pos.xy / input[0].pos.w, p1 = input[1].pos.xy / input[1].pos.w, p2 = input[2].pos.xy / input[2].pos.w;
    float e0 = dot(p1 - p2, p1 - p2); // opposite vertex 0
    float e1 = dot(p2 - p0, p2 - p0);
    float e2 = dot(p0 - p1, p0 - p1);
    uint c = (e0 >= e1 && e0 >= e2) ? 0 : (e1 >= e2 ? 1 : 2);
    V a = input[c], b = input[(c + 1) % 3], d = input[(c + 2) % 3];
    V last;
    last.pos = b.pos + d.pos - a.pos;
    [unroll] for (uint i = 0; i < 16; i++) last.t[i] = b.t[i] + d.t[i] - a.t[i];
    stream.Append(a); stream.Append(b); stream.Append(d); stream.Append(last);
    stream.RestartStrip();
}
)HLSL";
                xenos::CompiledShader gs = xenos::CompileCachedHlsl(src, "main", "gs_6_0", binaryFormat);
                if (!gs.ok)
                {
                    LOG_WARNING("renderer: rect list GS failed: {}", gs.errors);
                    return;
                }
                rectListGs = device->createShader(gs.bytecode.data(), gs.bytecode.size(), "main", renderFormat);
            }

            // ---- command list / upload ring ------------------------------------
            // ReBlue-style two-slot ring: submit without waiting, then wait only
            // the slot about to be recorded into (one generation old).
#if defined(LO_GPU_PLUME)
            void ExportFsrAlphaCapture(const FsrAlphaCapture& capture, size_t index)
            {
                if (!capture.submissionSerial || !capture.buffer || !capture.view) return;
                std::error_code ec;
                std::filesystem::create_directories(capture.directory, ec);
                if (ec) { LOG_WARNING("fsr alpha: capture directory failed: {}", ec.message()); return; }
                const auto& id = capture.view.identity;
                const auto stem = fmt::format("fsr-alpha-f{}-c{}-d{}-{}",
                    id.renderFrame, id.colorAllocation, id.depthAllocation, index);
                auto* mapped = static_cast<const uint8_t*>(capture.buffer->map());
                if (!mapped) { LOG_WARNING("fsr alpha: capture map failed frame={}", id.renderFrame); return; }
                std::ofstream raw(capture.directory / (stem + ".r8.bin"), std::ios::binary | std::ios::trunc);
                std::ofstream preview(capture.directory / (stem + ".pgm"), std::ios::binary | std::ios::trunc);
                if (preview) preview << "P5\n" << id.width << ' ' << id.height << "\n255\n";
                for (uint32_t y = 0; y < id.height; ++y) {
                    const char* row = reinterpret_cast<const char*>(mapped + size_t(y) * capture.rowPitch);
                    if (raw) raw.write(row, id.width);
                    if (preview) preview.write(row, id.width);
                }
                capture.buffer->unmap();
                raw.close(); preview.close();
                std::ofstream metadata(capture.directory / (stem + ".json"), std::ios::trunc);
                if (metadata) metadata << "{\"coverage\":\"PartialCoverage\",\"render_frame\":" << id.renderFrame
                    << ",\"geometry_epoch\":" << id.geometryEpoch
                    << ",\"color_allocation\":" << id.colorAllocation
                    << ",\"depth_allocation\":" << id.depthAllocation
                    << ",\"resolve_write_ordinal\":null,\"width\":" << id.width
                    << ",\"height\":" << id.height
                    << ",\"viewport_bits\":[" << id.viewportBits[0] << ',' << id.viewportBits[1]
                    << ',' << id.viewportBits[2] << ',' << id.viewportBits[3]
                    << "],\"jitter_bits\":[" << id.jitterBits[0] << ',' << id.jitterBits[1]
                    << "],\"audited_draws\":" << capture.view.auditedDraws
                    << ",\"submission_serial\":" << capture.submissionSerial << "}\n";
                if (!raw.fail() && !preview.fail() && metadata && !metadata.fail())
                    LOG_INFO("fsr alpha: captured PartialCoverage frame={} draws={} {}x{} path={}",
                        id.renderFrame, capture.view.auditedDraws, id.width, id.height, stem);
                else LOG_WARNING("fsr alpha: capture export failed frame={}", id.renderFrame);
            }

            void ExportFsrAlphaEquality(const FsrAlphaEquality& capture)
            {
                if (!capture.submissionSerial) return;
                std::error_code ec;
                std::filesystem::create_directories(capture.directory, ec);
                if (ec) { LOG_WARNING("fsr alpha: equality directory failed: {}", ec.message()); return; }
                const auto stem = fmt::format("fsr-alpha-equality-f{}-draw{}", capture.frame, capture.drawOrdinal);
                std::string reason = capture.reason;
                if (reason.empty() && !capture.replayRecorded) reason = "replay_not_recorded";
                std::array<const uint8_t*, 4> mapped{};
                if (reason.empty()) {
                    for (size_t i = 0; i < mapped.size(); ++i) {
                        mapped[i] = static_cast<const uint8_t*>(capture.buffers[i]->map());
                        if (!mapped[i]) { reason = "readback_map_failed"; break; }
                    }
                }
                uint64_t colorMismatch = 0, depthMismatch = 0;
                if (reason.empty()) {
                    const auto comparePlane = [&](size_t first, size_t second, uint32_t bpp, uint32_t pitch,
                        uint64_t& mismatch, const char* firstName, const char* secondName) {
                        std::ofstream before(capture.directory / (stem + firstName), std::ios::binary | std::ios::trunc);
                        std::ofstream after(capture.directory / (stem + secondName), std::ios::binary | std::ios::trunc);
                        for (uint32_t y = 0; y < capture.height; ++y) {
                            const auto* a = mapped[first] + size_t(y) * pitch;
                            const auto* b = mapped[second] + size_t(y) * pitch;
                            for (uint32_t x = 0; x < capture.width; ++x)
                                mismatch += std::memcmp(a + size_t(x) * bpp, b + size_t(x) * bpp, bpp) != 0;
                            if (before) before.write(reinterpret_cast<const char*>(a), size_t(capture.width) * bpp);
                            if (after) after.write(reinterpret_cast<const char*>(b), size_t(capture.width) * bpp);
                        }
                        before.close(); after.close();
                        if (before.fail() || after.fail()) reason = "readback_export_failed";
                    };
                    comparePlane(0, 1, capture.colorBpp, capture.colorPitch, colorMismatch,
                        "-color-before.bin", "-color-after.bin");
                    comparePlane(2, 3, 4, capture.depthPitch, depthMismatch,
                        "-depth-before.r32.bin", "-depth-after.r32.bin");
                }
                for (size_t i = 0; i < mapped.size(); ++i)
                    if (mapped[i]) capture.buffers[i]->unmap();
                std::ofstream metadata(capture.directory / (stem + ".json"), std::ios::trunc);
                if (metadata) {
                    const char* format = capture.colorFormat == RenderFormat::R8G8B8A8_UNORM ? "R8G8B8A8_UNORM" :
                        capture.colorFormat == RenderFormat::R16G16B16A16_FLOAT ? "R16G16B16A16_FLOAT" : "unsupported";
                    const char* depthFormat = capture.depthFormat == RenderFormat::D32_FLOAT_S8_UINT ?
                        "D32_FLOAT_S8_UINT" : "unsupported";
                    metadata << "{\"status\":\"" << (reason.empty() ? "complete" : reason)
                        << "\",\"render_frame\":" << capture.frame
                        << ",\"draw_ordinal\":" << capture.drawOrdinal
                        << ",\"color_allocation\":" << capture.colorAllocation
                        << ",\"depth_allocation\":" << capture.depthAllocation
                        << ",\"color_format\":\"" << format
                        << "\",\"color_format_value\":" << uint32_t(capture.colorFormat)
                        << ",\"depth_format\":\"" << depthFormat
                        << "\",\"depth_format_value\":" << uint32_t(capture.depthFormat)
                        << ",\"depth_copy_format\":\"R32_FLOAT_plane_only\""
                        << ",\"width\":" << capture.width << ",\"height\":" << capture.height
                        << ",\"color_bytes_compared\":" << (reason.empty() ? uint64_t(capture.width) * capture.height * capture.colorBpp : 0)
                        << ",\"depth_bytes_compared\":" << (reason.empty() ? uint64_t(capture.width) * capture.height * 4 : 0)
                        << ",\"color_mismatch_pixels\":" << colorMismatch
                        << ",\"depth_mismatch_pixels\":" << depthMismatch
                        << ",\"color_equal\":" << (reason.empty() && !colorMismatch ? "true" : "false")
                        << ",\"depth_equal\":" << (reason.empty() && !depthMismatch ? "true" : "false")
                        << ",\"replay_recorded\":" << (capture.replayRecorded ? "true" : "false")
                        << ",\"submission_serial\":" << capture.submissionSerial << "}\n";
                }
                LOG_INFO("fsr alpha: same-draw equality frame={} draw={} status={} color_mismatch={} depth_mismatch={}",
                    capture.frame, capture.drawOrdinal, reason.empty() ? "complete" : reason,
                    colorMismatch, depthMismatch);
            }
            void ExportFsrAlphaBridgeDiagnostic(const FsrAlphaBridgeDiagnostic& event)
            {
                if (!event.submissionSerial) return;
                std::error_code ec;
                std::filesystem::create_directories(fsrAlphaCaptureDir, ec);
                if (ec) { LOG_WARNING("fsr alpha bridge: capture directory failed: {}", ec.message()); return; }
                std::string pairResult;
                if (event.buffers[0] && event.buffers[1]) {
                    auto* raw = static_cast<const uint8_t*>(event.buffers[0]->map());
                    auto* resolved = static_cast<const uint8_t*>(event.buffers[1]->map());
                    if (raw && resolved) {
                        const auto stem = fmt::format("fsr-alpha-bridge-f{}-e{}-ord{}",
                            event.frame, event.eventIndex, event.writeOrdinal);
                        std::ofstream rawFile(fsrAlphaCaptureDir / (stem + "-raw-at-resolve.r8.bin"),
                            std::ios::binary | std::ios::trunc);
                        std::ofstream resolvedFile(fsrAlphaCaptureDir / (stem + "-resolved.r8.bin"),
                            std::ios::binary | std::ios::trunc);
                        for (uint32_t y = 0; y < event.rawHeight; ++y)
                            if (rawFile) rawFile.write(reinterpret_cast<const char*>(raw + size_t(y) * event.rawPitch), event.rawWidth);
                        for (uint32_t y = 0; y < event.resolvedHeight; ++y)
                            if (resolvedFile) resolvedFile.write(reinterpret_cast<const char*>(resolved + size_t(y) * event.resolvedPitch), event.resolvedWidth);
                        rawFile.close(); resolvedFile.close();
                        uint64_t different = 0, nonzero = 0;
                        const auto rect = event.rect;
                        for (uint32_t y = rect.y; y < rect.y + rect.height; ++y)
                            for (uint32_t x = rect.x; x < rect.x + rect.width; ++x) {
                                const uint8_t value = resolved[size_t(y) * event.resolvedPitch + x];
                                different += raw[size_t(y) * event.rawPitch + x] != value;
                                nonzero += value != 0;
                            }
                        pairResult = fmt::format(
                            ",\"pair_capture\":\"{}\",\"compared_pixels\":{},"
                            "\"mismatch_pixels\":{},\"resolved_nonzero_pixels\":{},"
                            "\"raw_pitch\":{},\"resolved_pitch\":{}",
                            rawFile.fail() || resolvedFile.fail() ? "file_write_failed" : "complete",
                            uint64_t(rect.width) * rect.height, different, nonzero,
                            event.rawPitch, event.resolvedPitch);
                    } else pairResult = ",\"pair_capture\":\"map_failed\"";
                    if (raw) event.buffers[0]->unmap();
                    if (resolved) event.buffers[1]->unmap();
                }
                std::string snapshotsResult;
                if (!event.snapshots.empty()) {
                    snapshotsResult = ",\"snapshots\":[";
                    bool first = true;
                    for (const auto& snapshot : event.snapshots) {
                        if (!first) snapshotsResult += ',';
                        first = false;
                        const auto filename = fmt::format("fsr-postprocess-f{}-e{}-draw{}-{}.bin",
                            event.frame, event.eventIndex, event.writeOrdinal, snapshot.label);
                        auto* data = static_cast<const uint8_t*>(snapshot.buffer->map());
                        bool written = false;
                        if (data) {
                            std::ofstream output(fsrAlphaCaptureDir / filename,
                                std::ios::binary | std::ios::trunc);
                            for (uint32_t y = 0; y < snapshot.height && output; ++y)
                                output.write(reinterpret_cast<const char*>(data + size_t(y) * snapshot.rowPitch),
                                    size_t(snapshot.width) * snapshot.bytesPerPixel);
                            output.close(); written = !output.fail();
                            snapshot.buffer->unmap();
                        }
                        snapshotsResult += fmt::format(
                            "{{\"label\":\"{}\",\"status\":\"{}\",\"file\":\"{}\","
                            "\"format\":{},\"extent\":[{},{}],\"bytes_per_pixel\":{},"
                            "\"row_pitch\":{},\"gpu_row_pitch\":{}}}",
                            snapshot.label, written ? "complete" : data ? "write_failed" : "map_failed",
                            filename, uint32_t(snapshot.format), snapshot.width, snapshot.height,
                            snapshot.bytesPerPixel, snapshot.width * snapshot.bytesPerPixel,
                            snapshot.rowPitch);
                    }
                    snapshotsResult += ']';
                }
                if (!event.captureFailures.empty()) {
                    snapshotsResult += ",\"capture_failures\":[";
                    for (size_t i = 0; i < event.captureFailures.size(); ++i) {
                        if (i) snapshotsResult += ',';
                        snapshotsResult += fmt::format("\"{}\"", event.captureFailures[i]);
                    }
                    snapshotsResult += ']';
                }
                std::ofstream trace(fsrAlphaCaptureDir /
                    fmt::format("fsr-alpha-bridge-f{}.jsonl", event.frame), std::ios::app);
                if (trace) trace << '{' << event.fields << pairResult << snapshotsResult
                    << ",\"render_frame\":" << event.frame
                    << ",\"event_index\":" << event.eventIndex
                    << ",\"submission_serial\":" << event.submissionSerial << "}\n";
            }
#endif
            bool RecycleSlot(uint32_t i)
            {
                auto& s = gpuSlots[i];
                if (!s.submitted)
                    return true;
                {
                    ScopedTimer timer{ tFlush, cpuTimingEnabled };
                    if (!video::WaitForGpuFence(s.fence.get())) return false;
                }
                s.submitted = false;
                for (const auto& e : s.evaluateCaptures) if (e->checkedSubmit) e->completed = true;
                s.evaluateCaptures.clear();
                if (s.timingQueries) {
                    s.timingQueries->queryResults();
                    const auto* results = s.timingQueries->getResults();
                    if (results) gpuTiming.AddBatch(results[0], results[1]);
                    else gpuTiming.AddUnavailableBatch();
                } else if (render_timing::Enabled()) gpuTiming.AddUnavailableBatch();
                s.drawProbe.ReadCompleted();
#if defined(LO_GPU_PLUME)
                for (size_t captureIndex = 0; captureIndex < s.fsrAlphaCaptures.size(); ++captureIndex)
                    ExportFsrAlphaCapture(*s.fsrAlphaCaptures[captureIndex], captureIndex);
                s.fsrAlphaCaptures.clear();
                for (const auto& capture : s.fsrAlphaEqualities)
                    ExportFsrAlphaEquality(*capture);
                s.fsrAlphaEqualities.clear();
                for (const auto& event : s.fsrAlphaBridgeDiagnostics)
                    ExportFsrAlphaBridgeDiagnostic(*event);
                s.fsrAlphaBridgeDiagnostics.clear();
                s.fsrAlphaBridgeUses.clear();
                // Release read-only depth framebuffers before retired guest depth.
                s.fsrAlphaBatches.clear();
#endif
                if (motionReplay) {
                    for (const auto& texture : s.retiredTextures)
                        motionReplay->ReleaseDepthAfterGpuCompletion(texture->texture.get());
                    motionReplay->ReleaseCompletedThrough(s.motionSerial);
                }
                for (const auto& texture : s.retiredTextures)
                    for (auto fb = framebuffers.begin(); fb != framebuffers.end();)
                        if (fb->first.first == texture->texture.get() || fb->first.second == texture->texture.get())
                            fb = framebuffers.erase(fb);
                        else
                            ++fb;
                for (auto& cache : s.textureSetCache) cache.Clear();
                s.retiredTextures.clear();
                s.bloomPrefilterUsed = 0;
                if(temporalHistory)temporalHistory->ReleaseCompletedThrough(s.temporalSerial);
                else if(sparseCollector)sparseCollector->ReleaseCompleted();
                if(hdrTemporalHistory)hdrTemporalHistory->ReleaseCompletedThrough(s.hdrTemporalSerial);
#if defined(LO_GPU_PLUME)
#if defined(LO_RENDERER_P2_EMBEDDED_TEST)
                if (dlssController && s.srSubmissionSerial)
                    dlssController->ReleaseCompletedThrough(s.srSubmissionSerial);
#else
                if (temporalUpscaler && s.srSubmissionSerial)
                    temporalUpscaler->ReleaseCompleted(s.srSubmissionSerial);
#endif
#endif
                s.srUseId = {};
                s.srSubmissionSerial = 0;
                s.srPrefixClosed = s.srIsolatedAccepted = s.srContinuationOpen = false;
                s.dlssSubmit = {};
                sceneAABusy=false;
                s.uploadOffset = 0;
                for (auto& used : s.setPoolUsed)
                    used = 0;
                uploadedConstants[i] = {};
                return true;
            }

            bool WaitForGpu()
            {
                for (uint32_t i = 0; i < kGpuSlots; ++i)
                    if (!RecycleSlot(i)) return false;
                return true;
            }

            bool WaitForReadback()
            {
                // Call only after Flush. Its outgoing slot is the newest queue
                // submission; waiting it first covers earlier copies before any
                // RecycleSlot releases shared temporal descriptor resources.
                return !video::GpuWorkStopped() &&
                    RecycleSlot((gpuSlot + kGpuSlots - 1) % kGpuSlots) && WaitForGpu();
            }

            bool Begin()
            {
                if (video::GpuWorkStopped() || !RecycleSlot(gpuSlot)) return false;
                BindGpuSlot();
                if (!listOpen)
                {
                    consecutiveResolveCopies.Invalidate();
                    if (render_timing::Enabled() && !timingInitialized) {
                        timingInitialized = true;
                    }
                    if (render_timing::Enabled() && !Gpu().timingQueries) {
                        Gpu().timingQueries = device->createQueryPool(2);
                        if (Gpu().timingQueries && Gpu().timingQueries->getCount() != 2) Gpu().timingQueries.reset();
                        if (!Gpu().timingQueries) LOG_WARNING("render timing: GPU timestamp queries unavailable");
                        timingQueries = Gpu().timingQueries.get();
                    }
                    if (!video::BeginGpuCommands(commandList)) return false;
                    Gpu().drawProbe.Begin(device, commandList, frame);
                    if (timingQueries) {
                        commandList->resetQueryPool(timingQueries, 0, 2);
                        commandList->writeTimestamp(timingQueries, 0);
                    }
                    listOpen = true;
                }
                return true;
            }

            bool Flush()
            {
                consecutiveResolveCopies.Invalidate();
                if (video::GpuWorkStopped()) {
#if defined(LO_GPU_PLUME)
                    if (!Gpu().submitted) {
                        Gpu().fsrAlphaCaptures.clear();
                        Gpu().fsrAlphaEqualities.clear();
                        Gpu().fsrAlphaBridgeDiagnostics.clear();
                        Gpu().fsrAlphaBridgeUses.clear();
                        if (fsrAlphaBridge) fsrAlphaBridge->DiscardUnsubmitted();
                        Gpu().fsrAlphaBatches.clear();
                    }
#endif
                    if (!Gpu().submitted) for (const auto& e : Gpu().evaluateCaptures)
                        e->reason = "gpu_stopped_before_submit";
#if defined(LO_RENDERER_P2_EMBEDDED_TEST)
                    if (dlssController && Gpu().srUseId && !Gpu().submitted)
                        dlssController->OnBatchDiscarded(Gpu().srUseId);
#else
                    if (temporalUpscaler && Gpu().srUseId && !Gpu().submitted)
                        temporalUpscaler->OnDiscarded(Gpu().srUseId);
#endif
                    if (!Gpu().submitted) Gpu().srUseId = {};
                    ClearDlssSubmit(Gpu(), true);
                    listOpen = false;
                    return false;
                }
                if (!listOpen) return true;
                if (motionReplay) motionReplay->SealTimings(commandList);
                if (!Gpu().srPrefixClosed) Gpu().drawProbe.End(commandList);
                if (timingQueries) commandList->writeTimestamp(timingQueries, 1);
                if (!video::EndGpuCommands(commandList)) {
#if defined(LO_GPU_PLUME)
                    Gpu().fsrAlphaCaptures.clear();
                    Gpu().fsrAlphaEqualities.clear();
                    Gpu().fsrAlphaBridgeDiagnostics.clear();
                    Gpu().fsrAlphaBridgeUses.clear();
                    if (fsrAlphaBridge) fsrAlphaBridge->DiscardUnsubmitted();
                    Gpu().fsrAlphaBatches.clear();
#endif
                    for (const auto& e : Gpu().evaluateCaptures) e->reason = "continuation_end_failed";
#if defined(LO_RENDERER_P2_EMBEDDED_TEST)
                    if (dlssController && Gpu().srUseId) dlssController->OnBatchDiscarded(Gpu().srUseId);
#else
                    if (temporalUpscaler && Gpu().srUseId) temporalUpscaler->OnDiscarded(Gpu().srUseId);
#endif
                    Gpu().srUseId = {};
                    ClearDlssSubmit(Gpu(), true);
                    listOpen = false;
                    return false;
                }
                listOpen = false;
                const RenderCommandList* lists[3] = { Gpu().list.get(), nullptr, nullptr };
                uint32_t listCount = 1;
                if (Gpu().srPrefixClosed) {
                    if (Gpu().srIsolatedAccepted) lists[listCount++] = Gpu().srIsolated.get();
                    if (Gpu().srContinuationOpen) lists[listCount++] = Gpu().srContinuation.get();
                }
                Gpu().temporalSerial = temporalHistory ? temporalHistory->RecordedSerial() : 0;
                Gpu().hdrTemporalSerial = hdrTemporalHistory ? hdrTemporalHistory->RecordedSerial() : 0;
                Gpu().motionSerial = motionReplay ? motionReplay->RecordedSerial() : 0;
                bool submitted = true;
#if defined(LO_GPU_PLUME)
                int32_t rawVkResult = 0;
                uint64_t submissionSerial = 0;
                if (vulkan)
                    submitted = video::SubmitRendererBatch(lists, listCount, fence, submissionSerial, rawVkResult);
                else
#endif
                    queue->executeCommandLists(lists, listCount, nullptr, 0, nullptr, 0, fence);
                if (!submitted) {
#if defined(LO_GPU_PLUME)
                    Gpu().fsrAlphaCaptures.clear();
                    Gpu().fsrAlphaEqualities.clear();
                    Gpu().fsrAlphaBridgeDiagnostics.clear();
                    Gpu().fsrAlphaBridgeUses.clear();
                    if (fsrAlphaBridge) fsrAlphaBridge->DiscardUnsubmitted();
                    Gpu().fsrAlphaBatches.clear();
#endif
                    for (const auto& e : Gpu().evaluateCaptures) e->reason = "checked_submit_failed";
#if defined(LO_GPU_PLUME)
#if defined(LO_RENDERER_P2_EMBEDDED_TEST)
                    if (dlssController && Gpu().srUseId) dlssController->OnBatchDiscarded(Gpu().srUseId);
#else
                    if (temporalUpscaler && Gpu().srUseId) temporalUpscaler->OnDiscarded(Gpu().srUseId);
#endif
                    Gpu().srUseId = {};
                    video::StopGpuWork(rawVkResult);
                    LOG_ERROR("renderer: Vulkan batch submit failed raw_vk={}; no fallback submission issued", rawVkResult);
#endif
                    ClearDlssSubmit(Gpu(), true);
                    return false;
                }
#if defined(LO_GPU_PLUME)
                for (const auto& use : Gpu().fsrAlphaBatches)
                    use->submissionSerial = submissionSerial;
                for (const auto& capture : Gpu().fsrAlphaCaptures)
                    capture->submissionSerial = submissionSerial;
                for (const auto& capture : Gpu().fsrAlphaEqualities)
                    capture->submissionSerial = submissionSerial;
                for (const auto& event : Gpu().fsrAlphaBridgeDiagnostics)
                    event->submissionSerial = submissionSerial;
#if defined(LO_RENDERER_P2_EMBEDDED_TEST)
                if (dlssController && Gpu().srUseId) {
                    Gpu().srSubmissionSerial = submissionSerial;
                    dlssController->OnBatchSubmitted(Gpu().srUseId, submissionSerial);
                }
#else
                if (temporalUpscaler && Gpu().srUseId) {
                    Gpu().srSubmissionSerial = submissionSerial;
                    temporalUpscaler->OnSubmitted(Gpu().srUseId, submissionSerial);
                }
#endif
                const bool srBatch = Gpu().srPrefixClosed && Gpu().srIsolatedAccepted && Gpu().srContinuationOpen;
                for (const auto& entry : Gpu().evaluateCaptures) {
                    auto& e = *entry;
                    e.isolatedIncluded = srBatch && e.isolatedAccepted;
                    e.checkedSubmit = vulkan && e.isolatedIncluded && submissionSerial != 0;
                    if (e.checkedSubmit) e.submissionSerial = submissionSerial;
                    else if (e.reason.empty()) e.reason = "discarded_isolated_list";
                }
                if (vulkan && srBatch && Gpu().dlssSubmit.pending && submissionSerial)
                    NoteDlssSubmitted(Gpu().dlssSubmit.plan, Gpu().dlssSubmit.renderFrame, submissionSerial);
#endif
                ClearDlssSubmit(Gpu(), Gpu().dlssSubmit.pending &&
                    !(dlssFrame.submitted && dlssFrame.submittedFrame == Gpu().dlssSubmit.renderFrame));
                Gpu().submitted = true;
                gpuSlot = (gpuSlot + 1) % kGpuSlots;
                BindGpuSlot();
                return true;
            }

            // Returns an offset into the upload ring or UINT64_MAX when full.
            uint64_t Upload(const void* data, size_t size, uint32_t alignment = 256)
            {
                if (video::GpuWorkStopped()) return UINT64_MAX;
                uint64_t offset = (Gpu().uploadOffset + alignment - 1) & ~uint64_t(alignment - 1);
                if (offset + size > kUploadRingSize)
                {
                    // Out of space mid-frame: finish what we have and start over.
                    if (!Flush() || !Begin()) return UINT64_MAX;
                    offset = 0;
                    if (size > kUploadRingSize)
                        return UINT64_MAX;
                }
                if (data)
                    memcpy(uploadMapped + offset, data, size);
                Gpu().uploadOffset = offset + size;
                return offset;
            }

            // Three 32-slot texture banks per draw. Unused banks reuse one dummy
            // set; unique 2D combinations consume descriptorBatchLimit.

            void SetConstantBuffer(uint64_t offset, uint32_t index)
            {
                if(vulkan) {
                    constantAddresses[index]=uploadRing->getDeviceAddress()+offset;
                    commandList->setGraphicsPushConstants(0,constantAddresses);
                } else commandList->setGraphicsRootDescriptor(RenderBufferReference(uploadRing,offset),index);
            }

            RenderDescriptorSet* AcquireSet(int which)
            {
                auto& pool = Gpu().setPools[which];
                uint32_t& used = Gpu().setPoolUsed[which];
                if (used >= pool.size())
                {
                    pool.push_back(setBuilders[which].create(device));
                    RenderDescriptorSet* set = pool.back().get();
                    {
                        HostTexture& dummy = which == 1 ? dummyTexture2D : which == 2 ? dummyTexture3D : dummyTextureCube;
                        for (uint32_t i = 0; i < kTextureSlots; i++)
                            set->setTexture(i, dummy.texture.get(), RenderTextureLayout::SHADER_READ);
                    }
                }
                return pool[used++].get();
            }

            RenderDescriptorSet* AcquireTextureSet(int which, const TextureSetCache::Key& textures, uint32_t activeSlots)
            {
                if (activeSlots == 0)
                    return staticDummySets[which - 1].get();
                // Complete immutable sets are reusable on both backends. The
                // cache and pool belong to the same completed-fence interval.
                const bool reuse = descriptorReuse;
                auto create = [&]() {
                    auto* set = AcquireSet(which);
                    for (uint32_t slot = 0; slot < kTextureSlots; ++slot)
                        if (reuse || ((activeSlots >> slot) & 1))
                            set->setTexture(slot, textures[slot], RenderTextureLayout::SHADER_READ);
                    return set;
                };
                if (!reuse) return create();
                bool reused = false;
                auto* set = Gpu().textureSetCache[which - 1].Acquire(textures, create, reused);
                if (cpuTimingEnabled) {
                    descriptorHits += reused;
                    descriptorMisses += !reused;
                }
                return set;
            }

            void Transition(HostTexture& tex, RenderTextureLayout layout, RenderBarrierStages stages)
            {
                if (tex.layout == layout)
                    return;
                commandList->barriers(stages, RenderTextureBarrier(tex.texture.get(), layout));
                tex.layout = layout;
            }

            // ---- samplers ---------------------------------------------------------
            uint32_t GetSamplerIndex(uint64_t key)
            {
                auto it = samplerPalette.find(key);
                if (it != samplerPalette.end())
                    return it->second;
                if (samplerPalette.size() >= kSamplerPalette)
                {
                    static bool reported = false;
                    if (!reported)
                    {
                        reported = true;
                        LOG_WARNING("renderer: sampler palette exhausted: key={:#x}, capacity={}; using slot 0", key, kSamplerPalette);
                    }
                    return 0;
                }
                uint32_t index = uint32_t(samplerPalette.size());
                samplerPalette.emplace(key, index);
                (vulkan?staticSamplerSet.get():staticSet0.get())->setSampler(samplerDescriptorBase + index, GetSampler(key));
                if (getenv("LO_TRACE_SAMPLERS"))
                    LOG_INFO("renderer: sampler palette slot={} key={:#x}", index, key);
                return index;
            }

            RenderSampler* GetSampler(uint64_t key)
            {
                auto it = samplers.find(key);
                if (it != samplers.end())
                    return it->second.get();

                // key: bits 0-1 mag, 2-3 min, 4-5 mip, 6-8 clampX, 9-11 clampY, 12-14 clampZ, 15 anisotropic
                RenderSamplerDesc desc;
                auto filter = [](uint32_t f) { return f == 0 ? RenderFilter::NEAREST : RenderFilter::LINEAR; };
                desc.magFilter = filter(key & 3);
                desc.minFilter = filter((key >> 2) & 3);
                desc.mipmapMode = ((key >> 4) & 3) == 0 ? RenderMipmapMode::NEAREST : RenderMipmapMode::LINEAR;
                auto clamp = [](uint32_t c)
                {
                    switch (c)
                    {
                    case 0: return RenderTextureAddressMode::WRAP;
                    case 1: return RenderTextureAddressMode::MIRROR;
                    case 3: case 5: return RenderTextureAddressMode::MIRROR_ONCE;
                    case 6: case 7: return RenderTextureAddressMode::BORDER;
                    default: return RenderTextureAddressMode::CLAMP;
                    }
                };
                desc.addressU = clamp((key >> 6) & 7);
                desc.addressV = clamp((key >> 9) & 7);
                desc.addressW = clamp((key >> 12) & 7);
                desc.anisotropyEnabled = ((key >> 15) & 1) != 0;
                desc.maxAnisotropy = 4;
                auto sampler = device->createSampler(desc);
                RenderSampler* result = sampler.get();
                samplers.emplace(key, std::move(sampler));
                return result;
            }

            // ---- shaders ------------------------------------------------------------
            static bool ValidPipelineRecipe(const PipelineKey& key)
            {
                // Only host formats emitted by GetRenderTarget are accepted.
                const auto color = static_cast<RenderFormat>(key.rtFormat);
                const auto depth = static_cast<RenderFormat>(key.depthFormat);
                const bool colorValid = color == RenderFormat::UNKNOWN || color == RenderFormat::R8G8B8A8_UNORM ||
                    color == RenderFormat::R16G16_FLOAT || color == RenderFormat::R16G16B16A16_FLOAT ||
                    color == RenderFormat::R32_FLOAT || color == RenderFormat::R32G32_FLOAT;
                return key.vs != 0 && key.prim <= 32 && colorValid &&
                    (depth == RenderFormat::UNKNOWN || depth == RenderFormat::D32_FLOAT_S8_UINT);
            }

            static void CheckPreparationCancel()
            {
                video::PumpEvents();
                if (video::ShaderPreparationSkipped()) throw xenos::preparation::Cancelled{};
            }

            void PrepareKnownPipelines()
            {
                pipelineCacheEnabled = !shaderCacheDir.empty() && !getenv("LO_NO_PIPELINE_CACHE");
                if (!pipelineCacheEnabled) return;
                const auto path = std::filesystem::path(shaderCacheDir) / (vulkan ? "pipelines_vk12_1.bin" : "pipelines.bin");
                const auto loaded = gpu::pipeline_cache::Load(path, xenos::cache::Version, kPipelineRecipeVersion, ValidPipelineRecipe);
                if (!loaded.error.empty()) LOG_WARNING("renderer: ignoring pipeline recipes: {}", loaded.error);
                for (const auto& key : loaded.keys) pipelineRecipes.insert(key);
                // Learning remains enabled when eager creation is skipped.
                if (getenv("LO_NO_PIPELINE_PREPARE") || getenv("LO_NO_SHADER_PREPARE") ||
                    settings::GetConfig().skipShaderPrebuild || video::ShaderPreparationSkipped()) return;
                struct Job { PipelineKey key; Shader* vs; Shader* ps; std::unique_ptr<RenderPipeline> pipeline; };
                std::vector<Job> jobs;
                size_t missingShaders = 0, failed = 0;
                const auto started = std::chrono::steady_clock::now();
                try {
                    if (portableShaderPack) for (const auto& key : loaded.keys) {
                        CheckPreparationCancel();
                        TryLoadPortableShader(false, key.vs);
                        if (key.ps) TryLoadPortableShader(true, key.ps);
                    }
                    for (const auto& key : loaded.keys) {
                        CheckPreparationCancel();
                        const auto vs = shaders[0].find(key.vs), ps = shaders[1].find(key.ps);
                        if (vs == shaders[0].end() || !vs->second.valid ||
                            (key.ps && (ps == shaders[1].end() || !ps->second.valid))) { ++missingShaders; continue; }
                        jobs.push_back({key, &vs->second, key.ps ? &ps->second : nullptr, {}});
                    }
                    const unsigned logical = std::thread::hardware_concurrency();
                    const size_t count = xenos::preparation::WorkerCount(logical, jobs.size(),
                        getenv("LO_PIPELINE_PREPARE_SERIAL") != nullptr,
                        unsigned(xenos::preparation::HostWorkerCap(logical)), "LO_PIPELINE_WORKERS");
                    size_t done = 0;
                    const auto stats = xenos::preparation::RunBounded<size_t>(jobs.size(), count,
                        std::max<size_t>(1, count * 2), [&](size_t i) {
                            auto& job = jobs[i];
                            try { job.pipeline = CreatePipeline(job.key, job.vs, job.ps, false); }
                            catch (const std::exception& e) { LOG_WARNING("renderer: pipeline precreation: {}", e.what()); }
                            return i;
                        }, [&](size_t i) {
                            auto& job = jobs[i];
                            if (!job.pipeline) ++failed;
                            else { preparedPipelineKeys.insert(job.key); pipelines.emplace(job.key, std::move(job.pipeline)); }
                            video::SetShaderPreparationProgress(uint32_t(++done), uint32_t(jobs.size()),
                                video::PreparationStage::Pipelines, video::PreparationUnit::Pipelines);
                            return !video::ShaderPreparationSkipped();
                        }, [] { video::PumpEvents(); return !video::ShaderPreparationSkipped(); });
                    LOG_INFO("renderer: pipeline preparation: {} recipes, {} ready, {} missing shaders, {} failed, {} workers, {:.0f} ms",
                        loaded.keys.size(), preparedPipelineKeys.size(), missingShaders, failed, stats.startedWorkers,
                        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-started).count());
                } catch (const xenos::preparation::Cancelled&) {
                    LOG_INFO("renderer: pipeline preparation skipped by user request");
                } catch (const std::exception& e) {
                    LOG_WARNING("renderer: pipeline preparation stopped: {}", e.what());
                }
                video::SetShaderPreparationProgress(0, 0);
            }

            void SavePipelineRecipes(bool force = false)
            {
                if (!pipelineCacheEnabled || (!force && frame % 60)) return;
                if (pipelineWrite.valid()) {
                    if (!force && pipelineWrite.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
                    try {
                        const auto result = pipelineWrite.get();
                        if (!result.ok) {
                            LOG_WARNING("renderer: pipeline recipe write failed: {}", result.error);
                            pipelineRecipesDirty = true;
                        }
                    } catch (const std::exception& e) {
                        LOG_WARNING("renderer: pipeline recipe writer: {}", e.what());
                        pipelineRecipesDirty = true;
                    }
                }
                if (!pipelineRecipesDirty) return;
                try {
                    std::vector<PipelineKey> snapshot(pipelineRecipes.begin(), pipelineRecipes.end());
                    const auto path = std::filesystem::path(shaderCacheDir) / (vulkan ? "pipelines_vk12_1.bin" : "pipelines.bin");
                    pipelineWrite = std::async(std::launch::async, [path, snapshot = std::move(snapshot)] {
                        return gpu::pipeline_cache::Write(path, snapshot, xenos::cache::Version, kPipelineRecipeVersion, ValidPipelineRecipe);
                    });
                    pipelineRecipesDirty = false;
                } catch (const std::exception& e) { LOG_WARNING("renderer: pipeline recipe writer: {}", e.what()); }
            }

            #include "shader/portable_shader_pack_renderer.inl"

            void PrepareKnownShaders()
            {
                video::ResetShaderPreparationSkip();
                try {
                const auto xex = std::span<const uint8_t>(static_cast<const uint8_t*>(g_memory.Translate(xenos::portable_pack::RuntimeXexAddress)), xenos::portable_pack::RuntimeXexBytes);
                // A distribution pack is independent of the writable local cache and local DXC identity.
                if (TryOpenPortableShaderPack(xex)) { ResetTimers(); return; }
                if (shaderCacheDir.empty() || getenv("LO_NO_SHADER_PREPARE") || settings::GetConfig().skipShaderPrebuild) {
                    LOG_INFO("renderer: shader preparation skipped by configuration or environment");
                    return;
                }
                namespace startup = xenos::startup_cache;
                const auto wholeStarted = std::chrono::steady_clock::now();
                const auto& compilerIdentity = xenos::DxcIdentity();
                const bool retryFailures = getenv("LO_SHADER_RETRY_FAILURES") != nullptr;
                const auto bundlePath = std::filesystem::path(shaderCacheDir) /
                    (vulkan ? "startup_vk12_v1.bundle" : "startup_dxil_v1.bundle");
                // Trust imported assets, not a cache file's self-reported identity.
                // This hashes the already-loaded XEX and current compiler contract;
                // no game/source/cache directory walk is needed for a warm start.
                const auto bundleIdentity = compilerIdentity.empty() ? std::string{} :
                    startup::RuntimeIdentity(FileSystem::GetGameRoot(), cacheIdentity, xex);
                const bool bundleEnabled = !compilerIdentity.empty();
                const bool reuseBundle = bundleEnabled && !getenv("LO_SHADER_FULL_SCAN") &&
                    !getenv("LO_SHADER_HLSL_DIR") && !retryFailures;
                LOG_INFO("renderer: shader startup cache: {}, compiler identity {}", bundlePath.string(),
                    compilerIdentity.empty() ? "unavailable (persistent reuse disabled)" : compilerIdentity);
                BeginPortableShaderExport(xex);
                if (reuseBundle) {
                    uint32_t modules = 0, cachedFailures = 0;
                    double moduleMs = 0;
                    try {
                        auto loaded = startup::LoadTransactional(bundlePath, bundleIdentity, cacheIdentity, [&](startup::Record&& record) {
                            ExportPortableShader(record.hash, record.info, record.binary, record.failure, true);
                            std::string{}.swap(record.info.hlsl); // Export counts source text, but the renderer does not retain it.
                            auto& entry = shaders[record.info.isPixelShader ? 1 : 0][record.hash];
                            entry.info = std::move(record.info);
                            if (!record.failure.empty()) {
                                entry.retry.Failed(true);
                                ++cachedFailures;
                                LOG_WARNING("renderer: cached compiler failure {}_{:016x}: {} (full diagnostic retained in startup cache)",
                                    entry.info.isPixelShader ? "ps" : "vs", record.hash,
                                    record.failure.substr(0, record.failure.find('\n')));
                                return;
                            }
                            const auto begin = std::chrono::steady_clock::now();
                            entry.shader = device->createShader(record.binary.data(), record.binary.size(), "main", renderFormat);
                            moduleMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-begin).count();
                            entry.valid = entry.shader != nullptr;
                            if (!entry.valid) throw std::runtime_error("cached shader device module creation failed");
                            ++modules;
                        }, [&] { shaders[0].clear(); shaders[1].clear(); }, [] {},
                        [] { CheckPreparationCancel(); }, [](uint32_t done, uint32_t total) {
                            video::SetShaderPreparationProgress(done, total, video::PreparationStage::CachedShaders,
                                video::PreparationUnit::Shaders);
                        }, PortableExportRequested()); // Reconstruct one record at a time only for export size accounting.
                        video::SetShaderPreparationProgress(0, 0);
                        if (video::ShaderPreparationSkipped()) throw xenos::preparation::Cancelled{};
                        if (!loaded.ok) throw std::runtime_error(loaded.reason);
                        FinishPortableShaderExport();
                        LOG_INFO("renderer: startup bundle hit: {} records, {} modules ready, {} cached failures; 0 source content reads, 0 translations, 0 DXC attempts, {} bytes verified/read",
                            loaded.records, modules, cachedFailures, loaded.bytesRead);
                        LOG_INFO("renderer: startup bundle elapsed {:.0f} ms including {:.0f} ms device module creation; source discovery/expansion skipped",
                            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-wholeStarted).count(), moduleMs);
                        ResetTimers();
                        return;
                    } catch (const std::exception& e) {
                        shaders[0].clear(); shaders[1].clear();
                        if (video::ShaderPreparationSkipped()) throw xenos::preparation::Cancelled{};
                        LOG_INFO("renderer: startup bundle fallback: {}", e.what());
                    }
                } else LOG_INFO("renderer: startup bundle bypass: explicit scan/dump/retry or unavailable compiler identity");
                BeginPortableShaderExport(xex); // Discard any partial export after transactional bundle rejection.
                video::SetShaderPreparationProgress(0, 1, video::PreparationStage::CacheValidation, video::PreparationUnit::Files);
                video::PumpEvents();
                xenos::resources::SourceStore sourceStore;
                const xenos::resources::SourceSink saveSource = [&](bool pixel, std::span<const uint8_t> code) {
                    CheckPreparationCancel();
                    sourceStore.Add(pixel, code);
                };
                const auto inventoryStarted = std::chrono::steady_clock::now();
                const auto extracted = xenos::resources::Scan(FileSystem::GetGameRoot(), shaderCacheDir,
                    [](const xenos::resources::ScanProgress& progress) {
                        using namespace xenos::resources;
                        auto stage = progress.stage == ScanStage::CacheValidation ? video::PreparationStage::CacheValidation :
                            progress.stage == ScanStage::IndexedExtraction ? video::PreparationStage::IndexedExtraction : video::PreparationStage::FallbackScan;
                        auto unit = progress.unit == ScanUnit::Bytes ? video::PreparationUnit::MiB :
                            progress.unit == ScanUnit::Entries ? video::PreparationUnit::Entries : video::PreparationUnit::Files;
                        uint64_t done = progress.completed, total = progress.total;
                        if (progress.unit == ScanUnit::Bytes) { done /= 1048576; total = (total + 1048575) / 1048576; }
                        video::SetShaderPreparationProgress(uint32_t(std::min<uint64_t>(done,UINT32_MAX)),
                            uint32_t(std::min<uint64_t>(total,UINT32_MAX)),stage,unit);
                        CheckPreparationCancel();
                    }, getenv("LO_SHADER_FULL_SCAN") ? std::span<const xenos::resources::IndexFile>{}
                                                     : std::span<const xenos::resources::IndexFile>{xenos::resources::builtin::files},
                       getenv("LO_SHADER_FULL_SCAN") ? std::span<const xenos::resources::CpxIndexPackage>{}
                                                     : std::span<const xenos::resources::CpxIndexPackage>{xenos::resources::builtin::cpxPackages},
                       getenv("LO_SHADER_FULL_SCAN") ? std::span<const xenos::resources::CpxIndexArchive>{}
                                                     : std::span<const xenos::resources::CpxIndexArchive>{xenos::resources::builtin::cpxArchives},
                       getenv("LO_SHADER_FULL_SCAN") != nullptr, saveSource);
                CheckPreparationCancel();
                if (!extracted.error.empty()) LOG_WARNING("renderer: resource shader preparation: {}", extracted.error);
                LOG_INFO("renderer: resource shader inventory: {} shaders ({})", extracted.shaders,
                    extracted.reused ? "reused" : "extracted");
                const auto inventoryMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - inventoryStarted).count();
                if (extracted.reused)
                    LOG_INFO("renderer: resource source-cache validation: {} shaders, {} source bytes read, {} ms",
                        extracted.shaders, extracted.cacheBytesRead, inventoryMs);
                else
                    LOG_INFO("renderer: resource discovery/extraction: {} indexed files, {} fallback-scanned files, {} resource bytes read, {} ms",
                        extracted.indexedFiles, extracted.scannedFiles, extracted.bytesRead, inventoryMs);
                if (!extracted.reused)
                    LOG_INFO("renderer: CPX shader discovery: {} packages, {} indexed, {} fallback, {} duplicate, {} decoded (full/partial), {} decoded bytes",
                        extracted.cpxPackages, extracted.indexedPackages, extracted.fallbackPackages, extracted.duplicatePackages,
                        extracted.decodedPackages, extracted.decodedBytes);
                const auto source = std::filesystem::path(shaderCacheDir) / "source";
                const auto expansionStarted = std::chrono::steady_clock::now();
                bool expansionComplete = true;
                try {
                    const auto staticCount = xenos::resources::ExtractXexShaders(
                        xex, source, saveSource);
                    const auto learned = sourceStore.ImportLearned(source);
                    const xenos::resources::variants::SourceReader readSource =
                        [&](uint64_t hash, uint32_t size, bool pixel, std::vector<uint8_t>& code) {
                            return sourceStore.Read(hash, size, pixel, code);
                        };
                    std::set<uint64_t> fixedHashes;
                    const auto generated = xenos::resources::variants::GenerateFixedVariants(source,
                        [&](uint64_t hash, std::span<const uint8_t> code) {
                            fixedHashes.insert(hash); sourceStore.Add(false, code, 1);
                        }, [] { CheckPreparationCancel(); return false; }, readSource);
                    const auto linked = xenos::resources::variants::GenerateLinkedVariants(source,
                        [&](uint64_t, std::span<const uint8_t> code) { sourceStore.Add(false, code, 1); },
                        [] { CheckPreparationCancel(); return false; }, readSource, &fixedHashes);
                    LOG_INFO("renderer: shader prebuild sources: {} in memory, {} bytes, {} learned sources; 0 intermediate source writes",
                        sourceStore.Size(), sourceStore.Bytes(), learned);
                    LOG_INFO("renderer: shader source expansion: {} static XEX, {} fixed VS candidates, {} verified bases, {} invalid bases",
                        staticCount, generated.generated, generated.verifiedBases, generated.invalidBases);
                    LOG_INFO("renderer: linked VS expansion: {} new candidates, {} verified VS bases, {} verified PS sources, {} invalid sources",
                        linked.generated, linked.verifiedBases, linked.verifiedPixelSources, linked.invalidBases + linked.invalidPixelSources);
                } catch (const std::exception& e) {
                    if (video::ShaderPreparationSkipped()) throw xenos::preparation::Cancelled{};
                    expansionComplete = false;
                    LOG_WARNING("renderer: shader source expansion: {}", e.what());
                }
                LOG_INFO("renderer: shader source expansion elapsed: {:.0f} ms",
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - expansionStarted).count());
                const auto enumerationStarted = std::chrono::steady_clock::now();
                // Freeze discovery before workers receive stable, read-only source pointers.
                const auto jobs = sourceStore.Jobs();
                std::unique_ptr<startup::Writer> bundleWriter;
                if (bundleEnabled && extracted.error.empty() && expansionComplete) {
                    try {
                        bundleWriter = std::make_unique<startup::Writer>(bundlePath, xenos::GetShaderCommonHlsl());
                    } catch (const std::exception& e) { LOG_WARNING("renderer: startup bundle writer unavailable: {}", e.what()); }
                }
                LOG_INFO("renderer: shader source enumeration: {} memory records, {:.0f} ms", jobs.size(),
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - enumerationStarted).count());
                const auto started = std::chrono::steady_clock::now();
                // CPU translation, DXC and recoverable compiler checkpoints run in
                // parallel. A bounded queue avoids retaining every shader's
                // HLSL and binary at once. Renderer maps and the device remain
                // on the command processor thread.
                struct PreparedSource {
                    xenos::TranslatedShader info;
                    std::vector<uint8_t> bytecode;
                    std::string name;
                    std::string cachePath;
                    std::string error;
                    std::string cacheWriteError;
                    uint64_t hash = 0;
                    uint64_t sourceUs = 0;
                    uint64_t cacheUs = 0;
                    uint64_t translateUs = 0;
                    uint64_t compileUs = 0;
                    bool pixel = false;
                    bool cacheChecked = false;
                    bool cachePresent = false;
                    bool cacheValid = false;
                    bool compileAttempted = false;
                    bool compiled = false;
                    bool deterministicFailure = false;
                    bool cachedFailure = false;
                };
                const unsigned logicalThreads = std::thread::hardware_concurrency();
                const auto workerCap = xenos::preparation::HostWorkerCap(logicalThreads);
                const auto workerCount = xenos::preparation::WorkerCount(logicalThreads, jobs.size(),
                    getenv("LO_SHADER_PREPARE_SERIAL") != nullptr, static_cast<unsigned>(workerCap));
                const size_t readyCapacity = std::max<size_t>(8, workerCount * 2);

                auto prepare = [&](size_t index) {
                    PreparedSource item;
                    try {
                        const auto sourceStarted = std::chrono::steady_clock::now();
                        const auto& input = *jobs[index];
                        item.pixel = input.pixel;
                        item.hash = input.hash;
                        item.name = fmt::format("{}_{:016x}.bin", item.pixel ? "ps" : "vs", item.hash);
                        // Microcode is validated once at ingestion, not exported and
                        // read back once per compiler worker. memcpy supplies alignment.
                        const size_t size = input.code.size();
                        std::vector<uint32_t> words(size / 4);
                        std::memcpy(words.data(), input.code.data(), size);
                        item.sourceUs = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - sourceStarted).count();

                        const auto cacheStarted = std::chrono::steady_clock::now();
                        item.cachePath = (std::filesystem::path(shaderCacheDir) /
                            xenos::cache::FileName(item.pixel, item.hash, cacheIdentity)).string();
                        item.cacheChecked = true;
                        item.bytecode = xenos::cache::ReadBinary(item.cachePath, item.pixel, item.hash, cacheIdentity, &item.cachePresent);
                        item.cacheValid = !item.bytecode.empty();
                        item.cacheUs = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - cacheStarted).count();

                        std::vector<uint32_t> swapped(words.size());
                        std::transform(words.begin(), words.end(), swapped.begin(), [](uint32_t word) { return ByteSwap(word); });
                        const auto translateStarted = std::chrono::steady_clock::now();
                        item.info = xenos::TranslateShader(swapped.data(), uint32_t(swapped.size()), item.pixel);
                        item.translateUs = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - translateStarted).count();
                        if (const char* hlslDir = getenv("LO_SHADER_HLSL_DIR"))
                            std::ofstream(fmt::format("{}/{}_{:016x}.hlsl", hlslDir,
                                item.pixel ? "ps" : "vs", item.hash)) << item.info.hlsl;

                        if (!item.cacheValid) {
                            const auto failurePath = item.cachePath + ".failed";
                            const auto failureKey = startup::FailureKey(item.info.hlsl, cacheIdentity, item.pixel);
                            if (!compilerIdentity.empty() && !retryFailures) {
                                item.error = startup::ReadFailure(failurePath, failureKey);
                                if (!item.error.empty()) {
                                    item.cachedFailure = item.deterministicFailure = true;
                                    return item;
                                }
                            }
                            item.compileAttempted = true;
                            const auto compileStarted = std::chrono::steady_clock::now();
                            auto compiled = xenos::CompileHlsl(item.info.hlsl, "main",
                                item.pixel ? "ps_6_0" : "vs_6_0", binaryFormat);
                            item.compileUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now() - compileStarted).count();
                            if (!compiled.ok) {
                                item.deterministicFailure = compiled.deterministicFailure && !compilerIdentity.empty();
                                item.error = std::move(compiled.errors);
                                if (item.error.empty()) item.error = "DXC compilation failed";
                                if (item.deterministicFailure && !startup::WriteFailure(failurePath, failureKey, item.error, true))
                                    item.cacheWriteError = "could not persist compiler failure diagnostic";
                                if (const char* dumpDir = getenv("LO_SHADER_DUMP_DIR"))
                                    std::ofstream(fmt::format("{}/{}_{:016x}.hlsl", dumpDir,
                                        item.pixel ? "ps" : "vs", item.hash)) << item.info.hlsl;
                            } else {
                                item.bytecode = std::move(compiled.bytecode);
                                item.compiled = true;
                                xenos::cache::WriteBinary(item.cachePath, item.pixel, item.hash, cacheIdentity,
                                    item.bytecode, &item.cacheWriteError);
                            }
                        }
                    } catch (const std::bad_alloc&) { throw; }
                    catch (const std::exception& e) { item.error = e.what(); }
                    return item;
                };

                LOG_INFO("renderer: shader preparation: {} logical threads, {} requested workers, {} shaders, {} queued results max",
                    logicalThreads, workerCount, jobs.size(), readyCapacity);

                uint32_t done = 0, failed = 0, modulesReady = 0, modulesFailed = 0;
                size_t cacheHits = 0, cacheMissing = 0, cacheInvalid = 0, compileAttempts = 0, compiledCount = 0, cachedFailureCount = 0;
                uint64_t sourceUs = 0, cacheUs = 0, translateUs = 0, compileUs = 0, moduleUs = 0;
                auto install = [&](PreparedSource item) {
                    sourceUs += item.sourceUs;
                    cacheUs += item.cacheUs;
                    translateUs += item.translateUs;
                    compileUs += item.compileUs;
                    cacheHits += item.cacheValid;
                    cacheMissing += item.cacheChecked && !item.cachePresent;
                    cacheInvalid += item.cacheChecked && item.cachePresent && !item.cacheValid;
                    compileAttempts += item.compileAttempted;
                    compiledCount += item.compiled;
                    cachedFailureCount += item.cachedFailure;
                    if (bundleWriter) {
                        // A failed loose checkpoint does not invalidate the
                        // successfully compiled record or the independent bundle.
                        if (!item.error.empty() && !item.deterministicFailure) bundleWriter.reset();
                        else try { bundleWriter->Add({item.hash, item.info, item.bytecode, item.error}); }
                        catch (const std::exception& e) {
                            LOG_WARNING("renderer: startup bundle write abandoned: {}", e.what());
                            bundleWriter.reset();
                        }
                    }
                    // A later explicit frame capture regenerates HLSL from that
                    // draw's guest microcode; ordinary rendering uses metadata only.
                    ExportPortableShader(item.hash, item.info, item.bytecode, item.error, item.deterministicFailure);
                    std::string{}.swap(item.info.hlsl);
                    if (item.cachePresent && !item.cacheValid)
                        SHADER_LOG_WARNING("cache-invalid", RendererByteFnv, "renderer: ignoring incomplete shader cache {}", item.cachePath);
                    if (!item.cacheWriteError.empty())
                        SHADER_LOG_WARNING("cache-write-failed", RendererByteFnv, "renderer: precompile {} cache write failed: {}", item.name, item.cacheWriteError);
                    if (!item.error.empty()) {
                        SHADER_LOG_WARNING("prepare-failed", RendererByteFnv, "renderer: precompile {} {}: {} (diagnostic: {}.failed)", item.name,
                            item.cachedFailure ? "cached failure" : "failed", item.error, item.cachePath);
                        if (item.deterministicFailure) {
                            auto& entry = shaders[item.pixel ? 1 : 0][item.hash];
                            entry.info = std::move(item.info);
                            entry.retry.Failed(true);
                        }
                        ++failed;
                    } else {
                        auto& cache = shaders[item.pixel ? 1 : 0];
                        Shader& entry = cache[item.hash];
                        entry.info = std::move(item.info);
                        const auto moduleStarted = std::chrono::steady_clock::now();
                        try {
                            entry.shader = device->createShader(item.bytecode.data(), item.bytecode.size(), "main", renderFormat);
                        } catch (const std::exception& e) {
                            LOG_WARNING("renderer: shader device module creation failed: {}", e.what());
                            entry.shader.reset();
                        }
                        moduleUs += std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - moduleStarted).count();
                        entry.valid = entry.shader != nullptr;
                        if (!entry.info.errors.empty())
                            SHADER_LOG_WARNING("translation-notes", RendererByteFnv, "renderer: {} shader {:016x} notes: {}",
                                item.pixel ? "pixel" : "vertex", item.hash, entry.info.errors);
                        if (entry.valid) { entry.retry.Succeeded(); ++modulesReady; }
                        else {
                            entry.retry.Failed(false);
                            SHADER_LOG_ERROR("shader-module-failed", RendererByteFnv, "preparation {} shader={:016x} bytes={} format={}",
                                item.pixel ? "pixel" : "vertex", item.hash, item.bytecode.size(), vulkan ? "spirv" : "dxil");
                            ++modulesFailed; ++failed; initializationModuleFailure = true; bundleWriter.reset();
                        }
                    }
                    ++done;
                    video::SetShaderPreparationProgress(done, uint32_t(jobs.size()));
                    video::PumpEvents();
                    if (video::ShaderPreparationSkipped()) {
                        LOG_INFO("renderer: shader preparation skipped by user request after {} shaders", done);
                        return false;
                    }
                    return !initializationModuleFailure;
                };

                xenos::preparation::QueueStats queueStats;
                try {
                    queueStats = xenos::preparation::RunBounded<PreparedSource>(jobs.size(), workerCount,
                        readyCapacity, prepare, install, [] { video::PumpEvents(); return !video::ShaderPreparationSkipped(); });
                } catch (const std::exception& e) {
                    initializationModuleFailure = true;
                    bundleWriter.reset();
                    LOG_WARNING("renderer: shader preparation queue stopped after {} shaders: {}", done, e.what());
                }
                if (queueStats.startFailures)
                    LOG_WARNING("renderer: started only {} shader workers: {}",
                        queueStats.startedWorkers, queueStats.startError);
                video::SetShaderPreparationProgress(0, 0);
                if (!queueStats.cancelled && !video::ShaderPreparationSkipped() && done == jobs.size() && expansionComplete && extracted.error.empty() && !initializationModuleFailure)
                    FinishPortableShaderExport();
                else portableShaderExport.reset(); // Cancellation/module failures never publish an incomplete export.
                if (bundleWriter && !queueStats.cancelled && !video::ShaderPreparationSkipped() && done == jobs.size()) {
                    try {
                        bundleWriter->Finish(bundleIdentity);
                        LOG_INFO("renderer: startup bundle published: {} records, {} bytes", done, std::filesystem::file_size(bundlePath));
                    } catch (const std::exception& e) { LOG_WARNING("renderer: startup bundle not published: {}", e.what()); }
                }
                if (done) {
                    LOG_INFO("renderer: shader cache: {} valid, {} missing, {} invalid; {} DXC attempts, {} compiled",
                        cacheHits, cacheMissing, cacheInvalid, compileAttempts, compiledCount);
                    LOG_INFO("renderer: cached compiler failures: {} (reported as failed, never compiled/ready)", cachedFailureCount);
                    LOG_INFO("renderer: shader preparation CPU: source read/validation {:.0f} ms, cache read/validation {:.0f} ms, translation {:.0f} ms, DXC {:.0f} ms (worker times summed)",
                        sourceUs / 1000.0, cacheUs / 1000.0, translateUs / 1000.0, compileUs / 1000.0);
                    LOG_INFO("renderer: shader modules: {} ready, {} failed, {:.0f} ms command-thread time",
                        modulesReady, modulesFailed, moduleUs / 1000.0);
                    LOG_INFO("renderer: prepared {} known shaders, {} failed, {} workers, {:.0f} ms elapsed",
                        done, failed, queueStats.startedWorkers,
                        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-started).count());
                }
                LOG_INFO("renderer: complete shader startup preparation elapsed: {:.0f} ms",
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-wholeStarted).count());
                ResetTimers();
                } catch (const xenos::preparation::Cancelled&) {
                    portableShaderExport.reset();
                    LOG_INFO("renderer: shader preparation skipped by user request");
                }
                video::SetShaderPreparationProgress(0, 0);
            }

            std::unique_ptr<position_evidence::Collection> positionEvidence;

            void PreparePositionEvidence(Shader& entry, const uint32_t* words, uint32_t count, uint64_t hash)
            {
                if(entry.positionReady || !entry.valid || !positionEvidence)return;
                if(!taa_collection::Enabled()&&debugCaptureDir.empty())return;
                // Busy, full, or pending diagnostics remain unproven. A later draw
                // can consume the finished CPU result without changing GPU shaders.
                entry.positionReady=positionEvidence->TryGet(hash,words,count,entry.position);
            }

            Shader* GetShader(bool pixel, const uint32_t* words, uint32_t count, uint64_t hash)
            {
                taa_collection::ObserveProgram(!pixel, hash, words, count);
                if (debugShaderSources && !debugCaptureDir.empty())
                    debugShaderSources->Observe(!pixel, hash, words, count, frame);
                auto& cache = shaders[pixel ? 1 : 0];
                auto it = cache.find(hash);
                if (it == cache.end() && TryLoadPortableShader(pixel, hash)) it = cache.find(hash);
                if (it != cache.end() && !it->second.valid && !it->second.retry.Ready()) return nullptr;
                if (it != cache.end() && it->second.valid) {
                    if (!debugCaptureDir.empty() && it->second.valid && it->second.info.hlsl.empty()) {
                        std::vector<uint32_t> swapped(count);
                        for (uint32_t i = 0; i < count; ++i) swapped[i] = ByteSwap(words[i]);
                        it->second.info.hlsl = xenos::TranslateShader(swapped.data(), count, pixel).hlsl;
                    }
                    if(!pixel)PreparePositionEvidence(it->second,words,count,hash);
                    return it->second.valid ? &it->second : nullptr;
                }

                Shader& entry = cache[hash];
                xenos::retry::Attempt attempt(entry.retry);
                if (!shaderCacheDir.empty()) {
                    const auto source = std::filesystem::path(shaderCacheDir) / "source";
                    std::error_code ec;
                    std::filesystem::create_directories(source, ec);
                    const auto path = source / fmt::format("{}_{:016x}.bin", pixel ? "ps" : "vs", hash);
                    if (!ec && !std::filesystem::exists(path, ec))
                        std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(words), size_t(count)*4);
                }
                ScopedTimer timer{ tShader, cpuTimingEnabled };
                nShader++;
                std::vector<uint32_t> swapped(count);
                for (uint32_t i = 0; i < count; i++)
                    swapped[i] = ByteSwap(words[i]);
                entry.info = xenos::TranslateShader(swapped.data(), count, pixel);
                struct ReleaseText {
                    std::string& text;
                    bool capture;
                    ~ReleaseText() { if (!capture) std::string{}.swap(text); }
                } releaseText{entry.info.hlsl, !debugCaptureDir.empty()};
                // LO_SHADER_HLSL_DIR: write the generated HLSL under the same hash the
                // draw trace prints, so a specific pass can be inspected offline.
                if (const char* hlslDir = getenv("LO_SHADER_HLSL_DIR"))
                    std::ofstream(fmt::format("{}/{}_{:016x}.hlsl", hlslDir, pixel ? "ps" : "vs", hash)) << entry.info.hlsl;

                std::vector<uint8_t> dxil;
                std::string cachePath;
                if (!shaderCacheDir.empty())
                {
                    // The cache name carries a translator version so changes to the
                    // generated HLSL don't resurrect stale DXIL.
                    cachePath = (std::filesystem::path(shaderCacheDir) / xenos::cache::FileName(pixel, hash, cacheIdentity)).string();
                    bool present = false;
                    dxil = xenos::cache::ReadBinary(cachePath, pixel, hash, cacheIdentity, &present);
                    if (present && dxil.empty()) SHADER_LOG_WARNING("cache-invalid", RendererByteFnv, "renderer: ignoring invalid/foreign shader cache {}", cachePath);
                }
                if (dxil.empty())
                {
                    const auto& compilerIdentity = xenos::DxcIdentity();
                    const auto failureKey = xenos::startup_cache::FailureKey(entry.info.hlsl, cacheIdentity, pixel);
                    const auto failurePath = cachePath + ".failed";
                    if (!cachePath.empty() && !compilerIdentity.empty() && !getenv("LO_SHADER_RETRY_FAILURES")) {
                        const auto failure = xenos::startup_cache::ReadFailure(failurePath, failureKey);
                        if (!failure.empty()) {
                            attempt.PermanentFailure();
                            SHADER_LOG_WARNING("compile-cached-failure", RendererByteFnv, "renderer: {} shader {:016x} cached compiler failure: {} (diagnostic: {})",
                                pixel ? "pixel" : "vertex", hash, failure, failurePath);
                            return nullptr;
                        }
                    }
                    xenos::CompiledShader compiled = xenos::CompileHlsl(entry.info.hlsl, "main", pixel ? "ps_6_0" : "vs_6_0", binaryFormat);
                    if (!compiled.ok)
                    {
                        attempt.PermanentFailure(compiled.deterministicFailure);
                        if (!cachePath.empty() && !compilerIdentity.empty())
                            xenos::startup_cache::WriteFailure(failurePath, failureKey, compiled.errors, compiled.deterministicFailure);
                        SHADER_LOG_WARNING("compile-failed", RendererByteFnv, "renderer: {} shader {:016x} failed to compile:\n{}", pixel ? "pixel" : "vertex", hash, compiled.errors);
                        if (getenv("LO_SHADER_DUMP_DIR"))
                            std::ofstream(fmt::format("{}/{}_{:016x}.hlsl", getenv("LO_SHADER_DUMP_DIR"), pixel ? "ps" : "vs", hash)) << entry.info.hlsl;
                        return nullptr;
                    }
                    dxil = std::move(compiled.bytecode);
                    if (!cachePath.empty()) {
                        std::string error;
                        if (!xenos::cache::WriteBinary(cachePath, pixel, hash, cacheIdentity, dxil, &error))
                            SHADER_LOG_WARNING("cache-write-failed", None, "renderer: shader cache write failed: {}", error);
                    }
                }
                try {
                    entry.shader = device->createShader(dxil.data(), dxil.size(), "main", renderFormat);
                } catch (const std::exception& e) {
                    SHADER_LOG_WARNING("shader-module-failed", RendererByteFnv,
                        "renderer: shader module creation failed: {}", e.what());
                    return nullptr;
                }
                entry.valid = entry.shader != nullptr;
                if (entry.valid) attempt.Succeeded();
                os::shaderlog::Log(entry.valid ? LogType::Info : LogType::Error,
                    entry.valid ? "shader-module-ready" : "shader-module-failed", os::shaderlog::HashNamespace::RendererByteFnv,
                    "{} shader={:016x} format={} words={} bytes={} frame={}", pixel ? "pixel" : "vertex", hash,
                    vulkan ? "spirv" : "dxil", count, dxil.size(), frame);
                if (!entry.info.errors.empty())
                    SHADER_LOG_WARNING("translation-notes", RendererByteFnv, "renderer: {} shader {:016x} notes: {}", pixel ? "pixel" : "vertex", hash, entry.info.errors);
                if(!pixel)PreparePositionEvidence(entry,words,count,hash);
                return entry.valid ? &entry : nullptr;
            }

            // ---- render targets ------------------------------------------------------
            // EDRAM has no height; take it from the scissor, or assume 16:9 / square
            // when the scissor is the "everything" 8192x8192 used by clears.
            static uint32_t GuessTargetHeight(uint32_t pitch, uint32_t scissorBottom)
            {
                if (scissorBottom >= 4096 || scissorBottom == 0)
                {
                    if (pitch == 1280) return 720;
                    if (pitch == 1920) return 1080;
                    return std::min<uint32_t>(pitch, 2048);
                }
                uint32_t h = (scissorBottom + 31) & ~31u;
                if (pitch == 1280 && h <= 720) return 720;
                if (pitch == 1920 && h <= 1080) return 1080;
                return std::clamp<uint32_t>(h, 32, 2048);
            }

            // EDRAM colour formats that share a bit layout and interpretation form one
            // "class". Tiles bound through a different class hold the same bits but
            // mean something else, so switching class needs a conversion pass
            // (Xenia calls this ownership transfer).
            enum ColorClass : uint32_t
            {
                kClass8888 = 0,      // k_8_8_8_8, k_8_8_8_8_GAMMA
                kClass2101010 = 1,   // k_2_10_10_10, _AS_10_10_10_10 (fixed point)
                kClass7e3 = 2,       // k_2_10_10_10_FLOAT, _FLOAT_AS_16_16_16_16
                kClass16F2 = 3,      // k_16_16(_FLOAT)
                kClass16F4 = 4,      // k_16_16_16_16(_FLOAT)
                kClass32F = 5,
                kClass32F2 = 6,
            };

            // 20e4 depth (D24FS8, [0, 2)) and 7e3 colour (k_2_10_10_10_FLOAT) packing,
            // after xenia/gpu/xenos.cc Float32To20e4 / Float7e3To32.
            static uint32_t Float32To20e4(float f32)
            {
                if (!(f32 > 0.0f))
                    return 0;
                uint32_t u; memcpy(&u, &f32, 4);
                if (u >= 0x3FFFFFF8u)
                    return 0xFFFFFF;
                if (u < 0x38800000u)
                {
                    uint32_t shift = std::min<uint32_t>(113u - (u >> 23), 24u);
                    u = (0x800000u | (u & 0x7FFFFFu)) >> shift;
                }
                else
                    u += 0xC8000000u;
                return (u >> 3) & 0xFFFFFF;
            }

            static float Float7e3To32(uint32_t f10)
            {
                f10 &= 0x3FF;
                if (!f10)
                    return 0.0f;
                uint32_t mantissa = f10 & 0x7F, exponent = f10 >> 7;
                if (!exponent)
                {
                    uint32_t lz = 0;
                    while (!((mantissa << lz) & 0x80)) lz++;
                    exponent = uint32_t(1 - int32_t(lz));
                    mantissa = (mantissa << lz) & 0x7F;
                }
                uint32_t u = ((exponent + 124u) << 23) | (mantissa << 16);
                float f; memcpy(&f, &u, 4);
                return f;
            }

            // What a colour view of the given class reads from a 32-bit EDRAM word.
            static RenderColor UnpackGuestWord(uint32_t word, uint32_t colorClass)
            {
                switch (colorClass)
                {
                case kClass8888:
                    return RenderColor(float(word & 0xFF) / 255.0f, float((word >> 8) & 0xFF) / 255.0f, float((word >> 16) & 0xFF) / 255.0f, float(word >> 24) / 255.0f);
                case kClass2101010:
                    return RenderColor(float(word & 0x3FF) / 1023.0f, float((word >> 10) & 0x3FF) / 1023.0f, float((word >> 20) & 0x3FF) / 1023.0f, float(word >> 30) / 3.0f);
                case kClass7e3:
                    return RenderColor(Float7e3To32(word), Float7e3To32(word >> 10), Float7e3To32(word >> 20), float(word >> 30) / 3.0f);
                default:
                    return RenderColor(0.0f, 0.0f, 0.0f, 0.0f);   // 16_16 / 32-bit float classes: only the zero word is exact
                }
            }

            static uint32_t ColorClassOf(uint32_t xenosFormat)
            {
                switch (xenosFormat)
                {
                case 0: case 1: return kClass8888;
                case 2: case 10: return kClass2101010;
                case 3: case 12: return kClass7e3;
                case 4: case 6: return kClass16F2;
                case 5: case 7: return kClass16F4;
                case 14: return kClass32F;
                case 15: return kClass32F2;
                default: return kClass8888;
                }
            }

            // All classes live in FP16 host textures: an 8-bit or 10-bit unorm value is
            // exact in half precision, and keeping one host format keeps resolves and
            // framebuffers simple. Only the interpretation differs between classes.
            static RenderFormat ClassHostFormat(uint32_t colorClass)
            {
                switch (colorClass)
                {
                case kClass16F2: return RenderFormat::R16G16_FLOAT;
                case kClass32F: return RenderFormat::R32_FLOAT;
                case kClass32F2: return RenderFormat::R32G32_FLOAT;
                default: return RenderFormat::R16G16B16A16_FLOAT;
                }
            }

            static RenderFormat ColorFormat(uint32_t xenosFormat)
            {
                switch (xenosFormat)
                {
                case 0: case 1: return RenderFormat::R8G8B8A8_UNORM;          // k_8_8_8_8(_GAMMA)
                case 2: case 3: case 10: case 12: return RenderFormat::R16G16B16A16_FLOAT; // 2_10_10_10 variants
                case 4: case 6: return RenderFormat::R16G16_FLOAT;
                case 5: case 7: return RenderFormat::R16G16B16A16_FLOAT;
                case 14: return RenderFormat::R32_FLOAT;
                case 15: return RenderFormat::R32G32_FLOAT;
                default: return RenderFormat::R8G8B8A8_UNORM;
                }
            }

            bool ApplyInternalResolution()
            {
                if (video::GpuWorkStopped()) return false;
                frame_plan::FramePlan selected;
                bool failed = false;
                {
                    std::lock_guard lock(framePlanMutex);
                    selected = committedPlan;
                    failed = selected.geometryEpoch != 0 && failedPlanEpochs.contains(selected.geometryEpoch);
                }
                activePlan = selected;
                if (failed)
                    return false;
                const resolution::Size requested{selected.width, selected.height};
                const bool first = appliedPlanEpoch == ~0ull;
                const bool requestChanged = requested != requestedInternalSize;
                const bool epochChanged = selected.geometryEpoch != appliedPlanEpoch;
                const bool recreateFeature =
#if defined(LO_RENDERER_P2_EMBEDDED_TEST)
                    dlssController &&
                    ((srReconfigureFrame != ~0ull && srReconfigureFrame != frame) ||
                      (epochChanged && dlssController->HasFeatureState()));
#else
                    temporalUpscaler &&
                    ((srReconfigureFrame != ~0ull && srReconfigureFrame != frame) ||
                      (epochChanged && temporalUpscaler->HasFeatureState()));
#endif
                if (!epochChanged && !requestChanged && !first && !recreateFeature) return true;
                requestedInternalSize = requested;
                const auto effective = requested;
                // No guest draw attachments, upload offsets or descriptors have
                // been borrowed yet. Reconfigure here, never in a scene-copy draw.
                // Keep the NGX session/parameters, releasing only the old feature.
                if (effective != internalSize || recreateFeature) {
                    if (!Flush() || !WaitForGpu() || !video::WaitForPresentGpu()) return false;
                    if (recreateFeature) {
#if defined(LO_RENDERER_P2_EMBEDDED_TEST)
                        dlssController->ReleaseFeatureAfterGpuDrain();
                        const bool featureRetained = dlssController->HasFeatureState();
#else
                        temporalUpscaler->ReleaseFeatureAfterGpuDrain();
                        const bool featureRetained = temporalUpscaler->HasFeatureState();
#endif
                        if (featureRetained) {
                            FailCurrentPlan(frame_plan::FailureReason::InvalidInput);
                            return false;
                        }
                        srReconfigureFrame = ~0ull;
                    }
                }
                if (effective != internalSize) {
                    // active points into renderTargets; parked/scratch may also
                    // be referenced by completed NGX lists. Clear only after the
                    // drain, before destroying the map it points into.
                    // Stencil views inside replay framebuffers must die first.
                    if (motionReplay) motionReplay->ReleaseDepthBindingsAfterGpuDrain();
#if defined(LO_GPU_PLUME)
                    if (fsrAlphaReplay) fsrAlphaReplay->ReleaseDepthBindingsAfterGpuDrain();
                    fsrAlphaRawViews.clear();
                    if (fsrAlphaBridge) fsrAlphaBridge->DiscardUnsubmitted();
#endif
                    sceneCopyPromotion = {};
                    framebuffers.clear();
                    renderTargets.clear();
                    resolved.clear();
                    tileOwner.clear();
                    if (temporalHistory) temporalHistory->Reset();
                    temporalScene.Reset(frame);
                    temporalSupportedFrame = ~0ull;
                    sceneAAAppliedFrame = ~0ull;
                    sceneAAConfigFrame = ~0ull;
                    ++temporalEpoch;
                }
                internalSize = effective;
                appliedPlanEpoch = selected.geometryEpoch;
                LOG_INFO("renderer: internal resolution f{} plan={} epoch={} requested={}x{} effective={}x{} cpu_readback={} allocation_fallback={}",
                    frame, selected.cpuSerial, selected.geometryEpoch, requested.width, requested.height, effective.width, effective.height,
                    resolveReadback, false);
                return true;
            }

            void FailCurrentPlan(frame_plan::FailureReason reason = frame_plan::FailureReason::Unknown)
            {
                if (!activePlan.cpuSerial || !activePlan.geometryEpoch) return;
                {
                    std::lock_guard lock(framePlanMutex);
                    if (!failedPlanEpochs.emplace(activePlan.geometryEpoch).second)
                        return;
                }
                const uint32_t fallback = activePlan.height > 720 ? std::max(720u, activePlan.height * 3 / 4) : 720u;
                if (reason == frame_plan::FailureReason::Unknown)
                    reason = upscaling::IsDlssConsumer(activePlan.consumer)
                        ? frame_plan::FailureReason::DlssOutOfMemory : frame_plan::FailureReason::InvalidInput;
                frame_plan::ReportPlanFailure({activePlan.geometryEpoch,activePlan.requestSignature,fallback,reason});
                LOG_ERROR("renderer: plan failure epoch={} signature={:#x} reason={} fallback={}",
                    activePlan.geometryEpoch,activePlan.requestSignature,uint32_t(reason),fallback);
            }

            // SR execution errors retain this frame's spatial/legacy result and
            // disable only the matching DLSS request on the next CPU plan. They
            // are not allocation failures and must never enter failedPlanEpochs.
            void DisableDlssRequest(frame_plan::FailureReason reason)
            {
                if ((!upscaling::IsDlssConsumer(activePlan.consumer) && !upscaling::IsSrConsumer(activePlan.consumer)) || !activePlan.cpuSerial ||
                    dlssDisableReportedEpoch == activePlan.geometryEpoch) return;
                dlssDisableReportedEpoch = activePlan.geometryEpoch;
                if (evaluatePage && evaluatePage->frame == frame)
                    evaluatePage->fallbackReason = fmt::format("request_failure_{}", uint32_t(reason));
                frame_plan::ReportPlanFailure({activePlan.geometryEpoch, activePlan.requestSignature,
                    activePlan.legacyHeight, reason});
            }

            bool PlanSuppressed()
            {
                if (video::GpuWorkStopped()) return true;
                std::lock_guard lock(framePlanMutex);
                return committedPlan.geometryEpoch != 0 && failedPlanEpochs.contains(committedPlan.geometryEpoch);
            }

            HostTexture* GetRenderTarget(uint32_t base, uint32_t format, uint32_t pitch, uint32_t height, bool depth)
            {
                // EDRAM targets have no intrinsic height and ours is only guessed
                // from the scissor, so draws and resolves may disagree on it: key
                // by tile base, format and pitch only and grow the texture when a
                // taller extent shows up.
                height = std::clamp<uint32_t>((height + 31) & ~31u, 32, 2048);
                // Depth formats (D24S8 / D24FS8) alias the same tiles and share our
                // host format, so they are one target.
                // One host texture per (base, pitch, storage class).
                const uint32_t colorClass = depth ? 0u : ColorClassOf(format);
                RenderTargetKey key{ base, colorClass, pitch, 0, depth };
                if (!PreparePromotionAccess(key, height, depth)) return nullptr;
                auto it = renderTargets.find(key);
                uint32_t effectiveHeight = height;
                if (it != renderTargets.end())
                    effectiveHeight = std::max(effectiveHeight, it->second->guestHeight);
                const auto role = ResolveCatalogRole(base, pitch);
                const resolution::Size legacySize{activePlan.legacyWidth,activePlan.legacyHeight};
                resolution::Size desiredSize = resolution::TargetSizeForPlan(role, pitch, effectiveHeight, internalSize, legacySize);
                if (sceneCopyPromotion.activeMapping && key == sceneCopyPromotion.key && sceneCopyPromotion.frame == frame &&
                    sceneCopyPromotion.epoch == activePlan.geometryEpoch)
                    desiredSize = {activePlan.output.width, activePlan.output.height};
                std::unique_ptr<HostTexture> oldTarget;
                if (it != renderTargets.end())
                {
                    // A catalog publication can arrive after a target's first
                    // use. Test its effective mapping before returning a cache
                    // hit so the old native/square allocation is never reused.
                    if (it->second->guestHeight >= height && it->second->resolutionSize == desiredSize)
                        return it->second.get();
                    oldTarget = std::move(it->second);
                    renderTargets.erase(it);
                }

                consecutiveResolveCopies.Invalidate();
                auto tex = std::make_unique<HostTexture>();
                tex->allocationSerial = ++nextTargetAllocation;
                tex->format = depth ? RenderFormat::D32_FLOAT_S8_UINT : ClassHostFormat(colorClass);
                tex->guestWidth = pitch;
                tex->guestHeight = effectiveHeight;
                tex->resolutionSize = desiredSize;
                tex->width = std::max(1u, tex->ScaleX(pitch));
                tex->height = std::max(1u, tex->ScaleY(effectiveHeight));
                RenderTextureDesc desc = RenderTextureDesc::Texture2D(tex->width, tex->height, 1, tex->format, depth ? RenderTextureFlag::DEPTH_TARGET : RenderTextureFlag::RENDER_TARGET);
                tex->texture = device->createTexture(desc);
                tex->layout = RenderTextureLayout::UNKNOWN;
                if (!tex->texture) {
                    FailCurrentPlan();
                    LOG_ERROR("renderer: render target allocation failed guest={}x{} physical={}x{}; native resolution fallback next frame", pitch, effectiveHeight, tex->width, tex->height);
                    if (oldTarget) Gpu().retiredTextures.push_back(std::move(oldTarget));
                    return nullptr;
                }
                const bool preserveOverlap = !depth && oldTarget && oldTarget->texture &&
                    oldTarget->format == tex->format && oldTarget->resolutionSize == tex->resolutionSize &&
                    oldTarget->guestWidth == tex->guestWidth && oldTarget->width == tex->width &&
                    tex->guestHeight > oldTarget->guestHeight && tex->height >= oldTarget->height;
                if (preserveOverlap) {
                    if (!Begin()) {
                        FailCurrentPlan();
                        LOG_ERROR("renderer: command list begin failed during render target growth preservation");
                        Gpu().retiredTextures.push_back(std::move(tex));
                        Gpu().retiredTextures.push_back(std::move(oldTarget));
                        return nullptr;
                    }
                    const uint32_t copyWidth = oldTarget->width;
                    const uint32_t copyHeight = oldTarget->height;
                    if (copyWidth > 0 && copyHeight > 0) {
                        Transition(*oldTarget, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
                        Transition(*tex, RenderTextureLayout::COPY_DEST, RenderBarrierStage::COPY);
                        RenderBox box{ 0, 0, int32_t(copyWidth), int32_t(copyHeight), 0, 1 };
                        commandList->copyTextureRegion(
                            RenderTextureCopyLocation::Subresource(tex->texture.get()),
                            RenderTextureCopyLocation::Subresource(oldTarget->texture.get()),
                            0, 0, 0, &box);
                    }
                }
                if (oldTarget) Gpu().retiredTextures.push_back(std::move(oldTarget));
                LOG_INFO("renderer: new {} target base={:#x} fmt={} guest={}x{} physical={}x{} scale={}x{}", depth ? "depth" : "color", base, format, pitch, effectiveHeight, tex->width, tex->height, tex->resolutionSize.width, tex->resolutionSize.height);
                HostTexture* result = tex.get();
                renderTargets.emplace(key, std::move(tex));
                return result;
            }

            // Hands the tiles at (base, pitch) to `colorClass`, converting whatever the
            // previous owner wrote through the guest bit representation.
            std::map<std::pair<uint32_t, uint32_t>, uint32_t> tileOwner;

            // Draws also need the previous owner's bits: blending, write masks,
            // depth/stencil rejection and partial coverage preserve destination
            // pixels. In the tank intro the title restores the saved scene as
            // fixed 2_10_10_10, then blends light into the same tiles as 7e3.
            // Skipping that transfer retains the old white attenuation clear.
            HostTexture* AcquireColorTarget(uint32_t base, uint32_t format, uint32_t pitch, uint32_t height, bool forRead = false)
            {
                const uint32_t colorClass = ColorClassOf(format);
                HostTexture* target = GetRenderTarget(base, format, pitch, height, false);
                // Convert on both draw and resolve by default. The old resolve-only
                // behavior remains opt-in for diagnostic A/B runs (=read); =0/none
                // disables transfers entirely.
                static const char* transferMode = getenv("LO_EDRAM_TRANSFER") ? getenv("LO_EDRAM_TRANSFER") : "draw";
                const bool doTransfer = strcmp(transferMode, "0") != 0 && strcmp(transferMode, "none") != 0 &&
                    (forRead || strcmp(transferMode, "draw") == 0);
                auto owner = tileOwner.find({ base, pitch });
                if (doTransfer && owner != tileOwner.end() && owner->second != colorClass)
                {
                    auto prev = renderTargets.find(RenderTargetKey{ base, owner->second, pitch, 0, false });
                    if (prev != renderTargets.end() && prev->second && prev->second->texture && target && target->texture)
                        TransferRegion(*prev->second, *target, owner->second, colorClass);
                }
                tileOwner[{ base, pitch }] = colorClass;
                return target;
            }

            RenderFramebuffer* GetFramebuffer(HostTexture* color, HostTexture* depth)
            {
                auto key = std::make_pair(color ? color->texture.get() : nullptr, depth ? depth->texture.get() : nullptr);
                auto it = framebuffers.find(key);
                if (it != framebuffers.end())
                    return it->second.get();
                const RenderTexture* colors[1] = { key.first };
                RenderFramebufferDesc desc(colors, color ? 1u : 0u, key.second);
                auto fb = device->createFramebuffer(desc);
                RenderFramebuffer* result = fb.get();
                framebuffers.emplace(key, std::move(fb));
                return result;
            }

            // ---- textures --------------------------------------------------------------
            struct TextureFormatInfo
            {
                RenderFormat host;
                uint32_t blockWidth, blockHeight, bytesPerBlock;
                bool convertToRgba8; // CPU conversion into R8G8B8A8
            };

            static bool GetTextureFormat(uint32_t xenosFormat, TextureFormatInfo& out)
            {
                switch (xenosFormat)
                {
                case 2: out = { RenderFormat::R8_UNORM, 1, 1, 1, false }; return true;          // k_8
                case 3: out = { RenderFormat::R8G8B8A8_UNORM, 1, 1, 2, true }; return true;     // k_1_5_5_5
                case 4: out = { RenderFormat::R8G8B8A8_UNORM, 1, 1, 2, true }; return true;     // k_5_6_5
                case 6: out = { RenderFormat::R8G8B8A8_UNORM, 1, 1, 4, false }; return true;    // k_8_8_8_8
                case 7: case 54: out = { RenderFormat::R8G8B8A8_UNORM, 1, 1, 4, true }; return true; // k_2_10_10_10(_AS_16_16_16_16), truncated to 8 bits
                case 10: out = { RenderFormat::R8G8_UNORM, 1, 1, 2, false }; return true;       // k_8_8
                case 15: out = { RenderFormat::R8G8B8A8_UNORM, 1, 1, 2, true }; return true;    // k_4_4_4_4
                case 18: out = { RenderFormat::BC1_UNORM, 4, 4, 8, false }; return true;        // DXT1
                case 19: out = { RenderFormat::BC2_UNORM, 4, 4, 16, false }; return true;       // DXT2/3
                case 20: out = { RenderFormat::BC3_UNORM, 4, 4, 16, false }; return true;       // DXT4/5
                case 32: case 29: out = { RenderFormat::R16G16B16A16_FLOAT, 1, 1, 8, false }; return true; // 29 = _EXPAND, float16 per Xenia
                case 31: case 28: out = { RenderFormat::R16G16_FLOAT, 1, 1, 4, false }; return true;
                case 30: case 27: out = { RenderFormat::R16_FLOAT, 1, 1, 2, false }; return true;
                case 36: out = { RenderFormat::R32_FLOAT, 1, 1, 4, false }; return true;
                default: return false;
                }
            }

            // A texture fetch at a resolved address reads the resolve's output when
            // the formats agree (the *_AS_16_16_16_16 aliases fetch the same bits).
            static constexpr uint32_t kDepthResolveTag = 0x1000; // destFormat tag for depth resolves

            static bool ResolveFormatMatches(uint32_t fetchFormat, uint32_t destFormat)
            {
                if (destFormat & kDepthResolveTag) return fetchFormat == 22 || fetchFormat == 23; // k_24_8(_FLOAT) reads the depth plane
                if (fetchFormat == destFormat) return true;
                if (fetchFormat == 29) return destFormat == 32;                      // k_16_16_16_16_EXPAND is float16 (Xenia)
                if (fetchFormat == 28) return destFormat == 31;
                if (fetchFormat == 27) return destFormat == 30;
                if (destFormat == 7) return fetchFormat == 54;                       // k_2_10_10_10
                if (destFormat == 6) return fetchFormat == 14 || fetchFormat == 50 || fetchFormat == 62; // k_8_8_8_8 aliases
                return false;
            }

            static uint32_t Expand(uint32_t v, uint32_t bits) { return bits == 0 ? 255 : (v * 255 + ((1u << bits) - 1) / 2) / ((1u << bits) - 1); }

            void DescribeBindingTexture(binding::Texture* output, const HostTexture& texture,
                binding::TextureKind kind, uint64_t epoch) const noexcept
            {
                if (!output) return;
                output->kind = kind;
                output->hostFormat = uint32_t(texture.format);
                output->hostExtent = {texture.bindingWidth ? texture.bindingWidth : texture.width,
                    texture.bindingHeight ? texture.bindingHeight : texture.height};
                output->parentExtent = output->hostExtent;
                texture.bindingProducer.Describe(*output, epoch, frame);
            }

            HostTexture* GetTexture(const uint32_t* fetch, uint32_t dimension,
                binding::Texture* bindingInfo = nullptr, uint64_t bindingEpoch = 0)
            {
                uint32_t format = fetch[1] & 0x3F;
                uint32_t endian = (fetch[1] >> 6) & 3;
                uint32_t base = (fetch[1] >> 12) << 12;
                bool tiled = (fetch[0] >> 31) & 1;
                uint32_t pitch32 = (fetch[0] >> 22) & 0x1FF;
                uint32_t width = (fetch[2] & 0x1FFF) + 1;
                uint32_t height = ((fetch[2] >> 13) & 0x1FFF) + 1;
                if (dimension == 2) // 3D
                {
                    width = (fetch[2] & 0x7FF) + 1;
                    height = ((fetch[2] >> 11) & 0x7FF) + 1;
                }

                const bool packedMips = ((fetch[5] >> 11) & 1) != 0;
                const uint32_t originalWidth = width, originalHeight = height;
                const uint32_t sourceMip = base == 0 ? std::max<uint32_t>(1, (fetch[4] >> 2) & 0xF) : 0;
                const uint32_t sourceAddress = base ? base : (fetch[5] >> 12) << 12;
                if (bindingInfo) {
                    *bindingInfo = {};
                    bindingInfo->guestFormat = format;
                    bindingInfo->dimension = dimension;
                    bindingInfo->sourceMip = sourceMip;
                    bindingInfo->guestExtent = {originalWidth, originalHeight};
                }
                TextureKey key{ sourceAddress, format, width, height, (tiled ? 1u : 0u) | (endian << 1) | (pitch32 << 3) | (dimension << 12) | (uint32_t(packedMips) << 14) | (sourceMip << 15) };
                // LO_NO_DEPTH_FETCH=1: hand shaders a constant instead of the resolved
                // depth, to tell depth-driven artefacts from shading ones.
                static const bool noDepthFetch = getenv("LO_NO_DEPTH_FETCH") != nullptr;
                if (noDepthFetch && (format == 22 || format == 23)) {
                    DescribeBindingTexture(bindingInfo, dummyTexture2D, binding::TextureKind::Dummy, bindingEpoch);
                    return &dummyTexture2D;
                }
                if (ResolvedSurface* rs = FindResolved(base, format))
                {
                    DescribeBindingTexture(bindingInfo, *rs->tex, binding::TextureKind::Resolved, bindingEpoch);
                    if (bindingInfo && rs->writeOrdinal) {
                        bindingInfo->resolveFrameAge = binding::RelativeAge(frame, rs->frame);
                        bindingInfo->resolveGap = binding::RelativeAge(resolveWriteOrdinal, rs->writeOrdinal);
                        bindingInfo->resolveRect = {rs->writeX, rs->writeY, rs->writeWidth, rs->writeHeight};
                    }
                    const uint32_t physicalWidth = std::max(1u, rs->tex->ScaleX(width));
                    const uint32_t physicalHeight = std::max(1u, rs->tex->ScaleY(height));
                    // Resolve pitch describes memory storage, whereas normalized
                    // sampling and GetDimensions use the fetch's logical size.
                    // For example, the 428-wide blur texture has a 448-pixel pitch.
                    // Sampling the padded resource shifts every subsequent blur pass.
                    if (dimension == 1 && width <= rs->tex->guestWidth && height <= rs->tex->guestHeight &&
                        (width != rs->tex->guestWidth || height != rs->tex->guestHeight))
                    {
                        const uint64_t viewKey = (uint64_t(width) << 32) | height;
                        auto& view = rs->fetchViews[viewKey];
                        if (!view || view->format != rs->tex->format || view->width != physicalWidth || view->height != physicalHeight)
                        {
                            if (view) Gpu().retiredTextures.push_back(std::move(view));
                            view = std::make_unique<HostTexture>();
                            view->format = rs->tex->format;
                            view->guestWidth = width;
                            view->guestHeight = height;
                            view->resolutionSize = rs->tex->resolutionSize;
                            view->width = physicalWidth;
                            view->height = physicalHeight;
                            view->texture = device->createTexture(RenderTextureDesc::Texture2D(physicalWidth, physicalHeight, 1, view->format));
                        }
                        if (!view->texture) {
                            FailCurrentPlan();
                            LOG_ERROR("renderer: fetch view allocation failed guest={}x{} physical={}x{}; native resolution fallback next frame", width, height, physicalWidth, physicalHeight);
                            return nullptr;
                        }
                        Transition(*rs->tex, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
                        Transition(*view, RenderTextureLayout::COPY_DEST, RenderBarrierStage::COPY);
                        RenderBox box{ 0, 0, int32_t(physicalWidth), int32_t(physicalHeight), 0, 1 };
                        commandList->copyTextureRegion(RenderTextureCopyLocation::Subresource(view->texture.get()),
                            RenderTextureCopyLocation::Subresource(rs->tex->texture.get()), 0, 0, 0, &box);
                        if (taa_collection::Enabled())
                            view->bindingProducer.Copy(rs->tex->bindingProducer, taa_collection::ConsentEpoch(), frame, true);
                        Transition(*view, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
                        if (bindingInfo) {
                            bindingInfo->kind = binding::TextureKind::CroppedResolve;
                            bindingInfo->hostExtent = {physicalWidth, physicalHeight};
                        }
                        return view.get();
                    }
                    Transition(*rs->tex, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
                    return rs->tex.get();
                }
                auto it = textures.find(key);
                if (it != textures.end())
                {
                    HostTexture* cached = it->second.get();
                    // Titles stream texture data in after the first draw that uses
                    // it, so a cached upload can be all zeroes forever. Re-hash the
                    // guest bytes once per frame and re-upload when they change.
                    if (!textureRevalidate || cached->checkedFrame == frame || cached->guestBytes == 0) {
                        DescribeBindingTexture(bindingInfo, *cached, binding::TextureKind::GuestUpload, bindingEpoch);
                        return cached;
                    }
                    cached->checkedFrame = frame;
                    const uint64_t now = SampleHash(Phys(cached->guestAddress), cached->guestBytes);
                    if (now == cached->guestHash) {
                        DescribeBindingTexture(bindingInfo, *cached, binding::TextureKind::GuestUpload, bindingEpoch);
                        return cached;
                    }
                    textureReuploads++;
                    Gpu().retiredTextures.push_back(std::move(it->second));
                    textures.erase(it);
                }

                ScopedTimer timer{ tTexture, cpuTimingEnabled };
                nTexture++;
                TextureFormatInfo fi;
                if (!GetTextureFormat(format, fi))
                {
                    if (loggedFormats.insert(format).second)
                        LOG_WARNING("renderer: unsupported texture format {} ({}x{} at {:#x})", format, width, height, base);
                    return nullptr;
                }
                // A fetch constant with no base address stores its data in the mip
                // chain instead; take the largest available level and scale the
                // dimensions to it (Xenia: mip_address / mip_min_level).
                if (base == 0)
                {
                    if (sourceAddress == 0)
                        return nullptr;
                    width = std::max<uint32_t>(1, width >> sourceMip);
                    height = std::max<uint32_t>(1, height >> sourceMip);
                    pitch32 = std::max<uint32_t>(1, pitch32 >> sourceMip);
                }

                // Even level zero lives inside the packed tail when a texture's
                // shorter dimension is <= 16. Apply the block origin before
                // tiling; adding an offset to the resulting byte address is wrong.
                const TextureBlockOffset packedOffset = packedMips && dimension != 2
                    ? PackedMipOffset2D(originalWidth, originalHeight, sourceMip, fi.blockWidth, fi.blockHeight)
                    : TextureBlockOffset{};
                // Guest layout: blocks, pitch in blocks aligned to the 32-block macro tile.
                uint32_t blocksX = (width + fi.blockWidth - 1) / fi.blockWidth;
                uint32_t blocksY = (height + fi.blockHeight - 1) / fi.blockHeight;
                uint32_t pitchBlocks = std::max<uint32_t>((pitch32 * 32) / fi.blockWidth, packedOffset.x + blocksX);
                pitchBlocks = (pitchBlocks + 31) & ~31u;
                uint32_t bpbLog2 = fi.bytesPerBlock == 1 ? 0 : fi.bytesPerBlock == 2 ? 1 : fi.bytesPerBlock == 4 ? 2 : fi.bytesPerBlock == 8 ? 3 : 4;

                const uint8_t* src = Phys(sourceAddress);
                uint32_t hostBpp = fi.convertToRgba8 ? 4 : fi.bytesPerBlock;
                uint32_t rowBytes = blocksX * hostBpp;
                uint32_t rowPitch = (rowBytes + 255) & ~255u;
                // Cube faces sit back to back, each face's tiled image padded to the
                // 4 KB subresource alignment (xenos.h kTextureSubresourceAlignment).
                const uint32_t faces = dimension == 3 ? 6u : 1u;
                const uint32_t blocksYAligned = (packedOffset.y + blocksY + 31) & ~31u;
                const uint32_t faceStride = ((pitchBlocks * blocksYAligned * fi.bytesPerBlock) + 4095u) & ~4095u;
                std::vector<uint8_t> staging(size_t(rowPitch) * blocksY * faces);
                std::vector<uint8_t> block(fi.bytesPerBlock);
                for (uint32_t f = 0; f < faces; f++)
                for (uint32_t by = 0; by < blocksY; by++)
                {
                    uint8_t* dstRow = staging.data() + (size_t(f) * blocksY + by) * rowPitch;
                    for (uint32_t bx = 0; bx < blocksX; bx++)
                    {
                        const uint32_t sx = bx + packedOffset.x, sy = by + packedOffset.y;
                        uint32_t offset = tiled ? video::TiledOffset2D(sx, sy, pitchBlocks, bpbLog2) : (sy * pitchBlocks + sx) * fi.bytesPerBlock;
                        memcpy(block.data(), src + size_t(f) * faceStride + offset, fi.bytesPerBlock);
                        // Endian swap within the block.
                        if (endian == 1 || (endian == 2 && fi.bytesPerBlock == 2))
                            for (uint32_t i = 0; i + 1 < fi.bytesPerBlock; i += 2) std::swap(block[i], block[i + 1]);
                        else if (endian == 2)
                            for (uint32_t i = 0; i + 3 < fi.bytesPerBlock; i += 4) { std::swap(block[i], block[i + 3]); std::swap(block[i + 1], block[i + 2]); }
                        else if (endian == 3)
                            for (uint32_t i = 0; i + 3 < fi.bytesPerBlock; i += 4) { std::swap(block[i], block[i + 2]); std::swap(block[i + 1], block[i + 3]); }

                        uint8_t* dst = dstRow + size_t(bx) * hostBpp;
                        if (!fi.convertToRgba8)
                            memcpy(dst, block.data(), fi.bytesPerBlock);
                        else
                        {
                            uint16_t v = uint16_t(block[0] | (block[1] << 8));
                            switch (format)
                            {
                            case 3: // 1_5_5_5: a in bit 15
                                dst[0] = uint8_t(Expand(v & 31, 5)); dst[1] = uint8_t(Expand((v >> 5) & 31, 5)); dst[2] = uint8_t(Expand((v >> 10) & 31, 5)); dst[3] = (v & 0x8000) ? 255 : 0; break;
                            case 4: // 5_6_5
                                dst[0] = uint8_t(Expand(v & 31, 5)); dst[1] = uint8_t(Expand((v >> 5) & 63, 6)); dst[2] = uint8_t(Expand(v >> 11, 5)); dst[3] = 255; break;
                            case 15: // 4_4_4_4
                                dst[0] = uint8_t(Expand(v & 15, 4)); dst[1] = uint8_t(Expand((v >> 4) & 15, 4)); dst[2] = uint8_t(Expand((v >> 8) & 15, 4)); dst[3] = uint8_t(Expand(v >> 12, 4)); break;
                            case 7: case 54: // 2_10_10_10: r in the low bits, a in the top two
                            {
                                uint32_t w; memcpy(&w, block.data(), 4);
                                dst[0] = uint8_t((w & 0x3FF) >> 2); dst[1] = uint8_t(((w >> 10) & 0x3FF) >> 2); dst[2] = uint8_t(((w >> 20) & 0x3FF) >> 2); dst[3] = uint8_t(Expand(w >> 30, 2));
                                break;
                            }
                            }
                        }
                    }
                }

                auto tex = std::make_unique<HostTexture>();
                tex->format = fi.host;
                tex->width = width;
                tex->height = height;
                tex->guestAddress = sourceAddress;
                tex->guestBytes = uint32_t(std::min<uint64_t>(uint64_t(faceStride) * faces, 64u << 20));
                tex->guestHash = SampleHash(src, tex->guestBytes);
                tex->checkedFrame = frame;
                uint32_t texWidth = fi.blockWidth > 1 ? blocksX * fi.blockWidth : width;
                uint32_t texHeight = fi.blockHeight > 1 ? blocksY * fi.blockHeight : height;
                tex->bindingWidth = texWidth; tex->bindingHeight = texHeight;
                if (dimension == 3)
                    tex->texture = device->createTexture(RenderTextureDesc::Texture(RenderTextureDimension::TEXTURE_2D, texWidth, texHeight, 1, 1, 6, fi.host, RenderTextureFlag::CUBE));
                else
                    tex->texture = device->createTexture(RenderTextureDesc::Texture2D(texWidth, texHeight, 1, fi.host));
                tex->layout = RenderTextureLayout::UNKNOWN;
                if (!tex->texture)
                {
                    LOG_WARNING("renderer: texture creation failed fmt={} {}x{}", format, texWidth, texHeight);
                    return nullptr;
                }

                texBytes += staging.size();
                uint64_t offset = Upload(staging.data(), staging.size(), 512);
                if (offset == UINT64_MAX)
                    return nullptr;
                Transition(*tex, RenderTextureLayout::COPY_DEST, RenderBarrierStage::COPY);
                for (uint32_t f = 0; f < faces; f++)
                    commandList->copyTextureRegion(
                        RenderTextureCopyLocation::Subresource(tex->texture.get(), 0, f),
                        RenderTextureCopyLocation::PlacedFootprint(uploadRing, fi.host, texWidth, texHeight, 1, (rowPitch / hostBpp) * fi.blockWidth,
                            offset + uint64_t(f) * rowPitch * blocksY));
                Transition(*tex, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);

                HostTexture* result = tex.get();
                DescribeBindingTexture(bindingInfo, *result, binding::TextureKind::GuestUpload, bindingEpoch);
                textures.emplace(key, std::move(tex));
                return result;
            }

            void InvalidateRange(uint32_t address, uint32_t size)
            {
                for (auto it = textures.begin(); it != textures.end();)
                {
                    uint32_t texSize = it->first.width * it->first.height * 4; // conservative
                    bool overlap = it->first.address < address + size && address < it->first.address + texSize;
                    if (overlap)
                    {
                        // The command list being recorded may still reference it, so
                        // hand it to the retired list (freed after the next fence wait).
                        Gpu().retiredTextures.push_back(std::move(it->second));
                        it = textures.erase(it);
                    }
                    else ++it;
                }
            }

            // ---- pipeline state ---------------------------------------------------------
            static RenderBlend BlendFactor(uint32_t f)
            {
                switch (f)
                {
                case 0: return RenderBlend::ZERO;
                case 1: return RenderBlend::ONE;
                case 4: return RenderBlend::SRC_COLOR;
                case 5: return RenderBlend::INV_SRC_COLOR;
                case 6: return RenderBlend::SRC_ALPHA;
                case 7: return RenderBlend::INV_SRC_ALPHA;
                case 8: return RenderBlend::DEST_COLOR;
                case 9: return RenderBlend::INV_DEST_COLOR;
                case 10: return RenderBlend::DEST_ALPHA;
                case 11: return RenderBlend::INV_DEST_ALPHA;
                case 12: return RenderBlend::BLEND_FACTOR;
                case 13: return RenderBlend::INV_BLEND_FACTOR;
                case 14: return RenderBlend::BLEND_FACTOR;
                case 15: return RenderBlend::INV_BLEND_FACTOR;
                case 16: return RenderBlend::SRC_ALPHA_SAT;
                default: return RenderBlend::ONE;
                }
            }

            static RenderBlendOperation BlendOp(uint32_t op)
            {
                switch (op)
                {
                case 1: return RenderBlendOperation::SUBTRACT;
                case 2: return RenderBlendOperation::MIN;
                case 3: return RenderBlendOperation::MAX;
                case 4: return RenderBlendOperation::REV_SUBTRACT;
                default: return RenderBlendOperation::ADD;
                }
            }

            static RenderComparisonFunction Compare(uint32_t f)
            {
                static const RenderComparisonFunction table[] = {
                    RenderComparisonFunction::NEVER, RenderComparisonFunction::LESS, RenderComparisonFunction::EQUAL, RenderComparisonFunction::LESS_EQUAL,
                    RenderComparisonFunction::GREATER, RenderComparisonFunction::NOT_EQUAL, RenderComparisonFunction::GREATER_EQUAL, RenderComparisonFunction::ALWAYS };
                return table[f & 7];
            }

            RenderPipeline* GetPipeline(const PipelineKey& key, Shader* vs, Shader* ps, RenderFormat, RenderFormat)
            {
                auto it = pipelines.find(key);
                if (it != pipelines.end()) {
                    return it->second.get();
                }
                ScopedTimer timer{ tPipeline, cpuTimingEnabled };
                nPipeline++;
                ++runtimePipelineCreates;
                auto pipeline = CreatePipeline(key, vs, ps, true);
                RenderPipeline* result = pipeline.get();
                // A failed speculative creation must not poison the draw cache.
                if (result) {
                    pipelines.emplace(key, std::move(pipeline));
                    if (pipelineCacheEnabled && pipelineRecipes.size() < gpu::pipeline_cache::kMaxRecords &&
                        gpu::pipeline_cache::IsValid(key) && ValidPipelineRecipe(key) && pipelineRecipes.insert(key).second)
                        pipelineRecipesDirty = true;
                }
                return result;
            }

            std::unique_ptr<RenderPipeline> CreatePipeline(const PipelineKey& key, Shader* vs, Shader* ps, bool trace)
            {
                return device->createGraphicsPipeline(DescribePipeline(key, vs, ps, trace));
            }
            RenderGraphicsPipelineDesc DescribePipeline(const PipelineKey& key, Shader* vs, Shader* ps, bool trace)
            {
                const auto rtFormat = static_cast<RenderFormat>(key.rtFormat);
                const auto depthFormat = static_cast<RenderFormat>(key.depthFormat);

                RenderGraphicsPipelineDesc desc;
                desc.pipelineLayout = pipelineLayout.get();
                desc.vertexShader = vs->shader.get();
                desc.pixelShader = ps ? ps->shader.get() : nullptr;
                if (key.prim == 8 && rectListGs)
                    desc.geometryShader = rectListGs.get();

                uint32_t depthControl = key.depthControl;
                desc.depthEnabled = (depthControl & 2) != 0 && depthFormat != RenderFormat::UNKNOWN;
                desc.depthWriteEnabled = (depthControl & 4) != 0;
                desc.depthFunction = desc.depthEnabled ? Compare((depthControl >> 4) & 7) : RenderComparisonFunction::ALWAYS;
                static const bool noDepth = getenv("LO_NO_DEPTH") != nullptr; // debugging: pass every fragment
                if (noDepth)
                    desc.depthFunction = RenderComparisonFunction::ALWAYS;
                desc.depthClipEnabled = true;
                desc.depthTargetFormat = depthFormat;
                desc.depthBias = key.depthBias;
                desc.slopeScaledDepthBias = std::bit_cast<float>(key.slopeBias);
                if (trace && getenv("LO_TRACE_POLYGON_OFFSET") && (key.modeCull & 0x3800))
                {
                    static uint32_t reports = 0;
                    if (reports++ < 128)
                        LOG_INFO("renderer: polygon offset vs={:016x} ps={:016x} mode={:#x} depth={:#x} bias={} slope={}",
                            key.vs, key.ps, key.modeCull, key.depthControl, key.depthBias, desc.slopeScaledDepthBias);
                }
                desc.stencilEnabled = (depthControl & 1) != 0 && depthFormat != RenderFormat::UNKNOWN;
                if (desc.stencilEnabled)
                {
                    if (trace && getenv("LO_STENCIL_TRACE"))
                        SHADER_LOG_INFO("pipeline-state", RendererByteFnv, "renderer: stencil pipeline vs={:016x} ps={:016x} ctl={:#x} refs={:#x}/{:#x} mask={:#x}", key.vs, key.ps, depthControl, key.stencilRefMask, key.stencilRefMaskBack, key.colorMask);
                    static constexpr RenderStencilOp ops[] = {
                        RenderStencilOp::KEEP, RenderStencilOp::ZERO, RenderStencilOp::REPLACE,
                        RenderStencilOp::INCREMENT_AND_CLAMP, RenderStencilOp::DECREMENT_AND_CLAMP,
                        RenderStencilOp::INVERT, RenderStencilOp::INCREMENT_AND_WRAP, RenderStencilOp::DECREMENT_AND_WRAP };
                    auto face = [&](uint32_t control) {
                        RenderStencilFaceDesc f;
                        f.compareFunction = Compare(control & 7);
                        f.failOp = ops[(control >> 3) & 7];
                        f.passOp = ops[(control >> 6) & 7];
                        f.depthFailOp = ops[(control >> 9) & 7];
                        return f;
                    };
                    desc.stencilReference = key.stencilRefMask & 0xFF;
                    desc.stencilReadMask = (key.stencilRefMask >> 8) & 0xFF;
                    desc.stencilWriteMask = (key.stencilRefMask >> 16) & 0xFF;
                    desc.stencilFrontFace = face(depthControl >> 8);
                    desc.stencilBackFace = (depthControl & 0x80) ? face(depthControl >> 20) : desc.stencilFrontFace;
                    if (trace && (depthControl & 0x80) && key.stencilRefMask != key.stencilRefMaskBack)
                        LOG_WARNING("renderer: distinct front/back stencil masks {:#x}/{:#x}", key.stencilRefMask, key.stencilRefMaskBack);
                }

                uint32_t blend = key.blend;
                RenderBlendDesc& rt = desc.renderTargetBlend[0];
                rt.srcBlend = BlendFactor(blend & 0x1F);
                rt.blendOp = BlendOp((blend >> 5) & 7);
                rt.dstBlend = BlendFactor((blend >> 8) & 0x1F);
                rt.srcBlendAlpha = BlendFactor((blend >> 16) & 0x1F);
                rt.blendOpAlpha = BlendOp((blend >> 21) & 7);
                rt.dstBlendAlpha = BlendFactor((blend >> 24) & 0x1F);
                rt.blendEnabled = !(rt.srcBlend == RenderBlend::ONE && rt.dstBlend == RenderBlend::ZERO && rt.blendOp == RenderBlendOperation::ADD &&
                                    rt.srcBlendAlpha == RenderBlend::ONE && rt.dstBlendAlpha == RenderBlend::ZERO && rt.blendOpAlpha == RenderBlendOperation::ADD);
                static const bool noBlend = getenv("LO_NO_BLEND") != nullptr; // debugging
                if (noBlend)
                    rt.blendEnabled = false;
                rt.renderTargetWriteMask = uint8_t(key.colorMask & 0xF);
                desc.renderTargetFormat[0] = rtFormat;
                desc.renderTargetCount = rtFormat != RenderFormat::UNKNOWN ? 1 : 0;

                uint32_t modeCull = key.modeCull;
                bool cullFront = modeCull & 1, cullBack = (modeCull >> 1) & 1;
                desc.cullMode = (cullFront && cullBack) ? RenderCullMode::NONE : cullFront ? RenderCullMode::FRONT : cullBack ? RenderCullMode::BACK : RenderCullMode::NONE;
                static const bool noCull = getenv("LO_NO_CULL") != nullptr; // debugging
                if (noCull)
                    desc.cullMode = RenderCullMode::NONE;
                desc.frontFace = ((modeCull >> 2) & 1) ? RenderFrontFace::CLOCKWISE : RenderFrontFace::COUNTER_CLOCKWISE;

                switch (key.prim)
                {
                case 1: desc.primitiveTopology = RenderPrimitiveTopology::POINT_LIST; break;
                case 2: desc.primitiveTopology = RenderPrimitiveTopology::LINE_LIST; break;
                case 3: desc.primitiveTopology = RenderPrimitiveTopology::LINE_STRIP; break;
                case 6: desc.primitiveTopology = RenderPrimitiveTopology::TRIANGLE_STRIP; break;
                default: desc.primitiveTopology = RenderPrimitiveTopology::TRIANGLE_LIST; break;
                }

                return desc;
            }

            // ---- vertex buffers ------------------------------------------------------------
            // Cheap change detection: everything for small buffers, otherwise the
            // head, the tail and 64 evenly spread 64-byte windows.
            static uint64_t SampleHash(const uint8_t* data, size_t bytes)
            {
                uint64_t h = 0x9E3779B97F4A7C15ull ^ bytes;
                auto mix = [&](const uint8_t* q, size_t n)
                {
                    for (size_t i = 0; i + 8 <= n; i += 8)
                    {
                        uint64_t v; memcpy(&v, q + i, 8);
                        h = (h ^ v) * 0x100000001B3ull;
                        h ^= h >> 29;
                    }
                };
                if (bytes <= 8192)
                {
                    mix(data, bytes);
                    return h;
                }
                mix(data, 512);
                mix(data + bytes - 512, 512);
                const size_t step = (bytes - 1024) / 64;
                for (int i = 0; i < 64; i++)
                    mix(data + 512 + size_t(i) * step, 64);
                return h;
            }

            // Returns the arena offset of the swapped copy of a guest vertex buffer.
            uint64_t GetVertexBuffer(uint32_t address, uint32_t sizeDwords, uint32_t endian)
            {
                const size_t bytes = size_t(sizeDwords) * 4;
                const uint64_t key = (uint64_t(address) << 32) | (uint64_t(sizeDwords) << 2) | endian;
                const uint8_t* guest = Phys(address);
                if (vertexTimingEnabled && vertexTiming.find.calls == 0)
                {
                    vertexTiming.initialSize = vertexCache.size();
                    vertexTiming.initialBuckets = vertexCache.bucket_count();
                    vertexTiming.initialEvictions = vertexCache.Evictions();
                }
                auto it = [&] {
                    VertexStageTimer timer(vertexTiming.find, vertexTimingEnabled, address, bytes);
                    return vertexCache.find(key);
                }();
                if (it != vertexCache.end())
                {
                    const bool matches = [&] {
                        VertexStageTimer timer(vertexTiming.match, vertexTimingEnabled, address, bytes);
                        return it->second.content.Matches(guest, bytes);
                    }();
                    if (gpu::render_arena::VertexCacheReusable(matches))
                    {
                        it->second.lastFrame = frame;
                        return it->second.offset;
                    }
                    // Contents changed. Overwriting in place would corrupt draws
                    // already recorded into an open command list, so allocate fresh.
                    if (it->second.slot == uint8_t(gpuSlot))
                        vertexRevalidations++;
                    {
                        VertexStageTimer timer(vertexTiming.erase, vertexTimingEnabled, address, bytes);
                        vertexCache.erase(it);
                    }
                }

                // Allocate (16-byte aligned, 16 bytes of slack for the shader's
                // last fetch). Prefer the current half; if it is full, append to
                // the other half without Flush. DrawImpl wraps only when neither
                // half can hold this copy.
                const size_t needed = ((bytes + 16 + 15) & ~size_t(15));
                const uint32_t otherSlot = (gpuSlot + 1) % kGpuSlots;
                const uint32_t allocSlot = gpu::render_arena::VertexAllocSlot(
                    gpuSlot, Gpu().arenaOffset, gpuSlots[otherSlot].arenaOffset, needed);
                if (allocSlot == gpu::render_arena::kGpuSlots)
                    return UINT64_MAX;
                uint64_t& local = gpuSlots[allocSlot].arenaOffset;
                const uint64_t offset = gpu::render_arena::SlotBase(allocSlot) + local;
                local += needed;
                VertexEntry entry{ offset, {}, frame, uint8_t(allocSlot) };
                {
                    VertexStageTimer timer(vertexTiming.capture, vertexTimingEnabled, address, bytes);
                    entry.content.Capture(guest, bytes);
                }
                {
                    VertexStageTimer timer(vertexTiming.copy, vertexTimingEnabled, address, bytes);
                    geometry_prepare::CopyDwordsSwapped(arenaMapped + offset, guest, sizeDwords, endian);
                    memset(arenaMapped + offset + bytes, 0, 16);
                }
                const size_t bucketsBefore = vertexTimingEnabled ? vertexCache.bucket_count() : 0;
                {
                    VertexStageTimer timer(vertexTiming.insert, vertexTimingEnabled, address, bytes);
                    vertexCache.emplace(key, std::move(entry));
                }
                if (vertexTimingEnabled && vertexCache.bucket_count() != bucketsBefore)
                    ++vertexTiming.rehashes;
                vertexUploads++;
                vertexBytesUploaded += bytes;
                return offset;
            }

            // ---- draw -------------------------------------------------------------------
            void Draw(const DrawInfo& info)
            {
                ScopedTimer timer{ tDraw, cpuTimingEnabled };
                if(collectionFrame!=frame){collectionFrame=frame;taa_collection::BeginDiagnosticsFrame(frame);}
                if (!debugCaptureDir.empty())
                {
                    debugTrace << fmt::format("draw {} prim={} indices={} indexed={} base={:#x} words={} endian={} index32={}\n",
                        debugDraw++, info.primitiveType, info.indexCount, info.indexed, info.indexBase, info.indexBufferWords, info.indexEndian, info.index32);
                    const bool first = debugRegisters.empty();
                    if (first) debugRegisters.resize(REGISTER_COUNT);
                    for (uint32_t i = 0; i < debugRegisters.size(); ++i)
                    {
                        const auto value = Reg(i);
                        if (first || debugRegisters[i] != value)
                            debugTrace << fmt::format("{:04x} {:08x}\n", i, value);
                        debugRegisters[i] = value;
                    }
                }
                DrawImpl(info);
            }

            void DrawImpl(const DrawInfo& info)
            {
                const uint32_t modeControl = Reg(REG_RB_MODECONTROL) & 7;
                // All draw-side writes, including uploads, AA, alias transfers and
                // optimized clears, are outside the consecutive-resolve window.
                if (modeControl != 6) consecutiveResolveCopies.Invalidate();
                if (!ApplyInternalResolution())
                    return;
#if defined(LO_GPU_PLUME)
                if (fsrAlphaBridge && activePlan.requestedUpscaler != upscaling::Upscaler::Fsr)
                    fsrAlphaBridge->Invalidate();
#endif
                // LO_DRAW_LIMIT=<n>: only record the first n draws of each frame,
                // to bisect which pass ruins the image.
                static const uint32_t drawLimit = getenv("LO_DRAW_LIMIT") ? strtoul(getenv("LO_DRAW_LIMIT"), nullptr, 10) : 0;
                if (drawLimit && drawsThisFrame >= drawLimit)
                    return;
                // Out of descriptor sets: submit what we have before this draw
                // uploads anything (Flush rewinds the upload ring and the pools).
                // All resource recycling happens BETWEEN draws: a Flush inside one
                // would rewind the descriptor pools and upload ring that this draw's
                // already-recorded state points at.
                const bool poolsFull = Gpu().setPoolUsed[1] >= descriptorBatchLimit ||
                    Gpu().setPoolUsed[2] >= descriptorBatchLimit || Gpu().setPoolUsed[3] >= descriptorBatchLimit;
                const bool ringLow = Gpu().uploadOffset + kUploadHeadroom > kUploadRingSize;
                const auto wrap = gpu::render_arena::EvaluateWrap(
                    gpuSlot, Gpu().arenaOffset, gpuSlots[(gpuSlot + 1) % kGpuSlots].arenaOffset);
                const bool arenaLow = wrap.action == gpu::render_arena::WrapAction::FlushAndRecycleIncoming;
                if (poolsFull || ringLow || arenaLow)
                {
                    render_batch::CpuTimer<> nestedFlushTimer(cpuTimingEnabled);
                    if (cpuTimingEnabled) {
                        descriptorSplits += poolsFull;
                        uploadSplits += ringLow;
                        arenaSplits += arenaLow;
                    }
                    if (!Flush()) return;
                    if (arenaLow)
                    {
                        const uint32_t incoming = wrap.incomingSlot;
                        const auto fences = gpu::render_arena::WrapFenceWaits(wrap);
                        if (fences.waitIncoming && !RecycleSlot(incoming)) return;
                        if (fences.waitSubmitted && !RecycleSlot(gpu::render_arena::SubmittedSlotAfterFlush(incoming))) return;
                        if (wrap.resetIncoming)
                        {
                            ResetSlotArena(incoming);
                            LOG_INFO("renderer: vertex arena slot {} reset", incoming);
                        }
                    }
                    if (!Begin()) return;
                    nestedFlushTimer.AddTo(tNestedFlush);
                }
                if (!Begin()) return;

                if (modeControl == 6)
                {
                    Resolve();
                    return;
                }
                if (modeControl != 4 && modeControl != 5)
                {
                    drops.mode++;
                    drops.modeMask |= 1u << modeControl;
                    return;
                }
                const bool trackBinding = taa_collection::Enabled();
                const uint64_t bindingEpoch = trackBinding ? taa_collection::ConsentEpoch() : 0;

                // Shaders come from the command processor's last IM_LOAD.
                uint32_t vsCount = 0, psCount = 0;
                uint64_t vsCommandHash = 0, psCommandHash = 0;
                const uint32_t* vsWords = nullptr;
                const uint32_t* psWords = nullptr;
                uint64_t vsHash = 0, psHash = 0;
                {
                    render_batch::CpuTimer<> shaderLookupTimer(cpuTimingEnabled);
                    vsWords = g_commandProcessor.GetActiveShader(false, vsCount, vsCommandHash);
                    psWords = g_commandProcessor.GetActiveShader(true, psCount, psCommandHash);
                    if (!vsWords || vsCount == 0)
                    {
                        shaderLookupTimer.AddTo(tShaderLookup);
                        if (debugShaderSources && !debugCaptureDir.empty())
                            debugShaderSources->Observe(true, 0, vsWords, vsCount, frame);
                        drops.shader++;
                        return;
                    }
                    vsHash = g_commandProcessor.GetActiveShaderByteHash(false);
                    psHash = modeControl == 4 && psWords && psCount
                        ? g_commandProcessor.GetActiveShaderByteHash(true) : 0;
                    shaderLookupTimer.AddTo(tShaderLookup);
                }
                Shader* vs = GetShader(false, vsWords, vsCount, vsHash);
                // RB_MODECONTROL=5 is depth-only: the last loaded pixel shader
                // is inactive, including its discard and depth exports. Running
                // a stale shadow-depth PS here corrupts stencil volume tests.
                Shader* ps = modeControl == 4 && psWords && psCount ? GetShader(true, psWords, psCount, psHash) : nullptr;
                if (debugShaderSources && !debugCaptureDir.empty() && !(modeControl == 4 && psWords && psCount))
                    debugShaderSources->NotePixelNotBound();
                if (!vs)
                {
                    drops.shader++;
                    return;
                }

                // Render targets.
                uint32_t surfaceInfo = Reg(REG_RB_SURFACE_INFO);
                uint32_t pitch = surfaceInfo & 0x3FFF;
                if (pitch == 0)
                {
                    drops.pitch++;
                    return;
                }
                uint32_t scissorBr = Reg(REG_PA_SC_WINDOW_SCISSOR_BR);
                uint32_t scissorTl = Reg(REG_PA_SC_WINDOW_SCISSOR_TL);
                uint32_t rtHeight = GuessTargetHeight(pitch, (scissorBr >> 16) & 0x3FFF);

                uint32_t colorInfo = Reg(REG_RB_COLOR_INFO);
                uint32_t depthInfo = Reg(REG_RB_DEPTH_INFO);
                uint32_t depthControl = Reg(REG_RB_DEPTHCONTROL);
                bool colorWrites = modeControl == 4;
                HostTexture* color = nullptr;
                HostTexture* depth = nullptr;
                {
                    ScopedTimer rtTimer{ tRt, cpuTimingEnabled };
                    // Restore before taking color: looking up depth afterwards
                    // must not retire the color pointer we just borrowed.
                    const RenderTargetKey drawKey{colorInfo & 0xFFF, ColorClassOf((colorInfo >> 16) & 0xF), pitch, 0, false};
                    if (!PreparePromotionAccess(drawKey, rtHeight, (depthControl & 3) != 0)) return;
                    color = AcquireColorTarget(colorInfo & 0xFFF, (colorInfo >> 16) & 0xF, pitch, rtHeight);
                    depth = (depthControl & 3) ? GetRenderTarget(depthInfo & 0xFFF, (depthInfo >> 16) & 1, pitch, rtHeight, true) : nullptr;
                    if (!color || !color->texture || ((depthControl & 3) && (!depth || !depth->texture))) {
                        ++drops.pitch;
                        return;
                    }
                    if (depth) depth->depthMsaa = (surfaceInfo >> 16) & 3;

                    Transition(*color, RenderTextureLayout::COLOR_WRITE, RenderBarrierStage::GRAPHICS);
                    if (depth)
                        Transition(*depth, RenderTextureLayout::DEPTH_WRITE, RenderBarrierStage::GRAPHICS);

                    // LO_CLEAR_RT=1: wipe every colour target the first time a frame
                    // touches it. Targets normally survive across frames, so a
                    // per-draw dump shows last frame's image until something covers
                    // it - which makes it impossible to tell which draw of THIS
                    // frame painted a given pixel.
                    // LO_CLEAR_RT=magenta paints the wipe bright instead of black, so
                    // anything the frame leaves untouched stands out in the final image.
                    static const char* clearTargets = getenv("LO_CLEAR_RT");
                    if (clearTargets && color->clearedFrame != frame)
                    {
                        static const bool loud = strcmp(clearTargets, "magenta") == 0;
                        color->clearedFrame = frame;
                        commandList->setFramebuffer(GetFramebuffer(color, nullptr));
                        commandList->clearColor(0, loud ? RenderColor(1.0f, 0.0f, 1.0f, 1.0f) : RenderColor(0.0f, 0.0f, 0.0f, 0.0f));
#if defined(LO_GPU_PLUME)
                        HandleFsrAlphaRgbWriter(*color, "debug_clear_rt");
#endif
                        if (trackBinding) color->bindingProducer.Clear(bindingEpoch, frame, true);
                        color->aaProvenance.Invalidate(frame,color->allocationSerial,true);
                        color->sdrProducerFrame = ~0ull;
                        if (loud)
                            LOG_INFO("renderer: frame {} wiped target base={:#x} fmt={} pitch={} {}x{}", frame, colorInfo & 0xFFF, (colorInfo >> 16) & 0xF, pitch, color->width, color->height);
                    }
                }

                // Pipeline.
                PipelineKey key{};
                float layerDepthOffset = 0.0f;
                {
                    render_batch::CpuTimer<> pipelineLookupTimer(cpuTimingEnabled);
                    key.vs = vsHash;
                key.ps = ps ? psHash : 0;
                key.blend = Reg(REG_RB_BLENDCONTROL0);
                key.depthControl = depthControl;
                key.stencilRefMask = Reg(REG_RB_STENCILREFMASK) & 0xFFFFFF;
                key.stencilRefMaskBack = Reg(REG_RB_STENCILREFMASK_BF) & 0xFFFFFF;
                // LO_DEBUG_NODEPTH=1 (+ LO_DEBUG_VS=<hash>): depth test ALWAYS for the
                // selected draws only, to tell "rejected by the depth test" from
                // "never rasterised" without disturbing the rest of the frame.
                {
                    static const bool debugNoDepth = getenv("LO_DEBUG_NODEPTH") != nullptr;
                    static const uint64_t debugVsForDepth = getenv("LO_DEBUG_VS") ? strtoull(getenv("LO_DEBUG_VS"), nullptr, 16) : 0;
                    if (debugNoDepth && (!debugVsForDepth || key.vs == debugVsForDepth))
                        key.depthControl = (key.depthControl & ~0x70u) | (7u << 4);
                }
                key.modeCull = Reg(REG_PA_SU_SC_MODE_CNTL) & 0x3807;
                if (depth && (depthControl & 2))
                {
                    // The supported polygonal draws are triangles, fans, strips
                    // and quads. Rectangle lists use the separate PARA enable.
                    const bool polygonal = info.primitiveType == 4 || info.primitiveType == 5 ||
                        info.primitiveType == 6 || info.primitiveType == 13;
                    const auto bias = gpu::GetPolygonOffset(key.modeCull, polygonal,
                        (depthInfo & (1u << 16)) != 0,
                        RegF(0x2380), RegF(0x2381), RegF(0x2382), RegF(0x2383));
                    // Same-binary A/B regression switch; normal rendering applies
                    // guest bias without changing exposure or shadow materials.
                    static const bool disableBias = getenv("LO_NO_POLYGON_OFFSET") != nullptr;
                    if (!disableBias) {
                        key.depthBias = bias.constant;
                        key.slopeBias = std::bit_cast<uint32_t>(bias.slope);
                        // Read-only lighting layers need the guest's absolute bias.
                        // A D32 integer bias shrinks with the primitive's exponent:
                        // at Map12's reversed depth ~0.012, 24 ULPs are only ~2e-8,
                        // smaller than the base/light VS rounding difference. Keep
                        // depth writers on their existing rasterizer bias path.
                        static const bool legacyLayerBias = getenv("LO_LEGACY_LAYER_BIAS") != nullptr;
                        if (!legacyLayerBias && modeControl == 4 && ps && !ps->info.writesDepth && !(depthControl & 4) &&
                            (depthInfo & (1u << 16)) && bias.absolute != 0.0f) {
                            layerDepthOffset = bias.absolute;
                            key.depthBias = 0;
                        }
                    }
                }
                key.colorMask = colorWrites ? (Reg(REG_RB_COLOR_MASK) & 0xF) : 0;
                key.prim = info.primitiveType;
                key.rtFormat = uint32_t(color->format);
                key.depthFormat = depth ? uint32_t(depth->format) : 0;
                    pipelineLookupTimer.AddTo(tPipelineLookup);
                }
                RenderPipeline* pipeline = GetPipeline(key, vs, ps, color->format, depth ? depth->format : RenderFormat::UNKNOWN);
                if (!pipeline)
                {
                    drops.pipeline++;
                    drops.primMask |= 1u << (info.primitiveType & 31);
                    return;
                }
                // A draw with no color write mask rasterizes to its depth target.
                // Its bound color attachment can legitimately use a different
                // catalog mapping while the pixel shader still writes depth.
                const bool depthOnlyRaster = key.colorMask == 0 && depth != nullptr;
                HostTexture* rasterTarget = depthOnlyRaster ? depth : color;

                // Constants.
                render_batch::CpuTimer<> tConst0(cpuTimingEnabled);
                // Both halves of the ALU constant file are 256 vec4 wide
                // (0x4000-0x43FF for the vertex shader, 0x4400-0x47FF for the
                // pixel shader) and the translated HLSL declares float4 c[256]
                // for either stage. Uploading fewer left the tail reading back
                // as zero, which zeroed the light terms of every character
                // material - they index c[253..255].
                uint32_t vsConstants[256 * 4], psConstants[256 * 4];
                // Preserve the full banks and zero-register MMIO fallback. The
                // diagnostic switch provides a same-binary performance control.
                static const bool legacyConstants = getenv("LO_LEGACY_CONSTANT_READS") != nullptr;
                if (legacyConstants) {
                    for (uint32_t i = 0; i < 256 * 4; i++) vsConstants[i] = Reg(REG_ALU_CONSTANTS + i);
                    for (uint32_t i = 0; i < 256 * 4; i++) psConstants[i] = Reg(REG_ALU_CONSTANTS + 256 * 4 + i);
                } else {
                    g_commandProcessor.ReadRegisters(REG_ALU_CONSTANTS, 256 * 4, vsConstants);
                    g_commandProcessor.ReadRegisters(REG_ALU_CONSTANTS + 256 * 4, 256 * 4, psConstants);
                }
                // Diagnostic selection uses only GPU draw constants, not the CPU
                // presented-swap counter. Shader/layout recognition is deliberately
                // limited to the path verified in the captured Map2 scene.
                render_batch::CpuTimer<> taaInit(cpuTimingEnabled);
                if(sceneAAConfigFrame!=frame) {
                    sceneAAConfigFrame=frame;
                    frameSrOptions = {};
                    if (vulkan && activePlan.requestedUpscaler == upscaling::Upscaler::Fsr) {
                        const uint32_t percent = std::min(settings::GetConfig().fsrSharpnessPercent, 100u);
                        frameSrOptions = {percent != 0, float(percent) / 100.0f};
                    }
                    PollTaaDiagnostic();
                    PollTaaLive();
                    const char* dlssProbeValue = std::getenv("LO_DLSS_INPUT_PROBE");
                    const auto route=temporal::RouteConsumer(activePlan,dlssProbeValue&&std::string_view(dlssProbeValue)=="1");
                    const auto mode=route.effectiveAA;
                    if(sceneAAMode!=mode) {if(temporalHistory)temporalHistory->Reset();temporalSupportedFrame=~0ull;++temporalEpoch;}
                    sceneAAMode=mode;
                    temporal::FrameStartPolicy frameStart;
                    frameStart.legacyTaa = route.legacyTaa;
                    frameStart.dlssInputs = route.dlssInputs || route.sr;
                    frameStart.dlssSr = route.sr;
                    frameStart.inputProbe = route.inputProbe;
                    frameStart.resolveReadback = resolveReadback;
                    frameStart.forced = temporalForced;
                    frameStart.forcedHistory = temporalForcedHistory;
                    frameStart.forcedJitter = temporalForcedJitter;
                    frameStart.forcedStable = temporalForcedStable;
                    frameStart.frame = frame;
                    frameStart.supportedFrame = temporalSupportedFrame;
                    frameStart.diagnosticJitter = taaDiagnosticJitter;
                    frameStart.diagnosticHistory = taaDiagnosticHistory;
                    frameStart.motionSupportsDlss = motionOptions.Supports(upscaling::TemporalConsumer::DlssInputs);
                    const auto started = temporal::ResolveFrameStartConsumers(frameStart);
                    temporalExperiment = started.experiment;
                    temporalAllowHistory = started.allowHistory;
                    temporalJitter = started.jitter;
                    temporalStableGrid = started.stableGrid;
                    temporalInputProbe = started.inputProbe;
                    dlssSrRequested = started.dlssSr;
                    if (started.rejectDlss)
                        DisableDlssRequest(frame_plan::FailureReason::InvalidInput);
                    activeSpatialAA = route.spatialAA&&!resolveReadback;
                    const bool temporalActive = temporal::TemporalConsumerActive(temporalExperiment, temporalInputProbe, dlssSrRequested);
                    if(temporalActive&&!temporalHistory&&!temporalInitFailed) {
                        temporalHistory=std::make_unique<temporal::HistoryOwner>();
                        if(!temporalHistory->Init(device,sparseCollector)) {temporalHistory.reset();temporalInitFailed=true;LOG_ERROR("renderer: TAA initialization failed; SMAA fallback");}
                    }
                    if(!temporalHistory) { temporalExperiment=false; temporalInputProbe=false; dlssSrRequested=false; }
                    if (temporalExperiment && taaDiagnosticHDR == 1 && !hdrTemporalHistory && !hdrTemporalInitFailed) {
                        hdrTemporalHistory = std::make_unique<temporal::HistoryOwner>();
                        if (!hdrTemporalHistory->Init(device, {}, true)) {
                            hdrTemporalHistory.reset(); hdrTemporalInitFailed = true;
                            LOG_ERROR("renderer: HDR temporal candidate initialization failed");
                        }
                    }
                }
                const bool temporalActive = temporal::TemporalConsumerActive(temporalExperiment, temporalInputProbe, dlssSrRequested);
                const bool trackTemporalScene = !debugCaptureDir.empty() || temporalActive || activeSpatialAA;
                if (trackTemporalScene && temporalScene.Frame() != frame) {
                    const auto now = std::chrono::steady_clock::now();
                    srFrameDeltaMilliseconds = srTimedFrame == ~0ull ? 0.0f :
                        std::chrono::duration<float, std::milli>(now - srFrameTime).count();
                    srTimeReset = srTimedFrame == ~0ull || srTimedFrame + 1 != frame || srFrameDeltaMilliseconds > 250.0f;
                    srFrameTime = now; srTimedFrame = frame;
                    frameRasterJitter = temporal::FrameJitter(frame, activePlan.width, activePlan.height, ActiveTaaOptions().jitter_scale);
                    temporalScene.Reset(frame);
#if defined(LO_GPU_PLUME)
                    fsrAlphaRawViews.clear();
                    if (fsrAlphaReplayEnabled && fsrAlphaBridgeEnabled && vulkan &&
                        activePlan.requestedUpscaler == upscaling::Upscaler::Fsr && !fsrAlphaInitFailed) {
                        if (!fsrAlphaReplay) {
                            fsrAlphaReplay = std::make_unique<fsr_alpha::ReplayGPU>();
                            if (!fsrAlphaReplay->Init(device, setBuilders, 5)) {
                                LOG_WARNING("fsr alpha: replay initialization failed");
                                fsrAlphaReplay.reset(); fsrAlphaInitFailed = true;
                            }
                        }
                        if (fsrAlphaReplay && !fsrAlphaBridge)
                            fsrAlphaBridge = std::make_unique<fsr_alpha::PropagationGPU>(device);
                        if (fsrAlphaBridge && !fsrAlphaPostprocess) {
                            auto replay = std::make_unique<fsr_alpha::PostprocessGPU>();
                            if (replay->Init(device, pipelineLayout.get(), blitVs.get()))
                                fsrAlphaPostprocess = std::move(replay);
                            else LOG_WARNING("fsr alpha: postprocess initialization failed");
                        }
                    }
#endif
                    hdrTemporalOutput = nullptr; hdrTemporalSource = {}; hdrTonemapApplied = false;
                    actualRasterJitter = {}; actualRasterJitterCaptured = false;
                    temporal::ApplyTemporalLongInterval(temporalHistory.get(), std::chrono::steady_clock::now(), frame,
                        temporalInputProbe, dlssSrRequested, taaDiagnosticJitter, temporalForcedJitter,
                        temporalFrameTime, temporalJitter, temporalSupportedFrame, temporalEpoch, temporalGapResetFrame);
#if defined(LO_GPU_PLUME)
                    if (activePlan.requestedUpscaler == upscaling::Upscaler::Fsr) {
                        if (fsrAlphaReplay) fsrAlphaReplay->BeginFrame(frame, temporalEpoch);
                        if (fsrAlphaBridge) fsrAlphaBridge->BeginFrame(frame, temporalEpoch);
                    }
                    fsrAlphaBridgePairCount = fsrAlphaBridgeTraceCount = 0;
#endif
                }
                if(temporalActive&&temporalHistory) {
                    static const char* diagnosticStart=getenv("LO_TEMPORAL_LOG_START_FRAME");
                    static const uint64_t diagnosticFrame=diagnosticStart?strtoull(diagnosticStart,nullptr,10):0;
                    static const bool withTrace = getenv("LO_TEMPORAL_DRAW_LOG_WITH_RESOLVE_TRACE") &&
                        strcmp(getenv("LO_TEMPORAL_DRAW_LOG_WITH_RESOLVE_TRACE"), "1") == 0;
                    temporalHistory->BeginFrame(frame,temporalEpoch,
                        taa_collection::DiagnosticsActive() || (diagnosticStart&&frame>=diagnosticFrame&&temporalFramesLogged<256) || (withTrace&&resolveTraceRemaining));
                    if (hdrTemporalHistory) hdrTemporalHistory->BeginFrame(frame, temporalEpoch, resolveTraceRemaining != 0);
                    const bool gpuTiming = motionOptions.timing || (taaLiveApplied && taaLiveOptions.gpu_timing);
                    temporalHistory->EnableGpuTiming(gpuTiming);
                    if (hdrTemporalHistory) hdrTemporalHistory->EnableGpuTiming(gpuTiming);
                    if (motionOptions.enabled) {
                        render_batch::CpuTimer<> mvTimer(motionOptions.timing);
                        drawTemporalTracker.BeginFrame(frame, temporalEpoch);
                        mvTimer.AddTo(mvTrackCpuMs);
                        if (motionView.frame != frame || motionView.epoch != temporalEpoch) motionView = {};
                        if (motionOptions.replay && !motionReplay && !motionInitFailed) {
                            motionReplay = std::make_unique<temporal::MotionReplayGPU>();
                            if (!motionReplay->Init(device, setBuilders, vulkan ? 5 : 4)) {
                                LOG_ERROR("mv: initialization failed; requested geometric history rejected: {}", motionReplay->LastError());
                                motionReplay.reset(); motionInitFailed = true;
                            }
                        }
                        if (motionReplay) {motionReplay->EnableGpuTiming(gpuTiming);motionReplay->BeginFrame(frame, temporalEpoch);}
                    }
                }
                taaInit.AddTo(tTaa);
                std::optional<temporal::SceneResolve> temporalSceneCopy;
                std::optional<temporal::TemporalFrameInputs> dlssSceneCopyInputs;
                std::shared_ptr<fsr_alpha::MaskLease> selectedFsrMaskLease;
                bool sceneAARecorded=false,temporalAARecorded=false,hdrTonemapRecorded=false;
                std::optional<temporal::SceneAnchor> temporalDrawAnchor;

                SharedConstants shared{};
                for (uint32_t i = 0; i < 8; i++) shared.bools[i] = Reg(REG_BOOL_CONSTANTS + i);
                for (uint32_t i = 0; i < 32; i++) shared.loops[i] = Reg(REG_LOOP_CONSTANTS + i);

                uint32_t vte = Reg(REG_PA_CL_VTE_CNTL);
                float xs = RegF(REG_PA_CL_VPORT_XSCALE), xo = RegF(REG_PA_CL_VPORT_XSCALE + 1);
                float ys = RegF(REG_PA_CL_VPORT_XSCALE + 2), yo = RegF(REG_PA_CL_VPORT_XSCALE + 3);
                float zs = RegF(REG_PA_CL_VPORT_XSCALE + 4), zo = RegF(REG_PA_CL_VPORT_XSCALE + 5);
                RenderViewport viewport(0.0f, 0.0f, float(rasterTarget->guestWidth), float(rasterTarget->guestHeight));
                shared.ndcScale[0] = shared.ndcScale[1] = shared.ndcScale[2] = 1.0f;
                if (vte & 1) // viewport scale enabled: vertices are in NDC, use a real viewport
                {
                    float w = 2.0f * std::fabs(xs), h = 2.0f * std::fabs(ys);
                    viewport = RenderViewport(xo - std::fabs(xs), yo - std::fabs(ys), w > 0 ? w : float(rasterTarget->guestWidth), h > 0 ? h : float(rasterTarget->guestHeight));
                    if (ys > 0) shared.ndcScale[1] = -1.0f; // flipped viewport
                }
                else // screen-space vertices: map pixels to NDC ourselves
                {
                    shared.ndcScale[0] = 2.0f / float(rasterTarget->guestWidth);
                    shared.ndcScale[1] = -2.0f / float(rasterTarget->guestHeight);
                    shared.ndcOffset[0] = -1.0f;
                    shared.ndcOffset[1] = 1.0f;
                }
                // Z scale/offset map NDC z to the depth range; a negative scale (reversed
                // depth, the usual Xenos setup with GREATER_EQUAL tests and clear-to-0)
                // is expressed as minDepth > maxDepth, which D3D12 allows.
                // Applied in the vertex shader (z' = z * scale + offset) as Xenia does,
                // so a reversed range (scale -1, offset 1) needs no reversed host viewport.
                shared.ndcScale[2] = (vte & 0x10) ? zs : 1.0f;
                shared.ndcOffset[2] = (vte & 0x20) ? zo : 0.0f;
                const bool temporalViewport = temporal::IsJitterViewport(
                    {viewport.x, viewport.y, viewport.width, viewport.height}, vte, shared.ndcScale, shared.ndcOffset);
                // The absolute lighting bias changes Z only. Classify the guest
                // camera first so those lighting layers retain the base pass's XY jitter.
                shared.ndcOffset[2] += layerDepthOffset;
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                shared.vtxFmt = (vte >> 8) & 7;
                if ((Reg(REG_PA_SU_VTX_CNTL) & 1) == 0)
                {
                    shared.halfPixel[0] = 1.0f / viewport.width;
                    shared.halfPixel[1] = -1.0f / viewport.height;
                }
                // Shader constants stay in guest coordinates; only rasterization
                // and API pixel rectangles move to the physical target grid.
                const double rasterScaleX = double(rasterTarget->resolutionSize.width) / 1280.0;
                const double rasterScaleY = double(rasterTarget->resolutionSize.height) / 720.0;
                RenderViewport rasterViewport = viewport;
                rasterViewport.x *= rasterScaleX; rasterViewport.y *= rasterScaleY;
                rasterViewport.width *= rasterScaleX; rasterViewport.height *= rasterScaleY;
                render_batch::CpuTimer<> taaJitter(cpuTimingEnabled);
                const int temporalSlot=temporal::PositionVPSlot(key.vs);
                if((temporalActive||activeSpatialAA)&&temporalSlot>=0&&temporalViewport&&depth&&(depthControl&4)) {
                    temporal::SceneAnchor anchor;
                    std::copy_n(vsConstants+temporalSlot*4,16,anchor.vpBits.begin());
                    anchor.depthAllocation=depth->allocationSerial;
                    anchor.viewport={rasterViewport.x,rasterViewport.y,rasterViewport.width,rasterViewport.height,shared.ndcScale[1],shared.halfPixel[0],shared.halfPixel[1]};
                    temporalDrawAnchor=anchor;
                }
                const auto* jitterAnchor = temporalDrawAnchor ? &*temporalDrawAnchor :
                    (temporalScene.Draws() ? &temporalScene.Anchor() : nullptr);
                std::optional<temporal::SceneResolve> jitterSampledDepth;
                const bool jitterShadowPair = key.vs == 0x99c2b4b0960a9ccdull && key.ps == 0xd55a20d004031279ull;
                if (temporalActive && temporalJitter && jitterShadowPair)
                {
                    const uint32_t fetch0 = Reg(REG_FETCH_CONSTANTS), fetch1 = Reg(REG_FETCH_CONSTANTS + 1);
                    const uint32_t address = (fetch1 >> 12) << 12;
                    auto* source = (fetch0 & 3) == 2 ? FindResolved(address, fetch1 & 0x3f) : nullptr;
                    if (source && source->tex && source->tex->texture && source->tex->format == RenderFormat::R32_FLOAT &&
                        temporal::IsFullSceneDepthFetch(Reg(REG_FETCH_CONSTANTS + 2), Reg(REG_FETCH_CONSTANTS + 5),
                            source->tex->guestWidth, source->tex->guestHeight))
                        jitterSampledDepth = temporal::SceneResolve{source->frame, source->writeOrdinal,
                            address, source->destFormat, source->tex->width, source->tex->height,
                            source->writeX == 0 && source->writeY == 0 &&
                            source->writeWidth == source->tex->width && source->writeHeight == source->tex->height};
                }
                uint32_t collectionCandidates = 0;
                if (taa_collection::Enabled() && temporalExperiment && jitterAnchor && temporalViewport) {
                    constexpr int slots[] = {0,4,7,8,230,233};
                    for (unsigned i=0; i<6; ++i)
                        if (memcmp(vsConstants+slots[i]*4,jitterAnchor->vpBits.data(),16*sizeof(uint32_t))==0) collectionCandidates |= 1u<<i;
                }
                const uint32_t positionGuards=temporalExperiment && (taa_collection::Enabled() || temporalSlot<0) ?
                    temporal::PositionGuards(vs->position,temporalViewport,jitterAnchor,depth?depth->allocationSerial:0,
                        {rasterViewport.x,rasterViewport.y,rasterViewport.width,rasterViewport.height},vsConstants):0;
                binding::Transform bindingTransform;
                if (trackBinding) {
                    bindingTransform.slot = temporalSlot >= 0 ? temporalSlot :
                        (vs->positionReady && vs->position.kind == 1 && vs->position.issues == 0 ? vs->position.slot : -1);
                    bindingTransform.phase = uint32_t(frame % 32 + 1);
                    bindingTransform.viewport = {std::bit_cast<uint32_t>(float(rasterViewport.x)),
                        std::bit_cast<uint32_t>(float(rasterViewport.y)), std::bit_cast<uint32_t>(float(rasterViewport.width)),
                        std::bit_cast<uint32_t>(float(rasterViewport.height))};
                    if (bindingTransform.slot >= 0 && bindingTransform.slot <= 252)
                        std::copy_n(vsConstants + bindingTransform.slot * 4, 16, bindingTransform.guestVP.begin());
                }
                // Motion follows the proven main scene allocation. The same shader
                // hash in a shadow/offscreen view never authorizes replay.
                const bool motionScene = motionOptions.enabled && (!motionOptions.replay || motionReplay) && temporalActive && jitterAnchor && temporalViewport && depth &&
                    depth->allocationSerial == jitterAnchor->depthAllocation && rasterViewport.x == 0 && rasterViewport.y == 0 &&
                    rasterViewport.width == jitterAnchor->viewport.width && rasterViewport.height == jitterAnchor->viewport.height;
                const bool motionDepthWrite = motionScene && (depthControl & 6) == 6;
                const float motionSlopeBias = std::bit_cast<float>(key.slopeBias);
                const bool motionSupported = motionDepthWrite && !drawTemporalTracker.Failed() && temporalSlot >= 0 && temporalSlot <= 252 && !vs->info.textureSlotMask &&
                    !vs->info.usesPointSize && vs->info.errors.empty() && (!ps || (!ps->info.writesDepth && ps->info.errors.empty())) &&
                    // Replay uses the same rasterizer key, so a constant polygon offset
                    // remains self-consistent. Reject a real slope offset and NaNs.
                    !(key.depthControl & 1) && key.prim != 8 && std::isfinite(motionSlopeBias) &&
                    motionSlopeBias == 0.0f && layerDepthOffset == 0;
                // This six-index helper reads clip positions directly from vfetch95 and
                // has no previous transform to replay. Its final scene depth locally
                // rejects any underlying replay sample in the validity-mask pass.
                const bool motionLocallyMaskedClipWriter = motionDepthWrite &&
                    key.vs == 0x8bbd4da701845d16ull && temporalSlot < 0 && info.indexCount == 6;
                // Save only the words jitter can change. Collect copies the FULL
                // VS bank once and restores this proven matrix window in its arena.
                std::array<uint32_t, 16> motionOriginalVP;
                if (motionSupported) {
                    render_batch::CpuTimer<> timer(motionOptions.timing);
                    std::memcpy(motionOriginalVP.data(), vsConstants + temporalSlot * 4, sizeof(motionOriginalVP));
                    mvScratchBytes += sizeof(motionOriginalVP); timer.AddTo(mvTrackCpuMs);
                }
                uint64_t motionGeometry = 0xcbf29ce484222325ull;
                bool motionStreamsValid = true;
                const bool diagnosticMaterialBypass = taaDiagnosticMaterials == 0 &&
                    (key.vs == 0x3c86f4a89d220ee8ull || key.vs == 0xf3b9f20b3d3a62d5ull || key.vs == 0xe7b38eb08c70e5e1ull);
                const auto drawJitter = temporal::ApplyDrawJitter(key.vs, key.ps, frame,
                    temporalActive && temporalJitter && !diagnosticMaterialBypass, temporalViewport, jitterAnchor,
                    depth ? depth->allocationSerial : 0,
                    {rasterViewport.x, rasterViewport.y, rasterViewport.width, rasterViewport.height},
                    vsConstants, psConstants, &temporalScene.Depth(), jitterSampledDepth ? &*jitterSampledDepth : nullptr,
                    ActiveTaaOptions().jitter_scale, dlssSrRequested ? &frameRasterJitter : nullptr);
                if (temporalActive && drawJitter.applied) {
                    if (!actualRasterJitterCaptured) {
                        actualRasterJitter = drawJitter.sample;
                        actualRasterJitterCaptured = true;
                    } else if (actualRasterJitter.phase != drawJitter.sample.phase ||
                        actualRasterJitter.pixelX != drawJitter.sample.pixelX || actualRasterJitter.pixelY != drawJitter.sample.pixelY) {
                        temporalScene.Reset(frame);
                        actualRasterJitter = {};
                        actualRasterJitterCaptured = false;
                    }
                }
                const uint32_t collectionFlags=(temporalViewport?1u:0u)|(temporalJitter?2u:0u)|
                    (drawJitter.applied?4u:0u)|((depthControl&4)?8u:0u)|
                    (jitterAnchor&&depth&&depth->allocationSerial==jitterAnchor->depthAllocation?16u:0u);
                if (trackBinding) {
                    bindingTransform.applied = drawJitter.applied;
                    if (bindingTransform.slot >= 0 && bindingTransform.slot <= 252)
                        std::copy_n(vsConstants + bindingTransform.slot * 4, 16, bindingTransform.uploadedVP.begin());
                    if (drawJitter.applied) bindingTransform.jitterNdc = {
                        std::bit_cast<uint32_t>(drawJitter.sample.ndcX), std::bit_cast<uint32_t>(drawJitter.sample.ndcY)};
                }
                std::optional<binding::Record> bindingRecord;
                const uint32_t bindingPair = key.ps == 0x78a5c96b2d7eaa91ull ? 1 : 0;
                if (trackBinding && bindingRecordedFrame[bindingPair] != frame && key.vs == 0xe810cfacc107fd3cull &&
                    (key.ps == 0x5b11f88a8bb293dfull || key.ps == 0x78a5c96b2d7eaa91ull) &&
                    rasterViewport.width >= 1 && rasterViewport.width <= 7680 &&
                    rasterViewport.height >= 1 && rasterViewport.height <= 4320 &&
                    ps && (ps->info.textureSlotMask & 1) && taa_collection::WantBinding()) {
                    auto& record = bindingRecord.emplace();
                    record.vs = key.vs; record.ps = key.ps;
                    record.width = uint32_t(rasterViewport.width); record.height = uint32_t(rasterViewport.height);
                    record.slot = temporalSlot; record.candidates = collectionCandidates;
                    record.flags = collectionFlags; record.rejection = uint32_t(drawJitter.rejection);
                    record.position = vs->position; record.guards = positionGuards;
                    record.consumer = bindingTransform;
                    std::copy_n(psConstants, 4, record.psC0.begin());
                    record.texture.bank = ps->info.textureDimension[0] == 2 ? 1 : ps->info.textureDimension[0] == 3 ? 2 : 0;
                }
                if (taa_collection::Enabled() && temporalExperiment)
                    taa_collection::Observe(frame,key.vs,key.ps,uint32_t(rasterViewport.width),uint32_t(rasterViewport.height),
                        temporalSlot,collectionCandidates,collectionFlags,uint32_t(drawJitter.rejection),vs->position,positionGuards);
                if(temporalExperiment && temporalJitter && temporalSlot<0 && temporalViewport) {
                    ++temporalJitterUnknowns;
                }
                if (drawJitter.applied) ++temporalJitterDraws;
                else if (temporalExperiment && temporalJitter && temporalSlot >= 0 && temporalViewport) ++temporalJitterMisses;
                taaJitter.AddTo(tTaa);
                // Range of the bound colour format, clamped in the shader epilogue.
                {
                    const uint32_t cfmt = (colorInfo >> 16) & 0xF;
                    {
                        // RB_COLOR_INFO.color_exp_bias (signed 6 bits at +20) scales what
                        // the hardware writes to EDRAM; log which values the title uses.
                        const int32_t bias = int32_t(colorInfo << 6) >> 26;
                        static std::set<int32_t> seenBias;
                        if (seenBias.insert(bias | (int32_t(cfmt) << 8)).second)
                            LOG_INFO("renderer: colour format {} uses exp_bias {}", cfmt, bias);
                    }
                    float m = 1.0f;            // 8_8_8_8, 8_8_8_8_GAMMA, 2_10_10_10, _AS_10_10_10_10
                    if (cfmt == 3 || cfmt == 12) m = 31.875f;   // 2_10_10_10_FLOAT (7e3)
                    else if (cfmt >= 4 && cfmt != 10) m = 65504.0f;           // 16_16(_16_16)(_FLOAT), 32_FLOAT
                    shared.colorMax[0] = shared.colorMax[1] = shared.colorMax[2] = m;
                    shared.colorMax[3] = (cfmt == 3 || cfmt == 12 || cfmt <= 2 || cfmt == 10) ? 1.0f : m;
                    if (cfmt >= 4 && cfmt != 10 && cfmt != 12)
                        shared.flags |= 32u;    // signed format (16_16_FLOAT, 32_FLOAT)
                }
                uint32_t colorControl = Reg(REG_RB_COLORCONTROL);
                // LO_PS_DEBUG=<n>: paint draws with at least n indices magenta.
                static const uint32_t psDebugMin = getenv("LO_PS_DEBUG") ? std::max(1ul, strtoul(getenv("LO_PS_DEBUG"), nullptr, 10)) : 0;
                // LO_DEBUG_VS=<hex hash>: restrict the debug overrides to one vertex shader.
                static const uint64_t debugVs = getenv("LO_DEBUG_VS") ? strtoull(getenv("LO_DEBUG_VS"), nullptr, 16) : 0;
                const bool debugMatch = !debugVs || key.vs == debugVs;
                if (psDebugMin && info.indexCount >= psDebugMin && debugMatch)
                    shared.flags |= 2;
                // LO_VS_DEBUG=<n>: draws with at least n indices become a fixed triangle.
                static const uint32_t vsDebugMin = getenv("LO_VS_DEBUG") ? std::max(1ul, strtoul(getenv("LO_VS_DEBUG"), nullptr, 10)) : 0;
                if (vsDebugMin && info.indexCount >= vsDebugMin && debugMatch)
                    shared.flags |= 4;
                // LO_VS_RAW=1: skip the VTE epilogue (raw shader clip-space output).
                static const bool vsRaw = getenv("LO_VS_RAW") != nullptr;
                if (vsRaw)
                    shared.flags |= 8;
                static const bool texDebug = getenv("LO_PS_TEXDEBUG") != nullptr; // debugging: show the sampled texture
                if (texDebug)
                    shared.flags |= 16;
                static const bool noAlphaTest = getenv("LO_NO_ALPHATEST") != nullptr; // debugging
                if ((colorControl & 8) && !noAlphaTest)
                {
                    shared.flags |= 1;
                    shared.alphaTest[0] = RegF(REG_RB_ALPHA_REF);
                    shared.alphaTest[1] = float(colorControl & 7);
                }

                tConst0.AddTo(tConst);

                // Descriptor sets: vertex fetch buffers + samplers, textures.
                RenderDescriptorSet* set0 = staticSet0.get();
                TextureSetCache::Key textureBindings[3];
                uint32_t activeTextureSlots[3] = {};
                textureBindings[0].fill(dummyTexture2D.texture.get());
                textureBindings[1].fill(dummyTexture3D.texture.get());
                textureBindings[2].fill(dummyTextureCube.texture.get());
                render_batch::CpuTimer<> tVertex0(cpuTimingEnabled);
                bool slot95Bound = false;
                uint64_t slot95ArenaOffset = UINT64_MAX;
                uint64_t slot95StreamBytes = 0;
                const uint32_t vfTraceFrame = TraceFrame();
                static const uint32_t vfTraceCount = getenv("LO_DRAW_TRACE_COUNT") ? strtoul(getenv("LO_DRAW_TRACE_COUNT"), nullptr, 10) : 1;
                const bool vfTrace = vfTraceFrame && frame >= vfTraceFrame && frame < vfTraceFrame + vfTraceCount;
                std::string vfTraceLine;
                for (uint32_t slot = 0; slot < kVertexFetchSlots; slot++)
                {
                    if (!((vs->info.vertexFetchSlotMask[slot >> 6] >> (slot & 63)) & 1))
                        continue;
                    uint32_t d0 = Reg(REG_FETCH_CONSTANTS + slot * 2);
                    uint32_t d1 = Reg(REG_FETCH_CONSTANTS + slot * 2 + 1);
                    // A slot the shader reads but we cannot bind is a silent failure: the
                    // shader then fetches from arena offset 0, i.e. some other draw's data.
                    const char* skip = nullptr;
                    uint32_t address = d0 & ~3u;
                    uint32_t sizeDwords = (d1 >> 2) & 0xFFFFFF;
                    uint64_t offset = UINT64_MAX;
                    if ((d0 & 3) != 3) skip = "not a vertex fetch constant";
                    else if (sizeDwords == 0 || sizeDwords > (16u << 20)) skip = "bad size";
                    else if ((offset = GetVertexBuffer(address, sizeDwords, d1 & 3)) == UINT64_MAX) skip = "upload failed";
                    if (skip)
                    {
                        if (motionSupported) motionStreamsValid = false;
                        drops.vfetchSkips++;
                        if (vfTrace)
                            vfTraceLine += fmt::format(" vf{}=SKIP({} d0={:#x} d1={:#x})", slot, skip, d0, d1);
                        continue;
                    }
                    shared.vfetchOffset[slot] = uint32_t(offset);
                    if (slot == 95) {
                        slot95Bound = true;
                        slot95ArenaOffset = offset;
                        slot95StreamBytes = uint64_t(sizeDwords) * 4;
                    }
                    if (motionSupported) {
                        render_batch::CpuTimer<> mvTimer(motionOptions.timing);
                        // Every stream, not the first fetch. Arena uploads are immutable;
                        // an overwrite/re-upload or recycled arena invalidates history.
                        motionGeometry = temporal::MotionHashWord(motionGeometry, slot);
                        motionGeometry = temporal::MotionHashWord(motionGeometry, d0);
                        motionGeometry = temporal::MotionHashWord(motionGeometry, d1);
                        motionGeometry = temporal::MotionHashWord(motionGeometry, offset);
                        motionGeometry = temporal::MotionHashWord(motionGeometry, motionArenaGeneration[offset / gpu::render_arena::kSlotArenaSize]);
                        mvTimer.AddTo(mvTrackCpuMs);
                    }
                    if (vfTrace)
                        vfTraceLine += fmt::format(" vf{}=arena+{:#x}({:#x},{}dw,e{})", slot, offset, address, sizeDwords, d1 & 3);
                }
                if (vfTrace)
                {
                    // Raw words of the constants the position math reads, so two draws can
                    // be compared bit for bit rather than to the six digits of {:g}.
                    std::string raw;
                    for (uint32_t ci = 0; ci <= 10; ci++)
                        raw += fmt::format(" c{}={:08x},{:08x},{:08x},{:08x}", ci, Reg(REG_ALU_CONSTANTS + ci * 4), Reg(REG_ALU_CONSTANTS + ci * 4 + 1), Reg(REG_ALU_CONSTANTS + ci * 4 + 2), Reg(REG_ALU_CONSTANTS + ci * 4 + 3));
                    SHADER_LOG_INFO("vertex-fetch", None, "renderer: draw vfetch{} | indxOffset={} raw{}", vfTraceLine, int32_t(Reg(REG_VGT_INDX_OFFSET)), raw);
                }

                tVertex0.AddTo(tVertex);

                // Prove the actual destination coverage before replacing its source.
                // This shader fetches float4 positions from slot 95 with 32-byte stride.
                // Restrict to two triangles forming a rectangle; viewport size alone
                // cannot justify treating an arbitrary fullscreen-looking draw as a copy.
                uint32_t fullCopyReason=0;std::string fullCopyVertices;
                bool fullSceneCopy = false;
                {
                    render_batch::CpuTimer<> sceneCopyTimer(cpuTimingEnabled);
                    fullSceneCopy = [&]() {
                    auto reject=[&](uint32_t why){fullCopyReason=why;return false;};
                    if(key.vs!=0x8bbd4da701845d16ull||key.ps!=0xcda578aef1724fdcull||
                       info.primitiveType!=4||!info.indexed||info.indexCount!=6||info.indexBufferWords<6||
                       viewport.x!=0||viewport.y!=0||viewport.width<=0||viewport.height<=0||viewport.width>pitch||viewport.height>rtHeight||
                       key.colorMask!=15||(depthControl&3)||shared.vtxFmt!=4||
                       (key.modeCull&3)|| (Reg(REG_RB_COLORCONTROL)&8))return reject(1);
                    const uint32_t blend=key.blend;
                    if(BlendFactor(blend&31)!=RenderBlend::ONE||BlendFactor((blend>>8)&31)!=RenderBlend::ZERO||
                       BlendOp((blend>>5)&7)!=RenderBlendOperation::ADD)return reject(2);
                    int l=scissorTl&0x3fff,t=(scissorTl>>16)&0x3fff,r=scissorBr&0x3fff,b=(scissorBr>>16)&0x3fff;
                    const uint32_t window=Reg(REG_PA_SC_WINDOW_OFFSET);
                    if(!(scissorTl&0x80000000u)&&window){int ox=int32_t(window<<17)>>17,oy=int32_t(window<<1)>>17;l+=ox;r+=ox;t+=oy;b+=oy;}
                    if(l>0||t>0||r<viewport.width||b<viewport.height)return reject(3);
                    uint32_t d0=Reg(REG_FETCH_CONSTANTS+95*2),d1=Reg(REG_FETCH_CONSTANTS+95*2+1),words=(d1>>2)&0xffffff;
                    if((d0&3)!=3||!words||uint64_t(d0&0x1ffffffcu)+uint64_t(words)*4>0x20000000ull||
                       uint64_t(info.indexBase&0x1fffffffu)+(info.index32?24:12)>0x20000000ull)return reject(4);
                    float xy[6][2];float xmin=INFINITY,ymin=INFINITY,xmax=-INFINITY,ymax=-INFINITY;
                    for(unsigned i=0;i<6;++i){
                        uint32_t v=0;if(info.index32)memcpy(&v,Phys(info.indexBase)+i*4,4);else memcpy(&v,Phys(info.indexBase)+i*2,2);
                        v=GpuSwap(v,info.indexEndian);if(!info.index32)v&=0xffff;
                        int64_t index=int64_t(v)+int32_t(Reg(REG_VGT_INDX_OFFSET));if(index<0||uint64_t(index)*8+4>words)return reject(5);
                        float pos[4];for(unsigned k=0;k<4;++k){uint32_t raw;memcpy(&raw,Phys(d0&~3u)+index*32+k*4,4);raw=GpuSwap(raw,d1&3);memcpy(pos+k,&raw,4);}
                        fullCopyVertices+=fmt::format(" i{}={} pos=({:g},{:g},{:g},{:g})",i,index,pos[0],pos[1],pos[2],pos[3]);
                        if(!std::isfinite(pos[0])||!std::isfinite(pos[1])||pos[3]!=1)return reject(6);
                        xy[i][0]=(pos[0]*shared.ndcScale[0]+shared.ndcOffset[0]+shared.halfPixel[0]+1)*viewport.width*.5f;
                        xy[i][1]=(1-pos[1]*shared.ndcScale[1]-shared.ndcOffset[1]-shared.halfPixel[1])*viewport.height*.5f;
                        xmin=std::min(xmin,xy[i][0]);xmax=std::max(xmax,xy[i][0]);ymin=std::min(ymin,xy[i][1]);ymax=std::max(ymax,xy[i][1]);
                    }
                    fullCopyVertices+=fmt::format(" bounds=({:.9g},{:.9g},{:.9g},{:.9g})",xmin,ymin,xmax,ymax);
                    if(xmin>.5f||ymin>.5f||xmax<viewport.width-.5f||ymax<viewport.height-.5f)return reject(7);
                    unsigned masks[2]={};for(unsigned i=0;i<6;++i){
                        bool right=std::abs(xy[i][0]-xmax)<.001f,bottom=std::abs(xy[i][1]-ymax)<.001f;
                        if(!right&&std::abs(xy[i][0]-xmin)>=.001f)return reject(8);
                        if(!bottom&&std::abs(xy[i][1]-ymin)>=.001f)return reject(9);
                        masks[i/3]|=1u<<((right?1:0)+(bottom?2:0));
                    }
                    fullCopyVertices+=fmt::format(" bounds=({:g},{:g},{:g},{:g}) masks={}/{}",xmin,ymin,xmax,ymax,masks[0],masks[1]);
                    unsigned common=masks[0]&masks[1];fullCopyReason=10;return std::popcount(masks[0])==3&&std::popcount(masks[1])==3&&
                        (masks[0]|masks[1])==15&&(common==9||common==6);
                }();
                if(key.vs==0x8bbd4da701845d16ull&&key.ps==0xcda578aef1724fdcull) {
                    static const uint64_t start=getenv("LO_SCENE_AA_LOG_START_FRAME")?strtoull(getenv("LO_SCENE_AA_LOG_START_FRAME"),nullptr,10):~0ull;
                    if(frame>=start&&frame-start<128)SHADER_LOG_INFO("scene-aa", None, "renderer scene AA guard f{} full={} reason={} mode={} jitter={} blend={:#x} mask={} vtx={} prim={} n={} cull={:#x} ctl={:#x} vp=({},{},{},{}) extent={}x{} fetch95={:08x},{:08x} quad={} ",frame,fullSceneCopy,fullCopyReason,sceneAAMode,temporalJitter,key.blend,key.colorMask,shared.vtxFmt,info.primitiveType,info.indexCount,key.modeCull,Reg(REG_RB_COLORCONTROL),viewport.x,viewport.y,viewport.width,viewport.height,pitch,rtHeight,Reg(REG_FETCH_CONSTANTS+190),Reg(REG_FETCH_CONSTANTS+191),fullCopyVertices);
                }
                    sceneCopyTimer.AddTo(tSceneCopy);
                }
                // Textures used by the pixel and vertex shaders.
                render_batch::CpuTimer<> tBind0(cpuTimingEnabled);
                const uint32_t bindingBank = bindingRecord ? bindingRecord->texture.bank : 0;
                const uint32_t sceneCopyBank = ps && ps->info.textureDimension[0] == 2 ? 1 :
                    ps && ps->info.textureDimension[0] == 3 ? 2 : 0;
                std::optional<binding::Producer> boundSceneProducer;
                bool failedPlan = false;
#if defined(LO_GPU_PLUME)
                struct AlphaBridgeFetchRequest {
                    uint32_t slot = 0, bank = 0, address = 0, fetchFormat = 0, resolveFormat = 0;
                    uint32_t cropWidth = 0, cropHeight = 0, parentWidth = 0, parentHeight = 0;
                    uint32_t guestFetchWidth = 0, guestParentWidth = 0;
                    uint64_t destinationAllocation = 0, writeOrdinal = 0;
                    uint64_t samplerKey = 0;
                    RenderTexture* returnedColor = nullptr;
                    RenderTexture* prefilterOutput = nullptr;
                    const char* substitution = "none";
                    bool prefilterSourceIsReturnedColor = false;
                };
                std::vector<AlphaBridgeFetchRequest> alphaBridgeFetches;
                struct AlphaPostInput {
                    fsr_alpha::FetchCandidate fetch;
                    uint64_t samplerKey = 0;
                    RenderTexture* actualColor = nullptr;
                    const char* substitution = "none";
                };
                std::array<std::optional<AlphaPostInput>, 4> alphaPostInputs{};
                struct AlphaPostDepthInput {
                    RenderTexture* texture = nullptr;
                    RenderFormat format = RenderFormat::UNKNOWN;
                    uint32_t width = 0, height = 0;
                    uint64_t samplerKey = 0;
                };
                std::optional<AlphaPostDepthInput> alphaPostDepth;
#endif
                auto bindTextures = [&](Shader* s)
                {
                    if (!s) return;
                    for (uint32_t slot = 0; slot < kTextureSlots; slot++)
                    {
                        if (failedPlan)
                            return;
                        if (!((s->info.textureSlotMask >> slot) & 1))
                            continue;
                        uint32_t fetch[6];
                        for (int i = 0; i < 6; i++) fetch[i] = Reg(REG_FETCH_CONSTANTS + slot * 6 + i);
                        const uint32_t declared = s->info.textureDimension[slot];
                        const uint32_t bank = declared == 2 ? 1 : declared == 3 ? 2 : 0;
                        activeTextureSlots[bank] |= uint32_t(1) << slot;
                        HostTexture* dummy = declared == 2 ? &dummyTexture3D : declared == 3 ? &dummyTextureCube : &dummyTexture2D;
                        uint32_t dimension = (fetch[5] >> 9) & 3; // 0 1D, 1 2D, 2 3D, 3 cube
                        std::optional<binding::Texture> selectedBinding;
                        if (bindingRecord && slot == 0 && bank == bindingBank) selectedBinding.emplace();
                        HostTexture* tex = (fetch[0] & 3) == 2 ? GetTexture(fetch, dimension,
                            selectedBinding ? &*selectedBinding : nullptr, bindingEpoch) : nullptr;
                        if (!tex)
                        {
                            if (PlanSuppressed())
                            {
                                failedPlan = true;
                                return;
                            }
                            // Descriptor sets are pooled and reused, so a slot the
                            // shader reads must always be written: otherwise it keeps
                            // the texture some earlier draw left there.
                            textureBindings[bank][slot] = dummy->texture.get();
                            shared.samplerIndex[slot] = 0;
                            shared.textureInfo[slot] = 0x68800u;
                            if (selectedBinding) {
                                *selectedBinding = {};
                                DescribeBindingTexture(&*selectedBinding, *dummy, binding::TextureKind::Dummy, bindingEpoch);
                                selectedBinding->dimension = dimension;
                                selectedBinding->bank = bank;
                                selectedBinding->guestExtent = {1, 1};
                                bindingRecord->texture = *selectedBinding;
                            }
                            if (trackBinding && fullSceneCopy && slot == 0 && bank == sceneCopyBank)
                                boundSceneProducer.emplace().Unknown(bindingEpoch, frame);
#if defined(LO_GPU_PLUME)
                            if (fsrAlphaBridge && activePlan.requestedUpscaler == upscaling::Upscaler::Fsr &&
                                s == ps && fsr_alpha::AuditedPostprocessPs(key.ps)) {
                                AlphaBridgeFetchRequest request{};
                                request.slot = slot; request.bank = bank;
                                request.address = (fetch[1] >> 12) << 12;
                                request.fetchFormat = fetch[1] & 0x3F;
                                request.returnedColor = dummy->texture.get();
                                alphaBridgeFetches.push_back(request);
                            }
#endif
                            dummyBindings++;
                            continue;
                        }
#if defined(LO_GPU_PLUME)
                        if (fsrAlphaBridge && activePlan.requestedUpscaler == upscaling::Upscaler::Fsr &&
                            s == ps && fsr_alpha::AuditedPostprocessPs(key.ps)) {
                            AlphaBridgeFetchRequest request{};
                            request.slot = slot; request.bank = bank;
                            request.address = (fetch[1] >> 12) << 12;
                            request.fetchFormat = fetch[1] & 0x3F;
                            request.cropWidth = tex->width; request.cropHeight = tex->height;
                            request.guestFetchWidth = tex->guestWidth;
                            request.returnedColor = tex->texture.get();
                            if (const auto* source = FindResolved(request.address, request.fetchFormat);
                                source && source->tex) {
                                request.resolveFormat = source->destFormat;
                                request.destinationAllocation = source->tex->allocationSerial;
                                request.writeOrdinal = source->writeOrdinal;
                                request.parentWidth = source->tex->width;
                                request.parentHeight = source->tex->height;
                                request.guestParentWidth = source->tex->guestWidth;
                            }
                            alphaBridgeFetches.push_back(request);
                        }
#endif
                        if (trackTemporalScene && s == ps && slot == 0 &&
                            key.vs == 0x8bbd4da701845d16ull && key.ps == 0xcda578aef1724fdcull)
                        {
                            const uint32_t address = (fetch[1] >> 12) << 12;
                            const uint32_t format = fetch[1] & 0x3F;
                            if (auto* source = FindResolved(address, format); format == 6 && source && source->tex)
                                temporalSceneCopy = temporal::SceneResolve{source->frame, source->writeOrdinal,
                                    address, format, tex->width, tex->height,
                                    source->writeX == 0 && source->writeY == 0 &&
                                    source->writeWidth == tex->width && source->writeHeight == tex->height &&
                                    source->tex->width == tex->width && source->tex->height == tex->height};
                        }
                        HostTexture* bloomFiltered = nullptr;
                        RenderTexture* bloomPrefilterInput = nullptr;
                        RenderTexture* hdrBinding = nullptr;
                        const bool useBloomPrefilter = taaDiagnosticBloom >= 0 ? taaDiagnosticBloom == 1 : bloomPrefilterEnabled && sceneAAMode == 3;
                        // Keep bloom reconstruction stable while temporal history
                        // resets or temporarily suspends jitter (including capture stalls).
                        if ((useBloomPrefilter || taaDiagnosticHDR == 1 || resolveTraceRemaining) && s == ps && slot == 0 &&
                            key.vs == 0x2f6bbed8149a7804ull && key.ps == 0x7c260eacff1d681dull &&
                            declared == 1 && dimension == 1 && tex != color &&
                            tex->format == RenderFormat::R16G16B16A16_FLOAT &&
                            tex->guestWidth == 1280 && tex->guestHeight == 720 &&
                            (fetch[2] & 0x1FFF) + 1 == 1280 && ((fetch[2] >> 13) & 0x1FFF) + 1 == 720 &&
                            tex->width > 1280 && tex->height > 720 && tex->width <= 1280 * 8 && tex->height <= 720 * 8 &&
                            ((fetch[3] >> 19) & 0x3F) == 0 && // Nearest min/mag/mip, as captured.
                            (((fetch[0] >> 10) & 7) == 2 || ((fetch[0] >> 10) & 7) == 4) &&
                            (((fetch[0] >> 13) & 7) == 2 || ((fetch[0] >> 13) & 7) == 4)) {
                            auto* source = FindResolved((fetch[1] >> 12) << 12, fetch[1] & 0x3F);
                            if (source && source->tex.get() == tex && source->frame == frame && source->writeOrdinal &&
                                source->writeX == 0 && source->writeY == 0 &&
                                source->writeWidth == tex->width && source->writeHeight == tex->height) {
                                const auto traceCopiesBefore = resolveTraceCopies.size();
                                QueueResolveTrace(*tex, 0xffff0030u);
                                if (resolveTraceCopies.size() != traceCopiesBefore)
                                    LOG_INFO("renderer: bloom source trace frame={} write_ordinal={} address={:#x} source={}x{} synthetic=0xffff0030",
                                        frame, source->writeOrdinal, (fetch[1] >> 12) << 12, tex->width, tex->height);
                                if (taaDiagnosticHDR == 1 && temporalExperiment && hdrTemporalHistory && !hdrTemporalOutput &&
                                    temporalScene.Reason() == temporal::SceneObservation::Rejection::None && temporalScene.Copies() == 0) {
                                    auto hdrScene = temporalScene;
                                    temporal::SceneResolve hdrSource{source->frame, source->writeOrdinal,
                                        (fetch[1] >> 12) << 12, fetch[1] & 0x3F, tex->width, tex->height, true};
                                    if (hdrScene.ObserveColor(hdrSource)) {
                                        Transition(*tex, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
                                        const auto jitter = temporal::FrameJitter(frame, tex->width, tex->height,
                                            ActiveTaaOptions().jitter_scale);
                                        FinishMotion(hdrTemporalHistory.get());
                                        hdrTemporalOutput = hdrTemporalHistory->ResolveColor(commandList, tex->texture.get(), hdrScene,
                                            temporalJitter ? jitter.pixelX : 0, temporalJitter ? jitter.pixelY : 0,
                                            temporalAllowHistory, true, true,
                                            motionOptions.consume && (!taaLiveApplied || taaLiveOptions.mv_consume) ? &motionView : nullptr,
                                            motionOptions.debug, &ActiveTaaOptions());
                                        if (motionOptions.consume && motionView.ready && motionReplay) motionReplay->RecordConsumerUse();
                                        Transition(*tex, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
                                        if (hdrTemporalOutput) {
                                            hdrTemporalSource = hdrSource;
                                            QueueResolveTrace(hdrTemporalOutput, tex->format, tex->width, tex->height, RenderTextureLayout::SHADER_READ, 0xffff0031u);
                                            if (resolveTraceRemaining || hdrTemporalLogs++ < 4)
                                                LOG_INFO("renderer: HDR temporal frame={} source_ordinal={} depth_ordinal={} reused={} size={}x{}",
                                                    frame, hdrSource.ordinal, hdrScene.Depth().ordinal, hdrTemporalHistory->Reused(), tex->width, tex->height);
                                        }
                                    }
                                }
                                const bool sameHDRSource = hdrTemporalOutput && hdrTemporalSource.frame == frame &&
                                    hdrTemporalSource.address == ((fetch[1] >> 12) << 12) && hdrTemporalSource.format == (fetch[1] & 0x3F) &&
                                    hdrTemporalSource.ordinal == source->writeOrdinal && hdrTemporalSource.width == tex->width && hdrTemporalSource.height == tex->height;
                                if (useBloomPrefilter) {
                                    bloomPrefilterInput = sameHDRSource ? hdrTemporalOutput : tex->texture.get();
                                    bloomFiltered = PrefilterBloom(*tex, sameHDRSource ? hdrTemporalOutput : nullptr);
                                }
                                else if (sameHDRSource) hdrBinding = hdrTemporalOutput;
                                if (bloomFiltered) {
                                    if (bloomPrefilterLogs++ < 4)
                                        LOG_INFO("renderer: bloom prefilter frame={} vs={:016x} ps={:016x} source={}x{} filtered=1280x720 HDR area linear",
                                            frame, key.vs, key.ps, tex->width, tex->height);
                                    if (!debugCaptureDir.empty())
                                        debugTrace << fmt::format("bloom_prefilter frame={} vs={:016x} ps={:016x} source={}x{} filtered=1280x720 resolve={} HDR_area=1 linear=1\n",
                                            frame, key.vs, key.ps, tex->width, tex->height, source->writeOrdinal);
                                }
                            }
                        }
                        // The same resolved version feeds the verified tone-map shader.
                        // Rewrites, address reuse, other consumers, and other frames do not match.
                        if (hdrTemporalOutput && s == ps && slot == 0 && declared == 1 && dimension == 1 &&
                            key.vs == 0x9b81c55ca39bb529ull && key.ps == 0xb4b4d54a7a2d6b96ull &&
                            tex->format == RenderFormat::R16G16B16A16_FLOAT && tex != color) {
                            const auto address = (fetch[1] >> 12) << 12;
                            const auto format = fetch[1] & 0x3F;
                            const auto* source = FindResolved(address, format);
                            if (source && source->tex.get() == tex && address == hdrTemporalSource.address &&
                                format == hdrTemporalSource.format && source->frame == hdrTemporalSource.frame && source->frame == frame &&
                                source->writeOrdinal == hdrTemporalSource.ordinal && tex->width == hdrTemporalSource.width && tex->height == hdrTemporalSource.height) {
                                hdrBinding = hdrTemporalOutput; hdrTonemapRecorded = true;
                                if (resolveTraceRemaining)
                                    LOG_INFO("renderer: HDR tone-map frame={} source_ordinal={} same_output=1", frame, source->writeOrdinal);
                            }
                        }
                        const bool diagnosticFullSceneCopy = fullSceneCopy && s == ps && slot == 0 &&
                            tex->format == RenderFormat::R8G8B8A8_UNORM && rasterViewport.width == tex->width && rasterViewport.height == tex->height;
                        if (diagnosticFullSceneCopy) QueueResolveTrace(*tex, 0xffff0020u);
                        RenderTexture* temporalDisplay=nullptr;
                        RenderFormat temporalDisplayFormat=tex->format;
                        bool temporalDisplayFromHistory = false;
                        if(temporalSceneCopy && fullSceneCopy && s==ps && slot==0 &&
                           rasterViewport.width==tex->width && rasterViewport.height==tex->height &&
                           rasterViewport.width==temporalScene.Anchor().viewport.width && rasterViewport.height==temporalScene.Anchor().viewport.height) {
                            render_batch::CpuTimer<> taaResolve(cpuTimingEnabled);
                            temporalScene.ObserveColor(*temporalSceneCopy);
                            if ((temporalInputProbe || dlssSrRequested) && temporalHistory && temporalScene.Ready() &&
                                tex->format==RenderFormat::R8G8B8A8_UNORM) {
                                Transition(*tex,RenderTextureLayout::COPY_SOURCE,RenderBarrierStage::COPY);
                                FinishMotion(temporalHistory.get());
                                const auto sample = actualRasterJitterCaptured ? actualRasterJitter : temporal::JitterSample{};

                                temporal::ColorEncoding qualifiedEncoding = temporal::ColorEncoding::Unknown;
                                const uint32_t address = (fetch[1] >> 12) << 12;
                                const uint32_t format = fetch[1] & 0x3F;
                                auto* source = FindResolved(address, format);
                                const bool isRbSwapIdentity = color_qualification::CheckNetIdentityRBSwap(fetch[0], fetch[3], source ? source->swapRedBlue : false);
                                const bool viewMatches = source && source->tex && source->tex.get() == tex;
                                const bool ordinalMatches = source && source->sdrWriteOrdinal != 0 &&
                                    source->sdrWriteOrdinal == source->writeOrdinal &&
                                    source->sdrWriteOrdinal == temporalSceneCopy->ordinal;
                                const bool frameMatches = source && source->frame == frame;
                                const bool extentMatches = tex->width == rasterViewport.width && tex->height == rasterViewport.height;

                                if (viewMatches && frameMatches && ordinalMatches && extentMatches && isRbSwapIdentity) {
                                    qualifiedEncoding = temporal::ColorEncoding::Sdr;
                                } else {
                                    const uint32_t rejectKey =
                                        (uint32_t(viewMatches) << 0) | (uint32_t(frameMatches) << 1) |
                                        (uint32_t(ordinalMatches) << 2) | (uint32_t(extentMatches) << 3) |
                                        (uint32_t(isRbSwapIdentity) << 4);
                                    static uint32_t lastLoggedConsumerRejectKey = ~0u;
                                    if (lastLoggedConsumerRejectKey != rejectKey) {
                                        lastLoggedConsumerRejectKey = rejectKey;
                                        LOG_INFO("renderer: DLSS SDR consumer rejected frame={} view_match={} frame_match={} ordinal_match={} extent_match={} rb_identity={} sdr_ord={} ord={} copy_ord={}",
                                            frame, viewMatches, frameMatches, ordinalMatches, extentMatches, isRbSwapIdentity,
                                            source ? source->sdrWriteOrdinal : 0, source ? source->writeOrdinal : 0,
                                            temporalSceneCopy ? temporalSceneCopy->ordinal : 0);
                                    }
                                }

                                    const auto logDlssInputFailure = [&](const char* stage, const char* disposition) {
                                    static uint64_t loggedSignature = 0;
                                    static std::string_view loggedStage, loggedDisposition;
                                    static temporal::InputCaptureFailure loggedCaptureReason = temporal::InputCaptureFailure::None;
                                    const auto captureReason = temporalHistory->LastInputCaptureFailure();
                                    if (loggedSignature == activePlan.requestSignature && loggedStage == stage &&
                                        loggedDisposition == disposition && loggedCaptureReason == captureReason) return;
                                    loggedSignature = activePlan.requestSignature;
                                    loggedStage = stage;
                                    loggedDisposition = disposition;
                                    loggedCaptureReason = captureReason;
                                    const auto& depth = temporalScene.Depth();
                                    const char* mvError = "unknown";
                                    if (motionReplay) {
                                        const auto& err = motionReplay->LastError();
                                        if (!err.empty()) mvError = err.c_str();
                                    } else if (motionInitFailed) {
                                        mvError = "init_failed";
                                    }
                                    if (strcmp(stage, "capture_color_inputs") == 0) {
                                        LOG_INFO("renderer: DLSS InvalidInput stage={} disposition={} reason={} frame={} cpu_serial={} epoch={} signature={:#x} consumer={} input={}x{} output={}x{} history_frame={} history_epoch={} history_extent={}x{} camera={} captured_depth_ord={} captured_depth_alloc={} scene_depth_ord={} scene_depth={}x{} full_extent={} scene_color={}x{} plan={}x{} color_already={} motion_ready={} motion_frame={} motion_epoch={} motion_alloc={} motion_extent={}x{} motion_state={} tracker_failed={} motion_init_failed={} replay_error={} capture_inputs_fresh=1 current_inputs_used=0",
                                            stage, disposition, temporal::InputCaptureFailureName(captureReason), frame, activePlan.cpuSerial, activePlan.geometryEpoch,
                                            activePlan.requestSignature, uint32_t(activePlan.consumer), activePlan.width, activePlan.height,
                                            activePlan.output.width, activePlan.output.height, temporalHistory->InputFrame(), temporalHistory->InputEpoch(),
                                            temporalHistory->InputWidth(), temporalHistory->InputHeight(), temporalHistory->HasCapturedCamera() ? 1 : 0,
                                            temporalHistory->CapturedDepthOrdinal(), temporalHistory->CapturedDepthAllocation(),
                                            depth.ordinal, depth.width, depth.height, depth.fullExtent ? 1 : 0,
                                            temporalScene.Color().width, temporalScene.Color().height, activePlan.width, activePlan.height,
                                            temporalHistory->CapturedColorOrdinal() ? 1 : 0,
                                            motionView.ready ? 1 : 0, motionView.frame, motionView.epoch, motionView.depthAllocation,
                                            motionView.width, motionView.height, uint32_t(motionView.state), drawTemporalTracker.Failed() ? 1 : 0,
                                            motionInitFailed ? 1 : 0, mvError);
                                    } else {
                                        auto inputs = temporalHistory->CurrentInputs();
                                    inputs.frameTimeDeltaMilliseconds = srFrameDeltaMilliseconds;
                                    if (srTimeReset) {
                                        inputs.resetHistory = true;
                                        inputs.resetReasons = inputs.resetReasons | temporal::TemporalResetReason::FrameDiscontinuity;
                                    }
                                        LOG_INFO("renderer: DLSS InvalidInput stage={} disposition={} frame={} cpu_serial={} epoch={} signature={:#x} consumer={} input={}x{} output={}x{} complete={} motion_state={} color_ok={} depth_ok={} motion_ok={} invalidity_ok={} encoding={} depth_conv={} depth_captured_ord={} depth_captured_alloc={} depth_captured_extent={}x{} motion_view_frame={} motion_view_epoch={} motion_view_alloc={} motion_view_extent={}x{} tracker_failed={} motion_init_failed={} replay_error={} current_inputs_fresh=1 capture_succeeded=1",
                                            stage, disposition, frame, activePlan.cpuSerial, activePlan.geometryEpoch, activePlan.requestSignature,
                                            uint32_t(activePlan.consumer), activePlan.width, activePlan.height,
                                            activePlan.output.width, activePlan.output.height, inputs.currentInputsComplete ? 1 : 0,
                                            uint32_t(inputs.motionState), inputs.color.Complete() ? 1 : 0, inputs.depth.Complete() ? 1 : 0,
                                            inputs.motion.Complete() ? 1 : 0, inputs.motionInvalidity.Complete() ? 1 : 0,
                                            uint32_t(inputs.colorEncoding), uint32_t(inputs.depthConvention),
                                            temporalHistory->CapturedDepthOrdinal(), temporalHistory->CapturedDepthAllocation(),
                                            temporalHistory->InputWidth(), temporalHistory->InputHeight(),
                                            motionView.frame, motionView.epoch, motionView.depthAllocation, motionView.width, motionView.height,
                                            drawTemporalTracker.Failed() ? 1 : 0, motionInitFailed ? 1 : 0, mvError);
                                    }
                                };
                                if (!temporalHistory->CaptureColorInputs(commandList, tex->texture.get(), temporalScene, activePlan, sample,
                                        qualifiedEncoding, &motionView)) {
                                    const bool frameOnly = temporal::ClassifySrCaptureFailure(temporalHistory->LastInputCaptureFailure()) ==
                                        temporal::SrSceneInputFailure::FrameFallback;
                                    logDlssInputFailure("capture_color_inputs", frameOnly ? "frame_fallback" : "request_failure");
                                    if (frameOnly)
                                        NoteDlssFrameFallback(frame_plan::DlssEffectReason::NoEligibleScene);
                                    else DisableDlssRequest(frame_plan::FailureReason::InvalidInput);
                                } else {
                                    auto inputs = temporalHistory->CurrentInputs();
                                    inputs.frameTimeDeltaMilliseconds = srFrameDeltaMilliseconds;
                                    if (srTimeReset) {
                                        inputs.resetHistory = true;
                                        inputs.resetReasons = inputs.resetReasons | temporal::TemporalResetReason::FrameDiscontinuity;
                                    }
                                    const auto selected = SelectDlssSceneCopyInputs(inputs);
                                    if (selected.kind == DlssSceneInputSelection::Kind::Pending) {
                                        static uint64_t loggedPendingSignature = 0;
                                        if (loggedPendingSignature != activePlan.requestSignature) {
                                            loggedPendingSignature = activePlan.requestSignature;
                                            LOG_INFO("renderer: DLSS spatial fallback frame={} reason=motion_pipeline_pending signature={:#x} reset={}",
                                                frame, activePlan.requestSignature, inputs.resetHistory ? 1 : 0);
                                        }
                                    } else if (selected.kind == DlssSceneInputSelection::Kind::Incomplete) {
                                        const bool resourceFailed = motionInitFailed ||
                                            (motionReplay && motionReplay->ResourceFailedThisFrame());
                                        const bool frameOnly = temporal::ClassifySrIncomplete(inputs, resourceFailed) ==
                                            temporal::SrSceneInputFailure::FrameFallback;
                                        logDlssInputFailure("complete_for_consumer", frameOnly ? "frame_fallback" : "request_failure");
                                        if (frameOnly)
                                            NoteDlssFrameFallback(frame_plan::DlssEffectReason::NoEligibleScene);
                                        else DisableDlssRequest(frame_plan::FailureReason::InvalidInput);
                                    }
                                    else if (selected.kind == DlssSceneInputSelection::Kind::UnknownColor) {
                                        // Clean bypass without permanently latching DLSS as disabled.
                                        static bool loggedUnknownEncodingBypass = false;
                                        if (!loggedUnknownEncodingBypass) {
                                            loggedUnknownEncodingBypass = true;
                                            LOG_INFO("renderer: DLSS SR bypass frame={} reason=unknown_color_encoding", frame);
                                        }
                                    }
                                    else if (selected.kind == DlssSceneInputSelection::Kind::Selected) {
                                        dlssSceneCopyInputs = selected.inputs;
                                        dlssSceneCopyInputs->colorOrdinal = temporalSceneCopy->ordinal;
                                        selectedFsrMaskLease.reset();
#if defined(LO_GPU_PLUME)
                                        if (fsrAlphaBridge && activePlan.requestedUpscaler == upscaling::Upscaler::Fsr &&
                                            qualifiedEncoding == temporal::ColorEncoding::Sdr && source &&
                                            viewMatches && frameMatches && ordinalMatches && extentMatches) {
                                            auto candidate = fsrAlphaBridge->RecordFetchView(commandList, frame, temporalEpoch,
                                                address, source->destFormat, source->tex->allocationSerial,
                                                source->writeOrdinal, tex->texture.get(), tex->width, tex->height,
                                                Gpu().fsrAlphaBridgeUses);
                                            const bool frozen = fsrAlphaBridge->FreezeForSceneCopy(candidate,
                                                tex->texture.get(), selected.inputs.color.texture,
                                                frame, temporalEpoch, temporalSceneCopy->ordinal,
                                                selected.inputs.color.width, selected.inputs.color.height);
                                            const bool attached = frozen && fsr_alpha::AttachSceneCopyMask(
                                                fsrAlphaBridge->SceneCopy(), *dlssSceneCopyInputs);
                                            if (attached)
                                                selectedFsrMaskLease = candidate.mask;
                                            if (FsrAlphaBridgeTraceEnabled() && fsrAlphaBridgeTraceCount < 128) {
                                                auto event = std::make_shared<FsrAlphaBridgeDiagnostic>();
                                                event->fields = fmt::format(
                                                    "\"kind\":\"scene_copy_mask\",\"status\":\"{}\","
                                                    "\"temporal_epoch\":{},\"geometry_epoch\":{},\"device_epoch\":{},"
                                                    "\"color_ordinal\":{},\"handoff_attached\":{},"
                                                    "\"source_write_ordinal\":{},"
                                                    "\"source_allocation\":{},\"source_stage\":{},"
                                                    "\"source_revision\":{},\"captured_color_image\":{},"
                                                    "\"mask_image\":{},\"extent\":[{},{}]",
                                                    frozen ? "available" : candidate.reason,
                                                    temporalEpoch, activePlan.geometryEpoch, activePlan.deviceEpoch,
                                                    temporalSceneCopy->ordinal, attached ? 1 : 0, source->writeOrdinal,
                                                    candidate.version.sourceAllocation,
                                                    uint32_t(candidate.version.sourceStage), candidate.version.sourceRevision,
                                                    uintptr_t(selected.inputs.color.texture),
                                                    frozen ? uintptr_t(candidate.mask->texture.get()) : 0,
                                                    selected.inputs.color.width, selected.inputs.color.height);
                                                TraceFsrAlphaBridge(event);
                                                if (frozen) QueueFsrAlphaSnapshot(event, "final-scene-mask",
                                                    candidate.mask->texture.get(), RenderFormat::R8_UNORM,
                                                    selected.inputs.color.width, selected.inputs.color.height, 1,
                                                    RenderTextureLayout::SHADER_READ);
                                            }
                                        }
#endif
                                    }
                                }
                                if (motionView.ready && motionReplay) motionReplay->RecordConsumerUse();
                                Transition(*tex,RenderTextureLayout::SHADER_READ,RenderBarrierStage::GRAPHICS);
                            }
                            if(temporalExperiment && temporalHistory && temporalScene.Ready() && tex->format==RenderFormat::R8G8B8A8_UNORM) {
                                Transition(*tex,RenderTextureLayout::COPY_SOURCE,RenderBarrierStage::COPY);
                                const auto sample = temporal::FrameJitter(frame, rasterViewport.width, rasterViewport.height,
                                    ActiveTaaOptions().jitter_scale);
                                const double jx = temporalJitter ? sample.pixelX : 0, jy = temporalJitter ? sample.pixelY : 0;
                                FinishMotion(temporalHistory.get());
                                const bool consumeMotion = motionOptions.consume && (!taaLiveApplied || taaLiveOptions.mv_consume);
                                temporalDisplay=temporalHistory->ResolveColor(commandList,tex->texture.get(),temporalScene,jx,jy,temporalAllowHistory && !hdrTonemapApplied,temporalStableGrid,sceneAAMode==3,
                                    consumeMotion ? &motionView : nullptr, motionOptions.debug, &ActiveTaaOptions());
                                if (taaLiveDirectory) {
                                    taaLiveResolvedFrame=frame;taaLiveWidth=tex->width;taaLiveHeight=tex->height;
                                    taaLiveJitterX=jx;taaLiveJitterY=jy;taaLiveHistoryReused=temporalDisplay&&temporalHistory->Reused();
                                    taaLiveMotionReady=motionView.ready;taaLiveMotionConsumed=temporalDisplay&&consumeMotion&&temporalHistory->MotionVectorValid();
                                    taaLiveSourceFormat=temporalDisplay?temporalHistory->SourceFormat():RenderFormat::UNKNOWN;
                                    taaLiveHistoryFormat=temporalDisplay?temporalHistory->HistoryFormat():RenderFormat::UNKNOWN;
                                    taaLiveOutputFormat=temporalDisplay?temporalHistory->OutputFormat():RenderFormat::UNKNOWN;
                                }
                                if (motionOptions.consume && motionView.ready && motionReplay) motionReplay->RecordConsumerUse();
                                Transition(*tex,RenderTextureLayout::SHADER_READ,RenderBarrierStage::GRAPHICS);
                                if(temporalDisplay) {
                                    temporalDisplayFormat=temporalHistory->OutputFormat();
                                    temporalDisplayFromHistory = true;
                                    temporalAARecorded=true;
                                    // Reserved diagnostic IDs (not guest addresses): same draw source,
                                    // reconstructed display, and owned current depth, copied before reuse.
                                    QueueResolveTrace(*tex,0xffff0001u);
                                    QueueResolveTrace(temporalDisplay,temporalDisplayFormat,tex->width,tex->height,RenderTextureLayout::SHADER_READ,0xffff0002u);
                                    QueueResolveTrace(temporalHistory->CurrentDepth(),RenderFormat::R32_FLOAT,tex->width,tex->height,RenderTextureLayout::SHADER_READ,0xffff0003u);
                                    if(temporalHistory->CurrentMotionVector())
                                        QueueResolveTrace(temporalHistory->CurrentMotionVector(),RenderFormat::R16G16_FLOAT,tex->width,tex->height,RenderTextureLayout::SHADER_READ,0xffff0004u);
                                    if(temporalHistory->CurrentMotionDepths())
                                        QueueResolveTrace(temporalHistory->CurrentMotionDepths(),RenderFormat::R32G32_FLOAT,tex->width,tex->height,RenderTextureLayout::SHADER_READ,0xffff0005u);
                                    if(temporalHistory->CurrentReactiveMask())
                                        QueueResolveTrace(temporalHistory->CurrentReactiveMask(),RenderFormat::R8_UNORM,tex->width,tex->height,RenderTextureLayout::SHADER_READ,0xffff0006u);
                                }
                            }
                            if(!temporalDisplay && activeSpatialAA && sceneProcessor && !sceneAABusy &&
                               sceneAAAppliedFrame!=frame && temporalScene.Ready() &&
                               (sceneAAMode==1||sceneAAMode==2||sceneAAMode==3) && tex->format==RenderFormat::R8G8B8A8_UNORM) {
                                if(!sceneAAOutput||sceneAAWidth!=tex->width||sceneAAHeight!=tex->height) {
                                    sceneAAOutput=device->createTexture(RenderTextureDesc::Texture2D(tex->width,tex->height,1,
                                        RenderFormat::R8G8B8A8_UNORM,RenderTextureFlag::RENDER_TARGET));
                                    sceneAAWidth=tex->width;sceneAAHeight=tex->height;
                                }
                                if(sceneAAOutput && sceneProcessor->ProcessSceneColor(commandList,tex->texture.get(),sceneAAOutput.get(),
                                    tex->width,tex->height,static_cast<gpu::Antialiasing>(sceneAAMode==3?2:sceneAAMode))) {
                                    tex->layout=RenderTextureLayout::SHADER_READ;
                                    temporalDisplay=sceneAAOutput.get();sceneAABusy=true;sceneAARecorded=true;
                                    QueueResolveTrace(*tex,0xffff0011u);
                                    QueueResolveTrace(temporalDisplay,RenderFormat::R8G8B8A8_UNORM,tex->width,tex->height,RenderTextureLayout::SHADER_READ,0xffff0012u);
                                }
                            }
                            taaResolve.AddTo(tTaa);
                        }
                        if (diagnosticFullSceneCopy)
                            QueueResolveTrace(temporalDisplay ? temporalDisplay : tex->texture.get(), temporalDisplay ? temporalDisplayFormat : tex->format,
                                tex->width, tex->height, RenderTextureLayout::SHADER_READ, 0xffff0021u);
                        uint32_t d3 = fetch[3];
                        shared.textureInfo[slot] = ((fetch[0] >> 2) & 0xFF) | (((d3 >> 1) & 0xFFF) << 8);
                        shared.textureSize[slot] = tex->guestWidth | (tex->guestHeight << 16);
                        // GPU resolves retain canonical RGBA for presentation.
                        // Recreate COPY_DEST_SWAP at the guest texture-read boundary,
                        // before applying that fetch's source signs and swizzle.
                        if (ResolvedSurface* rs = FindResolved((fetch[1] >> 12) << 12, fetch[1] & 0x3F))
                            if (rs->swapRedBlue) shared.textureInfo[slot] |= 1u << 20;
                        uint64_t samplerKey = ((d3 >> 19) & 3) | (((d3 >> 21) & 3) << 2) | (((d3 >> 23) & 3) << 4)
                            | (((fetch[0] >> 10) & 7) << 6) | (((fetch[0] >> 13) & 7) << 9) | (((fetch[0] >> 16) & 7) << 12);
                        // The area-filtered guest grid still needs continuous
                        // reconstruction at the bloom shader's fractional UVs.
                        if (bloomFiltered && !temporalDisplay)
                            samplerKey = (samplerKey & ~uint64_t(0xF)) | 0x5;
                        shared.samplerIndex[slot] = GetSamplerIndex(samplerKey);
                        textureBindings[bank][slot] = temporalDisplay ? temporalDisplay :
                            bloomFiltered ? bloomFiltered->texture.get() : hdrBinding ? hdrBinding : tex->texture.get();
#if defined(LO_GPU_PLUME)
                        if (s == ps && key.ps == 0xb4b4d54a7a2d6b96ull && slot == 1 && bank == 0 &&
                            !temporalDisplay && !bloomFiltered && !hdrBinding)
                            alphaPostDepth = AlphaPostDepthInput{tex->texture.get(), tex->format,
                                tex->width, tex->height, samplerKey};
                        if (s == ps && !alphaBridgeFetches.empty() &&
                            alphaBridgeFetches.back().slot == slot && alphaBridgeFetches.back().bank == bank) {
                            alphaBridgeFetches.back().substitution = temporalDisplay ? "temporal_display" :
                                bloomFiltered ? "bloom_prefilter" : hdrBinding ? "hdr_temporal" : "none";
                            alphaBridgeFetches.back().samplerKey = samplerKey;
                            alphaBridgeFetches.back().prefilterOutput =
                                bloomFiltered ? bloomFiltered->texture.get() : nullptr;
                            alphaBridgeFetches.back().prefilterSourceIsReturnedColor =
                                bloomFiltered && !temporalDisplay &&
                                bloomPrefilterInput == alphaBridgeFetches.back().returnedColor;
                        }
#endif
                        if (selectedBinding) {
                            selectedBinding->bank = bank;
                            if (temporalDisplay || bloomFiltered || hdrBinding) {
                                // No bloom-filtered TextureKind exists. Unknown
                                // avoids reporting a filtered texture as a direct resolve.
                                selectedBinding->kind = temporalDisplay ?
                                    (temporalDisplayFromHistory ? binding::TextureKind::TemporalDisplay : binding::TextureKind::SpatialAA) :
                                    binding::TextureKind::Unknown;
                                // This output combines/filter samples; its source's matrix is not its producer proof.
                                selectedBinding->producerState = binding::ProducerState::Unknown;
                                selectedBinding->producer = {};
                                selectedBinding->producerFrameAge = -1;
                                selectedBinding->producerDraws = 0;
                                if (bloomFiltered && !temporalDisplay) {
                                    selectedBinding->hostExtent = {1280, 720};
                                    selectedBinding->parentExtent = {1280, 720};
                                    selectedBinding->resolveRect = {};
                                    selectedBinding->resolveFrameAge = -1;
                                    selectedBinding->resolveGap = -1;
                                }
                            }
                            bindingRecord->texture = *selectedBinding;
                        }
                        if (trackBinding && fullSceneCopy && slot == 0 && bank == sceneCopyBank) {
                            boundSceneProducer = tex->bindingProducer;
                            if (temporalDisplay) boundSceneProducer->Unknown(bindingEpoch, frame);
                        }
                    }
                };
                bindTextures(ps);
                if (failedPlan)
                    return;
                bindTextures(vs);
                if (failedPlan)
                    return;
#if defined(LO_GPU_PLUME)
                if (fsrAlphaBridge) for (const auto& request : alphaBridgeFetches) {
                    auto* bound = textureBindings[request.bank][request.slot];
                    const bool unchanged = bound == request.returnedColor;
                    const char* substitution = !unchanged && std::strcmp(request.substitution, "none") == 0 ?
                        "later_shader_binding_override" : request.substitution;
                    fsr_alpha::FetchCandidate candidate{};
                    const char* reason = "no_resolved_parent";
                    if (!unchanged) reason = "bound_image_substituted";
                    if (request.destinationAllocation && request.writeOrdinal &&
                         (unchanged || (fsrAlphaPostprocess && request.slot == 0 && request.bank == 0 &&
                             request.prefilterSourceIsReturnedColor &&
                             bound == request.prefilterOutput &&
                             std::strcmp(substitution, "bloom_prefilter") == 0))) {
                        candidate = fsrAlphaBridge->RecordFetchView(commandList, frame, temporalEpoch,
                            request.address,
                            request.resolveFormat, request.destinationAllocation, request.writeOrdinal,
                            request.returnedColor, request.cropWidth, request.cropHeight,
                            Gpu().fsrAlphaBridgeUses);
                        if (candidate && !unchanged && request.parentWidth == request.cropWidth &&
                            request.parentHeight == request.cropHeight &&
                            fsr_alpha::Contains(candidate.version.validRect,
                                {0, 0, request.cropWidth, request.cropHeight})) {
                            auto* areaSet = AcquireSet(1);
                            areaSet->setTexture(0, candidate.mask->texture.get(), RenderTextureLayout::SHADER_READ);
                            RenderDescriptorSet* areaSets[] = {staticSet0.get(), areaSet,
                                AcquireSet(2), AcquireSet(3), staticSamplerSet.get()};
                            auto area = fsrAlphaPostprocess->AreaMaximum(commandList, candidate.mask,
                                areaSets, 5, Gpu().fsrAlphaBridgeUses);
                            if (area) {
                                if (FsrAlphaBridgeTraceEnabled() && fsrAlphaBridgeTraceCount < 128) {
                                    auto evidence = std::make_shared<FsrAlphaBridgeDiagnostic>();
                                    evidence->writeOrdinal = request.writeOrdinal;
                                    evidence->fields = fmt::format(
                                        "\"kind\":\"host_bloom_prefilter\",\"status\":\"available\","
                                        "\"source_color_image\":{},\"source_mask_image\":{},"
                                        "\"source_extent\":[{},{}],\"source_valid_rect\":[{},{},{},{}],"
                                        "\"source_allocation\":{},\"source_revision\":{},"
                                        "\"output_color_image\":{},\"output_mask_image\":{},"
                                        "\"output_extent\":[1280,720]",
                                        uintptr_t(request.returnedColor), uintptr_t(candidate.mask->texture.get()),
                                        candidate.cropWidth, candidate.cropHeight,
                                        candidate.version.validRect.x, candidate.version.validRect.y,
                                        candidate.version.validRect.width, candidate.version.validRect.height,
                                        candidate.version.sourceAllocation, candidate.version.sourceRevision,
                                        uintptr_t(bound), uintptr_t(area->texture.get()));
                                    TraceFsrAlphaBridge(evidence);
                                    QueueFsrAlphaSnapshot(evidence, "prefilter-input-mask",
                                        candidate.mask->texture.get(), RenderFormat::R8_UNORM,
                                        candidate.cropWidth, candidate.cropHeight, 1,
                                        RenderTextureLayout::SHADER_READ);
                                    QueueFsrAlphaSnapshot(evidence, "prefilter-output-mask",
                                        area->texture.get(), RenderFormat::R8_UNORM,
                                        1280, 720, 1, RenderTextureLayout::SHADER_READ);
                                }
                                candidate.mask = std::move(area);
                                candidate.returnedColor = bound;
                                candidate.cropWidth = 1280; candidate.cropHeight = 720;
                            }
                        }
                        reason = candidate.Matches(bound) ? "available" :
                            unchanged ? candidate.reason : "unmatched_host_prefilter";
                    }
                    if (request.bank == 0 && request.slot < alphaPostInputs.size() &&
                        candidate.Matches(bound))
                        alphaPostInputs[request.slot] = AlphaPostInput{candidate, request.samplerKey,
                            bound, substitution};
                    if (FsrAlphaBridgeTraceEnabled() && fsrAlphaBridgeTraceCount < 128) {
                        auto event = std::make_shared<FsrAlphaBridgeDiagnostic>();
                        event->writeOrdinal = request.writeOrdinal;
                        event->fields = fmt::format(
                            "\"kind\":\"fetch_bound\",\"status\":\"{}\","
                            "\"vs_hash\":\"{:016x}\",\"ps_hash\":\"{:016x}\",\"ps_slot\":{},\"bank\":{},"
                            "\"address\":{},\"fetch_format\":{},\"resolve_format\":{},"
                            "\"destination_allocation\":{},\"write_ordinal\":{},"
                            "\"source_allocation\":{},\"raw_revision\":{},"
                            "\"guest_fetch_width\":{},\"guest_parent_width\":{},"
                            "\"crop_extent\":[{},{}],\"parent_extent\":[{},{}],"
                            "\"returned_color_image\":{},\"finally_bound_image\":{},\"mask_image\":{},"
                            "\"substitution\":\"{}\"",
                            reason, key.vs, key.ps, request.slot, request.bank,
                            request.address, request.fetchFormat, request.resolveFormat,
                            request.destinationAllocation, request.writeOrdinal,
                            candidate.version.sourceAllocation, candidate.version.rawDraws,
                            request.guestFetchWidth, request.guestParentWidth,
                            request.cropWidth, request.cropHeight, request.parentWidth, request.parentHeight,
                            uintptr_t(request.returnedColor), uintptr_t(bound),
                            candidate ? uintptr_t(candidate.mask->texture.get()) : 0, substitution);
                        TraceFsrAlphaBridge(std::move(event));
                    }
                }
#endif
                RenderDescriptorSet *set1, *set2, *set3;
                {
                    ScopedTimer timer{ tSets, cpuTimingEnabled };
                    set1 = AcquireTextureSet(1, textureBindings[0], activeTextureSlots[0]);
                    set2 = AcquireTextureSet(2, textureBindings[1], activeTextureSlots[1]);
                    set3 = AcquireTextureSet(3, textureBindings[2], activeTextureSlots[2]);
                }
                if (bindingRecord) {
                    // Shared sign/swizzle/sampler constants may also have been written by VS texture0.
                    auto& texture = bindingRecord->texture;
                    texture.sign = shared.textureInfo[0] & 0xff;
                    texture.swizzle = (shared.textureInfo[0] >> 8) & 0xfff;
                    texture.swapRedBlue = (shared.textureInfo[0] & (1u << 20)) != 0;
                    uint64_t actualSampler = 0x2 | (0x2 << 2) | (0x1 << 4); // Palette initialization.
                    for (const auto& [sampler, index] : samplerPalette)
                        if (index == shared.samplerIndex[0]) { actualSampler = sampler; break; }
                    texture.sampler = {uint32_t((actualSampler >> 6) & 7), uint32_t((actualSampler >> 9) & 7),
                        uint32_t((actualSampler >> 12) & 7), uint32_t((actualSampler >> 2) & 3),
                        uint32_t(actualSampler & 3), uint32_t((actualSampler >> 4) & 3)};
                }
                tBind0.AddTo(tBind);
                render_batch::CpuTimer<> tIndex0(cpuTimingEnabled);

                uint64_t vsOffset = UploadUnchanged(0, vsConstants, sizeof(vsConstants));
                uint64_t psOffset = UploadUnchanged(1, psConstants, sizeof(psConstants));
                uint64_t sharedOffset = UploadUnchanged(2, &shared, sizeof(shared));
                if (vsOffset == UINT64_MAX || psOffset == UINT64_MAX || sharedOffset == UINT64_MAX)
                {
                    drops.upload++;
                    return;
                }

                // Index buffer / primitive conversion. Static geometry skips
                // ConvertIndices and primitive expansion on an exact-content
                // hit; the cached output is already post-expansion.
                auto& indices = indexScratch;
                if (!info.indexed) indices.clear();
                bool useIndices = false;
                RenderFormat indexFormat = RenderFormat::R32_UINT;
                uint32_t indexCount = info.indexCount;
                bool indexCached = false;
                geometry_prepare::IndexEntry* cachedIndexEntry = nullptr;
                uint32_t indexSrcCount = 0;
                const uint8_t* indexSrc = nullptr;
                size_t indexSrcBytes = 0;
                geometry_prepare::IndexKey indexKey{};
                if (info.indexed)
                {
                    indexSrcCount = std::min<uint32_t>(info.indexCount, info.indexBufferWords);
                    indexSrc = Phys(info.indexBase);
                    indexSrcBytes = size_t(indexSrcCount) * (info.index32 ? 4 : 2);
                    if (indexSrcCount >= geometry_prepare::IndexCache::kMinCount)
                    {
                        indexKey = { info.indexBase, indexSrcCount, info.primitiveType,
                            uint8_t(info.index32 ? 1 : 0), uint8_t(info.indexEndian & 3) };
                        auto it = indexCache.find(indexKey);
                        if (it != indexCache.end() && it->second.content.Matches(indexSrc, indexSrcBytes))
                        {
                            indices = it->second.data;
                            cachedIndexEntry = &it->second;
                            it->second.lastFrame = frame;
                            ++indexCacheHits;
                            useIndices = true;
                            indexCached = true;
                        }
                        else
                        {
                            // Convert below, then replace the cached result.
                            indices.resize(indexSrcCount);
                            geometry_prepare::ConvertIndices(indexSrc, indices.data(), indexSrcCount, info.index32, info.indexEndian);
                            useIndices = true;
                        }
                    }
                    else
                    {
                        indices.resize(indexSrcCount);
                        geometry_prepare::ConvertIndices(indexSrc, indices.data(), indexSrcCount, info.index32, info.indexEndian);
                        useIndices = true;
                    }
                }
                if (!indexCached)
                switch (info.primitiveType)
                {
                case 13: // quad list -> triangle list
                {
                    geometry_prepare::ExpandQuadList(indices, primitiveScratch, useIndices, info.indexCount);
                    break;
                }
                case 5: // triangle fan -> list
                {
                    auto& out = primitiveScratch;
                    out.clear();
                    uint32_t n = useIndices ? uint32_t(indices.size()) : info.indexCount;
                    out.reserve(n > 2 ? size_t(n - 2) * 3 : 0);
                    for (uint32_t i = 2; i < n; i++)
                    {
                        uint32_t a = useIndices ? indices[0] : 0, b = useIndices ? indices[i - 1] : i - 1, c = useIndices ? indices[i] : i;
                        out.insert(out.end(), { a, b, c });
                    }
                    indices.swap(out);
                    useIndices = true;
                    break;
                }
                default:
                    break;
                }
                if (info.indexed && !indexCached && indexSrcCount >= geometry_prepare::IndexCache::kMinCount)
                {
                    // Store the post-expansion result against the exact source
                    // bytes; a later identical draw copies it verbatim.
                    geometry_prepare::IndexEntry entry;
                    entry.data = indices;
                    entry.content.Capture(indexSrc, indexSrcBytes);
                    entry.lastFrame = frame;
                    indexCache.emplace(indexKey, std::move(entry));
                    ++indexCacheMisses;
                }
                if (useIndices)
                    indexCount = uint32_t(indices.size());
                if (indexCount == 0)
                {
                    drops.index++;
                    drops.primMask |= 1u << (info.primitiveType & 31);
                    return;
                }

                tIndex0.AddTo(tIndex);

                // Activation may split this batch into prefix/isolated/continuation.
                // Upload indices first so no allocator pressure can Flush between the
                // promoted fallback and the original guarded draw.
                uint64_t preparedIndexOffset = UINT64_MAX;
                if (useIndices) {
                    preparedIndexOffset = Upload(indices.data(), indices.size() * 4, 16);
                    if (preparedIndexOffset == UINT64_MAX) return;
                }

                // Record.
                ScopedTimer recordTimer{ tRecord, cpuTimingEnabled };
                RenderRect scissor(int32_t(scissorTl & 0x3FFF), int32_t((scissorTl >> 16) & 0x3FFF), int32_t(scissorBr & 0x3FFF), int32_t((scissorBr >> 16) & 0x3FFF));
                uint32_t windowOffset = Reg(REG_PA_SC_WINDOW_OFFSET);
                if (!(scissorTl & 0x80000000u) && windowOffset)
                {
                    int32_t ox = int32_t(windowOffset << 17) >> 17, oy = int32_t(windowOffset << 1) >> 17;
                    scissor.left += ox; scissor.right += ox; scissor.top += oy; scissor.bottom += oy;
                }
                scissor.left = std::clamp(scissor.left, 0, int32_t(rasterTarget->guestWidth)); scissor.right = std::clamp(scissor.right, 0, int32_t(rasterTarget->guestWidth));
                scissor.top = std::clamp(scissor.top, 0, int32_t(rasterTarget->guestHeight)); scissor.bottom = std::clamp(scissor.bottom, 0, int32_t(rasterTarget->guestHeight));
                const RenderRect guestScissor = scissor;
                bool scenePromotionActivated = false;
                if (dlssSrRequested && dlssSceneCopyInputs && fullSceneCopy && !depth
#if defined(LO_GPU_PLUME)
                    && vulkan && dlssController
#else
                    && false
#endif
                    ) {
                    const RenderTargetKey promotionKey{colorInfo & 0xFFF, ColorClassOf((colorInfo >> 16) & 0xF), pitch, 0, false};
                    if (PrepareSceneCopyDestination(promotionKey, *color, *dlssSceneCopyInputs)) {
                        sceneCopyPromotion.fsrMaskLease = selectedFsrMaskLease;
                        scenePromotionActivated = ActivateSceneCopyDestination(color, rasterTarget, rasterViewport, scissor,
                            viewport, guestScissor);
                    }
                }
                if (!scenePromotionActivated) {
                    scissor.left = int32_t(rasterTarget->ScaleX(uint32_t(scissor.left))); scissor.right = int32_t(rasterTarget->ScaleX(uint32_t(scissor.right)));
                    scissor.top = int32_t(rasterTarget->ScaleY(uint32_t(scissor.top))); scissor.bottom = int32_t(rasterTarget->ScaleY(uint32_t(scissor.bottom)));
                } else {
                    // Activation can replace the map entry after the original
                    // render-target acquisition. Re-read the final attachments
                    // before framebuffer, viewport, and pipeline binding so a
                    // restored/retargeted mapping never produces a mixed grid.
                    auto finalColor = renderTargets.find(sceneCopyPromotion.key);
                    if (finalColor == renderTargets.end() || finalColor->second.get() != color)
                        return;
                    color = finalColor->second.get();
                    if (!depthOnlyRaster)
                        rasterTarget = color;
                    key.rtFormat = uint32_t(color->format);
                    key.depthFormat = depth ? uint32_t(depth->format) : 0;
                    pipeline = GetPipeline(key, vs, ps, color->format, depth ? depth->format : RenderFormat::UNKNOWN);
                    if (!pipeline) {
                        ++drops.pipeline;
                        drops.primMask |= 1u << (info.primitiveType & 31);
                        return;
                    }
                }
                // Height is a historical EDRAM allocation estimate. Attachments
                // may have different padding while covering the same draw; keep
                // that draw and constrain it to their common physical extent.
                const int32_t attachmentWidth = int32_t(depthOnlyRaster ? depth->width : depth ? std::min(color->width, depth->width) : color->width);
                const int32_t attachmentHeight = int32_t(depthOnlyRaster ? depth->height : depth ? std::min(color->height, depth->height) : color->height);
                scissor.left = std::min(scissor.left, attachmentWidth); scissor.right = std::min(scissor.right, attachmentWidth);
                scissor.top = std::min(scissor.top, attachmentHeight); scissor.bottom = std::min(scissor.bottom, attachmentHeight);
                if (scissor.right <= scissor.left || scissor.bottom <= scissor.top)
                {
                    drops.scissor++;
                    return;
                }
                RenderFramebuffer* framebuffer = GetFramebuffer(color, depth);
                commandList->setFramebuffer(framebuffer);
                commandList->setViewports(&rasterViewport, 1);
                commandList->setScissors(&scissor, 1);
                if (p2Evidence.is_open() && ps &&
                    (key.ps == 0xb4b4d54a7a2d6b96ull || key.ps == 0xcda578aef1724fdcull))
                {
                    // This is intentionally the local color attachment for the draw,
                    // rather than a sampled pre-UI texture. It is the only target
                    // identity that can later support an oracle mapping decision.
                    const uint32_t targetFormat = (colorInfo >> 16) & 0xF;
                    const uint32_t targetBase = colorInfo & 0xFFF;
                    const uint32_t f0 = Reg(REG_FETCH_CONSTANTS);
                    const uint32_t f1 = Reg(REG_FETCH_CONSTANTS + 1);
                    const uint32_t sampledAddress = (f1 >> 12) << 12;
                    const uint32_t sampledFormat = f1 & 0x3F;
                    const ResolvedSurface* sampled = (f0 & 3) == 2 ? FindResolved(sampledAddress, sampledFormat) : nullptr;
                    const uint32_t captureDraw = debugDraw ? debugDraw - 1 : 0;
                    p2Evidence << "{\"schema\":\"lostodyssey.p2-oracle-evidence.v1\",\"event\":\"draw\",\"renderer_frame\":" << frame
                        << ",\"draw_id\":" << captureDraw
                        << ",\"shader\":{\"vs\":\"" << fmt::format("{:016x}", key.vs)
                        << "\",\"ps\":\"" << fmt::format("{:016x}", key.ps) << "\"}"
                        << ",\"plan\":{\"cpu_serial\":" << activePlan.cpuSerial
                        << ",\"geometry_epoch\":" << activePlan.geometryEpoch
                        << ",\"consumer\":" << uint32_t(activePlan.consumer)
                        << ",\"input\":[" << activePlan.width << ',' << activePlan.height << "]"
                        << ",\"output\":[" << activePlan.output.width << ',' << activePlan.output.height << "]"
                        << ",\"output_rect\":[" << activePlan.output.x << ',' << activePlan.output.y << ','
                        << activePlan.output.width << ',' << activePlan.output.height << "]"
                        << ",\"output_xy_scope\":\"presentation_only_not_destination_mapping\"}"
                        << ",\"ps_constants\":{\"bank_base\":\"0x4400\",\"values\":[";
                    bool firstConstant = true;
                    for (uint32_t constant = 0; constant <= 10; ++constant) {
                        const uint32_t* words = psConstants + constant * 4;
                        p2Evidence << (firstConstant ? "" : ",") << "{\"constant\":" << constant
                            << ",\"register\":\"" << fmt::format("0x{:04x}", 0x4400 + constant * 4)
                            << "\",\"u32\":[" << words[0] << ',' << words[1] << ',' << words[2] << ',' << words[3] << "]}";
                        firstConstant = false;
                    }
                    const uint32_t* c255 = psConstants + 255 * 4;
                    p2Evidence << ",{\"constant\":255,\"register\":\"0x47fc\",\"u32\":[" << c255[0] << ',' << c255[1] << ',' << c255[2] << ',' << c255[3] << "]}]}";
                    p2Evidence << ",\"fetch_slots\":[";
                    for (uint32_t slot = 0; slot < 4; ++slot) {
                        uint32_t fetch[6];
                        for (uint32_t word = 0; word < 6; ++word) fetch[word] = Reg(REG_FETCH_CONSTANTS + slot * 6 + word);
                        std::optional<uint64_t> samplerKey;
                        for (const auto& [candidate, index] : samplerPalette)
                            if (index == shared.samplerIndex[slot]) { samplerKey = candidate; break; }
                        p2Evidence << (slot ? "," : "") << "{\"slot\":" << slot << ",\"words\":["
                            << fetch[0] << ',' << fetch[1] << ',' << fetch[2] << ',' << fetch[3] << ',' << fetch[4] << ',' << fetch[5]
                            << "],\"shared_texture_info\":" << shared.textureInfo[slot]
                            << ",\"shared_sampler_index\":" << shared.samplerIndex[slot]
                            << ",\"actual_sampler_key\":";
                        if (samplerKey) p2Evidence << '"' << fmt::format("0x{:x}", *samplerKey) << '"'; else p2Evidence << "null";
                        p2Evidence << ",\"actual_sampler_fields\":";
                        if (samplerKey) p2Evidence << '[' << ((*samplerKey >> 6) & 7) << ',' << ((*samplerKey >> 9) & 7)
                            << ',' << ((*samplerKey >> 12) & 7) << ',' << ((*samplerKey >> 2) & 3)
                            << ',' << (*samplerKey & 3) << ',' << ((*samplerKey >> 4) & 3) << ']';
                        else p2Evidence << "null";
                        p2Evidence << ",\"srv_guest_format\":" << (fetch[1] & 0x3F) << '}';
                    }
                    p2Evidence << ']'
                        << ",\"sampled_slot0_resolved\":";
                    if (sampled && sampled->tex) {
                        const auto& source = *sampled->tex;
                        p2Evidence << "{\"guest_base\":" << sampledAddress << ",\"guest_format\":" << sampledFormat
                            << ",\"allocation\":" << source.allocationSerial << ",\"host_format\":" << uint32_t(source.format)
                            << ",\"write_frame\":" << sampled->frame << ",\"write_version\":" << sampled->writeOrdinal
                            << ",\"write_ordinal\":" << sampled->writeOrdinal
                            << ",\"sdr_ordinal\":" << sampled->sdrWriteOrdinal
                            << ",\"rect\":[" << sampled->writeX << ',' << sampled->writeY << ',' << sampled->writeWidth << ',' << sampled->writeHeight << "]}";
                    } else p2Evidence << "null";
                    p2Evidence << ",\"destination\":{\"render_target_key\":{\"base\":" << targetBase
                        << ",\"color_storage_class\":" << ColorClassOf(targetFormat) << ",\"pitch\":" << pitch
                        << ",\"height\":0,\"depth\":false}"
                        << ",\"guest_base\":" << targetBase << ",\"guest_pitch\":" << pitch
                        << ",\"guest_storage_format\":" << targetFormat << ",\"color_storage_class\":" << ColorClassOf(targetFormat)
                        << ",\"allocation\":" << color->allocationSerial << ",\"host_format\":" << uint32_t(color->format)
                        << ",\"extent\":[" << color->width << ',' << color->height << "]"
                        << ",\"guest_extent\":[" << color->guestWidth << ',' << color->guestHeight << "]"
                        << ",\"resolution_size\":[" << color->resolutionSize.width << ',' << color->resolutionSize.height << "]"
                        << ",\"guest_viewport\":[" << viewport.x << ',' << viewport.y << ',' << viewport.width << ',' << viewport.height << "]"
                        << ",\"guest_scissor\":[" << guestScissor.left << ',' << guestScissor.top << ',' << guestScissor.right << ',' << guestScissor.bottom << "]"
                        << ",\"physical_scissor\":[" << scissor.left << ',' << scissor.top << ',' << scissor.right << ',' << scissor.bottom << "]}"
                        << ",\"framebuffer_attachments\":{\"color_allocation\":" << color->allocationSerial << ",\"depth_allocation\":";
                    if (depth) p2Evidence << depth->allocationSerial; else p2Evidence << "null";
                    p2Evidence << ",\"depth_extent\":";
                    if (depth) p2Evidence << '[' << depth->width << ',' << depth->height << ']'; else p2Evidence << "null";
                    p2Evidence << "},\"render_state\":{\"color_control\":" << Reg(REG_RB_COLORCONTROL)
                        << ",\"blend_control\":" << key.blend << ",\"color_mask\":" << key.colorMask
                        << ",\"exp_bias\":\"unknown\"}}\n";
                }
                commandList->setPipeline(pipeline);
                commandList->setGraphicsPipelineLayout(pipelineLayout.get());
                if (vulkan) {
                    const uint64_t base = uploadRing->getDeviceAddress();
                    constantAddresses[0] = base + vsOffset;
                    constantAddresses[1] = base + sharedOffset;
                    constantAddresses[2] = base + psOffset;
                    commandList->setGraphicsPushConstants(0, constantAddresses);
                } else {
                    SetConstantBuffer(vsOffset, 0);
                    SetConstantBuffer(sharedOffset, 1);
                    SetConstantBuffer(psOffset, 2);
                }
                commandList->setGraphicsDescriptorSet(set0, 0);
                commandList->setGraphicsDescriptorSet(set1, 1);
                commandList->setGraphicsDescriptorSet(set2, 2);
                commandList->setGraphicsDescriptorSet(set3, 3);
                if(vulkan) commandList->setGraphicsDescriptorSet(staticSamplerSet.get(),4);

                int32_t baseVertex = int32_t(Reg(REG_VGT_INDX_OFFSET));
                static uint32_t drawLogs = 0;
                if (psTraceRemaining && ps && key.ps == psTraceHash)
                {
                    if (++psTraceDraws <= 64)
                    {
                        std::string values;
                        for (uint32_t ci = psTraceFirst; ci < psTraceFirst + psTraceCount; ++ci)
                        {
                            const uint32_t* v = psConstants + ci * 4;
                            values += fmt::format(" c{}={:08x},{:08x},{:08x},{:08x}", ci, v[0], v[1], v[2], v[3]);
                        }
                        SHADER_LOG_INFO("pixel-constants", RendererByteFnv, "renderer: ps trace f{} ps={:016x} indices={}{}", frame, key.ps, info.indexCount, values);
                    }
                }
                if (!debugCaptureDir.empty())
                {
                    debugTrace << fmt::format("shaders vs={:016x} ps={:016x} ps_status={}\n", key.vs, key.ps,
                        ps ? "bound" : (psHash ? "shader_unavailable" : "not_bound"));
                    for (const auto& entry : {std::make_pair(key.vs, vs), std::make_pair(key.ps, ps)})
                    {
                        if (!entry.second) continue;
                        const auto path = debugCaptureRoot / "shaders" / fmt::format("{:016x}.hlsl", entry.first);
                        if (debugShaders.insert(entry.first).second)
                        {
                            std::ofstream out(path); out << entry.second->info.hlsl; out.close();
                            if (out.fail()) debugTrace.setstate(std::ios::failbit);
                            if (debugShaderSources)
                                debugShaderSources->MarkHlsl(entry.second == vs, entry.first,
                                    !out.fail() && !entry.second->info.hlsl.empty());
                        }
                    }
                }
                const uint32_t traceFrame = TraceFrame();
                static const uint32_t traceCount = getenv("LO_DRAW_TRACE_COUNT") ? strtoul(getenv("LO_DRAW_TRACE_COUNT"), nullptr, 10) : 1;
                if (traceFrame && frame >= traceFrame && frame < traceFrame + traceCount && (info.indexCount >= 200 || info.primitiveType == 8 || info.indexCount == 36))
                {
                    // Extra detail for big draws: constants the 3D path relies on and
                    // the head of every vertex stream (as floats).
                    std::string detail;
                    for (uint32_t ci : { 0u, 8u, 9u, 10u, 233u, 234u, 235u, 236u, 254u, 255u })
                        detail += fmt::format(" c{}=({:g},{:g},{:g},{:g})", ci, RegF(REG_ALU_CONSTANTS + ci * 4), RegF(REG_ALU_CONSTANTS + ci * 4 + 1), RegF(REG_ALU_CONSTANTS + ci * 4 + 2), RegF(REG_ALU_CONSTANTS + ci * 4 + 3));
                    for (uint32_t slot = 0; slot < kVertexFetchSlots; slot++)
                    {
                        if (!((vs->info.vertexFetchSlotMask[slot >> 6] >> (slot & 63)) & 1))
                            continue;
                        uint32_t d0 = Reg(REG_FETCH_CONSTANTS + slot * 2), d1 = Reg(REG_FETCH_CONSTANTS + slot * 2 + 1);
                        detail += fmt::format(" vf{}=[type {} addr {:#x} dwords {} endian {}:", slot, d0 & 3, d0 & ~3u, (d1 >> 2) & 0xFFFFFF, d1 & 3);
                        const uint32_t* src = reinterpret_cast<const uint32_t*>(Phys(d0 & ~3u));
                        for (uint32_t i = 0; i < std::min(24u, (d1 >> 2) & 0xFFFFFF); i++)
                        {
                            uint32_t v = GpuSwap(src[i], d1 & 3); float f; memcpy(&f, &v, 4);
                            detail += fmt::format(" {:g}/{:#x}", f, v);
                        }
                        detail += "]";
                    }
                    {
                        std::string consts;
                        for (uint32_t ci = 0; ci < 256; ci++)
                        {
                            float v[4]; bool any = false;
                            for (int k = 0; k < 4; k++) { v[k] = RegF(REG_ALU_CONSTANTS + ci * 4 + k); any |= v[k] != 0.0f; }
                            if (any) consts += fmt::format(" c{}=({:g},{:g},{:g},{:g})", ci, v[0], v[1], v[2], v[3]);
                        }
                        SHADER_LOG_INFO("draw-constants", RendererByteFnv, "renderer: draw consts{}", consts);
                    }
                    if (info.indexed)
                    {
                        detail += fmt::format(" idx=[base {:#x} words {} endian {} 32bit {} offset {}:", info.indexBase, info.indexBufferWords, info.indexEndian, info.index32, int32_t(Reg(REG_VGT_INDX_OFFSET)));
                        const uint8_t* src = Phys(info.indexBase);
                        for (int i = 0; i < 12; i++)
                        {
                            uint32_t v;
                            if (info.index32) { memcpy(&v, src + i * 4, 4); v = GpuSwap(v, info.indexEndian); }
                            else { uint16_t s16; memcpy(&s16, src + i * 2, 2); v = GpuSwap(s16, info.indexEndian) & 0xFFFF; }
                            detail += fmt::format(" {}", v);
                        }
                        detail += "]";
                    }
                    SHADER_LOG_INFO("draw-state", RendererByteFnv, "renderer: draw detail{}", detail);
                }
                if (traceFrame && frame >= traceFrame && frame < traceFrame + traceCount)
                {
                    // Capture the actual uploaded PS bank as well as the VS data.
                    // Lighting changes cannot be diagnosed from bone matrices alone.
                    if (ps)
                    {
                        std::string values;
                        for (uint32_t ci = 0; ci < 256; ci++)
                        {
                            const uint32_t* v = psConstants + ci * 4;
                            if (!(v[0] | v[1] | v[2] | v[3])) continue;
                            values += fmt::format(" c{}={:08x},{:08x},{:08x},{:08x}", ci, v[0], v[1], v[2], v[3]);
                        }
                        SHADER_LOG_INFO("pixel-constants", RendererByteFnv, "renderer: draw psconsts f{} ps={:016x} upload={:#x}{}", frame, key.ps, psOffset, values);
                    }
                    std::string texs;
                    for (Shader* sh : { ps, vs })
                    {
                        if (!sh) continue;
                        for (uint32_t slot = 0; slot < kTextureSlots; slot++)
                        {
                            if (!((sh->info.textureSlotMask >> slot) & 1)) continue;
                            uint32_t f0 = Reg(REG_FETCH_CONSTANTS + slot * 6), f1 = Reg(REG_FETCH_CONSTANTS + slot * 6 + 1), f2 = Reg(REG_FETCH_CONSTANTS + slot * 6 + 2);
                            if ((f0 & 3) != 2) { texs += fmt::format(" t{}=[none]", slot); continue; }
                            uint32_t base = (f1 >> 12) << 12;
                            bool fromResolve = FindResolved(base, f1 & 0x3F) != nullptr;
                            texs += fmt::format(" t{}=[fmt {} {}x{} at {:#x}{} sign={:#x} swizzle={:#x}]", slot, f1 & 0x3F, (f2 & 0x1FFF) + 1, ((f2 >> 13) & 0x1FFF) + 1, base, fromResolve ? " resolved" : "", (f0 >> 2) & 0xFF, (Reg(REG_FETCH_CONSTANTS + slot * 6 + 3) >> 1) & 0xFFF);
                            texs += fmt::format(" fetch{}={:08x},{:08x},{:08x},{:08x},{:08x},{:08x}", slot, f0, f1, f2,
                                Reg(REG_FETCH_CONSTANTS + slot * 6 + 3), Reg(REG_FETCH_CONSTANTS + slot * 6 + 4), Reg(REG_FETCH_CONSTANTS + slot * 6 + 5));
                        }
                    }
                    SHADER_LOG_INFO("draw-textures", RendererByteFnv, "renderer: draw textures{}", texs);
                }
                if (drawLogs < 24 || (traceFrame && frame >= traceFrame && frame < traceFrame + traceCount))
                {
                    drawLogs++;
                    SHADER_LOG_INFO("draw-state", RendererByteFnv, "renderer: clip f{} vs={:016x} ps={:016x} control={:#x}", frame, key.vs, key.ps, Reg(0x2204));
                    SHADER_LOG_INFO("draw-state", RendererByteFnv, "renderer: draw f{} prim={} n={} idx={} vs={:016x} ps={:016x} rt={:#x}/{} {}x{} depth={:#x} dinfo={:#x} blend={:#x} mask={:#x} cull={:#x} colorctl={:#x} aref={:g} ring(vs={:#x} ps={:#x} sh={:#x}) vp=({},{} {}x{} z {}..{}) vte={:#x} scissor=({},{})-({},{}) ndc=({},{}) off=({},{}) mode={} c255=({:g},{:g},{:g},{:g})",
                        frame, info.primitiveType, indexCount, useIndices, key.vs, key.ps, colorInfo & 0xFFF, (colorInfo >> 16) & 0xF, pitch, rtHeight, depthControl, depthInfo,
                        key.blend, key.colorMask, key.modeCull, Reg(REG_RB_COLORCONTROL), RegF(REG_RB_ALPHA_REF), vsOffset, psOffset, sharedOffset,
                        viewport.x, viewport.y, viewport.width, viewport.height, viewport.minDepth, viewport.maxDepth, vte,
                        scissor.left, scissor.top, scissor.right, scissor.bottom, shared.ndcScale[0], shared.ndcScale[1], shared.ndcOffset[0], shared.ndcOffset[1], modeControl,
                        RegF(REG_ALU_CONSTANTS + 255 * 4), RegF(REG_ALU_CONSTANTS + 255 * 4 + 1), RegF(REG_ALU_CONSTANTS + 255 * 4 + 2), RegF(REG_ALU_CONSTANTS + 255 * 4 + 3));
                }
                if (useIndices)
                {
                    RenderIndexBufferView view(RenderBufferReference(uploadRing, preparedIndexOffset), uint32_t(indices.size() * 4), RenderFormat::R32_UINT);
                    commandList->setIndexBuffer(&view);
                    commandList->drawIndexedInstanced(indexCount, 1, 0, baseVertex, 0);
                }
                else
                {
                    commandList->drawInstanced(indexCount, 1, uint32_t(baseVertex), 0);
                }
  #if defined(LO_GPU_PLUME)
                bool alphaReplayRecorded = false;
                // Controlled P2 collection: this raw R8 mask is deliberately
                // independent of the later scene-copy/SDK inputs. Only the six
                // audited original PS outputs can contribute to it.
                if (fsrAlphaReplay && dlssSrRequested && !scenePromotionActivated &&
                    activePlan.requestedUpscaler == upscaling::Upscaler::Fsr &&
                    fsr_alpha::AuditedPair(key.vs, key.ps) &&
                    fsr_alpha::AuditedState(key.blend, depthControl, depthInfo, key.colorMask, info.primitiveType) &&
                    ps && !vs->info.usesPointSize && vs->info.errors.empty() &&
                    !ps->info.writesDepth && ps->info.errors.empty() &&
                    (ps->info.colorTargetsWritten & 1u) && depth && jitterAnchor && drawJitter.applied &&
                    depth->allocationSerial == jitterAnchor->depthAllocation &&
                    color->width == depth->width && color->height == depth->height &&
                    rasterViewport.x == 0 && rasterViewport.y == 0 &&
                    rasterViewport.width > 0 && rasterViewport.height > 0 &&
                    rasterViewport.x >= 0 && rasterViewport.y >= 0 &&
                    rasterViewport.width > 0 && rasterViewport.height > 0 &&
                    std::floor(rasterViewport.x) == rasterViewport.x &&
                    std::floor(rasterViewport.y) == rasterViewport.y &&
                    std::floor(rasterViewport.width) == rasterViewport.width &&
                    std::floor(rasterViewport.height) == rasterViewport.height &&
                    rasterViewport.x + rasterViewport.width <= color->width &&
                    rasterViewport.y + rasterViewport.height <= color->height) {
                    const auto alphaDesc = DescribePipeline(key, vs, ps, false);
                    if (alphaDesc.depthEnabled && !alphaDesc.depthWriteEnabled &&
                        alphaDesc.depthFunction == RenderComparisonFunction::GREATER_EQUAL &&
                        !alphaDesc.stencilEnabled && !alphaDesc.geometryShader) {
                        auto* alphaPipeline = fsrAlphaReplay->Prepare(key, alphaDesc, vs->info, ps->info,
                            vsWords, vsCount, psWords, psCount);
                        if (alphaPipeline) {
                            fsr_alpha::FrameIdentity identity{};
                            identity.renderFrame = frame; identity.geometryEpoch = temporalEpoch;
                            identity.colorAllocation = color->allocationSerial;
                            identity.depthAllocation = depth->allocationSerial;
                            identity.width = color->width; identity.height = color->height;
                            identity.viewportBits = {
                                std::bit_cast<uint32_t>(float(rasterViewport.x)),
                                std::bit_cast<uint32_t>(float(rasterViewport.y)),
                                std::bit_cast<uint32_t>(float(rasterViewport.width)),
                                std::bit_cast<uint32_t>(float(rasterViewport.height))};
                            identity.jitterBits = {
                                std::bit_cast<uint32_t>(float(drawJitter.sample.pixelX)),
                                std::bit_cast<uint32_t>(float(drawJitter.sample.pixelY))};
                            const RenderBufferReference alphaConstants[3] = {
                                {uploadRing, vsOffset}, {uploadRing, sharedOffset}, {uploadRing, psOffset}};
                            RenderDescriptorSet* alphaSets[] = {set0, set1, set2, set3, staticSamplerSet.get()};
                            auto equality = PrepareFsrAlphaEquality(*color, *depth, drawsThisFrame);
                            RecordFsrAlphaEqualitySnapshot(equality, *color, *depth, false);
                            Transition(*depth, RenderTextureLayout::DEPTH_READ, RenderBarrierStage::GRAPHICS);
                            const bool recorded = fsrAlphaReplay->Draw(commandList, alphaPipeline, identity,
                                depth->texture.get(), alphaConstants, alphaSets, 5, rasterViewport, scissor,
                                useIndices, indexCount, baseVertex, drawsThisFrame, Gpu().fsrAlphaBatches);
                            Transition(*depth, RenderTextureLayout::DEPTH_WRITE, RenderBarrierStage::GRAPHICS);
                            if (equality) equality->replayRecorded = recorded;
                            RecordFsrAlphaEqualitySnapshot(equality, *color, *depth, true);
                            if (recorded) {
                                alphaReplayRecorded = true;
                                auto view = fsrAlphaReplay->ViewFor(identity);
                                if (fsrAlphaBridge) fsrAlphaBridge->PublishRaw(view);
                                auto existing = std::find_if(fsrAlphaRawViews.begin(), fsrAlphaRawViews.end(),
                                    [&](const auto& value) { return value.identity == identity; });
                                if (existing == fsrAlphaRawViews.end()) fsrAlphaRawViews.push_back(std::move(view));
                                else *existing = std::move(view);
                            }
                            else {
                                fsrAlphaRawViews.clear();
                                if (fsrAlphaBridge) fsrAlphaBridge->DiscardUnsubmitted();
                                if (frame % 120 == 0)
                                    LOG_WARNING("fsr alpha: replay failed frame={} draw={} reason={}",
                                        frame, drawsThisFrame, fsrAlphaReplay->LastError());
                            }
                            // The original guest bindings are still needed by
                            // later draws and scene-copy promotion in this list.
                            commandList->setFramebuffer(framebuffer);
                            commandList->setViewports(&rasterViewport, 1);
                            commandList->setScissors(&scissor, 1);
                            commandList->setGraphicsPipelineLayout(pipelineLayout.get());
                            commandList->setPipeline(pipeline);
                            commandList->setGraphicsPushConstants(0, constantAddresses);
                            commandList->setGraphicsDescriptorSet(set0, 0);
                            commandList->setGraphicsDescriptorSet(set1, 1);
                            commandList->setGraphicsDescriptorSet(set2, 2);
                            commandList->setGraphicsDescriptorSet(set3, 3);
                            commandList->setGraphicsDescriptorSet(staticSamplerSet.get(), 4);
                        } else if (fsrAlphaReplay->FailedThisFrame()) {
                            fsrAlphaRawViews.clear();
                            if (fsrAlphaBridge) fsrAlphaBridge->DiscardUnsubmitted();
                        }
                    }
                }
                bool alphaPostprocessRecorded = false;
                const char* alphaPostprocessReason = "state_guard";
                std::string alphaPostprocessGuards;
                std::string alphaPostprocessCoverageTrace;
                if (fsrAlphaPostprocess && fsrAlphaBridge && dlssSrRequested &&
                    !scenePromotionActivated && color && ps &&
                    activePlan.requestedUpscaler == upscaling::Upscaler::Fsr &&
                    fsr_alpha::AuditedPostprocessPair(key.vs, key.ps) &&
                    info.primitiveType == 4 && info.indexed && indexCount == 6 && indices.size() >= 6 &&
                    slot95Bound && slot95ArenaOffset != UINT64_MAX &&
                    (key.colorMask & 7u) == 7u && !(shared.flags & 31u) &&
                    !vs->info.usesPointSize && vs->info.errors.empty() &&
                    !ps->info.writesDepth && ps->info.errors.empty() &&
                    (ps->info.colorTargetsWritten & 1u) &&
                    rasterViewport.width <= color->width && rasterViewport.height <= color->height) {
                    const auto postDesc = DescribePipeline(key, vs, ps, false);
                    float positions[24]{};
                    bool vertexRange = true;
                    const uint8_t* arenaBase = arenaMapped + slot95ArenaOffset;
                    for (unsigned i = 0; i < 6; ++i) {
                        const int64_t vertex = int64_t(indices[i]) + int64_t(baseVertex);
                        if (vertex < 0 || uint64_t(vertex) > (UINT64_MAX - 24) / 32 ||
                            uint64_t(vertex) * 32 + 24 > slot95StreamBytes ||
                            slot95ArenaOffset + uint64_t(vertex) * 32 + 24 > gpu::render_arena::kVertexArenaSize) {
                            vertexRange = false; break;
                        }
                        std::memcpy(&positions[i * 4], arenaBase + uint64_t(vertex) * 32, 16);
                    }
                    const auto quad = vertexRange ? color_qualification::CheckPostprocessQuadCoverage(
                        positions, rasterViewport.x, rasterViewport.y, rasterViewport.width,
                        rasterViewport.height, scissor.left, scissor.top, scissor.right, scissor.bottom,
                        shared.ndcScale, shared.ndcOffset, shared.halfPixel, color->width, color->height) :
                        color_qualification::QuadGeometryCheckResult{};
                    const bool fullQuad = vertexRange && quad.ok;
                    const auto insetQuad = vertexRange && !fullQuad &&
                        quad.rejectReason == color_qualification::ProducerRejectReason::CoverageNotFull ?
                        color_qualification::CheckPostprocessQuadCoverage(
                            positions, rasterViewport.x, rasterViewport.y, rasterViewport.width,
                            rasterViewport.height, scissor.left, scissor.top, scissor.right, scissor.bottom,
                            shared.ndcScale, shared.ndcOffset, shared.halfPixel,
                            color->width, color->height, true) : color_qualification::QuadGeometryCheckResult{};
                    const auto& provenGeometry = fullQuad ? quad : insetQuad;
                    const fsr_alpha::MaskRect publishedRect{0, 0,
                        provenGeometry.coveredWidth, provenGeometry.coveredHeight};
                    const fsr_alpha::MaskRect writtenRect{provenGeometry.writtenX,
                        provenGeometry.writtenY, provenGeometry.writtenWidth, provenGeometry.writtenHeight};
                    const auto* recentClear = fsrAlphaBridge->RecentClear(color->allocationSerial);
                    const bool clearBackgroundAvailable = insetQuad.ok &&
                        fsr_alpha::QualifiesInsetReplacement(recentClear, frame, temporalEpoch,
                            color->allocationSerial, color->width, color->height,
                            drawsThisFrame, publishedRect, writtenRect);
                    const bool geometrySupported = fullQuad || clearBackgroundAvailable;
                    const auto psFloat = [&](unsigned constant, unsigned component) {
                        return std::bit_cast<float>(psConstants[constant * 4 + component]);
                    };
                    const bool tone = key.ps == 0xb4b4d54a7a2d6b96ull;
                    const bool needDof = tone && (psFloat(6, 0) != 0.0f ||
                        psFloat(6, 1) != 0.0f || psFloat(6, 2) != 0.0f);
                    const bool needBloom = tone && psFloat(7, 0) != 0.0f;
                    const bool completeInputs = alphaPostInputs[0].has_value() &&
                        (!needDof || alphaPostInputs[2].has_value()) &&
                        (!needBloom || alphaPostInputs[3].has_value()) &&
                        (!tone || (alphaPostDepth && alphaPostDepth->texture == textureBindings[0][1] &&
                            alphaPostDepth->format == RenderFormat::R32_FLOAT));
                    if (!completeInputs) alphaPostprocessReason = "input_unavailable";
                    uint32_t pointSlots = 0;
                    bool samplerSupported = true;
                    for (unsigned slot : {0u, 2u, 3u}) {
                        if (slot != 0 && !tone) continue;
                        if (slot == 2 && !needDof) continue;
                        if (slot == 3 && !needBloom) continue;
                        if (!alphaPostInputs[slot]) continue;
                        // GetSamplerIndex falls back to palette slot zero when
                        // exhausted. The shader samples that actual slot, so
                        // never infer its footprint from the requested key.
                        const auto palette = samplerPalette.find(alphaPostInputs[slot]->samplerKey);
                        if (palette == samplerPalette.end() ||
                            palette->second != shared.samplerIndex[slot]) {
                            samplerSupported = false;
                            continue;
                        }
                        const auto footprint = fsr_alpha::LOD0ClampFootprint(
                            alphaPostInputs[slot]->samplerKey, true);
                        if (footprint == fsr_alpha::SamplerFootprint::Unsupported)
                            samplerSupported = false;
                        else if (footprint == fsr_alpha::SamplerFootprint::Point)
                            pointSlots |= 1u << slot;
                    }
                    if (FsrAlphaBridgeTraceEnabled() && fsrAlphaBridgeTraceCount < 128) {
                        alphaPostprocessCoverageTrace = fmt::format(
                            ",\"draw_written_rect\":[{},{},{},{}],"
                            "\"inset_geometry_ok\":{},\"clear_background_available\":{},"
                            "\"geometry_supported\":{}",
                            writtenRect.x, writtenRect.y, writtenRect.width, writtenRect.height,
                            insetQuad.ok ? "true" : "false",
                            clearBackgroundAvailable ? "true" : "false",
                            geometrySupported ? "true" : "false");
                        if (recentClear) {
                            alphaPostprocessCoverageTrace += fmt::format(
                                ",\"clear_background\":{{\"frame\":{},\"epoch\":{},"
                                "\"allocation\":{},\"extent\":[{},{}],\"ordinal\":{},"
                                "\"kind\":\"{}\",\"affected_rect\":[{},{},{},{}],"
                                "\"invalidated_by\":",
                                recentClear->frame, recentClear->epoch, recentClear->colorAllocation,
                                recentClear->width, recentClear->height, recentClear->ordinal,
                                recentClear->kind, recentClear->affectedRect.x, recentClear->affectedRect.y,
                                recentClear->affectedRect.width, recentClear->affectedRect.height);
                            if (recentClear->invalidatedBy)
                                alphaPostprocessCoverageTrace += fmt::format("\"{}\"}}", recentClear->invalidatedBy);
                            else alphaPostprocessCoverageTrace += "null}";
                        } else alphaPostprocessCoverageTrace += ",\"clear_background\":null";
                        alphaPostprocessGuards = fmt::format(
                            ",\"viewport\":[{:.9g},{:.9g},{:.9g},{:.9g}],"
                            "\"scissor\":[{},{},{},{}],\"attachment_extent\":[{},{}],"
                            "\"vtx_fmt\":{},\"base_vertex\":{},\"vertex_range\":{},"
                            "\"quad_ok\":{},\"quad_reject_reason\":{},"
                            "\"quad_bounds\":[{:.9g},{:.9g},{:.9g},{:.9g}],"
                            "\"proven_valid_rect\":[0,0,{},{}],"
                            "\"need_dof\":{},\"need_bloom\":{},\"complete_inputs\":{},"
                            "\"slot0_available\":{},\"slot2_available\":{},\"slot3_available\":{},"
                            "\"depth_input_available\":{},\"depth_input_matches\":{},"
                            "\"sampler_supported\":{},\"point_slots\":{},"
                            "\"depth_enabled\":{},\"stencil_enabled\":{},"
                            "\"geometry_shader\":{},\"cull_mode\":{},\"blend_enabled\":{},"
                            "\"indices\":[{},{},{},{},{},{}]",
                            rasterViewport.x, rasterViewport.y, rasterViewport.width, rasterViewport.height,
                            scissor.left, scissor.top, scissor.right, scissor.bottom,
                            color->width, color->height, shared.vtxFmt, baseVertex,
                            vertexRange ? "true" : "false", fullQuad ? "true" : "false",
                            vertexRange ? uint32_t(quad.rejectReason) : uint32_t(color_qualification::ProducerRejectReason::IndexRangeOverflow),
                            quad.bounds[0], quad.bounds[1], quad.bounds[2], quad.bounds[3],
                            quad.coveredWidth, quad.coveredHeight,
                            needDof ? "true" : "false", needBloom ? "true" : "false",
                            completeInputs ? "true" : "false",
                            alphaPostInputs[0] ? "true" : "false",
                            alphaPostInputs[2] ? "true" : "false",
                            alphaPostInputs[3] ? "true" : "false",
                            alphaPostDepth ? "true" : "false",
                            alphaPostDepth && alphaPostDepth->texture == textureBindings[0][1] &&
                                alphaPostDepth->format == RenderFormat::R32_FLOAT ? "true" : "false",
                            samplerSupported ? "true" : "false", pointSlots,
                            postDesc.depthEnabled ? "true" : "false", postDesc.stencilEnabled ? "true" : "false",
                            postDesc.geometryShader ? "true" : "false", uint32_t(postDesc.cullMode),
                            postDesc.renderTargetBlend[0].blendEnabled ? "true" : "false",
                            indices[0], indices[1], indices[2], indices[3], indices[4], indices[5]);
                        alphaPostprocessGuards += alphaPostprocessCoverageTrace;
                        const auto appendFloatBits = [&](const char* name, const float* values, size_t count) {
                            alphaPostprocessGuards += fmt::format(",\"{}\":[", name);
                            for (size_t i = 0; i < count; ++i) {
                                if (i) alphaPostprocessGuards += ',';
                                alphaPostprocessGuards += std::to_string(std::bit_cast<uint32_t>(values[i]));
                            }
                            alphaPostprocessGuards += ']';
                        };
                        const float viewportValues[4] = {rasterViewport.x, rasterViewport.y,
                            rasterViewport.width, rasterViewport.height};
                        appendFloatBits("viewport_u32", viewportValues, 4);
                        appendFloatBits("ndc_scale_u32", shared.ndcScale, 4);
                        appendFloatBits("ndc_offset_u32", shared.ndcOffset, 4);
                        appendFloatBits("half_pixel_u32", shared.halfPixel, 2);
                        if (vertexRange) {
                            appendFloatBits("clip_positions_u32", positions, 24);
                            uint32_t baseUvBits[12]{};
                            for (unsigned i = 0; i < 6; ++i) {
                                const auto vertex = uint64_t(int64_t(indices[i]) + int64_t(baseVertex));
                                std::memcpy(baseUvBits + 2 * i,
                                    arenaBase + vertex * 32 + 16, 2 * sizeof(uint32_t));
                            }
                            alphaPostprocessGuards += ",\"base_uv_u32\":[";
                            for (unsigned i = 0; i < 12; ++i) {
                                if (i) alphaPostprocessGuards += ',';
                                alphaPostprocessGuards += std::to_string(baseUvBits[i]);
                            }
                            alphaPostprocessGuards += ']';
                        }
                        const auto appendConstantWords = [&](const char* name, const uint32_t* words, unsigned count) {
                            alphaPostprocessGuards += fmt::format(",\"{}\":[", name);
                            for (unsigned i = 0; i < count; ++i) {
                                if (i) alphaPostprocessGuards += ',';
                                alphaPostprocessGuards += std::to_string(words[i]);
                            }
                            alphaPostprocessGuards += ']';
                        };
                        appendConstantWords("vs_c0_c7_u32x4", vsConstants, 32);
                        appendConstantWords("ps_c0_c15_u32x4", psConstants, 64);
                        appendConstantWords("ps_c255_u32x4", psConstants + 255 * 4, 4);
                        alphaPostprocessGuards += ",\"samplers\":[";
                        for (unsigned slot : {0u, 2u, 3u}) {
                            if (slot != 0) alphaPostprocessGuards += ',';
                            const auto& input = alphaPostInputs[slot];
                            const auto palette = input ? samplerPalette.find(input->samplerKey) : samplerPalette.end();
                            uint64_t effectiveKey = 0;
                            for (const auto& [candidate, index] : samplerPalette)
                                if (index == shared.samplerIndex[slot]) { effectiveKey = candidate; break; }
                            alphaPostprocessGuards += fmt::format(
                                "{{\"slot\":{},\"input\":{},\"requested_key\":{},"
                                "\"palette_index\":{},\"actual_index\":{},"
                                "\"effective_key\":{},\"footprint\":{}}}",
                                slot, input ? "true" : "false", input ? input->samplerKey : 0,
                                palette != samplerPalette.end() ? int64_t(palette->second) : -1,
                                shared.samplerIndex[slot], effectiveKey,
                                input ? uint32_t(fsr_alpha::LOD0ClampFootprint(input->samplerKey, true)) : 0u);
                        }
                        alphaPostprocessGuards += ']';
                    }
                    if (completeInputs && geometrySupported && !postDesc.depthEnabled && !postDesc.stencilEnabled &&
                        !postDesc.geometryShader && postDesc.cullMode == RenderCullMode::NONE &&
                        !postDesc.renderTargetBlend[0].blendEnabled &&
                        samplerSupported) {
                        alphaPostprocessReason = "shader_pending";
                        auto* maskPipeline = fsrAlphaPostprocess->Prepare(key, pointSlots, postDesc,
                            vs->info, ps->info, vsWords, vsCount, psWords, psCount);
                        if (!maskPipeline && !fsrAlphaPostprocess->LastError().empty())
                            alphaPostprocessReason = "shader_or_pipeline_failed";
                        if (maskPipeline) {
                            TextureSetCache::Key maskBindings[3] = {
                                textureBindings[0], textureBindings[1], textureBindings[2]};
                            for (unsigned slot : {0u, 2u, 3u}) {
                                if (slot != 0 && !tone) continue;
                                if (slot == 2 && !needDof) continue;
                                if (slot == 3 && !needBloom) continue;
                                auto& input = *alphaPostInputs[slot];
                                Gpu().fsrAlphaBridgeUses.push_back(input.fetch.mask);
                                maskBindings[0][slot] = input.fetch.mask->texture.get();
                            }
                            auto* maskSet1 = AcquireTextureSet(1, maskBindings[0], activeTextureSlots[0]);
                            auto* maskSet2 = AcquireTextureSet(2, maskBindings[1], activeTextureSlots[1]);
                            auto* maskSet3 = AcquireTextureSet(3, maskBindings[2], activeTextureSlots[2]);
                            const RenderBufferReference maskConstants[3] = {
                                {uploadRing, vsOffset}, {uploadRing, sharedOffset}, {uploadRing, psOffset}};
                            RenderDescriptorSet* maskSets[] = {set0, maskSet1, maskSet2,
                                maskSet3, staticSamplerSet.get()};
                            std::shared_ptr<FsrAlphaBridgeDiagnostic> postEvidence;
                            if (FsrAlphaBridgeTraceEnabled() && fsrAlphaBridgeTraceCount < 128) {
                                postEvidence = std::make_shared<FsrAlphaBridgeDiagnostic>();
                                postEvidence->writeOrdinal = drawsThisFrame;
                                auto& fields = postEvidence->fields;
                                fields = fmt::format(
                                    "\"kind\":\"postprocess_draw\",\"status\":\"recording\","
                                    "\"vs_hash\":\"{:016x}\",\"ps_hash\":\"{:016x}\","
                                    "\"draw_ordinal\":{},\"geometry_epoch\":{},"
                                    "\"color_allocation\":{},"
                                    "\"output_extent\":[{},{}],\"primitive\":{},"
                                    "\"actual_topology\":\"TRIANGLE_LIST\",\"indexed\":true,"
                                    "\"index_count\":{},\"base_vertex\":{},"
                                    "\"viewport\":[{},{},{},{},{},{}],"
                                    "\"scissor\":[{},{},{},{}],\"blend\":{},\"color_mask\":{},"
                                    "\"shared_flags\":{},\"point_slots\":{}",
                                    key.vs, key.ps, drawsThisFrame, temporalEpoch, color->allocationSerial,
                                    color->width, color->height, info.primitiveType,
                                    indexCount, baseVertex, rasterViewport.x, rasterViewport.y,
                                    rasterViewport.width, rasterViewport.height,
                                    rasterViewport.minDepth, rasterViewport.maxDepth,
                                    scissor.left, scissor.top, scissor.right, scissor.bottom,
                                    key.blend, key.colorMask, shared.flags, pointSlots);
                                fields += alphaPostprocessCoverageTrace;
                                const auto appendWords = [&](const char* name, const uint32_t* words, size_t count) {
                                    fields += fmt::format(",\"{}\":[", name);
                                    for (size_t i = 0; i < count; ++i) {
                                        if (i) fields += ',';
                                        fields += std::to_string(words[i]);
                                    }
                                    fields += ']';
                                };
                                appendWords("vs_c0_c7_u32x4", vsConstants, 32);
                                appendWords("ps_c0_c15_u32x4", psConstants, 64);
                                appendWords("ps_c255_u32x4", psConstants + 255 * 4, 4);
                                const uint32_t ndcScaleBits[4] = {
                                    std::bit_cast<uint32_t>(shared.ndcScale[0]),
                                    std::bit_cast<uint32_t>(shared.ndcScale[1]),
                                    std::bit_cast<uint32_t>(shared.ndcScale[2]),
                                    std::bit_cast<uint32_t>(shared.ndcScale[3])};
                                const uint32_t ndcOffsetBits[4] = {
                                    std::bit_cast<uint32_t>(shared.ndcOffset[0]),
                                    std::bit_cast<uint32_t>(shared.ndcOffset[1]),
                                    std::bit_cast<uint32_t>(shared.ndcOffset[2]),
                                    std::bit_cast<uint32_t>(shared.ndcOffset[3])};
                                const uint32_t halfPixelBits[2] = {
                                    std::bit_cast<uint32_t>(shared.halfPixel[0]),
                                    std::bit_cast<uint32_t>(shared.halfPixel[1])};
                                appendWords("ndc_scale_u32", ndcScaleBits, 4);
                                appendWords("ndc_offset_u32", ndcOffsetBits, 4);
                                appendWords("half_pixel_u32", halfPixelBits, 2);
                                fields += fmt::format(",\"vtx_fmt\":{}", shared.vtxFmt);
                                appendWords("shared_texture_size_u32", shared.textureSize, 4);
                                appendWords("shared_texture_info_u32", shared.textureInfo, 4);
                                fields += ",\"vertices_in_index_order\":[";
                                for (unsigned i = 0; i < 6; ++i) {
                                    if (i) fields += ',';
                                    const int64_t vertex = int64_t(indices[i]) + int64_t(baseVertex);
                                    float attributes[6]{};
                                    std::memcpy(attributes, arenaBase + uint64_t(vertex) * 32, sizeof(attributes));
                                    fields += fmt::format("{{\"index\":{},\"clip_position_bits\":[{},{},{},{}],"
                                        "\"base_uv_bits\":[{},{}]}}", vertex,
                                        std::bit_cast<uint32_t>(attributes[0]), std::bit_cast<uint32_t>(attributes[1]),
                                        std::bit_cast<uint32_t>(attributes[2]), std::bit_cast<uint32_t>(attributes[3]),
                                        std::bit_cast<uint32_t>(attributes[4]), std::bit_cast<uint32_t>(attributes[5]));
                                }
                                fields += "],\"inputs\":[";
                                bool firstInput = true;
                                const auto paletteKey = [&](uint32_t index) {
                                    for (const auto& [candidateKey, paletteIndex] : samplerPalette)
                                        if (paletteIndex == index) return candidateKey;
                                    return uint64_t(0);
                                };
                                for (unsigned slot : {0u, 2u, 3u}) {
                                    if (!alphaPostInputs[slot] || (slot == 2 && !needDof) ||
                                        (slot == 3 && !needBloom)) continue;
                                    const auto& input = *alphaPostInputs[slot];
                                    if (!firstInput) fields += ',';
                                    firstInput = false;
                                    fields += fmt::format(
                                        "{{\"slot\":{},\"sampler_key\":{},\"sampler_index\":{},"
                                        "\"effective_sampler_key\":{},\"mip_levels\":1,"
                                        "\"texture_size\":{},"
                                        "\"texture_info\":{},\"actual_color_image\":{},"
                                        "\"mask_image\":{},\"source_allocation\":{},"
                                        "\"source_revision\":{},\"source_stage\":{},"
                                        "\"valid_rect\":[{},{},{},{}],\"crop\":[{},{}],"
                                        "\"substitution\":\"{}\"}}",
                                        slot, input.samplerKey, shared.samplerIndex[slot],
                                        paletteKey(shared.samplerIndex[slot]),
                                        shared.textureSize[slot], shared.textureInfo[slot],
                                        uintptr_t(input.actualColor), uintptr_t(input.fetch.mask->texture.get()),
                                        input.fetch.version.sourceAllocation,
                                        input.fetch.version.sourceRevision,
                                        uint32_t(input.fetch.version.sourceStage),
                                        0, 0, input.fetch.cropWidth, input.fetch.cropHeight,
                                        input.fetch.cropWidth, input.fetch.cropHeight, input.substitution);
                                }
                                if (tone && alphaPostDepth) {
                                    if (!firstInput) fields += ',';
                                    fields += fmt::format(
                                        "{{\"slot\":1,\"role\":\"actual_depth\","
                                        "\"sampler_key\":{},\"sampler_index\":{},"
                                        "\"effective_sampler_key\":{},\"mip_levels\":1,"
                                        "\"texture_size\":{},"
                                        "\"texture_info\":{},\"image\":{},"
                                        "\"format\":{},\"extent\":[{},{}]}}",
                                        alphaPostDepth->samplerKey, shared.samplerIndex[1],
                                        paletteKey(shared.samplerIndex[1]),
                                        shared.textureSize[1], shared.textureInfo[1],
                                        uintptr_t(alphaPostDepth->texture), uint32_t(alphaPostDepth->format),
                                        alphaPostDepth->width, alphaPostDepth->height);
                                }
                                fields += ']';
                                TraceFsrAlphaBridge(postEvidence);
                                for (unsigned slot : {0u, 2u, 3u}) {
                                    if (!alphaPostInputs[slot] || (slot == 2 && !needDof) ||
                                        (slot == 3 && !needBloom)) continue;
                                    const auto& input = *alphaPostInputs[slot];
                                    const auto label = fmt::format("input-t{}-mask", slot);
                                    QueueFsrAlphaSnapshot(postEvidence, label.c_str(),
                                        input.fetch.mask->texture.get(), RenderFormat::R8_UNORM,
                                        input.fetch.cropWidth, input.fetch.cropHeight, 1,
                                        RenderTextureLayout::SHADER_READ);
                                }
                                if (tone && alphaPostDepth)
                                    QueueFsrAlphaSnapshot(postEvidence, "input-t1-depth",
                                        alphaPostDepth->texture, RenderFormat::R32_FLOAT,
                                        alphaPostDepth->width, alphaPostDepth->height, 4,
                                        RenderTextureLayout::SHADER_READ);
                                const uint32_t colorBpp = color->format == RenderFormat::R8G8B8A8_UNORM ? 4u :
                                    color->format == RenderFormat::R16G16B16A16_FLOAT ? 8u : 0u;
                                if (colorBpp)
                                    QueueFsrAlphaSnapshot(postEvidence, "original-color-before", color->texture.get(),
                                        color->format, color->width, color->height, colorBpp, color->layout);
                                if (depth && depth->format == RenderFormat::D32_FLOAT_S8_UINT)
                                    QueueFsrAlphaSnapshot(postEvidence, "original-depth-before", depth->texture.get(),
                                        RenderFormat::R32_FLOAT, depth->width, depth->height, 4, depth->layout);
                            }
                            auto mask = fsrAlphaPostprocess->Draw(commandList, maskPipeline,
                                color->width, color->height, maskConstants, maskSets, 5,
                                rasterViewport, scissor, useIndices, indexCount, baseVertex,
                                 Gpu().fsrAlphaBridgeUses);
                            if (postEvidence) {
                                postEvidence->fields += fmt::format(",\"recorded\":{}",
                                    mask ? "true" : "false");
                                if (mask) QueueFsrAlphaSnapshot(postEvidence, "output-mask",
                                    mask->texture.get(), RenderFormat::R8_UNORM,
                                    color->width, color->height, 1, RenderTextureLayout::SHADER_READ);
                                const uint32_t colorBpp = color->format == RenderFormat::R8G8B8A8_UNORM ? 4u :
                                    color->format == RenderFormat::R16G16B16A16_FLOAT ? 8u : 0u;
                                if (colorBpp)
                                    QueueFsrAlphaSnapshot(postEvidence, "original-color-after", color->texture.get(),
                                        color->format, color->width, color->height, colorBpp, color->layout);
                                if (depth && depth->format == RenderFormat::D32_FLOAT_S8_UINT)
                                    QueueFsrAlphaSnapshot(postEvidence, "original-depth-after", depth->texture.get(),
                                        RenderFormat::R32_FLOAT, depth->width, depth->height, 4, depth->layout);
                            }
                            if (mask) {
                                fsr_alpha::SourceMask source{};
                                source.frame = frame; source.epoch = temporalEpoch;
                                source.colorAllocation = color->allocationSerial;
                                source.revision = drawsThisFrame;
                                source.width = color->width; source.height = color->height;
                                source.validRect = publishedRect;
                                source.stage = key.ps == 0x7c260eacff1d681dull ?
                                    fsr_alpha::SourceStage::Downsample :
                                    key.ps == 0x53dd5d081c7945cfull ? fsr_alpha::SourceStage::Dof :
                                    key.ps == 0xee90000c755c0472ull ? fsr_alpha::SourceStage::Bloom :
                                     fsr_alpha::SourceStage::Tonemap;
                                if (postEvidence) postEvidence->fields += fmt::format(
                                    ",\"source_revision\":{},\"source_stage\":{},"
                                    "\"published_valid_rect\":[{},{},{},{}],\"output_mask_image\":{}",
                                    source.revision, uint32_t(source.stage),
                                    source.validRect.x, source.validRect.y,
                                    source.validRect.width, source.validRect.height,
                                    uintptr_t(mask->texture.get()));
                                source.mask = std::move(mask);
                                fsrAlphaBridge->PublishPostprocess(std::move(source));
                                alphaPostprocessRecorded = fsrAlphaBridge->CurrentSource(
                                    color->allocationSerial, frame, temporalEpoch) != nullptr;
                                alphaPostprocessReason = alphaPostprocessRecorded ? "available" : "publish_rejected";
                                if (postEvidence) postEvidence->fields += fmt::format(
                                    ",\"published\":{}", alphaPostprocessRecorded ? "true" : "false");
                            }
                            else alphaPostprocessReason = "mask_draw_failed";
                            commandList->setFramebuffer(framebuffer);
                            commandList->setViewports(&rasterViewport, 1);
                            commandList->setScissors(&scissor, 1);
                            commandList->setGraphicsPipelineLayout(pipelineLayout.get());
                            commandList->setPipeline(pipeline);
                            commandList->setGraphicsPushConstants(0, constantAddresses);
                            commandList->setGraphicsDescriptorSet(set0, 0);
                            commandList->setGraphicsDescriptorSet(set1, 1);
                            commandList->setGraphicsDescriptorSet(set2, 2);
                            commandList->setGraphicsDescriptorSet(set3, 3);
                            commandList->setGraphicsDescriptorSet(staticSamplerSet.get(), 4);
                        }
                    }
                    else alphaPostprocessReason = fsr_alpha::PostprocessGuardReason(completeInputs, geometrySupported,
                        samplerSupported, postDesc.depthEnabled, postDesc.stencilEnabled,
                        postDesc.geometryShader, postDesc.cullMode != RenderCullMode::NONE,
                        postDesc.renderTargetBlend[0].blendEnabled,
                        insetQuad.ok, clearBackgroundAvailable);
                }
                if (fsrAlphaBridge && activePlan.requestedUpscaler == upscaling::Upscaler::Fsr &&
                    color && (key.colorMask & 7u) && (!ps || !(ps->info.colorTargetsWritten & 1u)))
                    fsrAlphaBridge->InvalidateClearBackground(color->allocationSerial, "unclassified_rgb_draw");
                if (fsrAlphaBridge && activePlan.requestedUpscaler == upscaling::Upscaler::Fsr &&
                    color && ps && (ps->info.colorTargetsWritten & 1u) && (key.colorMask & 7)) {
                    if (alphaReplayRecorded || alphaPostprocessRecorded)
                        fsrAlphaBridge->InvalidateClearBackground(color->allocationSerial, "audited_rgb_draw");
                    if (!alphaReplayRecorded && !alphaPostprocessRecorded)
                        HandleFsrAlphaRgbWriter(*color, "unreplayed_rgb_draw", drawsThisFrame,
                            key.vs, key.ps, key.blend, key.colorMask,
                            fsr_alpha::RetainsRawAfterAuditedLocalBlend(key.vs, key.ps,
                                key.blend, key.colorMask, (colorInfo >> 16) & 0xF,
                                color->format == RenderFormat::R16G16B16A16_FLOAT,
                                shared.flags, getenv("LO_NO_BLEND") != nullptr));
                    if (fsr_alpha::AuditedPostprocessPs(key.ps) && !alphaPostprocessRecorded)
                        fsrAlphaBridge->MarkUnsupportedPostprocess(color->allocationSerial);
                    if (fsr_alpha::AuditedPostprocessPs(key.ps) && !alphaPostprocessRecorded &&
                        FsrAlphaBridgeTraceEnabled() && fsrAlphaBridgeTraceCount < 128) {
                        auto event = std::make_shared<FsrAlphaBridgeDiagnostic>();
                        event->writeOrdinal = drawsThisFrame;
                        event->fields = fmt::format(
                            "\"kind\":\"postprocess_draw\",\"status\":\"unavailable\","
                            "\"reason\":\"{}\",\"vs_hash\":\"{:016x}\","
                            "\"ps_hash\":\"{:016x}\",\"draw_ordinal\":{},"
                            "\"geometry_epoch\":{},\"color_allocation\":{},"
                            "\"primitive\":{},\"indexed\":{},\"index_count\":{},"
                            "\"vfetch95_bound\":{},\"color_mask\":{},"
                            "\"shared_flags\":{},\"ps_writes_depth\":{}",
                            alphaPostprocessReason, key.vs, key.ps, drawsThisFrame,
                            temporalEpoch, color->allocationSerial, info.primitiveType,
                            info.indexed ? "true" : "false", indexCount,
                            slot95Bound ? "true" : "false", key.colorMask, shared.flags,
                            ps->info.writesDepth ? "true" : "false");
                        if (alphaPostprocessGuards.empty()) {
                            alphaPostprocessGuards = fmt::format(
                                ",\"viewport\":[{:.9g},{:.9g},{:.9g},{:.9g}],"
                                "\"scissor\":[{},{},{},{}],\"vtx_fmt\":{},"
                                "\"vfetch95_range\":{},\"slot0_available\":{},"
                                "\"slot2_available\":{},\"slot3_available\":{},"
                                "\"depth_input_available\":{},\"base_vertex\":{}",
                                rasterViewport.x, rasterViewport.y, rasterViewport.width, rasterViewport.height,
                                scissor.left, scissor.top, scissor.right, scissor.bottom,
                                shared.vtxFmt, slot95ArenaOffset != UINT64_MAX ? "true" : "false",
                                alphaPostInputs[0] ? "true" : "false",
                                alphaPostInputs[2] ? "true" : "false",
                                alphaPostInputs[3] ? "true" : "false",
                                alphaPostDepth ? "true" : "false", baseVertex);
                            const auto* clear = fsrAlphaBridge->RecentClear(color->allocationSerial);
                            alphaPostprocessGuards +=
                                ",\"draw_written_rect\":[0,0,0,0],\"inset_geometry_ok\":false,"
                                "\"clear_background_available\":false,\"geometry_supported\":false";
                            if (clear) {
                                alphaPostprocessGuards += fmt::format(
                                    ",\"clear_background\":{{\"frame\":{},\"epoch\":{},"
                                    "\"allocation\":{},\"extent\":[{},{}],\"ordinal\":{},"
                                    "\"kind\":\"{}\",\"affected_rect\":[{},{},{},{}],"
                                    "\"invalidated_by\":",
                                    clear->frame, clear->epoch, clear->colorAllocation,
                                    clear->width, clear->height, clear->ordinal, clear->kind,
                                    clear->affectedRect.x, clear->affectedRect.y,
                                    clear->affectedRect.width, clear->affectedRect.height);
                                if (clear->invalidatedBy)
                                    alphaPostprocessGuards += fmt::format("\"{}\"}}", clear->invalidatedBy);
                                else alphaPostprocessGuards += "null}";
                            } else alphaPostprocessGuards += ",\"clear_background\":null";
                            const auto appendConstants = [&](const char* name, const uint32_t* words, unsigned count) {
                                alphaPostprocessGuards += fmt::format(",\"{}\":[", name);
                                for (unsigned i = 0; i < count; ++i) {
                                    if (i) alphaPostprocessGuards += ',';
                                    alphaPostprocessGuards += std::to_string(words[i]);
                                }
                                alphaPostprocessGuards += ']';
                            };
                            appendConstants("vs_c0_c7_u32x4", vsConstants, 32);
                            appendConstants("ps_c0_c15_u32x4", psConstants, 64);
                            appendConstants("ps_c255_u32x4", psConstants + 255 * 4, 4);
                            if (vs) {
                                const auto guardDesc = DescribePipeline(key, vs, ps, false);
                                alphaPostprocessGuards += fmt::format(
                                    ",\"depth_enabled\":{},\"stencil_enabled\":{},"
                                    "\"cull_mode\":{},\"blend_enabled\":{}",
                                    guardDesc.depthEnabled ? "true" : "false",
                                    guardDesc.stencilEnabled ? "true" : "false",
                                    uint32_t(guardDesc.cullMode),
                                    guardDesc.renderTargetBlend[0].blendEnabled ? "true" : "false");
                            }
                            alphaPostprocessGuards += ",\"samplers\":[";
                            for (unsigned slot : {0u, 2u, 3u}) {
                                if (slot != 0) alphaPostprocessGuards += ',';
                                const auto& input = alphaPostInputs[slot];
                                const auto palette = input ? samplerPalette.find(input->samplerKey) : samplerPalette.end();
                                uint64_t effectiveKey = 0;
                                for (const auto& [candidate, index] : samplerPalette)
                                    if (index == shared.samplerIndex[slot]) { effectiveKey = candidate; break; }
                                alphaPostprocessGuards += fmt::format(
                                    "{{\"slot\":{},\"input\":{},\"requested_key\":{},"
                                    "\"palette_index\":{},\"actual_index\":{},\"effective_key\":{}}}",
                                    slot, input ? "true" : "false", input ? input->samplerKey : 0,
                                    palette != samplerPalette.end() ? int64_t(palette->second) : -1,
                                    shared.samplerIndex[slot], effectiveKey);
                            }
                            alphaPostprocessGuards += ']';
                        }
                        event->fields += alphaPostprocessGuards;
                        TraceFsrAlphaBridge(std::move(event));
                    }
                }
                if (scenePromotionActivated)
                    RecordSceneCopyDlss(color, rasterTarget);
                if (video::GpuWorkStopped()) return;
 #endif
                drawsThisFrame++;

                if (motionDepthWrite) {
                    const auto logFirstMotionFailure = [&](const char* reason, uint32_t reasonCode) {
                        if (!motionOptions.log) return;
                        // First healthy->failed transition is the causal draw.
                        // Later draws in this frame only increment suppression;
                        // an exhausted distinct-state table never hides a new
                        // frame's first writer.
                        if (motionFirstFailureFrame == frame) { ++motionFailureSuppressed; return; }
                        motionFirstFailureFrame = frame;
                        uint64_t state = temporal::MotionHashWord(key.vs, key.ps);
                        for (uint64_t part : {uint64_t(reasonCode), depth->allocationSerial,
                            uint64_t(uint32_t(temporalSlot)), uint64_t(depthControl), uint64_t(motionStreamsValid),
                            uint64_t(std::bit_cast<uint32_t>(rasterViewport.width)),
                            uint64_t(std::bit_cast<uint32_t>(rasterViewport.height))})
                            state = temporal::MotionHashWord(state, part);
                        const bool known = std::find(motionFailureKeys.begin(), motionFailureKeys.begin() + motionFailureKeyCount,
                            state) != motionFailureKeys.begin() + motionFailureKeyCount;
                        if (!known && motionFailureKeyCount < motionFailureKeys.size())
                            motionFailureKeys[motionFailureKeyCount++] = state;
                        LOG_INFO("mv first_failure reason={} frame={} draw={} temporal_epoch={} plan_epoch={} vs={:016x} ps={:016x} depth_alloc={} viewport=({},{},{},{}) scissor=({},{},{},{}) depth_control={:#x} primitive={} depth_bias={} slope_bias={} layer_depth_offset={} temporal_slot={} streams={} supported={} tracker_failed={} motion_init_failed={} replay_cleared={} replay_aborted={} replay_pending={} replay_error={} known_state={} distinct={} suppressed={}",
                            reason, frame, drawsThisFrame, temporalEpoch, activePlan.geometryEpoch,
                            key.vs, key.ps, depth->allocationSerial, rasterViewport.x, rasterViewport.y,
                            rasterViewport.width, rasterViewport.height, scissor.left, scissor.top,
                            scissor.right, scissor.bottom, depthControl, key.prim, key.depthBias,
                            std::bit_cast<float>(key.slopeBias), layerDepthOffset, temporalSlot, motionStreamsValid,
                            motionSupported, drawTemporalTracker.Failed(), motionInitFailed,
                            motionReplay && motionReplay->ClearedThisFrame(),
                            motionReplay && motionReplay->AbortedThisFrame(),
                            motionReplay && motionReplay->PipelinePendingThisFrame(),
                            motionReplay ? motionReplay->LastError() : std::string{},
                            known, motionFailureKeyCount, motionFailureSuppressed);
                    };
                    if (motionLocallyMaskedClipWriter) {
                        // Nothing to replay: final scene depth makes these pixels
                        // reactive while preserving motion from the rest of the frame.
                    } else if (drawTemporalTracker.Finalized()) {
                        // Do not change a motion field after its pre-UI consumer.
                        logFirstMotionFailure("late_depth_writer", 1);
                        drawTemporalTracker.Invalidate();
                        if (motionReplay && !motionReplay->AbortedThisFrame()) motionReplay->AbortFrame("late_depth_writer");
                    } else if (!motionSupported || !motionStreamsValid) {
                        // Unknown visibility writers require a conservative whole-frame
                        // fallback; camera reprojection is not valid object motion.
                        const char* reason = !motionStreamsValid ? "invalid_streams" :
                            temporalSlot < 0 || temporalSlot > 252 ? "position_slot_unavailable" :
                            vs->info.textureSlotMask ? "vertex_texture_fetch" :
                            vs->info.usesPointSize ? "point_size" : !vs->info.errors.empty() ? "vertex_translation" :
                            ps && ps->info.writesDepth ? "pixel_depth_write" :
                            ps && !ps->info.errors.empty() ? "pixel_translation" :
                            (depthControl & 1) ? "stencil_visibility" : "unsupported_raster_state";
                        if (!drawTemporalTracker.Failed()) logFirstMotionFailure(reason, 2);
                        else if (motionOptions.log) ++motionFailureSuppressed;
                        drawTemporalTracker.Invalidate();
                        if (motionReplay && !motionReplay->AbortedThisFrame()) motionReplay->AbortFrame(reason);
                    } else {
                        render_batch::CpuTimer<> mvTimer(motionOptions.timing);
                        temporal::DrawHistoryKey mk{};
                        mk.vsHash = key.vs; mk.psHash = key.ps; mk.sceneAllocation = depth->allocationSerial;
                        mk.indexBufferAddress = useIndices ? info.indexBase : 0;
                        mk.indexCount = indexCount; mk.baseVertex = baseVertex; mk.primitiveType = info.primitiveType;
                        // The exact-content index cache can reuse this digest
                        // across draws; uncached and replaced data recompute it.
                        uint64_t indexHash;
                        if (cachedIndexEntry && cachedIndexEntry->motionIndexHashReady)
                            indexHash = cachedIndexEntry->motionIndexHash;
                        else {
                            indexHash = temporal::MotionHashIndices(indices);
                            if (cachedIndexEntry) {
                                cachedIndexEntry->motionIndexHash = indexHash;
                                cachedIndexEntry->motionIndexHashReady = true;
                            }
                        }
                        motionGeometry = temporal::MotionHashWord(motionGeometry, indexHash);
                        mk.geometrySignature = motionGeometry;
                        const temporal::MotionRasterContract motionRaster{
                            uint32_t(rasterViewport.width), uint32_t(rasterViewport.height),
                            {rasterViewport.x, rasterViewport.y, rasterViewport.width, rasterViewport.height,
                             rasterViewport.minDepth, rasterViewport.maxDepth}, true};
                        auto match = drawTemporalTracker.Collect(mk, vsConstants, &shared, vs->info.usesRelativeConstants,
                            drawJitter.applied ? temporalSlot : -1, motionOriginalVP.data(), &motionRaster,
                            motionReplay ? motionReplay->ConstantUsage(key.vs) : nullptr);
                        if (taaLiveDirectory && match.previous && !match.exactStationary && frame % 120 == 0 &&
                            (key.vs == 0x702c643defe73320ull || key.vs == 0xb030ab4e17a20783ull)) {
                            static uint64_t lastProofFrame[2]{};
                            static uint32_t proofSamples = 0;
                            const unsigned proofSlot = key.vs == 0x702c643defe73320ull ? 0 : 1;
                            if (proofSamples < 8 && lastProofFrame[proofSlot] != frame) {
                                lastProofFrame[proofSlot] = frame; ++proofSamples;
                                std::string constantDiff, sharedDiff;
                                for (unsigned i = 0; i < 256; ++i) {
                                    const void* current = drawJitter.applied && int(i) >= temporalSlot && int(i) < temporalSlot + 4
                                        ? static_cast<const void*>(motionOriginalVP.data() + (int(i) - temporalSlot) * 4)
                                        : static_cast<const void*>(vsConstants + i * 4);
                                    if (std::memcmp(current, match.previous->vsConstants.data() + i * 4, 16) != 0)
                                        constantDiff += fmt::format(" {}", i);
                                }
                                const auto* sharedWords = reinterpret_cast<const uint32_t*>(&shared);
                                for (unsigned i = 0; i < 52; ++i)
                                    if (sharedWords[i] != match.previous->shared[i]) sharedDiff += fmt::format(" {}", i);
                                LOG_INFO("mv stationary proof vs={:016x} raster={} changed_c=[{}] changed_shared=[{}]", key.vs,
                                    motionRaster.Supported(), constantDiff, sharedDiff);
                            }
                        }
                        mvTimer.AddTo(mvTrackCpuMs);
                        if (motionReplay && motionReplay->UsableThisFrame()) {
                            const auto w = uint32_t(rasterViewport.width), h = uint32_t(rasterViewport.height);
                            // Cold start: allocate/clear real MV targets without previous or pipeline.
                            const auto prepared = motionReplay->PrepareSceneDraw(commandList, depth->allocationSerial, depth->texture.get(), w, h,
                                key, DescribePipeline(key, vs, ps, false),
                                vsWords, vsCount, ps ? psWords : nullptr, ps ? psCount : 0);
                            if (!prepared.sceneReady || prepared.status == temporal::MotionReplayGPU::PipelinePrepareStatus::Failed) {
                                logFirstMotionFailure(prepared.sceneReady ? "replay_pipeline_failed" : "replay_scene_failed", 3);
                                motionReplay->AbortFrame();
                            } else if (prepared.status == temporal::MotionReplayGPU::PipelinePrepareStatus::Ready && match.previous) {
                                const uint64_t aligned = (Gpu().uploadOffset + 255) & ~uint64_t(255);
                                // Never trigger a mid-draw Flush: it would invalidate bound
                                // index/texture state and the original constant references.
                                if (aligned + sizeof(temporal::MotionReplayConstants) > kUploadRingSize) {
                                    logFirstMotionFailure("replay_upload_ring_full", 4);
                                    motionReplay->AbortFrame("MV upload ring full");
                                } else {
                                    const auto c = temporal::MakeMotionReplayConstants(match, w, h,
                                        drawJitter.applied ? float(drawJitter.sample.pixelX) : 0,
                                        drawJitter.applied ? float(drawJitter.sample.pixelY) : 0);
                                    const uint64_t mvOffset = Upload(&c, sizeof(c));
                                    const RenderBufferReference cb[4] = {{uploadRing, vsOffset}, {uploadRing, sharedOffset},
                                        {uploadRing, psOffset}, {uploadRing, mvOffset}};
                                    RenderDescriptorSet* sets[] = {set0, set1, set2, set3, staticSamplerSet.get()};
                                    if (!motionReplay->Draw(commandList, prepared.pipeline, cb, sets, vulkan ? 5 : 4,
                                        rasterViewport, scissor, useIndices, indexCount, baseVertex)) {
                                        logFirstMotionFailure("replay_draw_failed", 5);
                                        motionReplay->AbortFrame("MV draw record failed");
                                    }
                                }
                            }
                            // Restore the guest binding contract after BeginScene framebuffer/layout changes.
                            commandList->setFramebuffer(framebuffer); commandList->setViewports(&rasterViewport, 1);
                            commandList->setScissors(&scissor, 1); commandList->setGraphicsPipelineLayout(pipelineLayout.get());
                            commandList->setPipeline(pipeline);
                            if (vulkan) commandList->setGraphicsPushConstants(0, constantAddresses);
                            else { SetConstantBuffer(vsOffset,0); SetConstantBuffer(sharedOffset,1); SetConstantBuffer(psOffset,2); }
                            commandList->setGraphicsDescriptorSet(set0,0); commandList->setGraphicsDescriptorSet(set1,1);
                            commandList->setGraphicsDescriptorSet(set2,2); commandList->setGraphicsDescriptorSet(set3,3);
                            if (vulkan) commandList->setGraphicsDescriptorSet(staticSamplerSet.get(),4);
                        }
                    }
                }
                // Offline vertex replay: capture one frame of relative-addressed
                // draws with their constants, indices and current guest streams.
                // Stream files are swapped CPU snapshots, not upload-heap reads.
                const auto& captureEnvironment = GetHotCaptureEnvironment();
                if (captureEnvironment.geometryCaptureEnabled)
                {
                    const char* captureDir = captureEnvironment.geometryCaptureDir.c_str();
                    static const uint32_t captureFrame = getenv("LO_GEOMETRY_CAPTURE_FRAME")
                        ? strtoul(getenv("LO_GEOMETRY_CAPTURE_FRAME"), nullptr, 10) : 2400;
                    static const bool captureWithResolveTrace = getenv("LO_GEOMETRY_CAPTURE_WITH_RESOLVE_TRACE") &&
                        strcmp(getenv("LO_GEOMETRY_CAPTURE_WITH_RESOLVE_TRACE"), "1") == 0;
                    static const uint64_t captureVs = getenv("LO_GEOMETRY_CAPTURE_VS")
                        ? strtoull(getenv("LO_GEOMETRY_CAPTURE_VS"), nullptr, 16) : 0;
                    static const uint64_t captureVs2 = getenv("LO_GEOMETRY_CAPTURE_VS2")
                        ? strtoull(getenv("LO_GEOMETRY_CAPTURE_VS2"), nullptr, 16) : 0;
                    static const uint32_t captureIndexCount = getenv("LO_GEOMETRY_CAPTURE_INDEX_COUNT")
                        ? strtoul(getenv("LO_GEOMETRY_CAPTURE_INDEX_COUNT"), nullptr, 10) : 0;
                    static const uint32_t captureIndexCount2 = getenv("LO_GEOMETRY_CAPTURE_INDEX_COUNT2")
                        ? strtoul(getenv("LO_GEOMETRY_CAPTURE_INDEX_COUNT2"), nullptr, 10) : 0;
                    static uint32_t captureDraw = 0;
                    const bool captureThisFrame = captureWithResolveTrace
                        ? resolveTraceRemaining && frame == resolveTraceFirstFrame : frame == captureFrame;
                    const bool captureThisVs = (!captureVs && !captureVs2) || key.vs == captureVs || key.vs == captureVs2;
                    if (captureThisFrame && captureThisVs && ((!captureIndexCount && !captureIndexCount2) || info.indexCount == captureIndexCount || info.indexCount == captureIndexCount2) &&
                        vs->info.usesRelativeConstants)
                    {
                        std::filesystem::create_directories(captureDir);
                        const std::string prefix = fmt::format("{}/{:04}", captureDir, captureDraw++);
                        auto save = [](const std::string& path, const void* data, size_t size)
                        {
                            std::ofstream(path, std::ios::binary).write(static_cast<const char*>(data), size);
                        };
                        save(prefix + ".constants.bin", vsConstants, sizeof(vsConstants));
                        save(prefix + ".shared.bin", &shared, sizeof(shared));
                        save(prefix + ".indices.bin", indices.data(), indices.size() * sizeof(uint32_t));
                        std::ofstream meta(prefix + ".txt");
                        meta << fmt::format("vs={:016x}\nps={:016x}\nmode={}\ncount={}\nbase_vertex={}\nindexed={}\nviewport={} {} {} {}\n",
                            key.vs, key.ps, modeControl, indexCount, int32_t(Reg(REG_VGT_INDX_OFFSET)), useIndices,
                            rasterViewport.x, rasterViewport.y, rasterViewport.width, rasterViewport.height);
                        meta << fmt::format("frame={}\nsubmitted={}\n", frame, drawsThisFrame);
                        for (uint32_t slot = 0; slot < kVertexFetchSlots; ++slot)
                        {
                            if (!((vs->info.vertexFetchSlotMask[slot >> 6] >> (slot & 63)) & 1)) continue;
                            uint32_t d0 = Reg(REG_FETCH_CONSTANTS + slot * 2), d1 = Reg(REG_FETCH_CONSTANTS + slot * 2 + 1);
                            uint32_t words = (d1 >> 2) & 0xFFFFFF;
                            std::vector<uint32_t> stream(words);
                            geometry_prepare::CopyDwordsSwapped(stream.data(), Phys(d0 & ~3u), words, d1 & 3);
                            const std::string name = fmt::format("vb_{:016x}.bin", Fnv1a(stream.data(), stream.size() * 4));
                            const std::string path = std::string(captureDir) + "/" + name;
                            if (!std::filesystem::exists(path)) save(path, stream.data(), stream.size() * 4);
                            meta << fmt::format("fetch{}={} address={:#x} words={} endian={}\n", slot, name, d0 & ~3u, words, d1 & 3);
                        }
                    }
                }

                if (trackBinding) {
                    if (key.colorMask) {
                        if (fullSceneCopy && boundSceneProducer)
                            color->bindingProducer.Copy(*boundSceneProducer, bindingEpoch, frame,
                                rasterViewport.x == 0 && rasterViewport.y == 0 &&
                                rasterViewport.width == color->width && rasterViewport.height == color->height);
                        else color->bindingProducer.Draw(bindingEpoch, frame, bindingTransform);
                    }
                    if (depth && (key.depthControl & 6) == 6) depth->bindingProducer.Draw(bindingEpoch, frame, bindingTransform);
                    if (bindingRecord) {
                        bindingRecordedFrame[bindingPair] = frame;
                        taa_collection::ObserveBinding(*bindingRecord, bindingEpoch, frame);
                    }
                }
                // Opt-in, bounded diagnostics of constants actually uploaded for
                // a submitted draw. F1's guest register trace precedes these edits.
                static const uint64_t jitterLogStart = getenv("LO_TEMPORAL_DRAW_LOG_START_FRAME") ?
                    strtoull(getenv("LO_TEMPORAL_DRAW_LOG_START_FRAME"), nullptr, 10) : ~0ull;
                static const uint64_t jitterLogVs = getenv("LO_TEMPORAL_DRAW_LOG_VS") ?
                    strtoull(getenv("LO_TEMPORAL_DRAW_LOG_VS"), nullptr, 16) : 0;
                static const uint64_t jitterLogVs2 = getenv("LO_TEMPORAL_DRAW_LOG_VS2") ?
                    strtoull(getenv("LO_TEMPORAL_DRAW_LOG_VS2"), nullptr, 16) : 0;
                // Optional geometry-focused logging keeps same-mesh base/light
                // pairs and one shadow/character sample per frame, reducing IO.
                static const uint32_t jitterLogIndexCount = getenv("LO_TEMPORAL_DRAW_LOG_INDEX_COUNT") ?
                    strtoul(getenv("LO_TEMPORAL_DRAW_LOG_INDEX_COUNT"), nullptr, 10) : 0;
                static const bool jitterLogWithResolveTrace = getenv("LO_TEMPORAL_DRAW_LOG_WITH_RESOLVE_TRACE") &&
                    strcmp(getenv("LO_TEMPORAL_DRAW_LOG_WITH_RESOLVE_TRACE"), "1") == 0;
                static uint64_t jitterLoggedShadowFrame = ~0ull, jitterLoggedCharacterFrame = ~0ull;
                const bool jitterLogStaticMesh = key.vs == 0xb030ab4e17a20783ull || key.vs == 0xa27a7234977e0d4aull ||
                    key.vs == 0xff9da3984ce8d094ull || key.vs == 0xf7fd88506d704a3dull ||
                    key.vs == 0xf1b330b3ceea9a3bull || key.vs == 0x8b5577db3ced3327ull ||
                    key.vs == 0x400df7c5a60819f5ull || key.vs == 0x08dcef32bd434f8cull ||
                    key.vs == 0xfcbb75d0feb3fcb9ull || key.vs == 0xe8c0d438c690c784ull || key.vs == 0x576d669b2ad3c898ull;
                const bool jitterLogSkinned = key.vs == 0x0eb223d33f8e8e0cull || key.vs == 0x1e9017d2b296f480ull;
                const bool jitterLogGeometry = !jitterLogIndexCount ||
                    (jitterLogStaticMesh ? info.indexCount == jitterLogIndexCount :
                        key.vs == 0x99c2b4b0960a9ccdull ? jitterShadowPair && jitterLoggedShadowFrame != frame :
                        key.vs == 0x3148f81d65d3b5f4ull ? jitterLoggedCharacterFrame != frame : true);
                if (((frame >= jitterLogStart && frame - jitterLogStart < 32) ||
                    (jitterLogWithResolveTrace && resolveTraceRemaining)) &&
                    jitterLogGeometry &&
                    ((jitterLogVs || jitterLogVs2) ? key.vs == jitterLogVs || key.vs == jitterLogVs2 :
                        jitterLogStaticMesh || jitterLogSkinned ||
                        key.vs == 0x3148f81d65d3b5f4ull || key.vs == 0x99c2b4b0960a9ccdull))
                {
                    if (key.vs == 0x99c2b4b0960a9ccdull) jitterLoggedShadowFrame = frame;
                    if (key.vs == 0x3148f81d65d3b5f4ull) jitterLoggedCharacterFrame = frame;
                    const auto bits = [](const uint32_t* values, unsigned count) {
                        std::string text;
                        for (unsigned i = 0; i < count; ++i) text += fmt::format("{}{:08x}", i ? "," : "", values[i]);
                        return text;
                    };
                    std::array<uint32_t, 16> guestVp{}, guestShadow{};
                    // Retain raw bank evidence for a baseline which rejects a
                    // reviewed shader. This diagnostic slot never authorizes jitter.
                    int jitterLogSlot = temporalSlot;
                    if (jitterLogSlot < 0)
                    {
                        if (key.vs == 0xf1b330b3ceea9a3bull) jitterLogSlot = 4;
                        else if (key.vs == 0x8b5577db3ced3327ull || key.vs == 0x400df7c5a60819f5ull ||
                            key.vs == 0x08dcef32bd434f8cull) jitterLogSlot = 7;
                        else if (jitterLogSkinned) jitterLogSlot = 233;
                    }
                    if (jitterLogSlot >= 0)
                        for (unsigned i = 0; i < 16; ++i) guestVp[i] = Reg(REG_ALU_CONSTANTS + jitterLogSlot * 4 + i);
                    const bool shadowPair = jitterShadowPair;
                    if (shadowPair)
                        for (unsigned i = 0; i < 16; ++i) guestShadow[i] = Reg(REG_ALU_CONSTANTS + 256 * 4 + 2 * 4 + i);
                    const bool staticMesh = jitterLogStaticMesh;
                    SHADER_LOG_INFO("temporal", RendererByteFnv, "renderer temporal draw f{} submitted={} vs={:016x} ps={:016x} slot={} log_slot={} indices={} index_base={:x} base_vertex={} fetch95={:08x},{:08x} world_c0_c3={:016x} enabled={} viewport={} applied={} shadow={} rejection={} phase={} ndc=({:.9g},{:.9g}) extent={}x{} depth={} layer_bias={:.9g} sampled_depth={:x}/{} scene_depth={:x}/{} vp_guest=[{}] vp_upload=[{}] ps_c2_c5_guest=[{}] ps_c2_c5_upload=[{}]",
                        frame, drawsThisFrame, key.vs, key.ps, temporalSlot, jitterLogSlot, info.indexCount, info.indexBase, baseVertex,
                        Reg(REG_FETCH_CONSTANTS + 190), Reg(REG_FETCH_CONSTANTS + 191), staticMesh ? Fnv1a(vsConstants, 16 * sizeof(uint32_t)) : 0,
                        temporalExperiment && temporalJitter, temporalViewport,
                        drawJitter.applied, drawJitter.shadowCompensated, uint32_t(drawJitter.rejection), uint32_t(frame % 32 + 1),
                        drawJitter.sample.ndcX, drawJitter.sample.ndcY, rasterViewport.width, rasterViewport.height,
                        depth ? depth->allocationSerial : 0, layerDepthOffset,
                        jitterSampledDepth ? jitterSampledDepth->address : 0, jitterSampledDepth ? jitterSampledDepth->ordinal : 0,
                        temporalScene.Depth().address, temporalScene.Depth().ordinal, bits(guestVp.data(), jitterLogSlot >= 0 ? 16 : 0),
                        jitterLogSlot >= 0 ? bits(vsConstants + jitterLogSlot * 4, 16) : "",
                        bits(guestShadow.data(), shadowPair ? 16 : 0), shadowPair ? bits(psConstants + 2 * 4, 16) : "");
                }
                if(temporalSceneCopy)temporalSubmittedFrame=frame;
                if(hdrTonemapRecorded)hdrTonemapApplied=true;
                if(fullSceneCopy)color->aaProvenance.Invalidate(frame,color->allocationSerial,
                    rasterViewport.width>=color->aaValidWidth && rasterViewport.height>=color->aaValidHeight);
                {
                    const bool isProducerCandidate = key.vs == color_qualification::kTonemapVS && key.ps == color_qualification::kTonemapPS;
                    if (isProducerCandidate && (key.colorMask & 7) != 0) {
                        static const bool debugNoDepth = getenv("LO_DEBUG_NODEPTH") != nullptr;
                        const uint32_t targetExpBias = (colorInfo >> 20) & 0x3F;
                        const color_qualification::ProducerPipelineCheck pipeCheck{
                            .vs = key.vs,
                            .ps = key.ps,
                            .c10xBits = psConstants[10 * 4 + 0],
                            .colorMask = key.colorMask,
                            .blend = key.blend,
                            .depthControl = key.depthControl,
                            .modeCull = key.modeCull,
                            .colorControl = Reg(REG_RB_COLORCONTROL),
                            .guestTargetFormat = (colorInfo >> 16) & 0xF,
                            .targetExpBias = targetExpBias,
                            .vtxFmt = shared.vtxFmt,
                            .sharedFlags = shared.flags,
                            .debugOverrides = debugNoDepth
                        };
                        bool quadOk = false;
                        color_qualification::ProducerRejectReason rejectReason = color_qualification::CheckProducerPipeline(pipeCheck);
                        float quadBounds[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                        if (rejectReason == color_qualification::ProducerRejectReason::None) {
                            if (info.primitiveType != 4 || !info.indexed || info.indexCount != 6 || indices.size() < 6) {
                                rejectReason = color_qualification::ProducerRejectReason::PrimitiveMismatch;
                            } else if (!slot95Bound || slot95ArenaOffset == UINT64_MAX) {
                                rejectReason = color_qualification::ProducerRejectReason::Slot95Unbound;
                            } else {
                                float positions[24];
                                bool fetchOk = true;
                                const uint8_t* arenaBase = arenaMapped + slot95ArenaOffset;
                                for (unsigned i = 0; i < 6; ++i) {
                                    const int64_t finalIndex = int64_t(indices[i]) + int64_t(baseVertex);
                                    if (finalIndex < 0) {
                                        rejectReason = color_qualification::ProducerRejectReason::IndexRangeOverflow;
                                        fetchOk = false;
                                        break;
                                    }
                                    const uint64_t streamPos = uint64_t(finalIndex) * 32;
                                    if (streamPos + 16 > slot95StreamBytes || slot95ArenaOffset + streamPos + 16 > gpu::render_arena::kVertexArenaSize) {
                                        rejectReason = color_qualification::ProducerRejectReason::StreamBytesOverflow;
                                        fetchOk = false;
                                        break;
                                    }
                                    memcpy(&positions[i * 4], arenaBase + streamPos, 16);
                                }
                                if (fetchOk) {
                                    const auto geo = color_qualification::CheckQuadCoverage(
                                        positions,
                                        rasterViewport.x, rasterViewport.y, rasterViewport.width, rasterViewport.height,
                                        scissor.left, scissor.top, scissor.right, scissor.bottom,
                                        shared.ndcScale, shared.ndcOffset, shared.halfPixel);
                                    quadOk = geo.ok;
                                    rejectReason = geo.rejectReason;
                                    std::copy_n(geo.bounds, 4, quadBounds);
                                }
                            }
                        }
                        if (quadOk) {
                            color_qualification::MarkHostTextureProducer(
                                color->sdrProducerFrame, color->qualifiedSdrWidth, color->qualifiedSdrHeight,
                                frame, uint32_t(rasterViewport.width), uint32_t(rasterViewport.height));
                        } else {
                            color_qualification::InvalidateHostTextureProducer(
                                color->sdrProducerFrame, color->qualifiedSdrWidth, color->qualifiedSdrHeight);
                            static color_qualification::ProducerRejectReason lastLoggedProducerReject = color_qualification::ProducerRejectReason::None;
                            if (lastLoggedProducerReject != rejectReason) {
                                lastLoggedProducerReject = rejectReason;
                                LOG_INFO("renderer: DLSS SDR producer rejected reason={} vp=({},{} {}x{}) scissor=({},{})-({},{}) bounds=({:.2f},{:.2f},{:.2f},{:.2f}) slot95_offset={:#x} slot95_bytes={}",
                                    uint32_t(rejectReason), rasterViewport.x, rasterViewport.y, rasterViewport.width, rasterViewport.height,
                                    scissor.left, scissor.top, scissor.right, scissor.bottom,
                                    quadBounds[0], quadBounds[1], quadBounds[2], quadBounds[3],
                                    slot95ArenaOffset, slot95StreamBytes);
                            }
                        }
                    } else if ((key.colorMask & 7) != 0) {
                        // Any other draw writing RGB invalidates producer status
                        color_qualification::InvalidateHostTextureProducer(
                            color->sdrProducerFrame, color->qualifiedSdrWidth, color->qualifiedSdrHeight);
                    }
                }
                if(sceneAARecorded||temporalAARecorded) {
                    color->aaProvenance.MarkFull(frame,color->allocationSerial);
                    color->aaValidWidth=uint32_t(rasterViewport.width);color->aaValidHeight=uint32_t(rasterViewport.height);
                    if(temporalAARecorded)temporalSupportedFrame=frame;
                    sceneAAAppliedFrame=frame;sceneAAAllocation=color->allocationSerial;
                    static const uint64_t logStart=getenv("LO_SCENE_AA_LOG_START_FRAME")?strtoull(getenv("LO_SCENE_AA_LOG_START_FRAME"),nullptr,10):~0ull;
                    if(frame>=logStart&&frame-logStart<128)SHADER_LOG_INFO("scene-aa", None, "renderer scene AA f{} mode={} temporal={} allocation={} full_copy=1 recorded=1",frame,sceneAAMode,temporalAARecorded,color->allocationSerial);
                }
                if (trackTemporalScene)
                {
                    if(temporalDrawAnchor)temporalScene.ObserveCamera(*temporalDrawAnchor);
                    else if (!temporalExperiment && key.vs == 0xb7557072899a63a1ull && key.ps == 0x9f4dfdd86211a018ull &&
                        depth && (depthControl & 4) && shared.vtxFmt == 4 &&
                        shared.ndcScale[2] == -1.0f && shared.ndcOffset[2] == 1.0f &&
                        shared.ndcScale[0] == 1.0f && shared.ndcOffset[0] == 0.0f && shared.ndcOffset[1] == 0.0f)
                    {
                        temporal::SceneAnchor anchor;
                        std::copy_n(vsConstants + 233 * 4, 16, anchor.vpBits.begin());
                        anchor.depthAllocation = depth->allocationSerial;
                        anchor.viewport = {rasterViewport.x, rasterViewport.y, rasterViewport.width, rasterViewport.height,
                            shared.ndcScale[1], shared.halfPixel[0], shared.halfPixel[1]};
                        temporalScene.ObserveCamera(anchor);
                    }
                }
                if (preparedPipelineKeys.contains(key)) {
                    ++preparedPipelineHits;
                    usedPreparedPipelineKeys.insert(key);
                }
                if (vs->info.usesRelativeConstants)
                {
                    static const bool traceRelative = getenv("LO_RELATIVE_DRAW_STATS") != nullptr;
                    if (traceRelative)
                    {
                        auto& group = relativeDraws[{modeControl, colorInfo, pitch, rtHeight,
                            key.vs, key.ps, key.colorMask, ps ? ps->info.colorTargetsWritten : 0}];
                        group.draws++;
                        group.indices += indexCount;
                    }
                    if (modeControl == 4)
                    {
                        skin.colorDraws++;
                        skin.colorIndices += indexCount;
                        // The scene target: full-width HDR colour, i.e. what the
                        // player actually sees, as opposed to shadow and mask passes.
                        if (pitch >= 1280 && ColorClassOf((colorInfo >> 16) & 0xF) == kClass7e3)
                        {
                            skin.sceneDraws++;
                            skin.sceneIndices += indexCount;
                        }
                    }
                    else skin.depthDraws++;
                }

                // Xbox 360 D3D clears a surface by drawing a screen-space rectangle
                // with ALWAYS depth/colour writes, usually through a different surface
                // pitch (and bit depth) than the scene uses, so the rectangle covers
                // the target's EDRAM tiles rather than its pixels. Every (base, pitch)
                // is a separate texture here, so propagate the clear to every other
                // target starting at the same tile base: depth targets get a real clear
                // with the rectangle's z, colour targets get the rectangle replayed
                // through a viewport that stretches it over the whole texture.
                const bool screenSpaceRect = info.primitiveType == 8 && !info.indexed && info.indexCount <= 6 && (vte & 0x100);
                const bool depthClearDraw = screenSpaceRect && depth && (depthControl & 4) && ((depthControl >> 4) & 7) == 7;
                const bool colorClearDraw = screenSpaceRect && colorWrites && (!depth || !(depthControl & 2));
                if (depthClearDraw || colorClearDraw)
                {
                    // Rectangle extent in the draw's own surface space, from the first vertex stream.
                    float minX = 0, minY = 0, maxX = float(pitch), maxY = float(rtHeight), rectZ = 0.0f;
                    for (uint32_t slot = 0; slot < kVertexFetchSlots; slot++)
                    {
                        if (!((vs->info.vertexFetchSlotMask[slot >> 6] >> (slot & 63)) & 1)) continue;
                        uint32_t d0 = Reg(REG_FETCH_CONSTANTS + slot * 2), d1 = Reg(REG_FETCH_CONSTANTS + slot * 2 + 1);
                        uint32_t sizeDwords = (d1 >> 2) & 0xFFFFFF;
                        uint32_t stride = info.indexCount ? sizeDwords / info.indexCount : 0;
                        if ((d0 & 3) != 3 || stride < 3) break;
                        const uint32_t* src = reinterpret_cast<const uint32_t*>(Phys(d0 & ~3u));
                        minX = minY = 1e9f; maxX = maxY = -1e9f;
                        for (uint32_t v = 0; v < info.indexCount; v++)
                        {
                            float xyz[3];
                            for (int k = 0; k < 3; k++) { uint32_t w = GpuSwap(src[v * stride + k], d1 & 3); memcpy(&xyz[k], &w, 4); }
                            minX = std::min(minX, xyz[0]); maxX = std::max(maxX, xyz[0]);
                            minY = std::min(minY, xyz[1]); maxY = std::max(maxY, xyz[1]);
                            if (v == 0) rectZ = xyz[2];
                        }
                        break;
                    }
                    if (vte & 0x10) rectZ = rectZ * shared.ndcScale[2] + shared.ndcOffset[2];
                    rectZ = std::clamp(rectZ, 0.0f, 1.0f);
                    // NDC bounds of the rectangle as the shader sees it.
                    const float nx0 = minX * shared.ndcScale[0] + shared.ndcOffset[0], nx1 = maxX * shared.ndcScale[0] + shared.ndcOffset[0];
                    const float ny0 = maxY * shared.ndcScale[1] + shared.ndcOffset[1], ny1 = minY * shared.ndcScale[1] + shared.ndcOffset[1]; // ny0 bottom, ny1 top

                    // The 360's D3D Clear() of a colour surface is this depth-only fill
                    // (mode 5, zfunc ALWAYS, z write) issued with RB_DEPTH_INFO.base set
                    // to the COLOUR surface's tiles: EDRAM is one memory, so the depth
                    // word it stores is what a colour view of the same tiles reads back
                    // (Xenia reproduces it as a depth->colour ownership transfer). Wipe
                    // every colour view of those tiles to the unpacked word; without
                    // this the scene, the distortion map and the luminance target kept
                    // the previous frame's image.
                    if (depthClearDraw && !colorWrites && minX <= 0.0f && minY <= 0.0f)
                    {
                        const bool depthFloat = ((depthInfo >> 16) & 1) != 0;
                        const uint32_t depth24 = depthFloat ? Float32To20e4(rectZ) : PackDepth24Unorm(rectZ);
                        const uint32_t word = (depth24 << 8) | (Reg(REG_RB_STENCILREFMASK) & 0xFF);
                        static uint32_t loggedFills = 0;
                        for (auto& [k, tex] : renderTargets)
                        {
                            if (!tex || k.depth || k.base != (depthInfo & 0xFFF))
                                continue;
                            RenderColor value = UnpackGuestWord(word, k.format);
                            Transition(*tex, RenderTextureLayout::COLOR_WRITE, RenderBarrierStage::GRAPHICS);
                            commandList->setFramebuffer(GetFramebuffer(tex.get(), nullptr));
                            commandList->clearColor(0, value);
#if defined(LO_GPU_PLUME)
                            HandleFsrAlphaRgbWriter(*tex, "depth_color_tile_clear");
                            if (fsrAlphaBridge && activePlan.requestedUpscaler == upscaling::Upscaler::Fsr)
                                fsrAlphaBridge->RecordFullColorClear(frame, temporalEpoch,
                                    tex->allocationSerial, tex->width, tex->height, drawsThisFrame,
                                    {0, 0, tex->width, tex->height}, "depth_color_tile_clear");
#endif
                            if (trackBinding) tex->bindingProducer.Clear(bindingEpoch, frame, true);
                            tex->aaProvenance.Invalidate(frame,tex->allocationSerial,true);
                            tex->sdrProducerFrame = ~0ull;
                            if (loggedFills++ < 12)
                                LOG_INFO("renderer: depth fill (base={:#x} pitch={} msaa={} rect {}x{} z={} word={:#x}) wiped colour view class {} pitch {} to ({:g},{:g},{:g},{:g})",
                                    depthInfo & 0xFFF, pitch, (surfaceInfo >> 16) & 3, maxX, maxY, rectZ, word, k.format, k.pitch, value.r, value.g, value.b, value.a);
                        }
                        // Re-bind the fill's own framebuffer for the draw that follows.
                        commandList->setFramebuffer(GetFramebuffer(color, depth));
                    }

                    std::vector<RenderTargetKey> others;
                    for (auto& [k, tex] : renderTargets)
                    {
                        if (!tex || k.pitch == pitch) continue;
                        if (depthClearDraw && k.depth && k.base == (depthInfo & 0xFFF)) others.push_back(k);
                        if (colorClearDraw && !k.depth && k.base == (colorInfo & 0xFFF)) others.push_back(k);
                    }
                    static uint32_t logged = 0;
                    for (const RenderTargetKey& k : others)
                    {
                        HostTexture* target = renderTargets[k].get();
                        if (k.depth)
                        {
                            Transition(*target, RenderTextureLayout::DEPTH_WRITE, RenderBarrierStage::GRAPHICS);
                            commandList->setFramebuffer(GetFramebuffer(nullptr, target));
                            const DepthClearRect sourceRect{int32_t(std::ceil(minX)), int32_t(std::ceil(minY)),
                                int32_t(std::ceil(maxX)), int32_t(std::ceil(maxY))};
                            auto mapped = MapDepthClear(pitch, (surfaceInfo >> 16) & 3, sourceRect,
                                k.pitch, target->guestHeight, target->depthMsaa);
                            if (vulkan) CoalesceDepthClearRects(mapped);
                            std::vector<RenderRect> clearRects;
                            clearRects.reserve(mapped.size());
                            for (const auto& r : mapped) clearRects.push_back({int32_t(target->ScaleX(uint32_t(r.left))), int32_t(target->ScaleY(uint32_t(r.top))),
                                int32_t(target->ScaleX(uint32_t(r.right))), int32_t(target->ScaleY(uint32_t(r.bottom)))});
                            // A zero rectangle count means a whole-resource clear in
                            // the graphics API, so an empty mapping must be skipped.
                            if (!clearRects.empty())
                            {
                                if (getenv("LO_TRACE_CLEAR_CALL"))
                                    LOG_INFO("clear begin f{} base={} pitch={} size={}x{} count={} z={}", frame, k.base, k.pitch, target->width, target->height, clearRects.size(), rectZ);
                                commandList->clearDepthStencil(true, false, rectZ, 0, clearRects.data(), uint32_t(clearRects.size()));
                                if (trackBinding) {
                                    const bool full = clearRects.size() == 1 && clearRects[0].left == 0 && clearRects[0].top == 0 &&
                                        clearRects[0].right == int32_t(target->width) && clearRects[0].bottom == int32_t(target->height);
                                    target->bindingProducer.Clear(bindingEpoch, frame, full);
                                }
                                if (getenv("LO_TRACE_CLEAR_CALL")) LOG_INFO("clear end");
                            }
                            if (logged++ < 8 || frame == captureFrame)
                                LOG_INFO("renderer: depth clear f{} rect (pitch {}, {}x{} .. {}x{}) -> depth base={:#x} pitch={} size={}x{} to {} msaa={}->{} regions={}", frame, pitch, minX, minY, maxX, maxY, k.base, k.pitch, target->width, target->height, rectZ, (surfaceInfo >> 16) & 3, target->depthMsaa, clearRects.size());
                            continue;
                        }
                        if (target->format != color->format || nx1 <= nx0 || ny1 <= ny0)
                            continue;
                        // The rectangle covers a run of EDRAM tiles, not a fraction of
                        // the other view's image: the same bytes span fewer rows in a
                        // wider target. Rows scale by the pitch ratio (same bpp here,
                        // the host formats are equal).
                        const uint32_t clearedRows = uint32_t(std::ceil(std::max(0.0f, maxY - minY)));
                        uint32_t mappedRows = uint32_t((uint64_t(clearedRows) * pitch + k.pitch - 1) / k.pitch);
                        mappedRows = target->ScaleY(std::clamp<uint32_t>(mappedRows, 1, target->guestHeight));
                        HostTexture* otherDepth = depth ? GetRenderTarget(depthInfo & 0xFFF, (depthInfo >> 16) & 1, k.pitch, target->guestHeight, true) : nullptr;
                        if (otherDepth && (otherDepth->width != target->width || otherDepth->height != target->height))
                            continue;
                        Transition(*target, RenderTextureLayout::COLOR_WRITE, RenderBarrierStage::GRAPHICS);
                        if (otherDepth)
                            Transition(*otherDepth, RenderTextureLayout::DEPTH_WRITE, RenderBarrierStage::GRAPHICS);
                        commandList->setFramebuffer(GetFramebuffer(target, otherDepth));
                        // Viewport such that the rectangle's NDC bounds land on the full texture.
                        const float vpW = 2.0f * float(target->width) / (nx1 - nx0);
                        const float vpH = 2.0f * float(mappedRows) / (ny1 - ny0);
                        RenderViewport stretched(-(nx0 + 1.0f) * 0.5f * vpW, -(1.0f - ny1) * 0.5f * vpH, vpW, vpH);
                        commandList->setViewports(&stretched, 1);
                        RenderRect fullRect{ 0, 0, int32_t(target->width), int32_t(mappedRows) };
                        commandList->setScissors(&fullRect, 1);
                        commandList->drawInstanced(indexCount, 1, uint32_t(baseVertex), 0);
#if defined(LO_GPU_PLUME)
                        if (key.colorMask & 7) HandleFsrAlphaRgbWriter(*target,
                            "color_clear_rect", drawsThisFrame, key.vs, key.ps, key.blend, key.colorMask);
#endif
                        if ((key.colorMask & 7) != 0) target->sdrProducerFrame = ~0ull;
                        if (trackBinding) {
                            // Replayed clear geometry uses a stretched viewport, so do not reuse the original transform.
                            if (key.colorMask) target->bindingProducer.Mixed(bindingEpoch, frame);
                            if (otherDepth && (key.depthControl & 6) == 6) otherDepth->bindingProducer.Mixed(bindingEpoch, frame);
                        }
                        if (logged++ < 8)
                            LOG_INFO("renderer: colour clear rect (pitch {}, {} rows) replayed into base={:#x} pitch={} {}x{} rows", pitch, clearedRows, k.base, k.pitch, target->width, mappedRows);
                    }
                }

                // Debug snapshot goes last: it flushes, which would drop the
                // pipeline state the clear replay above still relies on.
                Gpu().drawProbe.Record(commandList, drawsThisFrame - 1, key.vs, key.ps,
                    indexCount, color->width, color->height);
                DumpDrawStep(*color, key.vs);
            }

            // Writes any colour/depth texture to a PPM, tonemapping FP16 by clamping.
            // Used by the resolve and draw-step dumps; costs a full GPU sync per call.
            void DumpTexture(HostTexture& tex, const std::string& path, const char* what)
            {
                uint32_t bpp = tex.format == RenderFormat::R8G8B8A8_UNORM ? 4 : tex.format == RenderFormat::R16G16B16A16_FLOAT ? 8 : tex.format == RenderFormat::R32_FLOAT ? 4 : 0;
                if (!bpp)
                    return;
                const uint32_t rowPitch = (tex.width * bpp + 255) & ~255u;
                if (size_t(rowPitch) * tex.height > kReadbackSize)
                    return;
                if (!Begin()) return;
                Transition(tex, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
                commandList->copyTextureRegion(
                    RenderTextureCopyLocation::PlacedFootprint(readback.get(), tex.format, tex.width, tex.height, 1, rowPitch / bpp, 0),
                    RenderTextureCopyLocation::Subresource(tex.texture.get(), 0));
                if (!Flush() || !WaitForReadback()) return;
                if (!Begin()) return;
                const uint8_t* src = static_cast<const uint8_t*>(readback->map());
                if (FILE* f = fopen(path.c_str(), "wb"))
                {
                    fprintf(f, "P6%c%u %u%c255%c", 10, tex.width, tex.height, 10, 10);
                    std::vector<uint8_t> row(size_t(tex.width) * 3);
                    for (uint32_t y = 0; y < tex.height; y++)
                    {
                        const uint8_t* r = src + size_t(y) * rowPitch;
                        for (uint32_t x = 0; x < tex.width; x++)
                        {
                            float rgb[3] = { 0, 0, 0 };
                            if (bpp == 8)
                                for (int c = 0; c < 3; c++) { uint16_t h; memcpy(&h, r + size_t(x) * 8 + c * 2, 2); rgb[c] = HalfToFloat(h); }
                            else if (tex.format == RenderFormat::R32_FLOAT)
                            { float d; memcpy(&d, r + size_t(x) * 4, 4); rgb[0] = rgb[1] = rgb[2] = d; }
                            else
                                for (int c = 0; c < 3; c++) rgb[c] = r[size_t(x) * 4 + c] / 255.0f;
                            for (int c = 0; c < 3; c++)
                                row[x * 3 + c] = uint8_t(std::clamp(rgb[c], 0.0f, 1.0f) * 255.0f + 0.5f);
                        }
                        fwrite(row.data(), 1, row.size(), f);
                    }
                    fclose(f);
                }
                readback->unmap();
                LOG_INFO("renderer: {} -> {}", what, path);
            }

            // LO_DUMP_DRAW_SEQ=<frame> + LO_DUMP_DRAW_EVERY=<n>: snapshot the bound
            // colour target every n draws of that frame, so the draw that ruins the
            // image can be identified from a single run.
            // With LO_DUMP_DRAW_VS=<hex hash> the frame number is only a lower bound:
            // the dump fires after every draw of that vertex shader in the first
            // frame at or past it that uses the shader. Frame numbers drift between
            // runs (the dumps themselves slow the game), shader hashes do not.
            void DumpDrawStep(HostTexture& color, uint64_t vsHash)
            {
                // Per-draw previews are bulky and require extra GPU waits. Keep
                // them opt-in; the normal export retains every resolve and trace.
                static const bool captureDrawSteps = getenv("LO_DEBUG_CAPTURE_DRAW_STEPS") &&
                    strcmp(getenv("LO_DEBUG_CAPTURE_DRAW_STEPS"), "1") == 0;
                if (!debugCaptureDir.empty() && !captureDrawSteps) return;
                const auto& captureEnvironment = GetHotCaptureEnvironment();
                const uint32_t dumpFrame = captureFrame ? captureFrame : captureEnvironment.dumpDrawFrame;
                static const uint32_t every = getenv("LO_DUMP_DRAW_EVERY") ? std::max(1ul, strtoul(getenv("LO_DUMP_DRAW_EVERY"), nullptr, 10)) : 25;
                static const uint64_t dumpVs = getenv("LO_DUMP_DRAW_VS") ? strtoull(getenv("LO_DUMP_DRAW_VS"), nullptr, 16) : 0;
                static uint64_t lockedFrame = 0;
                if (!dumpFrame)
                    return;
                if (dumpVs)
                {
                    if (vsHash != dumpVs || frame < dumpFrame || (lockedFrame && frame != lockedFrame))
                        return;
                    lockedFrame = frame;
                }
                else if (frame != dumpFrame || (drawsThisFrame % every) != 0)
                    return;
                const char* dir = debugCaptureDir.empty()
                    ? (captureEnvironment.dumpResolveDirConfigured ? captureEnvironment.dumpResolveDir.c_str() : nullptr)
                    : debugCaptureDir.c_str();
                DumpTexture(color, fmt::format("{}/f{}_draw{:04}_{}x{}.ppm", dir ? dir : ".", frame, drawsThisFrame, color.width, color.height), "draw step");
            }

            // LO_DUMP_RESOLVE_SEQ=<frame>: dump every resolve of that frame in order.
            uint32_t resolveSeq = 0;
            void DumpResolveStep(HostTexture& tex, uint32_t destBase, uint64_t writeOrdinal, uint32_t guestFormat)
            {
                QueueResolveTrace(tex, destBase);
                const auto& captureEnvironment = GetHotCaptureEnvironment();
                const uint32_t dumpFrame = captureFrame ? captureFrame : captureEnvironment.dumpResolveFrame;
                if (!dumpFrame || frame != dumpFrame)
                    return;
                uint32_t bpp = tex.format == RenderFormat::R8G8B8A8_UNORM ? 4 : tex.format == RenderFormat::R16G16B16A16_FLOAT ? 8 : tex.format == RenderFormat::R32_FLOAT ? 4 : 0;
                if (!bpp)
                    return;
                const uint32_t rowPitch = (tex.width * bpp + 255) & ~255u;
                if (size_t(rowPitch) * tex.height > kReadbackSize)
                    return;
                Transition(tex, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
                commandList->copyTextureRegion(
                    RenderTextureCopyLocation::PlacedFootprint(readback.get(), tex.format == RenderFormat::R32_FLOAT ? RenderFormat::R32_FLOAT : tex.format, tex.width, tex.height, 1, rowPitch / bpp, 0),
                    RenderTextureCopyLocation::Subresource(tex.texture.get(), 0));
                if (!Flush() || !WaitForReadback()) return;
                if (!Begin()) return;
                const uint8_t* src = static_cast<const uint8_t*>(readback->map());
                const char* dir = debugCaptureDir.empty()
                    ? (captureEnvironment.dumpResolveDirConfigured ? captureEnvironment.dumpResolveDir.c_str() : nullptr)
                    : debugCaptureDir.c_str();
                std::string path = fmt::format("{}/f{}_seq{:02}_{:x}.ppm", dir ? dir : ".", frame, resolveSeq++, destBase);
                if (!debugCaptureDir.empty())
                {
                    const auto rawPath = std::filesystem::path(path).replace_extension(".bin");
                    std::ofstream raw(rawPath, std::ios::binary);
                    for (uint32_t y = 0; y < tex.height; ++y)
                        raw.write(reinterpret_cast<const char*>(src + size_t(y) * rowPitch), size_t(tex.width) * bpp);
                    raw.close();
                    debugTrace << fmt::format("resolve {} draw={} address={:#x} width={} height={} plume_format={} bpp={} raw_ok={} (packed rows, little endian)\n",
                        rawPath.filename().string(), debugDraw, destBase, tex.width, tex.height, uint32_t(tex.format), bpp, !raw.fail());
                    debugTrace << fmt::format("resolve_provenance {} frame={} write_ordinal={} guest_format={}\n",
                        rawPath.filename().string(), frame, writeOrdinal, guestFormat);
                    if (raw.fail()) debugTrace.setstate(std::ios::failbit);
                }
                // The preview quantizes depth to eight bits, hiding the small
                // differences involved in shadow comparisons. Keep exact R32
                // values beside explicitly requested resolve captures.
                if (tex.format == RenderFormat::R32_FLOAT && debugCaptureDir.empty())
                {
                    const auto rawPath = std::filesystem::path(path).replace_extension(".f32");
                    if (FILE* raw = fopen(rawPath.string().c_str(), "wb"))
                    {
                        for (uint32_t y = 0; y < tex.height; ++y)
                            fwrite(src + size_t(y) * rowPitch, sizeof(float), tex.width, raw);
                        fclose(raw);
                    }
                }
                if (FILE* f = fopen(path.c_str(), "wb"))
                {
                    fprintf(f, "P6%c%u %u%c255%c", 10, tex.width, tex.height, 10, 10);
                    std::vector<uint8_t> row(size_t(tex.width) * 3);
                    for (uint32_t y = 0; y < tex.height; y++)
                    {
                        const uint8_t* r = src + size_t(y) * rowPitch;
                        for (uint32_t x = 0; x < tex.width; x++)
                        {
                            float rgb[3] = { 0, 0, 0 };
                            if (bpp == 4 && tex.format == RenderFormat::R8G8B8A8_UNORM)
                                for (int c = 0; c < 3; c++) rgb[c] = r[x * 4 + c] / 255.0f;
                            else if (bpp == 8)
                                for (int c = 0; c < 3; c++) { uint16_t h; memcpy(&h, r + x * 8 + c * 2, 2); rgb[c] = HalfToFloat(h); }
                            else { float d; memcpy(&d, r + x * 4, 4); rgb[0] = rgb[1] = rgb[2] = d; }
                            for (int c = 0; c < 3; c++)
                                row[x * 3 + c] = uint8_t(std::clamp(rgb[c], 0.0f, 1.0f) * 255.0f + 0.5f);
                        }
                        fwrite(row.data(), 1, row.size(), f);
                    }
                    fclose(f);
                }
                // Raw statistics: tells apart "shading is too bright" from "the bits
                // are being interpreted as the wrong format".
                double sum = 0; float lo = 1e30f, hi = -1e30f; uint32_t over1 = 0, samples = 0;
                for (uint32_t y = 0; y < tex.height; y += 4)
                {
                    const uint8_t* r = src + size_t(y) * rowPitch;
                    for (uint32_t x = 0; x < tex.width; x += 4)
                    {
                        float v = 0;
                        if (bpp == 8) { uint16_t h; memcpy(&h, r + size_t(x) * 8, 2); v = HalfToFloat(h); }
                        else if (tex.format == RenderFormat::R32_FLOAT) memcpy(&v, r + size_t(x) * 4, 4);
                        else v = r[size_t(x) * 4] / 255.0f;
                        sum += v; lo = std::min(lo, v); hi = std::max(hi, v); samples++;
                        if (v > 1.0f) over1++;
                    }
                }
                readback->unmap();
                LOG_INFO("renderer: resolve step {} -> {:#x} dumped ({}x{}) red min={:g} max={:g} mean={:g} over1={}%",
                    resolveSeq - 1, destBase, tex.width, tex.height, lo, hi, sum / std::max(1u, samples), over1 * 100 / std::max(1u, samples));
            }

            // ---- resolve --------------------------------------------------------------------
            // Depth resolve: the depth plane is copied into an R32_FLOAT surface that
            // k_24_8 / k_24_8_FLOAT fetches read (.x = stored depth, our reversed
            // range included, exactly what the guest wrote).
            bool ResolveDepthOnGpu(HostTexture& depth, uint32_t destBase, uint32_t destFormat, uint32_t destPitch, uint32_t destHeight,
                                   uint32_t x0, uint32_t y0, uint32_t w, uint32_t h)
            {
                consecutiveResolveCopies.Invalidate();
                if (!Begin()) return false;
                const uint32_t guestW = std::clamp<uint32_t>(std::max(destPitch, x0 + w), 1, 8192);
                const uint32_t guestH = std::clamp<uint32_t>(std::max(destHeight, y0 + h), 1, 8192);
                const uint32_t texW = std::max(1u, depth.ScaleX(guestW)), texH = std::max(1u, depth.ScaleY(guestH));
                if (texW > 16384 || texH > 16384) {
                    FailCurrentPlan();
                    LOG_ERROR("renderer: depth resolve exceeds texture limit physical={}x{}; native resolution fallback next frame", texW, texH);
                    return false;
                }
                w = depth.ScaleX(x0 + w) - depth.ScaleX(x0); h = depth.ScaleY(y0 + h) - depth.ScaleY(y0);
                x0 = depth.ScaleX(x0); y0 = depth.ScaleY(y0);
                ResolvedSurface& rs = ResolvedSlot(destBase, destFormat);
                if (!rs.tex || rs.tex->format != RenderFormat::R32_FLOAT || rs.tex->width != texW || rs.tex->height != texH)
                {
                    if (rs.tex) Gpu().retiredTextures.push_back(std::move(rs.tex));
                    rs.writeOrdinal = 0; // The replacement allocation contains no previous resolve.
                    rs.sdrWriteOrdinal = 0;
                    rs.writeWidth = rs.writeHeight = 0;
                    rs.tex = std::make_unique<HostTexture>();
                    rs.tex->format = RenderFormat::R32_FLOAT;
                    rs.tex->width = texW;
                    rs.tex->height = texH;
                    rs.tex->guestWidth = guestW; rs.tex->guestHeight = guestH;
                    rs.tex->resolutionSize = depth.resolutionSize;
                    rs.tex->texture = device->createTexture(RenderTextureDesc::Texture2D(texW, texH, 1, RenderFormat::R32_FLOAT,
                        vulkan ? RenderTextureFlag::RENDER_TARGET : RenderTextureFlag::NONE));
                    rs.tex->layout = RenderTextureLayout::UNKNOWN;
                    if (!rs.tex->texture)
                    {
                        FailCurrentPlan();
                        LOG_ERROR("renderer: depth resolve allocation failed guest={}x{} physical={}x{}; native resolution fallback next frame", guestW, guestH, texW, texH);
                        DropResolved(destBase, destFormat);
                        return false;
                    }
                    static uint32_t created = 0;
                    if (created++ < 8)
                        LOG_INFO("renderer: resolved depth surface {:#x} guest={}x{} physical={}x{} dest fmt={}", destBase, guestW, guestH, texW, texH, destFormat & 0xFFF);
                }
                rs.destFormat = destFormat;
                rs.destPitch = destPitch;
                rs.swapRedBlue = false;
                if (x0 >= texW || y0 >= texH) return true;
                w = std::min(w, texW - x0);
                h = std::min(h, texH - y0);
                if (w == 0 || h == 0)
                    return true;
                if (vulkan) {
                    // Vulkan 1.2 image copies cannot reinterpret D32/S8 storage
                    // as an R32 color plane. Load the depth aspect and write the
                    // exact float value, retaining the destination rectangle.
                    if (!BlitRegion(depth, *rs.tex, x0, y0, w, h)) {
                        LOG_ERROR("renderer: Vulkan depth resolve blit failed");
                        return false;
                    }
                } else {
                    Transition(depth, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
                    Transition(*rs.tex, RenderTextureLayout::COPY_DEST, RenderBarrierStage::COPY);
                    RenderBox box{ int32_t(x0), int32_t(y0), int32_t(x0 + w), int32_t(y0 + h), 0, 1 };
                    commandList->copyTextureRegion(RenderTextureCopyLocation::Subresource(rs.tex->texture.get()),
                        RenderTextureCopyLocation::Subresource(depth.texture.get(), 0), x0, y0, 0, &box);
                }
                Transition(*rs.tex, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
                rs.frame = frame;
                rs.writeOrdinal = ++resolveWriteOrdinal;
                rs.writeX = x0; rs.writeY = y0; rs.writeWidth = w; rs.writeHeight = h;
                WriteP2ResolveEvent("depth", depth, destBase, rs, vulkan ? "blit" : "copy");
                if (taa_collection::Enabled())
                    rs.tex->bindingProducer.Copy(depth.bindingProducer, taa_collection::ConsentEpoch(), frame,
                        x0 == 0 && y0 == 0 && w == texW && h == texH);
                if (!debugCaptureDir.empty() || temporalExperiment || temporalInputProbe || dlssSrRequested || activeSpatialAA) {
                    temporalScene.ObserveDepth(depth.allocationSerial, {frame, rs.writeOrdinal, destBase, destFormat,
                        texW, texH, x0 == 0 && y0 == 0 && w == texW && h == texH});
                    if((temporalExperiment || temporalInputProbe || dlssSrRequested) && temporalHistory && temporalScene.Depth().ordinal==rs.writeOrdinal) {
                        Transition(*rs.tex,RenderTextureLayout::COPY_SOURCE,RenderBarrierStage::COPY);
                        temporalHistory->CaptureDepth(commandList,rs.tex->texture.get(),temporalScene);
                        if (taaDiagnosticHDR == 1 && hdrTemporalHistory)
                            hdrTemporalHistory->CaptureDepth(commandList,rs.tex->texture.get(),temporalScene);
                        Transition(*rs.tex,RenderTextureLayout::SHADER_READ,RenderBarrierStage::GRAPHICS);
                    }
                }
                DumpResolveStep(*rs.tex, destBase, rs.writeOrdinal, rs.destFormat);
                return true;
            }

            // Copies the resolve rectangle into the host texture standing in for the
            // destination memory (destPitch x destHeight texels, rectangle placed at its
            // window position). No GPU sync, no tiling, no guest memory writes.
            bool ResolveOnGpu(HostTexture& color, uint32_t destBase, uint32_t destFormat, uint32_t destPitch, uint32_t destHeight,
                              uint32_t x0, uint32_t y0, uint32_t w, uint32_t h)
            {
                if (!Begin()) return false;
                const uint32_t guestW = std::clamp<uint32_t>(std::max(destPitch, x0 + w), 1, 8192);
                const uint32_t guestH = std::clamp<uint32_t>(std::max(destHeight, y0 + h), 1, 8192);
                const uint32_t texW = std::max(1u, color.ScaleX(guestW)), texH = std::max(1u, color.ScaleY(guestH));
                if (texW > 16384 || texH > 16384) {
                    FailCurrentPlan();
                    LOG_ERROR("renderer: color resolve exceeds texture limit physical={}x{}; native resolution fallback next frame", texW, texH);
                    return false;
                }
                w = color.ScaleX(x0 + w) - color.ScaleX(x0); h = color.ScaleY(y0 + h) - color.ScaleY(y0);
                x0 = color.ScaleX(x0); y0 = color.ScaleY(y0);
                // The destination's own format decides what the surface holds, so the
                // frontbuffer stays 8888 even though EDRAM is kept in FP16.
                const RenderFormat destHost = (destFormat == 32 || destFormat == 7) ? RenderFormat::R16G16B16A16_FLOAT : RenderFormat::R8G8B8A8_UNORM;
                ResolvedSurface& rs = ResolvedSlot(destBase, destFormat);
                if (!rs.tex || rs.tex->format != destHost || rs.tex->width != texW || rs.tex->height != texH)
                {
                    consecutiveResolveCopies.Invalidate();
                    if (rs.tex) Gpu().retiredTextures.push_back(std::move(rs.tex));
                    rs.writeOrdinal = 0; // The replacement allocation contains no previous resolve.
                    rs.sdrWriteOrdinal = 0;
                    rs.writeWidth = rs.writeHeight = 0;
                    rs.tex = std::make_unique<HostTexture>();
                    rs.tex->allocationSerial = ++nextTargetAllocation;
                    rs.tex->format = destHost;
                    rs.tex->width = texW;
                    rs.tex->height = texH;
                    rs.tex->guestWidth = guestW; rs.tex->guestHeight = guestH;
                    rs.tex->resolutionSize = color.resolutionSize;
                    rs.tex->texture = device->createTexture(RenderTextureDesc::Texture2D(texW, texH, 1, destHost, RenderTextureFlag::RENDER_TARGET));
                    rs.tex->layout = RenderTextureLayout::UNKNOWN;
                    if (!rs.tex->texture)
                    {
                        FailCurrentPlan();
                        LOG_ERROR("renderer: color resolve allocation failed guest={}x{} physical={}x{}; native resolution fallback next frame", guestW, guestH, texW, texH);
                        DropResolved(destBase, destFormat);
                        return false;
                    }
                    // Placed render-target textures require full-subresource
                    // initialization before a partial copy. The blur resolve
                    // writes 432 pixels of a 448-pixel allocation; without this
                    // clear its data can read back as zero on AMD hardware.
                    // Initialize only on allocation, preserving later partial
                    // resolves and their untouched destination pixels.
                    Transition(*rs.tex, RenderTextureLayout::COLOR_WRITE, RenderBarrierStage::GRAPHICS);
                    commandList->setFramebuffer(GetFramebuffer(rs.tex.get(), nullptr));
                    commandList->clearColor(0, RenderColor(0, 0, 0, 0));
                    if (taa_collection::Enabled())
                        rs.tex->bindingProducer.Clear(taa_collection::ConsentEpoch(), frame, true);
                    static uint32_t created = 0;
                    if (created++ < 16)
                        LOG_INFO("renderer: resolved surface {:#x} guest={}x{} physical={}x{} host fmt={} dest fmt={}", destBase, guestW, guestH, texW, texH, uint32_t(color.format), destFormat);
                }
                rs.destFormat = destFormat;
                rs.destPitch = destPitch;
                rs.swapRedBlue = ((Reg(REG_RB_COPY_DEST_INFO) >> 24) & 1) != 0;
                if (x0 >= texW || y0 >= texH) return true;
                w = std::min(w, texW - x0);
                h = std::min(h, texH - y0);
                if (w == 0 || h == 0)
                    return true;
                const char* resolveOperation = rs.tex->format == color.format ? "copy" : "blit";
                if (rs.tex->format == color.format)
                {
                    Transition(color, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
                    Transition(*rs.tex, RenderTextureLayout::COPY_DEST, RenderBarrierStage::COPY);
                    const resolve_copy::Copy copy{color.allocationSerial, rs.tex->allocationSerial,
                        color.width, color.height, texW, texH, uint32_t(color.format), x0, y0, w, h};
                    if (resolveCopyReuse && consecutiveResolveCopies.CanReuse(copy)) {
                        ++resolveCopiesSkipped;
                        resolveOperation = "copy_reused";
                    }
                    else
                    {
                        RenderBox box{ int32_t(x0), int32_t(y0), int32_t(x0 + w), int32_t(y0 + h), 0, 1 };
                        commandList->copyTextureRegion(RenderTextureCopyLocation::Subresource(rs.tex->texture.get()),
                            RenderTextureCopyLocation::Subresource(color.texture.get()), x0, y0, 0, &box);
                        ++resolveCopiesRecorded;
                    }
                    consecutiveResolveCopies.Record(copy);
                }
                else if (!BlitRegion(color, *rs.tex, x0, y0, w, h))
                    return false;
                Transition(*rs.tex, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
                rs.frame = frame;
                rs.writeOrdinal = ++resolveWriteOrdinal;
                rs.writeX = x0; rs.writeY = y0; rs.writeWidth = w; rs.writeHeight = h;
                const bool fullResolved = x0==0&&y0==0&&w==texW&&h==texH;
                const bool isSdrProducerResolved = color_qualification::IsHostTextureQualified(color.sdrProducerFrame, frame) &&
                    color.qualifiedSdrWidth >= texW && color.qualifiedSdrHeight >= texH &&
                    destFormat == 6 && fullResolved;
                if (isSdrProducerResolved) {
                    color_qualification::MarkSurfaceResolved(rs.sdrWriteOrdinal, rs.writeOrdinal);
                } else {
                    color_qualification::InvalidateSurfaceResolved(rs.sdrWriteOrdinal);
                }
                WriteP2ResolveEvent("color", color, destBase, rs, resolveOperation);
#if defined(LO_GPU_PLUME)
                RecordFsrAlphaBridgeResolve(color, destBase, rs, resolveOperation);
#endif
                // A partial write can retain pixels from another plan/allocation.
                // It must not hand presentation a fabricated whole-surface plan.
                rs.sourcePlanValid = fullResolved&&activePlan.cpuSerial!=0;
                if (rs.sourcePlanValid) rs.sourcePlan = activePlan;
                if (taa_collection::Enabled())
                    rs.tex->bindingProducer.Copy(color.bindingProducer, taa_collection::ConsentEpoch(), frame,
                        x0 == 0 && y0 == 0 && w == texW && h == texH);
                auto sourceCoverage=color.aaProvenance.Get(frame,color.allocationSerial);
                if(sourceCoverage==scene_aa::Coverage::Full && (uint64_t(x0)+w>color.aaValidWidth || uint64_t(y0)+h>color.aaValidHeight))
                    sourceCoverage=scene_aa::Coverage::Mixed;
                rs.tex->aaProvenance.Resolve(frame,rs.tex->allocationSerial,sourceCoverage,
                    fullResolved);
                DumpResolveStep(*rs.tex, destBase, rs.writeOrdinal, rs.destFormat);
                return true;
            }

            void Resolve()
            {
                ScopedTimer timer{ tResolve, cpuTimingEnabled };
                nResolve++;
                consecutiveResolveCopies.BeginResolve();
                ResolveImpl();
                consecutiveResolveCopies.EndResolve();
            }

            void ResolveImpl()
            {
                uint32_t copyControl = Reg(REG_RB_COPY_CONTROL);
                uint32_t srcSelect = copyControl & 7;
                uint32_t destBase = Reg(REG_RB_COPY_DEST_BASE) & 0x1FFFFFFF;
                uint32_t destPitchReg = Reg(REG_RB_COPY_DEST_PITCH);
                uint32_t destPitch = destPitchReg & 0x3FFF;
                uint32_t destHeight = (destPitchReg >> 16) & 0x3FFF;
                uint32_t destInfo = Reg(REG_RB_COPY_DEST_INFO);
                uint32_t destFormat = (destInfo >> 7) & 0x3F;
                uint32_t destEndian = destInfo & 7;

                uint32_t surfaceInfo = Reg(REG_RB_SURFACE_INFO);
                uint32_t pitch = surfaceInfo & 0x3FFF;
                uint32_t scissorBr = Reg(REG_PA_SC_WINDOW_SCISSOR_BR);
                uint32_t scissorTl = Reg(REG_PA_SC_WINDOW_SCISSOR_TL);
                uint32_t rtHeight = GuessTargetHeight(pitch, (scissorBr >> 16) & 0x3FFF);
                if (pitch == 0 || destPitch == 0 || destHeight == 0 || destBase == 0)
                    return;

                uint32_t x0 = scissorTl & 0x3FFF, y0 = (scissorTl >> 16) & 0x3FFF;
                uint32_t x1 = std::min<uint32_t>(scissorBr & 0x3FFF, pitch), y1 = std::min<uint32_t>((scissorBr >> 16) & 0x3FFF, rtHeight);
                if (x1 <= x0 || y1 <= y0)
                    return;
                uint32_t copyWidth = std::min(x1 - x0, destPitch), copyHeight = std::min(y1 - y0, destHeight);

                bool depthCopy = srcSelect == 4;
                if (depthCopy)
                {
                    uint32_t depthInfo = Reg(REG_RB_DEPTH_INFO);
                    HostTexture* depthRt = GetRenderTarget(depthInfo & 0xFFF, (depthInfo >> 16) & 1, pitch, rtHeight, true);
                    if (depthRt && !resolveReadback)
                    {
                        if (!ResolveDepthOnGpu(*depthRt, destBase, destFormat | kDepthResolveTag, destPitch, destHeight, x0, y0, copyWidth, copyHeight))
                            return;
                        InvalidateRange(destBase, ((destPitch + 31) & ~31u) * copyHeight * 4);
                    }
                    else
                    {
                        static bool warned = false;
                        if (!warned) { LOG_WARNING("renderer: depth resolve readback not implemented"); warned = true; }
                    }
                    if (!Begin()) return;
                    if (copyControl & 0x200) ClearDepthTarget(pitch, rtHeight);
                    return;
                }
                if (resolveReadback && destFormat != 6 && destFormat != 7 && destFormat != 32)
                {
                    if (loggedFormats.insert(0x100 + destFormat).second)
                        LOG_WARNING("renderer: unsupported resolve destination format {}", destFormat);
                    return;
                }

                uint32_t colorInfo = Reg(REG_RB_COLOR_INFO + (srcSelect < 4 ? (srcSelect == 0 ? 0 : 2 + (srcSelect - 1)) : 0));
                {
                    const uint32_t traceFrame = TraceFrame();
                    static const uint32_t traceCount = getenv("LO_DRAW_TRACE_COUNT") ? strtoul(getenv("LO_DRAW_TRACE_COUNT"), nullptr, 10) : 1;
                    if (traceFrame && frame >= traceFrame && frame < traceFrame + traceCount)
                    {
                        const bool existed = renderTargets.count(RenderTargetKey{ colorInfo & 0xFFF, ColorClassOf((colorInfo >> 16) & 0xF), pitch, 0, false }) != 0;
                        LOG_INFO("renderer: resolve f{} src sel={} base={:#x} fmt={} pitch={} h={} existed={} -> {:#x} destfmt={} rect ({},{}) {}x{} destPitch={} destHeight={} clear={:#x}",
                            frame, srcSelect, colorInfo & 0xFFF, (colorInfo >> 16) & 0xF, pitch, rtHeight, existed, destBase, destFormat, x0, y0, copyWidth, copyHeight, destPitch, destHeight, copyControl & 0x300);
                    }
                }
                HostTexture* color = AcquireColorTarget(colorInfo & 0xFFF, (colorInfo >> 16) & 0xF, pitch, rtHeight, true);
                if (!color || !color->texture) return;
                if (!resolveReadback)
                {
                    if (!ResolveOnGpu(*color, destBase, destFormat, destPitch, destHeight, x0, y0, copyWidth, copyHeight))
                        return;
                    InvalidateRange(destBase, ((destPitch + 31) & ~31u) * copyHeight * (destFormat == 32 ? 8 : 4));
                }
                else
                {
                uint32_t hostBpp = 0;
                switch (color->format)
                {
                case RenderFormat::R8G8B8A8_UNORM: hostBpp = 4; break;
                case RenderFormat::R16G16B16A16_FLOAT: hostBpp = 8; break;
                case RenderFormat::R16G16_FLOAT: hostBpp = 4; break;
                case RenderFormat::R32_FLOAT: hostBpp = 4; break;
                case RenderFormat::R32G32_FLOAT: hostBpp = 8; break;
                default:
                    if (loggedFormats.insert(0x200 + uint32_t(color->format)).second)
                        LOG_WARNING("renderer: resolve from host format {} not implemented", uint32_t(color->format));
                    return;
                }

                // GPU -> readback buffer.
                uint32_t rowPitch = (copyWidth * hostBpp + 255) & ~255u;
                if (size_t(rowPitch) * copyHeight > kReadbackSize)
                    return;
                Transition(*color, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
                RenderBox box{ int32_t(x0), int32_t(y0), int32_t(x0 + copyWidth), int32_t(y0 + copyHeight), 0, 1 };
                commandList->copyTextureRegion(
                    RenderTextureCopyLocation::PlacedFootprint(readback.get(), color->format, copyWidth, copyHeight, 1, rowPitch / hostBpp, 0),
                    RenderTextureCopyLocation::Subresource(color->texture.get()), 0, 0, 0, &box);
                if (!Flush() || !WaitForGpu()) return;

                // Convert each pixel to four floats, then to the destination format, tiled.
                const uint8_t* src = static_cast<const uint8_t*>(readback->map());
                static uint32_t resolveLogs = 0;
                const uint32_t traceFrame = TraceFrame();
                static const uint32_t traceCount = getenv("LO_DRAW_TRACE_COUNT") ? strtoul(getenv("LO_DRAW_TRACE_COUNT"), nullptr, 10) : 1;
                if (resolveLogs++ < 12 || (destBase == 0x70f000 && (resolveLogs % 120) == 0) || (traceFrame && frame >= traceFrame && frame < traceFrame + traceCount))
                {
                    uint32_t nonZero = 0;
                    for (uint32_t y = 0; y < copyHeight; y += 8)
                        for (uint32_t x = 0; x < copyWidth; x += 8)
                        {
                            uint32_t v = 0; memcpy(&v, src + size_t(y) * rowPitch + size_t(x) * hostBpp, 4);
                            nonZero += v != 0;
                        }
                    LOG_INFO("renderer: resolve -> {:#x} fmt={} endian={} rect {},{} {}x{} destPitch={} host fmt={} sampled non-zero={} (draws so far {})",
                        destBase, destFormat, destEndian, x0, y0, copyWidth, copyHeight, destPitch, uint32_t(color->format), nonZero, drawsThisFrame);
                }
                uint8_t* dst = Phys(destBase);
                uint32_t pitchBlocks = (destPitch + 31) & ~31u;
                for (uint32_t y = 0; y < copyHeight; y++)
                {
                    const uint8_t* row = src + size_t(y) * rowPitch;
                    for (uint32_t x = 0; x < copyWidth; x++)
                    {
                        float px[4] = { 0, 0, 0, 1 };
                        const uint8_t* p = row + size_t(x) * hostBpp;
                        switch (color->format)
                        {
                        case RenderFormat::R8G8B8A8_UNORM:
                            for (int c = 0; c < 4; c++) px[c] = p[c] / 255.0f;
                            break;
                        case RenderFormat::R16G16B16A16_FLOAT:
                            for (int c = 0; c < 4; c++) { uint16_t h; memcpy(&h, p + c * 2, 2); px[c] = HalfToFloat(h); }
                            break;
                        case RenderFormat::R16G16_FLOAT:
                            for (int c = 0; c < 2; c++) { uint16_t h; memcpy(&h, p + c * 2, 2); px[c] = HalfToFloat(h); }
                            break;
                        case RenderFormat::R32_FLOAT:
                            memcpy(&px[0], p, 4);
                            break;
                        case RenderFormat::R32G32_FLOAT:
                            memcpy(&px[0], p, 8);
                            break;
                        default: break;
                        }
                        if (destFormat == 6)
                        {
                            uint32_t v = 0;
                            for (int c = 0; c < 4; c++)
                                v |= uint32_t(std::clamp(px[c], 0.0f, 1.0f) * 255.0f + 0.5f) << (c * 8);
                            uint32_t offset = video::TiledOffset2D(x, y, pitchBlocks, 2);
                            v = GpuSwap(v, destEndian);
                            memcpy(dst + offset, &v, 4);
                        }
                        else if (destFormat == 7) // k_2_10_10_10
                        {
                            uint32_t v = 0;
                            for (int c = 0; c < 3; c++)
                                v |= uint32_t(std::clamp(px[c], 0.0f, 1.0f) * 1023.0f + 0.5f) << (c * 10);
                            v |= uint32_t(std::clamp(px[3], 0.0f, 1.0f) * 3.0f + 0.5f) << 30;
                            uint32_t offset = video::TiledOffset2D(x, y, pitchBlocks, 2);
                            v = GpuSwap(v, destEndian);
                            memcpy(dst + offset, &v, 4);
                        }
                        else // k_16_16_16_16_FLOAT
                        {
                            uint32_t offset = video::TiledOffset2D(x, y, pitchBlocks, 3);
                            uint32_t w0 = uint32_t(FloatToHalf(px[0])) | (uint32_t(FloatToHalf(px[1])) << 16);
                            uint32_t w1 = uint32_t(FloatToHalf(px[2])) | (uint32_t(FloatToHalf(px[3])) << 16);
                            w0 = GpuSwap(w0, destEndian); w1 = GpuSwap(w1, destEndian);
                            memcpy(dst + offset, &w0, 4);
                            memcpy(dst + offset + 4, &w1, 4);
                        }
                    }
                }
                readback->unmap();
                InvalidateRange(destBase, pitchBlocks * copyHeight * (destFormat == 32 ? 8 : 4));
                }

                if (!Begin()) return;
                if (copyControl & 0x100)
                {
                    consecutiveResolveCopies.Invalidate();
                    uint32_t clear = Reg(REG_RB_COLOR_CLEAR);
                    RenderColor c(float((clear >> 16) & 0xFF) / 255.0f, float((clear >> 8) & 0xFF) / 255.0f, float(clear & 0xFF) / 255.0f, float(clear >> 24) / 255.0f);
                    Transition(*color, RenderTextureLayout::COLOR_WRITE, RenderBarrierStage::GRAPHICS);
                    commandList->setFramebuffer(GetFramebuffer(color, nullptr));
                    RenderRect rect{ int32_t(color->ScaleX(x0)), int32_t(color->ScaleY(y0)), int32_t(color->ScaleX(x1)), int32_t(color->ScaleY(y1)) };
                    commandList->clearColor(0, c, &rect, 1);
#if defined(LO_GPU_PLUME)
                    HandleFsrAlphaRgbWriter(*color, "resolve_color_clear");
#endif
                    if (taa_collection::Enabled())
                        color->bindingProducer.Clear(taa_collection::ConsentEpoch(), frame,
                            x0 == 0 && y0 == 0 && x1 == color->guestWidth && y1 == color->guestHeight);
                    color->aaProvenance.Invalidate(frame,color->allocationSerial,x0==0&&y0==0&&x1==color->guestWidth&&y1==color->guestHeight);
                    color->sdrProducerFrame = ~0ull;
                }
                if (copyControl & 0x200)
                    ClearDepthTarget(pitch, rtHeight);
            }

            void ClearDepthTarget(uint32_t pitch, uint32_t rtHeight)
            {
                consecutiveResolveCopies.Invalidate();
                uint32_t depthInfo = Reg(REG_RB_DEPTH_INFO);
                HostTexture* depth = GetRenderTarget(depthInfo & 0xFFF, (depthInfo >> 16) & 1, pitch, rtHeight, true);
                if (!depth || !depth->texture) return;
                Transition(*depth, RenderTextureLayout::DEPTH_WRITE, RenderBarrierStage::GRAPHICS);
                commandList->setFramebuffer(GetFramebuffer(nullptr, depth));
                uint32_t clear = Reg(REG_RB_DEPTH_CLEAR);
                commandList->clearDepthStencil(true, true, float(clear >> 8) / 16777215.0f, clear & 0xFF);
                if (taa_collection::Enabled()) depth->bindingProducer.Clear(taa_collection::ConsentEpoch(), frame, true);
            }

            void ClearMovieBars(uint32_t surfaceInfo, uint32_t colorInfo,
                                float x, float y, float width, float height, float safeLeft, float safeRight)
            {
                if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height) ||
                    !std::isfinite(safeLeft) || !std::isfinite(safeRight) || width <= 0.0f || height <= 0.0f)
                    return;

                const uint32_t pitch = surfaceInfo & 0x3FFF;
                if (!pitch)
                    return;
                if (!ApplyInternalResolution())
                    return;
                if (!Begin()) return;
                const uint32_t bottom = uint32_t(std::clamp(std::lround(y + height), 0l, 0x3FFFl));
                const uint32_t rtHeight = GuessTargetHeight(pitch, bottom);
                HostTexture* color = AcquireColorTarget(colorInfo & 0xFFF, (colorInfo >> 16) & 0xF, pitch, rtHeight);
                if (!color || !color->texture)
                    return;

                const auto scaleX = [&](float boundary) { return int32_t(gpu::movie_clear::ScaleBoundary(boundary, color->width, color->guestWidth)); };
                const auto scaleY = [&](float boundary) { return int32_t(gpu::movie_clear::ScaleBoundary(boundary, color->height, color->guestHeight)); };
                const int32_t targetLeft = std::clamp(scaleX(x), 0, int32_t(color->width));
                const int32_t targetRight = std::clamp(scaleX(x + width), targetLeft, int32_t(color->width));
                const int32_t targetTop = std::clamp(scaleY(y), 0, int32_t(color->height));
                const int32_t targetBottom = std::clamp(scaleY(y + height), targetTop, int32_t(color->height));
                const int32_t leftBarEnd = std::clamp(scaleX(safeLeft), targetLeft, targetRight);
                const int32_t rightBarStart = std::clamp(scaleX(safeRight), targetLeft, targetRight);
                if (targetBottom <= targetTop || (leftBarEnd <= targetLeft && rightBarStart >= targetRight))
                    return;

                std::array<RenderRect, 2> bars{};
                uint32_t count = 0;
                auto addBar = [&](int32_t left, int32_t right)
                {
                    if (right <= left)
                        return;
                    bars[count++] = { left, targetTop, right, targetBottom };
                };
                addBar(targetLeft, leftBarEnd);
                addBar(rightBarStart, targetRight);
                if (!count)
                    return;

                consecutiveResolveCopies.Invalidate();
                Transition(*color, RenderTextureLayout::COLOR_WRITE, RenderBarrierStage::GRAPHICS);
                commandList->setFramebuffer(GetFramebuffer(color, nullptr));
                commandList->clearColor(0, RenderColor(0.0f, 0.0f, 0.0f, 1.0f), bars.data(), count);
#if defined(LO_GPU_PLUME)
                HandleFsrAlphaRgbWriter(*color, "movie_bars_clear");
#endif
                if (taa_collection::Enabled())
                    color->bindingProducer.Clear(taa_collection::ConsentEpoch(), frame, false);
                color->aaProvenance.Invalidate(frame, color->allocationSerial, false);
                color->sdrProducerFrame = ~0ull;
            }
        };

        Renderer* g_renderer = nullptr;
    }

#if !defined(LO_RENDERER_P2_EMBEDDED_TEST)
    // The asset-free fixture instantiates Renderer directly. Exclude only game
    // entrypoints from that TU, not the production methods under test. COFF
    // exception funclets may otherwise retain unused game startup call graphs.
    bool Init()
    {
        if (g_renderer)
            return true;
        if (getenv("LO_NO_RENDERER"))
            return false;
        auto r = std::make_unique<Renderer>();
        if (!r->Init()) return false;
        g_renderer = r.release();
        return true;
    }

    void Shutdown()
    {
        WaitDebugCaptureArchive();
        if (g_renderer)
        {
            g_renderer->Flush();
            g_renderer->WaitForGpu();
            if (!g_renderer->fsrCapturePages.empty()) g_renderer->FinishFsrCapture();
            video::WaitForPresentGpu();
            video::DrainGpuForShutdown();
            // Open/failed batches never acquired a queue completion serial.
            for (auto& slot : g_renderer->gpuSlots)
#if defined(LO_RENDERER_P2_EMBEDDED_TEST)
                if (g_renderer->dlssController && slot.srUseId && !slot.submitted)
                    g_renderer->dlssController->OnBatchDiscarded(slot.srUseId);
#else
                if (g_renderer->temporalUpscaler && slot.srUseId && !slot.submitted)
                    g_renderer->temporalUpscaler->OnDiscarded(slot.srUseId);
#endif
            g_renderer->SavePipelineRecipes(true);
            delete g_renderer;
            g_renderer = nullptr;
        }
    }

    void Draw(const DrawInfo& info)
    {
        if (g_renderer)
            g_renderer->Draw(info);
    }

    void FinalizeDebugCapture(bool ok);
    void PrepareDebugCaptureFrame(uint32_t frontbuffer, uint32_t swap, present_capture::Ticket &ticket)
    {
        ticket = {};
        { std::lock_guard lock(captureMutex); UpdateCaptureArchive(); }
        if (!g_renderer || g_renderer->debugCaptureDir.empty()) return;
        g_renderer->captureAttemptFrame = g_renderer->frame;
        g_renderer->captureAttemptSwap = swap;
        g_renderer->capturePrepared = false;
        g_renderer->captureTicket = {};
        g_renderer->captureGuest = {};
        auto& r = *g_renderer;
        r.evaluatePagePrepared = bool(r.evaluatePage);
        bool ok = false;
        try
        {
            std::ofstream surfaces(std::filesystem::path(r.debugCaptureDir) / "temporal-surfaces.json");
            surfaces << "{\n  \"schema\": 1,\n  \"frame\": " << r.frame
                     << ",\n  \"frontbuffer_address\": " << (frontbuffer & 0x1FFFFFFF)
                     << ",\n  \"selection_verified\": false,\n  \"surfaces\": [";
            std::vector<std::pair<uint32_t, const Renderer::ResolvedSurface*>> ordered;
            for (const auto& [address, variants] : r.resolved)
                for (const auto& surface : variants)
                    if (surface.tex && surface.writeOrdinal) ordered.emplace_back(address, &surface);
            std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
                return std::pair{a.first, a.second->destFormat} < std::pair{b.first, b.second->destFormat};
            });
            bool first = true;
            for (const auto& [address, surface] : ordered)
            {
                if (!first) surfaces << ',';
                first = false;
                const auto& rs = *surface;
                surfaces << "\n    {\"address\":" << address << ",\"guest_format\":" << rs.destFormat
                         << ",\"host_format\":" << uint32_t(rs.tex->format)
                         << ",\"depth\":" << ((rs.destFormat & Renderer::kDepthResolveTag) ? "true" : "false")
                         << ",\"storage\":[" << rs.tex->width << ',' << rs.tex->height << ']'
                         << ",\"guest_storage\":[" << rs.tex->guestWidth << ',' << rs.tex->guestHeight << ']'
                         << ",\"resolution\":[" << rs.tex->resolutionSize.width << ',' << rs.tex->resolutionSize.height << ']'
                         << ",\"pitch\":" << rs.destPitch << ",\"last_write_frame\":" << rs.frame
                         << ",\"last_write_ordinal\":" << rs.writeOrdinal
                         << ",\"last_write_rect\":[" << rs.writeX << ',' << rs.writeY << ',' << rs.writeWidth << ',' << rs.writeHeight << ']'
                         << ",\"written_this_frame\":" << (rs.frame == r.frame ? "true" : "false") << '}';
            }
            surfaces << "\n  ]\n}\n";
            surfaces.close();
            if (surfaces.fail()) throw std::runtime_error("temporal surface metadata write failed");
            // Separate from raw resource provenance: this is the narrow selected
            // scene path, not a declaration that motion/history inputs are ready.
            const auto& scene = r.temporalScene;
            std::ofstream sceneFile(std::filesystem::path(r.debugCaptureDir) / "temporal-scene.json");
            sceneFile << "{\n  \"schema\":1,\n  \"renderer_frame\":" << r.frame
                      << ",\n  \"observation_frame\":" << scene.Frame()
                      << ",\n  \"candidate_ready\":" << (scene.Frame() == r.frame && scene.Ready() ? "true" : "false")
                      << ",\n  \"temporal_history_verified\":false,\n  \"camera_draws\":" << scene.Draws()
                      << ",\n  \"scene_copies\":" << scene.Copies()
                      << ",\n  \"rejection_code\":" << unsigned(scene.Reason())
                      << ",\n  \"depth_allocation\":" << scene.Anchor().depthAllocation
                      << ",\n  \"vp_u32\":[";
            for (size_t i = 0; i < 16; ++i) sceneFile << (i ? "," : "") << scene.Anchor().vpBits[i];
            const auto& viewport = scene.Anchor().viewport;
            sceneFile << "],\n  \"viewport\":[" << viewport.x << ',' << viewport.y << ',' << viewport.width << ',' << viewport.height
                      << "],\n  \"ndc_y_sign\":" << viewport.ndcYSign
                      << ",\n  \"half_pixel_ndc\":[" << std::setprecision(17) << viewport.halfPixelNdcX << ',' << viewport.halfPixelNdcY << ']';
            auto writeSceneResolve = [&](const char* name, const temporal::SceneResolve& value) {
                sceneFile << ",\n  \"" << name << "\":{\"frame\":" << value.frame << ",\"ordinal\":" << value.ordinal
                          << ",\"address\":" << value.address << ",\"format\":" << value.format
                          << ",\"width\":" << value.width << ",\"height\":" << value.height
                          << ",\"full_extent\":" << (value.fullExtent ? "true" : "false") << '}';
            };
            writeSceneResolve("depth", scene.Depth());
            writeSceneResolve("pre_ui_color", scene.Color());
            sceneFile << "\n}\n";
            sceneFile.close();
            if (sceneFile.fail()) throw std::runtime_error("temporal scene metadata write failed");
            present_capture::GuestImage guest;
            guest.address = frontbuffer & 0x1FFFFFFF;
            std::vector<uint32_t> pixels;
            uint32_t width = 0, height = 0;
            auto *resolved = r.NewestResolved(guest.address);
            if (resolved)
            {
                guest.sourceWriteFrame = resolved->frame;
                guest.sourceWriteOrdinal = resolved->writeOrdinal;
                if (resolved->sourcePlanValid)
                {
                    r.captureTicket.hasSourcePlan = true;
                    r.captureTicket.planWidth = resolved->sourcePlan.width;
                    r.captureTicket.planHeight = resolved->sourcePlan.height;
                    r.captureTicket.planOutputWidth = resolved->sourcePlan.output.width;
                    r.captureTicket.planOutputHeight = resolved->sourcePlan.output.height;
                    r.captureTicket.planUpscaler = uint32_t(resolved->sourcePlan.requestedUpscaler);
                    r.captureTicket.planQuality = uint32_t(resolved->sourcePlan.dlssQuality);
                    r.captureTicket.planCpuSerial = resolved->sourcePlan.cpuSerial;
                }
            }
            if (ReadbackResolvedSurface(frontbuffer, pixels, width, height) &&
                present_capture::WriteRgbaBmp(std::filesystem::path(r.debugCaptureDir) / "guest-frontbuffer.bmp", pixels, width, height))
            {
                guest.available = true;
                guest.reason = "readback";
                guest.width = width;
                guest.height = height;
            }
            else guest.reason = "guest_readback_failed";
            r.captureGuest = guest;
            r.debugTrace << fmt::format("end frame={} frontbuffer={:#x} size={}x{} submitted_draws={} guest_frontbuffer={}\n",
                r.frame, frontbuffer, width, height, r.drawsThisFrame, guest.available);
            r.debugTrace << fmt::format("drops mode={} shader={} pitch={} pipeline={} upload={} index={} scissor={} dummy_bindings={}\n",
                r.drops.mode, r.drops.shader, r.drops.pitch, r.drops.pipeline, r.drops.upload, r.drops.index, r.drops.scissor, r.dummyBindings);
            r.p2Evidence.close();
            if (r.p2Evidence.fail()) throw std::runtime_error("p2 evidence write failed");
            r.captureTicket.active = true;
            r.captureTicket.rendererFrame = r.captureAttemptFrame;
            r.captureTicket.swap = r.captureAttemptSwap;
            r.captureTicket.frontbuffer = guest.address;
            r.captureTicket.deviceEpoch = video::BackendDeviceState().deviceEpoch;
            r.capturePrepared = true;
            ok = true;
        }
        catch (const std::exception& e) { ok = false; LOG_ERROR("render capture finish: {}", e.what()); }
        if (r.capturePrepared)
        {
            ticket = r.captureTicket;
            return;
        }
        FinalizeDebugCapture(ok);
    }

    void FinalizeDebugCapture(bool ok)
    {
        if (!g_renderer) return;
        auto &r = *g_renderer;
        if (r.evaluatePage && !r.evaluatePageExported && !r.debugCaptureDir.empty()) {
            try {
                bool pending = false;
                for (const auto& e : r.evaluatePage->entries) pending |= e->checkedSubmit && !e->completed;
                if (pending) r.WaitForGpu();
                r.evaluatePageExportOk = r.evaluatePage->Export(r.debugCaptureDir);
            } catch (const std::exception&) { r.evaluatePageExportOk = false; }
            r.evaluatePageExported = true;
        }
        ok = ok && r.evaluatePageExportOk;
        r.evaluatePage.reset();
        r.evaluatePagePrepared = false;
        if (r.debugTrace.is_open()) r.debugTrace.close();
        if (r.p2Evidence.is_open()) r.p2Evidence.close();
        const uint32_t completedBefore = r.debugCaptureCompleted;
        bool frameOk = ok;
        auto close = present_capture::BeginCaptureClose(completedBefore, r.debugCaptureFrameCount, r.captureAttemptFrame, r.captureAttemptSwap, frameOk);
        LOG_INFO("render capture {}: {}", frameOk ? "saved" : "incomplete", FileSystem::PathUtf8(std::filesystem::path(r.debugCaptureDir)));
        try
        {
            std::ofstream manifest(r.debugCaptureRoot / "capture-info.txt");
            present_capture::WriteCaptureManifest(manifest, close, r.debugCaptureFirstFrame);
            manifest << "Frames are consecutive rendered frames. Capture readbacks may stall execution.\n"
                << "Each frame directory contains its own screenshot, register trace, resolves and metadata.\n"
                << "p2-oracle.jsonl is newline-delimited JSON for b4b4d54a7a2d6b96/cda578aef1724fdc draw and resolve provenance; it records no color-space conclusion.\n"
                << "Trigger this three-frame capture with F1 or LO_DEBUG_CAPTURE_SWAP=<target swap> at process start. LO_CAPTURE_REQUEST is a separate legacy single-frame trace.\n"
                << "Shaders are deduplicated in shaders/. runtime.log is flushed after the last captured frame.\n"
                << "Default omissions: draw-step previews, duplicate screenshot.ppm and duplicate depth .f32.\n"
                << "Use LO_DEBUG_CAPTURE_DRAW_STEPS=1 to include draw-step previews.\n";
            manifest.close();
            if (manifest.fail()) throw std::runtime_error("Cannot write capture-info.txt");
        }
        catch (const std::exception& e)
        {
            frameOk = false;
            close = present_capture::BeginCaptureClose(completedBefore, r.debugCaptureFrameCount, r.captureAttemptFrame, r.captureAttemptSwap, false);
            LOG_ERROR("render capture manifest: {}", e.what());
        }
        if (close.continueNext)
        {
            r.debugCaptureCompleted = close.completedFrames;
            r.debugTrace.clear();
            r.debugCaptureDir.clear();
            r.captureFrame = 0;
            std::lock_guard lock(captureMutex);
            capturePending = true;
            captureStatus = L"等待下一帧 / Waiting for frame " + std::to_wstring(close.completedFrames + 1) + L"/3";
            return;
        }
        // Snapshot the log owned by this process after frame export, before ZIP
        // creation. A disabled or unavailable log must not discard the capture.
        try
        {
            const auto directory = r.debugCaptureRoot;
            const auto error = os::logger::SnapshotFile(directory / "runtime.log");
            std::ofstream status(directory / "runtime-log-status.txt");
            present_capture::WriteRuntimeFrameStatus(status, close);
            if (error)
            {
                // This directory was created for this request; discard only a
                // possible partial snapshot from the failed copy.
                std::error_code cleanupError;
                std::filesystem::remove(directory / "runtime.log", cleanupError);
                status << "status=unavailable\nreason=" << error.message() << '\n';
                LOG_WARNING("render capture runtime log unavailable: {}", error.message());
            }
            else
            {
                status << "status=included\nfile=runtime.log\n"
                    "scope=Current process log file, flushed after frame export and before ZIP creation.\n";
            }
            status.close();
            if (status.fail()) LOG_WARNING("render capture: could not write runtime log status");
        }
        catch (const std::exception& e) { LOG_WARNING("render capture runtime log: {}", e.what()); }
        os::shaderlog::CaptureSnapshot(r.debugCaptureRoot, close.attemptFrame);
        // Detach the completed capture from the renderer before starting the
        // worker. Subsequent frames cannot append to or use its source files.
        const auto directory = std::move(r.debugCaptureRoot);
        auto shaderSources = std::move(r.debugShaderSources);
        r.debugTrace.close();
        r.debugTrace.clear();
        r.p2Evidence.close();
        r.p2Evidence.clear();
        r.debugCaptureDir.clear();
        r.debugCaptureRoot.clear();
        const bool publish = close.publish;
        const uint32_t completedFrames = close.completedFrames;
        r.debugCaptureCompleted = 0;
        r.captureFrame = 0;
        // Optional diagnostics use the existing consent and background uploader.
        // This signal never uploads the local archive, logs or paths.
        taa_collection::RequestUpload();
        std::lock_guard lock(captureMutex);
        if (ok || shaderSources)
        {
            try
            {
                captureArchive = os::StartCaptureArchive(directory, [shaderSources, publish](const std::filesystem::path& captureDirectory) {
                    if (shaderSources) shaderSources->Write(captureDirectory);
                    if (!publish) throw std::system_error(std::make_error_code(std::errc::io_error));
                });
                captureStatus = publish ? L"后台压缩，可继续游戏 / Compressing capture in background" :
                    L"后台保存不完整捕获 / Saving incomplete capture in background";
                LOG_INFO("render capture archive started in background: {} completed={} frame={} swap={} publish={}",
                    FileSystem::PathUtf8(directory), completedFrames, close.attemptFrame, close.attemptSwap, publish);
                return;
            }
            catch (const std::exception& e) { LOG_ERROR("render capture archive start: {}", e.what()); }
        }
        captureStatus = (ok ? L"归档失败，原始文件保留 / Archive failed: " :
            L"导出不完整 / Incomplete: ") + directory.wstring();
        captureBusy = false;
    }

    void CompleteDebugCaptureFrame(const present_capture::Result &presented)
    {
        if (!g_renderer || !g_renderer->capturePrepared) return;
        auto &r = *g_renderer;
        r.capturePrepared = false;
        present_capture::Result finalImage = presented;
        const auto &ticket = r.captureTicket;
        if (!finalImage.attempted || finalImage.rendererFrame != ticket.rendererFrame || finalImage.swap != ticket.swap)
        {
            finalImage.available = false;
            finalImage.pixels.clear();
            if (finalImage.reason.empty() || finalImage.reason == "swapchain_readback") finalImage.reason = "identity_mismatch";
        }
        const auto directory = std::filesystem::path(r.debugCaptureDir);
        const bool finalOk = present_capture::CommitFinalImage(finalImage, directory / "screenshot.bmp");
        if (r.evaluatePage && r.evaluatePagePrepared) {
            try {
                // Guest readback normally drained both slots already. Only a
                // capture whose ordinary readback did not do so needs this wait.
                bool pending = false;
                for (const auto& entry : r.evaluatePage->entries)
                    pending |= entry->checkedSubmit && !entry->completed;
                if (pending) r.WaitForGpu();
                r.evaluatePageExportOk = r.evaluatePage->Export(directory);
            } catch (const std::exception&) { r.evaluatePageExportOk = false; }
            r.evaluatePageExported = true;
        }
        const bool traceOpen = r.debugTrace.is_open();
        if (traceOpen)
        {
            present_capture::AppendFrameMetadata(r.debugTrace, ticket, r.captureGuest, finalImage);
            r.debugTrace.close();
        }
        const bool frameOk = r.captureGuest.available && finalOk && traceOpen && !r.debugTrace.fail() && r.evaluatePageExportOk;
        FinalizeDebugCapture(frameOk);
    }

    void PollDebugCapture()
    {
        if (g_renderer) {
            g_renderer->PollCaptureRequest();
            g_renderer->PollFsrCaptureRequest();
        }
    }

    void PreparePresent(uint32_t physicalAddress)
    {
        if (!g_renderer)
            return;
        if (g_renderer->PlanSuppressed())
            return;
        g_renderer->consecutiveResolveCopies.Invalidate();
        auto* rs = g_renderer->NewestResolved(physicalAddress & 0x1FFFFFFF);
        if (!rs || !rs->tex)
            return;
        if (!g_renderer->Begin()) return;
        g_renderer->Transition(*rs->tex, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
    }

    bool SuppressPresent()
    {
        return g_renderer && g_renderer->PlanSuppressed();
    }

    void Flush()
    {
        if (g_renderer)
        {
            static const bool stats = getenv("LO_GPU_STATS") != nullptr;
            static auto lastFrame = std::chrono::steady_clock::now();
            if (!g_renderer->Flush()) return;
            g_renderer->PublishDlssFrameOutcome();
            {
                auto& r = *g_renderer;
                if (r.motionOptions.log && r.frame % 120 == 0) {
                    const auto& st = r.drawTemporalTracker.Stats();
                    LOG_INFO("mv audit frame={} timing={} mv_track_cpu_ms={:.6f} draws={} ordered_duplicates={} snapshot_bytes={} scratch_bytes={} scope=current_frame_tracking_wall_and_copy_bytes",
                        r.frame,r.motionOptions.timing,r.mvTrackCpuMs,r.drawTemporalTracker.ActiveDrawCount(),
                        st.orderedDuplicateDraws,st.snapshotBytes,r.mvScratchBytes);
                    auto log = [&](const char* name,const temporal::GpuPassTimingStats& t) {
                        LOG_INFO("mv timestamp pass={} samples={} unavailable={} total_ms={:.6f} last_ms={:.6f} query_pools={} scope=lifetime_completed_queue_intervals_not_whole_frame negative_last=unavailable",
                            name,t.samples,t.unavailable,t.totalMilliseconds,t.lastMilliseconds,t.queryPoolAllocations);
                    };
                    if(r.motionReplay) {
                        log("replay_draw",r.motionReplay->DrawTiming()); log("validity_mask",r.motionReplay->MaskTiming());
                        LOG_INFO("mv resources pending={} pooled_batches={} batch_allocations={}",r.motionReplay->PendingCount(),r.motionReplay->BatchCount(),r.motionReplay->MaskBatchAllocations());
                    }
                    if(r.temporalHistory) {log("taa",r.temporalHistory->ResolveTiming());log("display",r.temporalHistory->DisplayTiming());}
                    if(r.hdrTemporalHistory) log("hdr_taa",r.hdrTemporalHistory->ResolveTiming());
                }
                r.mvTrackCpuMs = 0; r.mvScratchBytes = 0;
            }
            if ((stats || render_timing::Enabled()) && (render_timing::Enabled() || g_renderer->frame % 60 == 0))
                LOG_INFO("renderer resolve copies frame={} recorded={} skipped={} reuse={} scope=current_frame_consecutive_color_resolves",
                    g_renderer->frame, g_renderer->resolveCopiesRecorded, g_renderer->resolveCopiesSkipped, g_renderer->resolveCopyReuse);
            g_renderer->resolveCopiesRecorded = g_renderer->resolveCopiesSkipped = 0;
            if (g_renderer->vertexTimingEnabled)
            {
                auto& r = *g_renderer;
                const auto& v = r.vertexTiming;
                LOG_INFO("vertex timing frame={} calls={} uploads={} bytes={} find_ms={:.6f} match_ms={:.6f} erase_ms={:.6f} capture_ms={:.6f} copy_ms={:.6f} insert_ms={:.6f} find_max_ms={:.6f} match_max_ms={:.6f} capture_max_ms={:.6f} copy_max_ms={:.6f} insert_max_ms={:.6f} copy_max_address={:#x} copy_max_bytes={} capture_max_address={:#x} capture_max_bytes={} cache_before={} cache_after={} buckets_before={} buckets_after={} rehashes={} evictions={} arena0={} arena1={} scope=vertex_cache_cpu_wall_includes_scheduling",
                    r.frame, v.find.calls, v.copy.calls, v.copy.bytes,
                    v.find.totalMs, v.match.totalMs, v.erase.totalMs, v.capture.totalMs, v.copy.totalMs, v.insert.totalMs,
                    v.find.maxMs, v.match.maxMs, v.capture.maxMs, v.copy.maxMs, v.insert.maxMs,
                    v.copy.maxAddress, v.copy.maxBytes, v.capture.maxAddress, v.capture.maxBytes,
                    v.initialSize, r.vertexCache.size(), v.initialBuckets, r.vertexCache.bucket_count(), v.rehashes,
                    v.find.calls ? r.vertexCache.Evictions() - v.initialEvictions : 0,
                    r.gpuSlots[0].arenaOffset, r.gpuSlots[1].arenaOffset);
                r.vertexTiming = {};
            }
            if (g_renderer->cpuTimingEnabled) {
                auto& r = *g_renderer;
                if (render_timing::Enabled() || r.frame % 60 == 0)
                    LOG_INFO("render batch capacity frame={} limit={} descriptor_splits={} upload_splits={} arena_splits={} descriptor_hits={} descriptor_misses={} scope=current_frame_capacity_checks reasons_may_overlap=true",
                        r.frame, r.descriptorBatchLimit, r.descriptorSplits, r.uploadSplits, r.arenaSplits, r.descriptorHits, r.descriptorMisses);
                r.descriptorSplits = r.uploadSplits = r.arenaSplits = 0;
                r.descriptorHits = r.descriptorMisses = 0;
            }
            if (render_timing::Enabled() || g_renderer->frame % 60 == 0)
            {
                auto& r = *g_renderer;
                LOG_INFO("index cache frame={} hits={} misses={} entries={} bytes={} peak_bytes={} evictions={} scope=current_frame_index_conversion_cache",
                    r.frame, r.indexCacheHits, r.indexCacheMisses, r.indexCache.size(),
                    r.indexCache.AllocatedBytes(), r.indexCache.PeakBytes(), r.indexCache.Evictions());
                r.indexCacheHits = r.indexCacheMisses = 0;
            }
            if (render_timing::Enabled()) {
                Renderer& r = *g_renderer;
                const render_timing::CpuSegments cpu{r.drawsThisFrame, r.nShader, r.nPipeline, r.nTexture, r.nResolve,
                    r.tDraw, r.tConst, r.tSets, r.tVertex, r.tBind, r.tIndex, r.tRecord,
                    r.tShader, r.tPipeline, r.tTexture, r.tResolve, r.tFlush,
                    r.tRt, r.tTaa, r.tNestedFlush,
                    r.tShaderLookup, r.tPipelineLookup, r.tSceneCopy};
                render_timing::LogFrame(r.frame, cpu, r.gpuTiming, !r.debugCaptureDir.empty(),
                    r.resolveTraceRemaining || r.psTraceRemaining || GetHotCaptureEnvironment().geometryCaptureEnabled);
                r.gpuTiming.Reset();
                if (!stats) r.ResetTimers();
            }
            if (stats)
            {
                auto now = std::chrono::steady_clock::now();
                double frameMs = std::chrono::duration<double, std::milli>(now - lastFrame).count();
                lastFrame = now;
                Renderer& r = *g_renderer;
                if (frameMs > 150.0 || (r.frame % 60) == 0)
                    LOG_INFO("renderer frame {}: {:.0f} ms, draws {} ({:.0f} ms: const {:.0f} sets {:.0f} vertex {:.0f} bind {:.0f} index {:.0f} record {:.0f} rt {:.0f} taa {:.0f} nested_flush {:.0f} shader_lookup {:.0f} pipeline_lookup {:.0f} scene_copy {:.0f}), shaders {} ({:.0f} ms), pipelines {} ({:.0f} ms), textures {} ({:.0f} ms, {} KB), vertex uploads {}+{} ({} KB, arena {} MB), resolves {} ({:.0f} ms), gpu wait {:.0f} ms",
                        r.frame, frameMs, r.drawsThisFrame, r.tDraw, r.tConst, r.tSets, r.tVertex, r.tBind, r.tIndex, r.tRecord, r.tRt, r.tTaa, r.tNestedFlush, r.tShaderLookup, r.tPipelineLookup, r.tSceneCopy, r.nShader, r.tShader, r.nPipeline, r.tPipeline, r.nTexture, r.tTexture, r.texBytes / 1024,
                        r.vertexUploads, r.vertexRevalidations, r.vertexBytesUploaded / 1024, r.Gpu().arenaOffset >> 20, r.nResolve, r.tResolve, r.tFlush);
                if (stats && r.transfers)
                    LOG_INFO("renderer frame {}: {} EDRAM ownership transfers", r.frame, r.transfers);
                r.transfers = 0;
                if (stats && r.dummyBindings)
                    LOG_INFO("renderer frame {}: {} texture slots fell back to the dummy", r.frame, r.dummyBindings);
                r.dummyBindings = 0;
                if (stats && (r.skin.depthDraws || r.skin.colorDraws))
                    LOG_INFO("renderer frame {}: relative-constant draws - mode5 {} mode4 {} ({} indices), full-width 7e3 {} ({} indices)",
                        r.frame, r.skin.depthDraws, r.skin.colorDraws, r.skin.colorIndices, r.skin.sceneDraws, r.skin.sceneIndices);
                r.skin = {};
                if (r.frame % 60 == 0)
                {
                    for (const auto& [key, count] : r.relativeDraws)
                    {
                        const auto& [mode, colorInfo, pitch, height, vs, ps, mask, outputs] = key;
                        LOG_INFO("renderer frame {}: relative draws mode={} rt={:#x}/{} {}x{} vs={:016x} ps={:016x} mask={:#x} outputs={:#x} draws={} indices={}",
                            r.frame, mode, colorInfo & 0xFFF, (colorInfo >> 16) & 0xF, pitch, height,
                            vs, ps, mask, outputs, count.draws, count.indices);
                    }
                }
                r.relativeDraws.clear();
                if (stats && r.drops.Any())
                    LOG_INFO("renderer frame {}: dropped draws - mode {} (modes {:#x}) shader {} pitch {} pipeline {} upload {} index {} scissor {} (prims {:#x}); vertex fetch slots left unbound {}",
                        r.frame, r.drops.mode, r.drops.modeMask, r.drops.shader, r.drops.pitch, r.drops.pipeline, r.drops.upload, r.drops.index, r.drops.scissor, r.drops.primMask, r.drops.vfetchSkips);
                if (stats && r.textureReuploads)
                    LOG_INFO("renderer frame {}: {} textures re-uploaded after a guest write", r.frame, r.textureReuploads);
                r.textureReuploads = 0;
                r.ResetTimers();
                r.vertexUploads = r.vertexRevalidations = 0;
                r.vertexBytesUploaded = 0;
            }
            const bool temporalTraceActive = g_renderer->resolveTraceRemaining != 0;
            g_renderer->FinishResolveTraceFrame();
            auto& collectionRenderer=*g_renderer;
            if(collectionRenderer.collectionFrame!=collectionRenderer.frame){collectionRenderer.collectionFrame=collectionRenderer.frame;taa_collection::BeginDiagnosticsFrame(collectionRenderer.frame);}
            taa_collection::diagnostics::Frame collectionFrame;
            collectionFrame.taa=collectionRenderer.temporalExperiment;
            collectionFrame.ready=collectionRenderer.temporalScene.Frame()==collectionRenderer.frame&&collectionRenderer.temporalScene.Ready();
            if(collectionRenderer.temporalScene.Frame()==collectionRenderer.frame)
                collectionFrame.sceneRejection=uint32_t(collectionRenderer.temporalScene.Reason());
            collectionFrame.sparseReady=collectionRenderer.sparseCollector&&collectionRenderer.sparseCollector->Ready();
            if (auto& owner = g_renderer->temporalHistory; owner) {
                auto& r = *g_renderer;
                const auto now = std::chrono::steady_clock::now();
                const auto temporalEnd = temporal::EvaluateTemporalFrameEnd(*owner, now, r.frame,
                    r.temporalExperiment, r.temporalInputProbe, r.dlssSrRequested,
                    r.temporalScene.Frame() == r.frame && r.temporalScene.Ready(),
                    r.temporalSubmittedFrame == r.frame, r.temporalFrameTime, r.temporalGapResetFrame);
                if (temporalEnd.engaged) {
                if (r.evaluatePage && r.evaluatePage->frame == r.frame)
                    r.evaluatePage->resetAtFrameEnd = temporalEnd.reset;
                const bool gap = temporalEnd.gap;
                const bool complete = temporalEnd.complete;
                collectionFrame.completed=complete;
                collectionFrame.reused=complete&&owner->Reused();
                collectionFrame.resetAfterFrame=temporalEnd.reset||temporalEnd.gapAlreadyReset;
                const auto& collectionHistory=owner->Diagnostics();
                collectionFrame.historyCaptured=collectionHistory.captured&&collectionHistory.state.currentFrame==r.frame&&collectionHistory.state.currentEpoch==r.temporalEpoch;
                if(collectionFrame.historyCaptured) {
                    collectionFrame.historyRejection=collectionHistory.rejected;
                    collectionFrame.cameraChecks=collectionHistory.cameraChecksAvailable;
                    const auto& state=collectionHistory.state;
                    collectionFrame.sameEpoch=state.currentEpoch==state.previousEpoch;
                    if(state.currentFrame>=state.previousFrame&&state.currentFrame-state.previousFrame<=65535)
                        collectionFrame.previousFrameDelta=int32_t(state.currentFrame-state.previousFrame);
                }
                static const uint32_t temporalLogStart=getenv("LO_TEMPORAL_LOG_START_FRAME")?strtoul(getenv("LO_TEMPORAL_LOG_START_FRAME"),nullptr,10):0;
                static const bool temporalDetailsRequested=getenv("LO_TEMPORAL_LOG_START_FRAME")!=nullptr;
                static const bool withTrace = getenv("LO_TEMPORAL_DRAW_LOG_WITH_RESOLVE_TRACE") &&
                    strcmp(getenv("LO_TEMPORAL_DRAW_LOG_WITH_RESOLVE_TRACE"), "1") == 0;
                const bool logTemporalConsumer = r.temporalExperiment || r.temporalInputProbe || temporalDetailsRequested;
                if((logTemporalConsumer&&r.frame>=temporalLogStart&&r.temporalFramesLogged<256) || (withTrace&&temporalTraceActive)) {
                    SHADER_LOG_INFO("temporal", RendererByteFnv, "renderer temporal f{} epoch={} ready={} completed={} reused={} reason={} depth={} color={} gap={} jitter_draws={} jitter_misses={} jitter_unknowns={}",r.frame,r.temporalEpoch,r.temporalScene.Ready(),complete,owner->Reused(),uint32_t(r.temporalScene.Reason()),r.temporalScene.Depth().ordinal,r.temporalScene.Color().ordinal,gap,r.temporalJitterDraws,r.temporalJitterMisses,r.temporalJitterUnknowns);
                    const auto& diagnostic=owner->Diagnostics();
                    // Compact CPU inspection must not enable verbose renderer
                    // allocations/logging reserved for explicit local tracing.
                    if(diagnostic.captured&&(temporalDetailsRequested||(withTrace&&temporalTraceActive))) {
                        std::string reasons;
                        for(uint32_t bit=1;bit<=(1u<<9);bit<<=1)if(diagnostic.rejected&bit) {
                            if(!reasons.empty())reasons+='|';
                            reasons+=temporal::HistoryReuseRejectionName(temporal::HistoryReuseRejection(bit));
                        }
                        const auto& state=diagnostic.state;
                        SHADER_LOG_INFO("temporal", RendererByteFnv, "renderer temporal gates f{} mask={:#x} reasons={} valid={} previous_completed={} stable={}/{} allow_history={} frames={}/{} epochs={}/{} allocations={}/{} camera_checks={}",
                            r.frame,diagnostic.rejected,reasons.empty()?"none":reasons,state.valid,state.previousCompleted,state.currentStable,state.previousStable,state.allowHistory,
                            state.currentFrame,state.previousFrame,state.currentEpoch,state.previousEpoch,state.currentAllocation,state.previousAllocation,diagnostic.cameraChecksAvailable);
                        auto logCamera=[&](const char* which,const std::optional<temporal::Camera>& camera) {
                            if(!camera) {SHADER_LOG_INFO("temporal", RendererByteFnv, "renderer temporal camera f{} {} missing",r.frame,which);return;}
                            std::string vp;for(double value:camera->VP())vp+=fmt::format("{:.9g},",value);
                            const auto& v=camera->Raster();
                            SHADER_LOG_INFO("temporal", RendererByteFnv, "renderer temporal camera f{} {} raster=({:.17g},{:.17g},{:.17g},{:.17g}) ndc_y={:.17g} half_pixel=({:.17g},{:.17g}) vp=[{}]",
                                r.frame,which,v.x,v.y,v.width,v.height,v.ndcYSign,v.halfPixelNdcX,v.halfPixelNdcY,vp);
                        };
                        logCamera("current",diagnostic.currentCamera);logCamera("previous",diagnostic.previousCamera);
                        if(diagnostic.cameraChecksAvailable)
                            SHADER_LOG_INFO("temporal", RendererByteFnv, "renderer temporal depth_range f{} valid={} lower_bound={:.17g} far_world_w={:.17g} near_world_w={:.17g}",
                                r.frame,diagnostic.depthRange.valid,diagnostic.depthRange.lowerBound,diagnostic.depthRange.farWorldW,diagnostic.depthRange.nearWorldW);
                        if(diagnostic.cameraChecksAvailable&&diagnostic.depthRange.valid)for(const auto& probe:diagnostic.probes)
                            SHADER_LOG_INFO("temporal", RendererByteFnv, "renderer temporal probe f{} depth={:.9g} rejection={} projected_valid={} projected=({:.17g},{:.17g},{:.17g}) delta_fraction=({:.17g},{:.17g}) quarter_screen_rejected={}",
                                r.frame,probe.depth,uint32_t(probe.rejection),probe.projectedValid,probe.projected.x,probe.projected.y,probe.projected.depth,probe.deltaXFraction,probe.deltaYFraction,probe.quarterScreenRejected);
                    }
                    ++r.temporalFramesLogged;
                }
                temporal::CommitTemporalFrameEnd(*owner, temporalEnd, now, r.temporalSupportedFrame, r.temporalEpoch, r.temporalFrameTime);
                r.temporalJitterDraws=r.temporalJitterMisses=r.temporalJitterUnknowns=0;
                }
            }
            if (g_renderer->evaluatePage && g_renderer->evaluatePage->frame == g_renderer->frame) {
                auto& r = *g_renderer;
                auto& page = *r.evaluatePage;
                page.temporalEpochEnd = r.temporalEpoch;
                page.framePlan = r.activePlan;
                page.gapResetBeforeInputs |= r.temporalGapResetFrame == r.frame;
                if (!page.evaluates && page.fallbackReason == "no_eligible_scene") {
                    if (r.dlssFrame.hasFallback && r.dlssFrame.fallbackFrame == r.frame)
                        page.fallbackReason = dlss::capture::FrameFallbackName(r.dlssFrame.fallback);
                    else if (r.activePlan.requestedUpscaler == upscaling::Upscaler::Off) page.fallbackReason = "upscaling_off";
                    else if (r.activePlan.inputProbe) page.fallbackReason = "input_probe_only";
                    else if (r.activePlan.consumer == upscaling::TemporalConsumer::FsrSr)
                        page.fallbackReason = "fsr_sr_no_dispatch_this_frame";
                    else if (r.activePlan.consumer != upscaling::TemporalConsumer::DlssSr)
                        page.fallbackReason = "dlss_sr_plan_unavailable_this_frame";
                    else if (!page.entries.empty() && !page.entries.back()->reason.empty())
                        page.fallbackReason = page.entries.back()->reason;
                }
                page.closed = true;
            }
            taa_collection::EndDiagnosticsFrame(collectionRenderer.frame,collectionFrame);
            g_renderer->SavePipelineRecipes();
            if (stats && g_renderer->frame % 600 == 0)
                LOG_INFO("renderer: pipeline reuse frame {}: {} prepared hits, {}/{} prepared keys used, {} runtime creates, {} recipes",
                    g_renderer->frame, g_renderer->preparedPipelineHits, g_renderer->usedPreparedPipelineKeys.size(),
                    g_renderer->preparedPipelineKeys.size(), g_renderer->runtimePipelineCreates, g_renderer->pipelineRecipes.size());
            g_renderer->PollPsTraceRequest();
            g_renderer->frame++;
            g_renderer->drawsThisFrame = 0;
            g_renderer->drops = {};
        }
    }

    void InvalidateGuestRange(uint32_t physicalAddress, uint32_t size)
    {
        if (g_renderer)
        {
            g_renderer->consecutiveResolveCopies.Invalidate();
            g_renderer->InvalidateRange(physicalAddress, size);
        }
    }

    plume::RenderTexture* AcquireResolvedSurface(uint32_t physicalAddress, uint32_t& width, uint32_t& height, uint32_t& format,
        frame_plan::FramePlan* sourcePlan)
    {
        if (!g_renderer)
            return nullptr;
        if (g_renderer->PlanSuppressed())
            return nullptr;
        g_renderer->consecutiveResolveCopies.Invalidate();
        auto* rs = g_renderer->NewestResolved(physicalAddress & 0x1FFFFFFF);
        if (!rs)
            return nullptr;
        HostTexture& tex = *rs->tex;
        width = tex.width;
        height = tex.height;
        format = uint32_t(tex.format);
        if (sourcePlan)
            *sourcePlan = rs->sourcePlanValid ? rs->sourcePlan : frame_plan::FramePlan{};
        return tex.texture.get();
    }

    bool SceneAAApplied(uint32_t physicalAddress)
    {
        if(!g_renderer)return false;
        const auto* surface=g_renderer->NewestResolved(physicalAddress);
        if(!surface||!surface->tex||surface->frame+1!=g_renderer->frame)return false;
        auto coverage=surface->tex->aaProvenance.Get(surface->frame,surface->tex->allocationSerial);
        static const uint64_t start=getenv("LO_SCENE_AA_LOG_START_FRAME")?strtoull(getenv("LO_SCENE_AA_LOG_START_FRAME"),nullptr,10):~0ull;
        if(surface->frame>=start&&surface->frame-start<128)SHADER_LOG_INFO("scene-aa", None, "renderer scene AA present f{} address={:#x} coverage={} skip_final={}",surface->frame,physicalAddress,uint32_t(coverage),scene_aa::SkipFinalAA(coverage));
        return scene_aa::SkipFinalAA(coverage);
    }

    void ScaleResolvedSize(uint32_t physicalAddress, uint32_t& width, uint32_t& height)
    {
        if (!g_renderer) return;
        const auto* surface = g_renderer->NewestResolved(physicalAddress & 0x1FFFFFFF);
        if (!surface || !surface->tex) return;
        width = surface->tex->ScaleX(width);
        height = surface->tex->ScaleY(height);
    }

    std::vector<uint32_t> GetResolvedAddresses()
    {
        std::vector<uint32_t> out;
        if (g_renderer)
            for (auto& [address, surfaces] : g_renderer->resolved)
                for (auto& surface : surfaces)
                    if (surface.tex) { out.push_back(address); break; }
        return out;
    }

    static bool ReadbackTexture(HostTexture& tex, std::vector<uint32_t>& pixels, uint32_t& width, uint32_t& height)
    {
        uint32_t bpp = 0;
        RenderFormat copyFormat = tex.format;
        switch (tex.format)
        {
        case RenderFormat::R8G8B8A8_UNORM: bpp = 4; break;
        case RenderFormat::R16G16B16A16_FLOAT: bpp = 8; break;
        case RenderFormat::D32_FLOAT_S8_UINT: bpp = 4; copyFormat = RenderFormat::R32_FLOAT; break; // depth plane only
        case RenderFormat::R32_FLOAT: bpp = 4; break;
        default: return false;
        }
        const uint32_t rowPitch = (tex.width * bpp + 255) & ~255u;
        if (size_t(rowPitch) * tex.height > kReadbackSize)
            return false;
        if (!g_renderer->Begin()) return false;
        g_renderer->Transition(tex, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
        g_renderer->commandList->copyTextureRegion(
            RenderTextureCopyLocation::PlacedFootprint(g_renderer->readback.get(), copyFormat, tex.width, tex.height, 1, rowPitch / bpp, 0),
            RenderTextureCopyLocation::Subresource(tex.texture.get(), 0));
        if (!g_renderer->Flush() || !g_renderer->WaitForGpu()) return false;
        const uint8_t* src = static_cast<const uint8_t*>(g_renderer->readback->map());
        width = tex.width;
        height = tex.height;
        pixels.resize(size_t(width) * height);
        for (uint32_t y = 0; y < height; y++)
        {
            const uint8_t* row = src + size_t(y) * rowPitch;
            for (uint32_t x = 0; x < width; x++)
            {
                if (tex.format == RenderFormat::D32_FLOAT_S8_UINT || tex.format == RenderFormat::R32_FLOAT)
                {
                    float d; memcpy(&d, row + size_t(x) * 4, 4);
                    uint32_t g = uint32_t(std::clamp(d, 0.0f, 1.0f) * 255.0f + 0.5f);
                    pixels[size_t(y) * width + x] = g | (g << 8) | (g << 16) | 0xFF000000u;
                }
                else if (bpp == 4)
                    memcpy(&pixels[size_t(y) * width + x], row + size_t(x) * 4, 4);
                else
                {
                    uint32_t v = 0;
                    for (int c = 0; c < 4; c++)
                    {
                        uint16_t hf; memcpy(&hf, row + size_t(x) * 8 + c * 2, 2);
                        v |= uint32_t(std::clamp(HalfToFloat(hf), 0.0f, 1.0f) * 255.0f + 0.5f) << (c * 8);
                    }
                    pixels[size_t(y) * width + x] = v;
                }
            }
        }
        g_renderer->readback->unmap();
        return true;
    }

    bool ReadbackResolvedSurface(uint32_t physicalAddress, std::vector<uint32_t>& pixels, uint32_t& width, uint32_t& height)
    {
        if (!g_renderer)
            return false;
        auto* rs = g_renderer->NewestResolved(physicalAddress & 0x1FFFFFFF);
        if (!rs)
            return false;
        return ReadbackTexture(*rs->tex, pixels, width, height);
    }

    void DumpRenderTargets(const char* prefix)
    {
        if (!g_renderer)
            return;
        std::vector<uint32_t> pixels;
        for (auto& [key, tex] : g_renderer->renderTargets)
        {
            if (!tex)
                continue;
            uint32_t w = 0, h = 0;
            if (!ReadbackTexture(*tex, pixels, w, h))
                continue;
            std::string path = fmt::format("{}_{}_{:x}_{}_{}x{}.ppm", prefix, key.depth ? "depth" : "rt", key.base, key.format, w, h);
            FILE* f = fopen(path.c_str(), "wb");
            if (!f)
                continue;
            fprintf(f, "P6%c%u %u%c255%c", 10, w, h, 10, 10);
            std::vector<uint8_t> row(size_t(w) * 3);
            for (uint32_t y = 0; y < h; y++)
            {
                for (uint32_t x = 0; x < w; x++)
                {
                    uint32_t p = pixels[size_t(y) * w + x];
                    row[x * 3 + 0] = uint8_t(p); row[x * 3 + 1] = uint8_t(p >> 8); row[x * 3 + 2] = uint8_t(p >> 16);
                }
                fwrite(row.data(), 1, row.size(), f);
            }
            fclose(f);
        }
    }
#if defined(LO_RENDERER_P2_SELFTEST)
    int RunSceneCopyPromotionSelfTest(const std::filesystem::path& evidenceDirectory)
    {
        const auto cacheDirectory = evidenceDirectory / "renderer-p2-cache";
        std::error_code error;
        std::filesystem::create_directories(cacheDirectory, error);
        if (error)
            return 1;
#ifdef _WIN32
        _putenv_s("LO_BACKGROUND", "1");
        _putenv_s("LO_GRAPHICS_API", "vulkan");
        _putenv_s("LO_NO_SHADER_PREPARE", "1");
        _putenv_s("LO_NO_PORTABLE_SHADER_PACK", "1");
        _putenv_s("LO_NO_PIPELINE_CACHE", "1");
        _putenv_s("LO_SHADER_CACHE_DIR", FileSystem::PathUtf8(cacheDirectory).c_str());
        _putenv_s("LO_HEADLESS", "");
        _putenv_s("LO_NO_RENDERER", "");
#else
        setenv("LO_BACKGROUND", "1", 1); setenv("LO_GRAPHICS_API", "vulkan", 1);
        setenv("LO_NO_SHADER_PREPARE", "1", 1); setenv("LO_NO_PORTABLE_SHADER_PACK", "1", 1);
        setenv("LO_NO_PIPELINE_CACHE", "1", 1); setenv("LO_SHADER_CACHE_DIR", cacheDirectory.c_str(), 1);
        unsetenv("LO_HEADLESS"); unsetenv("LO_NO_RENDERER");
#endif
        auto preview = settings::GetConfig();
        preview.graphicsBackend = backend::Backend::Vulkan;
        preview.skipShaderPrebuild = true;
        settings::PreviewConfig(preview);
        const bool initialized = video::Init();
        const bool ready = initialized && video::IsVulkan() && video::GetDevice() && g_renderer &&
            g_renderer->device && g_renderer->queue && g_renderer->commandList;
        bool resamplePassed = false;
        uint32_t readbackWidth = 0, readbackHeight = 0, mismatches = 0;
        if (ready)
        {
            // This is deliberately limited to the production resample draw. It
            // neither installs a render-target mapping nor calls NGX.
            constexpr uint32_t sourceWidth = 4, sourceHeight = 4;
            constexpr uint32_t destinationWidth = 8, destinationHeight = 8;
            const std::array<std::array<uint8_t, 4>, 4> quadrants = {{
                {{255, 0, 0, 64}}, {{0, 255, 0, 128}},
                {{0, 0, 255, 192}}, {{255, 255, 0, 255}},
            }};
            std::array<uint8_t, sourceWidth * sourceHeight * 4> sourcePixels{};
            for (uint32_t y = 0; y < sourceHeight; ++y)
                for (uint32_t x = 0; x < sourceWidth; ++x)
                {
                    const auto& color = quadrants[(y >= sourceHeight / 2 ? 2 : 0) + (x >= sourceWidth / 2 ? 1 : 0)];
                    std::memcpy(sourcePixels.data() + (size_t(y) * sourceWidth + x) * 4, color.data(), color.size());
                }

            HostTexture source{}, destination{};
            // A failed upload, descriptor allocation, or pipeline creation can
            // leave this list referencing the local textures. Drain it before
            // their destructors run and before video tears down the device.
            struct FixtureGpuDrain {
                Renderer& renderer;
                ~FixtureGpuDrain() { renderer.Flush(); renderer.WaitForGpu(); video::DrainGpuForShutdown(); }
            } drain{*g_renderer};
            source.allocationSerial = 1;
            source.format = RenderFormat::R8G8B8A8_UNORM;
            source.width = source.guestWidth = sourceWidth;
            source.height = source.guestHeight = sourceHeight;
            source.resolutionSize = {sourceWidth, sourceHeight};
            source.texture = g_renderer->device->createTexture(RenderTextureDesc::Texture2D(sourceWidth, sourceHeight, 1,
                source.format));
            destination.allocationSerial = 2;
            destination.format = RenderFormat::R8G8B8A8_UNORM;
            destination.width = destination.guestWidth = destinationWidth;
            destination.height = destination.guestHeight = destinationHeight;
            destination.resolutionSize = {destinationWidth, destinationHeight};
            destination.texture = g_renderer->device->createTexture(RenderTextureDesc::Texture2D(destinationWidth, destinationHeight, 1,
                destination.format, RenderTextureFlag::RENDER_TARGET));

            if (source.texture && destination.texture)
            {
                auto& r = *g_renderer;
                const uint64_t sourceOffset = r.Begin() ? r.Upload(sourcePixels.data(), sourcePixels.size(), 512) : UINT64_MAX;
                if (sourceOffset != UINT64_MAX)
                {
                    r.Transition(source, RenderTextureLayout::COPY_DEST, RenderBarrierStage::COPY);
                    r.commandList->copyTextureRegion(RenderTextureCopyLocation::Subresource(source.texture.get()),
                        RenderTextureCopyLocation::PlacedFootprint(r.uploadRing, source.format, sourceWidth, sourceHeight, 1, sourceWidth, sourceOffset));
                    r.Transition(source, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
                    Renderer::SharedConstants constants{};
                    constants.transfer[0] = std::bit_cast<uint32_t>(float(sourceWidth) / destinationWidth);
                    constants.transfer[1] = std::bit_cast<uint32_t>(float(sourceHeight) / destinationHeight);
                    const uint64_t constantsOffset = r.Upload(&constants, sizeof(constants));
                    if (constantsOffset != UINT64_MAX)
                    {
                        auto* set = r.AcquireSet(1);
                        set->setTexture(0, source.texture.get(), RenderTextureLayout::SHADER_READ);
                        set->setTexture(1, r.dummyTexture2D.texture.get(), RenderTextureLayout::SHADER_READ);
                        if (r.DrawPromotionResample(destination, set, constantsOffset, false))
                        {
                            const bool submitted = r.Flush() && r.WaitForGpu();
                            std::vector<uint32_t> pixels;
                            if (submitted && ReadbackTexture(destination, pixels, readbackWidth, readbackHeight) &&
                                readbackWidth == destinationWidth && readbackHeight == destinationHeight &&
                                pixels.size() == size_t(destinationWidth) * destinationHeight)
                            {
                                for (uint32_t y = 0; y < destinationHeight; ++y)
                                    for (uint32_t x = 0; x < destinationWidth; ++x)
                                    {
                                        const auto& expected = quadrants[(y >= destinationHeight / 2 ? 2 : 0) + (x >= destinationWidth / 2 ? 1 : 0)];
                                        const uint32_t pixel = pixels[size_t(y) * destinationWidth + x];
                                        if (uint8_t(pixel) != expected[0] || uint8_t(pixel >> 8) != expected[1] ||
                                            uint8_t(pixel >> 16) != expected[2] || std::abs(int(uint8_t(pixel >> 24)) - int(expected[3])) > 1)
                                            ++mismatches;
                                    }
                                resamplePassed = mismatches == 0;
                            }
                        }
                    }
                }
            }
        }
        video::Shutdown();
        std::ofstream evidence(evidenceDirectory / "native-dlss-p2-resample.json", std::ios::app);
        evidence << "{\"schema\":\"lostodyssey.p2-promotion-resample.v1\",\"bootstrap\":" << (ready ? "true" : "false")
            << ",\"resample_tested\":" << (ready ? "true" : "false")
            << ",\"promotion_mapping_tested\":false,\"source_extent\":[4,4],\"destination_extent\":["
            << readbackWidth << ',' << readbackHeight << "],\"mismatches\":" << mismatches
            << ",\"result\":\"" << (!ready ? "unsupported_or_init_failed" : resamplePassed ? "pass" : "fail") << "\"}\n";
        std::fprintf(stdout, "scene-copy promotion resample %s; mapping untested\n", resamplePassed ? "PASS" : "FAIL");
        return !ready ? 77 : resamplePassed ? 0 : 1;
    }
#endif
#endif // !LO_RENDERER_P2_EMBEDDED_TEST
#else
    void PrepareDebugCaptureFrame(uint32_t, uint32_t, present_capture::Ticket &ticket) { ticket = {}; }
    void CompleteDebugCaptureFrame(const present_capture::Result &) {}
    void PollDebugCapture() {}
    bool Init() { return false; }
    void Shutdown() {}
    void ScaleResolvedSize(uint32_t, uint32_t&, uint32_t&) {}
    void Draw(const DrawInfo&) {}
    void Flush() {}
    void PreparePresent(uint32_t) {}
    bool SuppressPresent() { return false; }
    void InvalidateGuestRange(uint32_t, uint32_t) {}
    bool SceneAAApplied(uint32_t) { return false; }
    plume::RenderTexture* AcquireResolvedSurface(uint32_t, uint32_t&, uint32_t&, uint32_t&, frame_plan::FramePlan*) { return nullptr; }
    bool ReadbackResolvedSurface(uint32_t, std::vector<uint32_t>&, uint32_t&, uint32_t&) { return false; }
    std::vector<uint32_t> GetResolvedAddresses() { return {}; }
    void DumpRenderTargets(const char*) {}
#endif
#if !defined(LO_RENDERER_P2_EMBEDDED_TEST)
    void SetOutputSize(uint32_t width, uint32_t height)
    {
        frame_plan::PublishDrawable(width, height);
    }

    float ActiveOutputAspect()
    {
        const auto plan = frame_plan::CurrentProducerPlan();
        if (!plan) return 16.0f / 9.0f;
        const auto& output = plan->output;
        if (output.width && output.height) return float(output.width) / output.height;
        if (output.drawable.width && output.drawable.height) return float(output.drawable.width) / output.drawable.height;
        return 16.0f / 9.0f;
    }

    void SelectFramePlan(const frame_plan::FramePlan& plan)
    {
        if (!plan.width || !plan.height) return;
        std::lock_guard lock(framePlanMutex);
        committedPlan = plan;
    }

    void RegisterCatalogSurface(frame_plan::SurfaceRole role, uint32_t surfaceInfo, uint32_t colorInfo)
    {
        if (!surfaceInfo) return;
        std::lock_guard lock(catalogMutex);
        const uint64_t key = CatalogKey(surfaceInfo, colorInfo);
        const auto found = catalogRoles.find(key);
        if (role == frame_plan::SurfaceRole::Unknown) {
            if (found != catalogRoles.end()) catalogRoles.erase(found);
            return;
        }
        if (found != catalogRoles.end() && found->second != role)
            LOG_WARNING("renderer: catalog role conflict base={:#x} pitch={} old={} new={}", colorInfo & 0xFFF, surfaceInfo & 0x3FFF,
                uint32_t(found->second), uint32_t(role));
        catalogRoles[key] = role;
    }

    void ClearMovieBars(uint32_t surfaceInfo, uint32_t colorInfo,
                        float x, float y, float width, float height, float safeLeft, float safeRight)
    {
#ifdef LO_GPU_PLUME
        if (g_renderer)
            g_renderer->ClearMovieBars(surfaceInfo, colorInfo, x, y, width, height, safeLeft, safeRight);
#else
        (void)surfaceInfo; (void)colorInfo; (void)x; (void)y;
        (void)width; (void)height; (void)safeLeft; (void)safeRight;
#endif
    }
#endif // !LO_RENDERER_P2_EMBEDDED_TEST
}
