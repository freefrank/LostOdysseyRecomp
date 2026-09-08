#include "scene_aa_provenance.h"
#include <stdafx.h>
#include "renderer.h"
#include "render_resolution.h"
#include "video.h"
#include "command_processor.h"
#include "depth_format.h"
#include "depth_clear_layout.h"
#include "polygon_offset.h"
#include "pipeline_cache.h"
#include "texture_layout.h"
#include "temporal_scene.h"
#include "temporal_jitter.h"
#include "temporal_history.h"
#include "presentation.h"
#include <settings/config.h>
#include "shader/xenos_translator.h"
#include "shader/dxc_compiler.h"
#include "shader/cache.h"
#include "shader/binary_cache.h"
#include "shader/preparation_queue.h"
#include "shader/startup_cache.h"
#include "shader/resource_scan.h"
#include "shader/resource_xex.h"
#include "shader/resource_variants.h"
#include <kernel/io/file_system.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <os/log_file.h>
#include <os/capture_archive.h>
#include <version.h>

#ifdef LO_GPU_PLUME
#include <plume_render_interface.h>
#include <plume_render_interface_builders.h>
#endif

#include <algorithm>
#include <atomic>
#include <thread>
#include <cmath>
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

namespace gpu::renderer
{
    namespace {
        std::atomic<uint64_t> outputSize{(uint64_t(1280) << 32) | 720};
        std::mutex captureMutex;
        bool captureBusy = false, capturePending = false;
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
                LOG_INFO("render capture ZIP saved: {}", FileSystem::PathUtf8(result.archive));
                captureStatus = L"ZIP 已保存 / ZIP saved: " + result.archive.wstring();
                if (result.cleanupError)
                {
                    LOG_WARNING("render capture source cleanup failed: {}: {}",
                        FileSystem::PathUtf8(result.directory), result.cleanupError.message());
                    captureStatus += L" (原始目录清理失败 / Source cleanup failed)";
                }
            }
            else
            {
                LOG_WARNING("render capture ZIP failed: {}: {}",
                    FileSystem::PathUtf8(result.directory), result.error.message());
                captureStatus = L"ZIP 失败，原始文件保留 / ZIP failed: " + result.directory.wstring();
            }
            captureBusy = false;
        }
    }
    void RequestDebugCapture()
    {
        std::lock_guard lock(captureMutex);
        UpdateCaptureArchive();
        if (captureBusy) return;
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
        constexpr uint32_t kVertexArenaSize = 256u << 20;   // persistent, byte-swapped copies of guest vertex buffers
        constexpr uint32_t kUploadHeadroom = 24u << 20;     // per-draw slack checked before a draw records anything
        constexpr uint32_t kArenaHeadroom = 32u << 20;
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

        // Upload heaps are write-combined: never read them back. Swap on the way in.
        void CopySwapped(void* dst, const void* src, size_t dwords, uint32_t endian)
        {
            if ((endian & 3) == 0)
            {
                memcpy(dst, src, dwords * 4);
                return;
            }
            const uint32_t* s = static_cast<const uint32_t*>(src);
            uint32_t* d = static_cast<uint32_t*>(dst);
            for (size_t i = 0; i < dwords; i++)
                d[i] = GpuSwap(s[i], endian);
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
            scene_aa::Provenance aaProvenance;
            uint32_t aaValidWidth=0,aaValidHeight=0; // Proven scene domain; allocation may contain padding.
            std::unique_ptr<RenderTexture> texture;
            RenderTextureLayout layout = RenderTextureLayout::UNKNOWN;
            RenderFormat format = RenderFormat::UNKNOWN;
            uint32_t width = 0, height = 0;
            uint32_t guestWidth = 0, guestHeight = 0;
            uint32_t resolutionHeight = 720;
            uint32_t Scale(uint32_t value) const { return resolution::Scale(value, resolutionHeight); }
            uint32_t depthMsaa = 0;
            // Guest-memory footprint and a sampled hash of it, so a texture the
            // title streams in after we first uploaded it is noticed and re-read.
            uint32_t guestAddress = 0, guestBytes = 0;
            uint64_t guestHash = 0;
            uint64_t checkedFrame = ~0ull;
            uint64_t clearedFrame = ~0ull;   // LO_CLEAR_RT debugging
        };

        struct RenderTargetKey
        {
            uint32_t base, format, pitch, height;
            bool depth;
            bool operator==(const RenderTargetKey& o) const { return base == o.base && format == o.format && pitch == o.pitch && height == o.height && depth == o.depth; }
        };
        struct RenderTargetKeyHash { size_t operator()(const RenderTargetKey& k) const { return k.base * 1000003u ^ k.format * 8191u ^ k.pitch * 131u ^ k.height ^ (k.depth ? 0x9E3779B9u : 0); } };

        struct TextureKey
        {
            uint32_t address, format, width, height, flags; // flags: tiled | endian<<1 | pitch<<3
            bool operator==(const TextureKey& o) const { return address == o.address && format == o.format && width == o.width && height == o.height && flags == o.flags; }
        };
        struct TextureKeyHash { size_t operator()(const TextureKey& k) const { return k.address * 1000003u ^ k.format * 8191u ^ k.width * 131u ^ k.height * 17u ^ k.flags; } };

        struct Shader
        {
            std::unique_ptr<RenderShader> shader;
            xenos::TranslatedShader info;
            bool valid = false;
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
            std::unique_ptr<RenderCommandList> commandList;
            std::unique_ptr<RenderCommandFence> fence;
            bool listOpen = false;

            std::unique_ptr<RenderBuffer> uploadRing;
            uint8_t* uploadMapped = nullptr;

            // Vertex buffers live in a persistent arena keyed by (address, size,
            // endian). A fetch constant may describe a multi-megabyte buffer for a
            // draw that touches a few hundred vertices, so copying per draw is
            // hopeless; instead each buffer is uploaded once and re-validated with
            // a sampled hash of the guest memory when it is referenced again.
            std::unique_ptr<RenderBuffer> vertexArena;
            uint8_t* arenaMapped = nullptr;
            uint64_t arenaOffset = 0;
            struct VertexEntry { uint64_t offset; uint64_t hash; uint64_t lastFrame; };
            std::unordered_map<uint64_t, VertexEntry> vertexCache;
            uint32_t vertexUploads = 0, vertexRevalidations = 0;
            size_t vertexBytesUploaded = 0;
            uint64_t uploadOffset = 0;
            std::unique_ptr<RenderBuffer> readback;

            std::unique_ptr<RenderPipelineLayout> pipelineLayout;
            RenderDescriptorSetBuilder setBuilders[5];
            std::vector<std::unique_ptr<RenderDescriptorSet>> setPools[4];
            uint32_t setPoolUsed[4] = {};
            uint32_t vfetchDescriptorBase = 0, samplerDescriptorBase = 0;
            std::unique_ptr<RenderDescriptorSet> staticSet0;     // ring buffers + sampler palette
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
            std::vector<std::unique_ptr<HostTexture>> retiredTextures; // replaced targets, freed after the next Flush
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
            temporal::SceneObservation temporalScene;
            std::unique_ptr<temporal::HistoryOwner> temporalHistory;
            bool temporalExperiment=false,temporalAllowHistory=false,temporalJitter=false,temporalStableGrid=false;
            bool temporalForced=false,temporalForcedHistory=false,temporalForcedJitter=false,temporalForcedStable=false;
            bool temporalInitFailed=false;
            uint64_t temporalSupportedFrame=~0ull;
            uint32_t temporalJitterDraws=0,temporalJitterMisses=0;
            uint64_t temporalEpoch=1,temporalFramesLogged=0,temporalSubmittedFrame=~0ull;
            uint64_t resolutionConfigFrame=~0ull;
            resolution::Size internalSize{}, requestedInternalSize{};
            bool resolutionAllocationFailed=false;
            std::chrono::steady_clock::time_point temporalFrameTime=std::chrono::steady_clock::now();
            std::unique_ptr<gpu::Presentation> sceneProcessor;
            std::unique_ptr<RenderTexture> sceneAAOutput;
            uint32_t sceneAAWidth=0,sceneAAHeight=0,sceneAAMode=0;
            bool sceneAAEnabled=false,sceneAABusy=false;
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

            // Per-frame timing of the expensive paths (LO_GPU_STATS).
            struct ScopedTimer
            {
                double& acc;
                std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
                ~ScopedTimer() { acc += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); }
            };
            double tDraw = 0, tShader = 0, tPipeline = 0, tTexture = 0, tResolve = 0, tFlush = 0;
            double tConst = 0, tSets = 0, tVertex = 0, tBind = 0, tIndex = 0, tRecord = 0;
            uint32_t nShader = 0, nPipeline = 0, nTexture = 0, nResolve = 0;
            size_t texBytes = 0;
            void ResetTimers() { tDraw = tShader = tPipeline = tTexture = tResolve = tFlush = 0; tConst = tSets = tVertex = tBind = tIndex = tRecord = 0; nShader = nPipeline = nTexture = nResolve = 0; texBytes = 0; }
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
            uint32_t captureFrame = 0;
            uint64_t captureRequest = 0;
            std::string debugCaptureDir;
            std::filesystem::path debugCaptureRoot;
            static constexpr uint32_t debugCaptureFrameCount = 3;
            uint32_t debugCaptureFirstFrame = 0, debugCaptureCompleted = 0;
            std::ofstream debugTrace;
            uint32_t debugDraw = 0;
            std::vector<uint32_t> debugRegisters;
            std::set<uint64_t> debugShaders;

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
                        std::filesystem::create_directories(debugCaptureRoot / "shaders");
                    }
                    if (frame != debugCaptureFirstFrame + debugCaptureCompleted)
                        throw std::runtime_error("Capture frame sequence is not consecutive");
                    const auto path = debugCaptureRoot / fmt::format("frame-{:02}-f{}", debugCaptureCompleted + 1, frame);
                    std::filesystem::create_directories(path);
                    debugCaptureDir = path.string();
                    debugTrace.open(path / "render-state.txt", std::ios::trunc);
                    if (!debugTrace) throw std::runtime_error("Cannot write render-state.txt");
                    debugTrace << "Frame " << frame << "\nRegister index/value pairs are hex. First draw is a full register snapshot; later draws contain changes.\n";
                    const auto& description = device->getDescription();
                    debugTrace << fmt::format("GPU: {} driver_raw={}\n", description.name, description.driverVersion);
                    const auto config = settings::GetConfig();
                    debugTrace << fmt::format("Source version: {}\nConfigured graphics: output={}x{} internal_resolution={} window_mode={} AA={} scaling_quality={} frame_rate={}\n",
                        lo_version::Source, config.width, config.height, config.internalResolution,
                        uint32_t(config.windowMode), config.antialiasing, config.scalingQuality, config.frameRate);
                    debugTrace << "AA IDs: 0 Off, 1 FXAA, 2 SMAA, 3 experimental TAA. Configured mode does not prove per-draw application; inspect surfaces and draw state.\n";
                    debugTrace << "Shared translated shaders: ../shaders/<hash>.hlsl. Exact R32 depth is in .bin; no duplicate .f32 file.\n";
                    debugDraw = 0;
                    debugRegisters.clear();
                    resolveSeq = 0;
                    captureFrame = frame;
                    captureStatus = L"正在截取 / Capturing " + std::to_wstring(debugCaptureCompleted + 1) + L"/3: " + path.wstring();
                    LOG_INFO("render capture started: {}", FileSystem::PathUtf8(std::filesystem::path(debugCaptureDir)));
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("render capture: {}", e.what());
                    debugCaptureDir.clear();
                    debugTrace.close();
                    debugTrace.clear();
                    captureFrame = 0;
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
                                << "\nlast_attempted_frame=" << frame << "\nstatus=incomplete\n";
                        }
                    }
                    catch (...) {} // The output directory itself may be unwritable.
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
            uint64_t resolveTraceSerial = 0;
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
                        format == RenderFormat::R16G16B16A16_FLOAT ? 8 : format == RenderFormat::R32_FLOAT ? 4 : 0;
                    if (!bpp) continue;
                    const uint32_t pitch = (width * bpp + 255) & ~255u;
                    const uint32_t offset = (resolveTraceBytes + 511) & ~511u;
                    constexpr uint32_t capacity = 128u << 20; // 4K source + TAA output + R32 depth.
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
                    LOG_INFO("renderer: ps trace end f{} ps={:016x} draws={} truncated={}",
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
                LOG_INFO("renderer: ps trace request {} next-frame={} frames={} ps={:016x} constants={}+{}",
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
                uint64_t request = 0;
                std::ifstream in(path);
                if (in >> request && request && request != captureRequest)
                {
                    captureRequest = request;
                    captureFrame = frame;
                    LOG_INFO("renderer: capture request {} at frame {}", request, frame);
                }
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
            static_assert(offsetof(SharedConstants,samplerIndex)==640);
            static_assert(offsetof(SharedConstants,textureInfo)==768);
            static_assert(offsetof(SharedConstants,textureSize)==896);

            // ---- lifecycle -----------------------------------------------------
            xenos::cache::Identity cacheIdentity;
            bool initializationModuleFailure = false;
            bool Init()
            {
                device = video::GetDevice();
                queue = video::GetQueue();
                vulkan = video::IsVulkan();
                binaryFormat = vulkan ? xenos::ShaderBinaryFormat::Spirv : xenos::ShaderBinaryFormat::Dxil;
                renderFormat = vulkan ? RenderShaderFormat::SPIRV : RenderShaderFormat::DXIL;
                if (!device || !queue)
                    return false;
                cacheIdentity = xenos::cache::MakeIdentity(vulkan ? backend::Backend::Vulkan : backend::Backend::D3D12, xenos::DxcIdentity());

                commandList = queue->createCommandList();
                fence = device->createCommandFence();
                uploadRing = device->createBuffer(RenderBufferDesc::UploadBuffer(kUploadRingSize, vulkan ? RenderBufferFlag::DEVICE_ADDRESSABLE | RenderBufferFlag::INDEX | RenderBufferFlag::STORAGE : RenderBufferFlag::NONE));
                if (!commandList || !fence || !uploadRing) return false;
                uploadMapped = static_cast<uint8_t*>(uploadRing->map());
                vertexArena = device->createBuffer(RenderBufferDesc::UploadBuffer(kVertexArenaSize, RenderBufferFlag::STORAGE));
                if (!vertexArena || !uploadMapped) return false;
                arenaMapped = static_cast<uint8_t*>(vertexArena->map());
                readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(kReadbackSize));
                if (!readback || !arenaMapped) return false;
                resolveReadback = getenv("LO_RESOLVE_READBACK") != nullptr;
                textureRevalidate = getenv("LO_TEXTURE_STATIC") == nullptr;
                auto enabled=[](const char* key){const char* value=getenv(key);return value&&strcmp(value,"1")==0;};
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
                    if(!sceneProcessor->Init(device)) return false;
                }
                if(temporalExperiment) {
                    temporalHistory=std::make_unique<temporal::HistoryOwner>();
                    if(!temporalHistory->Init(device)) {temporalHistory.reset();temporalExperiment=false;LOG_ERROR("renderer: temporal experiment pipeline initialization failed");return false;}
                    else LOG_INFO("renderer: temporal pre-UI experiment enabled, camera_history={} jitter={} stable_grid={} (known scene VS only; no object motion vectors)",temporalAllowHistory,temporalJitter,temporalStableGrid);
                }
                dummyBuffer = device->createBuffer(RenderBufferDesc::DefaultBuffer(256));

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
                }
                for (int i = 0; i < (vulkan?5:4); i++)
                    layout.addDescriptorSet(setBuilders[i]);
                layout.end();
                pipelineLayout = layout.create(device);

                staticSet0 = setBuilders[0].create(device);
                if (!dummyBuffer || !pipelineLayout || !staticSet0 || (vulkan && !staticSamplerSet)) return false;
                for (uint32_t i = 0; i < (vulkan?1:kVertexFetchSlots); i++)
                    staticSet0->setBuffer(vfetchDescriptorBase + i, vertexArena.get(), kVertexArenaSize);
                RenderSampler* defaultSampler = GetSampler(0x2 | (0x2 << 2) | (0x1 << 4)); // linear, wrap
                if (!defaultSampler) return false;
                for (uint32_t i = 0; i < kSamplerPalette; i++)
                    (vulkan?staticSamplerSet.get():staticSet0.get())->setSampler(samplerDescriptorBase + i, defaultSampler);

                CreateDummyTexture(dummyTexture2D, RenderTextureDimension::TEXTURE_2D, 0);
                CreateDummyTexture(dummyTexture3D, RenderTextureDimension::TEXTURE_3D, 0);
                CreateDummyTexture(dummyTextureCube, RenderTextureDimension::TEXTURE_2D, RenderTextureFlag::CUBE);

                if (const char* dir = getenv("LO_SHADER_CACHE_DIR"))
                    shaderCacheDir = dir;
                else
                    shaderCacheDir = "cache/shaders";
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
                CompileTransferShader();
                if (!dummyTexture2D.texture || !dummyTexture3D.texture || !dummyTextureCube.texture ||
                    !rectListGs || !blitVs || !blitPs || !transferPs) return false;
                PrepareKnownShaders();
                if (initializationModuleFailure) return false;
                PrepareKnownPipelines();
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
                Begin();
                uint64_t offset = Upload(black, sizeof(black), 512);
                if (offset == UINT64_MAX)
                    return;
                Transition(tex, RenderTextureLayout::COPY_DEST, RenderBarrierStage::COPY);
                for (uint32_t slice = 0; slice < slices; slice++)
                    commandList->copyTextureRegion(
                        RenderTextureCopyLocation::Subresource(tex.texture.get(), 0, slice),
                        RenderTextureCopyLocation::PlacedFootprint(uploadRing.get(), RenderFormat::R8G8B8A8_UNORM, 1, 1, 1, 64, offset));
                Transition(tex, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
            }

            // Full-screen blit that reinterprets one render target into another
            // format. SV_Position is the same pixel in both, so a Load() needs no
            // constants and the viewport alone selects the rectangle.
            std::unique_ptr<RenderShader> blitVs, blitPs;
            std::map<uint32_t, std::unique_ptr<RenderPipeline>> blitPipelines;

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
                    "  float4 v = src.Load(int3(int2(pos.xy * asfloat(xeTransfer.z)), 0));\n"
                    "  return UnpackGuest(PackGuest(v, xeTransfer.x), xeTransfer.y);\n"
                    "}\n";
                xenos::CompiledShader f = xenos::CompileCachedHlsl(psSrc, "main", "ps_6_0", binaryFormat);
                if (!f.ok)
                {
                    LOG_WARNING("renderer: transfer shader compilation failed: {}", f.errors);
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
                // Only the 32-bit classes share a word layout; wider ones are left alone.
                if (srcClass > kClass7e3 || dstClass > kClass7e3)
                    return;
                RenderPipeline* pipeline = GetTransferPipeline(dst.format);
                if (!pipeline)
                    return;
                SharedConstants transferConstants{};
                transferConstants.transfer[0] = srcClass;
                transferConstants.transfer[1] = dstClass;
                transferConstants.transfer[2] = std::bit_cast<uint32_t>(float(src.resolutionHeight) / dst.resolutionHeight);
                uint64_t offset = Upload(&transferConstants, sizeof(transferConstants));
                if (offset == UINT64_MAX)
                    return;
                const uint32_t w = dst.Scale(std::min(src.guestWidth, dst.guestWidth));
                const uint32_t h = dst.Scale(std::min(src.guestHeight, dst.guestHeight));
                Begin();
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
                    LOG_WARNING("renderer: blit shader compilation failed: {}{}", v.errors, f.errors);
                    return;
                }
                blitVs = device->createShader(v.bytecode.data(), v.bytecode.size(), "main", renderFormat);
                blitPs = device->createShader(f.bytecode.data(), f.bytecode.size(), "main", renderFormat);
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
                RenderPipeline* pipeline = GetBlitPipeline(dst.format);
                if (!pipeline)
                    return false;
                Begin();
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
            void Begin()
            {
                if (!listOpen)
                {
                    commandList->begin();
                    listOpen = true;
                }
            }

            void Flush()
            {
                if (!listOpen)
                    return;
                commandList->end();
                listOpen = false;
                const RenderCommandList* lists[] = { commandList.get() };
                queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence.get());
                {
                    ScopedTimer timer{ tFlush };
                    queue->waitForCommandFence(fence.get());
                }
                // Initialization creates framebuffer views for resolve textures
                // too. Release those cached views after their last GPU use and
                // before a retired texture's pointer can be reused.
                for (const auto& texture : retiredTextures)
                    for (auto fb = framebuffers.begin(); fb != framebuffers.end();)
                        if (fb->first.first == texture->texture.get() || fb->first.second == texture->texture.get())
                            fb = framebuffers.erase(fb);
                        else
                            ++fb;
                retiredTextures.clear();
                if(temporalHistory)temporalHistory->ReleaseCompleted();
                sceneAABusy=false; // Existing queue fence completed; processor descriptors may be reused.
                uploadOffset = 0;
                for (auto& used : setPoolUsed)
                    used = 0;
            }

            // Returns an offset into the upload ring or UINT64_MAX when full.
            uint64_t Upload(const void* data, size_t size, uint32_t alignment = 256)
            {
                uint64_t offset = (uploadOffset + alignment - 1) & ~uint64_t(alignment - 1);
                if (offset + size > kUploadRingSize)
                {
                    // Out of space mid-frame: finish what we have and start over.
                    Flush();
                    Begin();
                    offset = 0;
                    if (size > kUploadRingSize)
                        return UINT64_MAX;
                }
                if (data)
                    memcpy(uploadMapped + offset, data, size);
                uploadOffset = offset + size;
                return offset;
            }

            // plume's shader-visible heap holds 65536 views; every draw takes three
            // 32-slot sets, so the pools are capped and the frame is split when full.
            static constexpr uint32_t kMaxSetsPerKind = 500;

            void SetConstantBuffer(uint64_t offset, uint32_t index)
            {
                if(vulkan) {
                    constantAddresses[index]=uploadRing->getDeviceAddress()+offset;
                    commandList->setGraphicsPushConstants(0,constantAddresses);
                } else commandList->setGraphicsRootDescriptor(RenderBufferReference(uploadRing.get(),offset),index);
            }

            RenderDescriptorSet* AcquireSet(int which)
            {
                auto& pool = setPools[which];
                uint32_t& used = setPoolUsed[which];
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

            void PrepareKnownPipelines()
            {
                pipelineCacheEnabled = !shaderCacheDir.empty() && !getenv("LO_NO_PIPELINE_CACHE");
                if (!pipelineCacheEnabled) return;
                const auto path = std::filesystem::path(shaderCacheDir) / (vulkan ? "pipelines_vk12_1.bin" : "pipelines.bin");
                const auto loaded = gpu::pipeline_cache::Load(path, xenos::cache::Version, kPipelineRecipeVersion, ValidPipelineRecipe);
                if (!loaded.error.empty()) LOG_WARNING("renderer: ignoring pipeline recipes: {}", loaded.error);
                for (const auto& key : loaded.keys) pipelineRecipes.insert(key);
                // Disabling precreation is a same-binary control. Learning remains
                // enabled so replay coverage can be measured independently.
                if (getenv("LO_NO_PIPELINE_PREPARE") || getenv("LO_NO_SHADER_PREPARE")) return;
                struct Job { PipelineKey key; Shader* vs; Shader* ps; std::unique_ptr<RenderPipeline> pipeline; };
                std::vector<Job> jobs;
                size_t missingShaders = 0;
                for (const auto& key : loaded.keys) {
                    const auto vs = shaders[0].find(key.vs), ps = shaders[1].find(key.ps);
                    if (vs == shaders[0].end() || !vs->second.valid ||
                        (key.ps && (ps == shaders[1].end() || !ps->second.valid))) { ++missingShaders; continue; }
                    jobs.push_back({key, &vs->second, key.ps ? &ps->second : nullptr, {}});
                }
                const auto started = std::chrono::steady_clock::now();
                std::atomic<size_t> next{0}, completed{0};
                auto worker = [&] {
                    for (;;) {
                        const size_t i = next.fetch_add(1);
                        if (i >= jobs.size()) return;
                        auto& job = jobs[i];
                        try { job.pipeline = CreatePipeline(job.key, job.vs, job.ps, false); }
                        catch (const std::exception& e) { LOG_WARNING("renderer: pipeline precreation: {}", e.what()); }
                        ++completed;
                    }
                };
                const unsigned logical = std::thread::hardware_concurrency();
                const auto count = std::min<size_t>(jobs.size(), getenv("LO_PIPELINE_PREPARE_SERIAL") ? 1u :
                    std::min(4u, logical > 1 ? logical - 1 : 1u));
                std::vector<std::jthread> workers;
                try { for (size_t i = 0; i < count; ++i) workers.emplace_back(worker); }
                catch (const std::system_error& e) {
                    LOG_WARNING("renderer: started only {} pipeline workers: {}", workers.size(), e.what());
                    if (workers.empty()) worker();
                }
                while (completed.load() < jobs.size()) {
                    video::SetShaderPreparationProgress(uint32_t(completed.load()), uint32_t(jobs.size()),
                        video::PreparationStage::Pipelines, video::PreparationUnit::Pipelines);
                    video::PumpEvents();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                for (auto& thread : workers) thread.join();
                size_t failed = 0;
                for (auto& job : jobs) {
                    if (!job.pipeline) { ++failed; continue; }
                    preparedPipelineKeys.insert(job.key);
                    pipelines.emplace(job.key, std::move(job.pipeline));
                }
                video::SetShaderPreparationProgress(0, 0);
                LOG_INFO("renderer: pipeline preparation: {} recipes, {} ready, {} missing shaders, {} failed, {} workers, {:.0f} ms",
                    loaded.keys.size(), preparedPipelineKeys.size(), missingShaders, failed, workers.size(),
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-started).count());
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

            void PrepareKnownShaders()
            {
                if (shaderCacheDir.empty() || getenv("LO_NO_SHADER_PREPARE")) return;
                namespace startup = xenos::startup_cache;
                const auto wholeStarted = std::chrono::steady_clock::now();
                const auto& compilerIdentity = xenos::DxcIdentity();
                const bool retryFailures = getenv("LO_SHADER_RETRY_FAILURES") != nullptr;
                const auto bundlePath = std::filesystem::path(shaderCacheDir) /
                    (vulkan ? "startup_vk12_v1.bundle" : "startup_dxil_v1.bundle");
                const auto xex = std::span<const uint8_t>(static_cast<const uint8_t*>(g_memory.Translate(0x82000000)), 0x185C60);
                auto snapshot = [&](bool compiled = true, bool sources = true) {
                    return startup::Snapshot(FileSystem::GetGameRoot(), shaderCacheDir, cacheIdentity, xex, compiled, sources);
                };
                const bool bundleEnabled = !compilerIdentity.empty() && !getenv("LO_SHADER_FULL_SCAN") &&
                    !getenv("LO_SHADER_HLSL_DIR") && !retryFailures;
                LOG_INFO("renderer: shader startup cache: {}, compiler identity {}", bundlePath.string(),
                    compilerIdentity.empty() ? "unavailable (persistent reuse disabled)" : compilerIdentity);
                if (bundleEnabled) {
                    uint32_t modules = 0, cachedFailures = 0;
                    double moduleMs = 0;
                    const auto probeStarted = std::chrono::steady_clock::now();
                    try {
                        const auto identity = snapshot();
                        LOG_INFO("renderer: shader startup metadata snapshot: {:.0f} ms",
                            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-probeStarted).count());
                        video::SetShaderPreparationProgress(0, 1, video::PreparationStage::CachedShaders);
                        auto loaded = startup::LoadTransactional(bundlePath, identity, cacheIdentity, [&](startup::Record&& record) {
                            auto& entry = shaders[record.info.isPixelShader ? 1 : 0][record.hash];
                            entry.info = std::move(record.info);
                            if (!record.failure.empty()) {
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
                        }, [&] { shaders[0].clear(); shaders[1].clear(); }, [&] {
                            if (identity != snapshot()) throw std::runtime_error("cache inputs changed while loading bundle");
                        }, [] { video::PumpEvents(); });
                        if (!loaded.ok) throw std::runtime_error(loaded.reason);
                        video::SetShaderPreparationProgress(0, 0);
                        LOG_INFO("renderer: startup bundle hit: {} records, {} modules ready, {} cached failures; 0 source content reads, 0 translations, 0 DXC attempts, {} bytes verified/read",
                            loaded.records, modules, cachedFailures, loaded.bytesRead);
                        LOG_INFO("renderer: startup bundle elapsed {:.0f} ms including {:.0f} ms device module creation; source discovery/expansion skipped",
                            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-wholeStarted).count(), moduleMs);
                        ResetTimers();
                        return;
                    } catch (const std::exception& e) {
                        // Pass one is validation-only. Pass two and module creation
                        // may still fail: discard all state before legacy fallback.
                        shaders[0].clear(); shaders[1].clear();
                        LOG_INFO("renderer: startup bundle fallback: {}", e.what());
                    }
                } else LOG_INFO("renderer: startup bundle bypass: explicit scan/dump/retry or unavailable compiler identity");
                std::string resourcesBefore;
                if (bundleEnabled) try { resourcesBefore = snapshot(false, false); }
                    catch (const std::exception& e) { LOG_WARNING("renderer: resource snapshot unavailable: {}", e.what()); }
                video::SetShaderPreparationProgress(0, 1, video::PreparationStage::CacheValidation, video::PreparationUnit::Files);
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
                        video::PumpEvents();
                    }, getenv("LO_SHADER_FULL_SCAN") ? std::span<const xenos::resources::IndexFile>{}
                                                     : std::span<const xenos::resources::IndexFile>{xenos::resources::builtin::files},
                       getenv("LO_SHADER_FULL_SCAN") ? std::span<const xenos::resources::CpxIndexPackage>{}
                                                     : std::span<const xenos::resources::CpxIndexPackage>{xenos::resources::builtin::cpxPackages},
                       getenv("LO_SHADER_FULL_SCAN") ? std::span<const xenos::resources::CpxIndexArchive>{}
                                                     : std::span<const xenos::resources::CpxIndexArchive>{xenos::resources::builtin::cpxArchives},
                       getenv("LO_SHADER_FULL_SCAN") != nullptr);
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
                std::error_code ec;
                std::filesystem::create_directories(source, ec);
                if (ec) return;
                const auto expansionStarted = std::chrono::steady_clock::now();
                bool expansionComplete = true;
                try {
                    const auto staticCount = xenos::resources::ExtractXexShaders(
                        {static_cast<const uint8_t*>(g_memory.Translate(0x82000000)), 0x185C60}, source);
                    const auto generated = xenos::resources::variants::GenerateFixedVariants(source,
                        [&](uint64_t, std::span<const uint8_t> code) { xenos::resources::SaveSource(source, false, code); });
                    const auto linked = xenos::resources::variants::GenerateLinkedVariants(source,
                        [&](uint64_t, std::span<const uint8_t> code) { xenos::resources::SaveSource(source, false, code); });
                    LOG_INFO("renderer: shader source expansion: {} static XEX, {} fixed VS candidates, {} verified bases, {} invalid bases",
                        staticCount, generated.generated, generated.verifiedBases, generated.invalidBases);
                    LOG_INFO("renderer: linked VS expansion: {} new candidates, {} verified VS bases, {} verified PS sources, {} invalid sources",
                        linked.generated, linked.verifiedBases, linked.verifiedPixelSources, linked.invalidBases + linked.invalidPixelSources);
                } catch (const std::exception& e) {
                    expansionComplete = false;
                    LOG_WARNING("renderer: shader source expansion: {}", e.what());
                }
                LOG_INFO("renderer: shader source expansion elapsed: {:.0f} ms",
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - expansionStarted).count());
                const auto enumerationStarted = std::chrono::steady_clock::now();
                std::string sourcesBefore;
                if (bundleEnabled && !resourcesBefore.empty()) try { sourcesBefore = snapshot(false); }
                    catch (const std::exception& e) { LOG_WARNING("renderer: source snapshot unavailable: {}", e.what()); }
                std::vector<std::filesystem::path> paths;
                for (std::filesystem::directory_iterator it(source, ec), end; !ec && it != end; it.increment(ec)) {
                    const auto name = it->path().filename().string();
                    if (it->path().extension() == ".bin" && (name.starts_with("vs_") || name.starts_with("ps_")))
                        paths.push_back(it->path());
                }
                std::sort(paths.begin(), paths.end());
                std::unique_ptr<startup::Writer> bundleWriter;
                if (bundleEnabled && !sourcesBefore.empty() && !ec && extracted.error.empty() && expansionComplete) {
                    try {
                        bundleWriter = std::make_unique<startup::Writer>(bundlePath, xenos::GetShaderCommonHlsl());
                    } catch (const std::exception& e) { LOG_WARNING("renderer: startup bundle writer unavailable: {}", e.what()); }
                }
                LOG_INFO("renderer: shader source enumeration: {} files, {:.0f} ms", paths.size(),
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - enumerationStarted).count());
                const auto started = std::chrono::steady_clock::now();
                // CPU translation, DXC and independent cache files run in
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
                const auto workerCount = xenos::preparation::WorkerCount(logicalThreads, paths.size(),
                    getenv("LO_SHADER_PREPARE_SERIAL") != nullptr);
                const size_t readyCapacity = std::max<size_t>(8, workerCount * 2);

                auto prepare = [&](size_t index) {
                    PreparedSource item;
                    try {
                        const auto sourceStarted = std::chrono::steady_clock::now();
                        const auto& path = paths[index];
                        item.name = path.filename().string();
                        std::ifstream in(path, std::ios::binary | std::ios::ate);
                        const auto size = in.tellg();
                        if (size < 12 || size > 0x40000 || size % 4 != 0)
                            throw std::runtime_error("invalid microcode size");
                        std::vector<uint32_t> words(size_t(size) / 4);
                        in.seekg(0);
                        if (!in.read(reinterpret_cast<char*>(words.data()), size))
                            throw std::runtime_error("cannot read microcode");
                        item.pixel = item.name.starts_with("ps_");
                        const auto bytes = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(words.data()), size_t(size));
                        if (item.name != xenos::resources::SourceName(item.pixel, bytes))
                            throw std::runtime_error("microcode hash does not match its filename");
                        item.hash = Fnv1a(words.data(), size_t(size));
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
                    } catch (const std::exception& e) { item.error = e.what(); }
                    return item;
                };

                LOG_INFO("renderer: shader preparation: {} logical threads, {} requested workers, {} shaders, {} queued results max",
                    logicalThreads, workerCount, paths.size(), readyCapacity);

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
                        if ((!item.error.empty() && !item.deterministicFailure) || !item.cacheWriteError.empty()) bundleWriter.reset();
                        else try { bundleWriter->Add({item.hash, item.info, item.bytecode, item.error}); }
                        catch (const std::exception& e) {
                            LOG_WARNING("renderer: startup bundle write abandoned: {}", e.what());
                            bundleWriter.reset();
                        }
                    }
                    if (item.cachePresent && !item.cacheValid)
                        LOG_WARNING("renderer: ignoring incomplete shader cache {}", item.cachePath);
                    if (!item.cacheWriteError.empty())
                        LOG_WARNING("renderer: precompile {} cache write failed: {}", item.name, item.cacheWriteError);
                    if (!item.error.empty()) {
                        LOG_WARNING("renderer: precompile {} {}: {} (diagnostic: {}.failed)", item.name,
                            item.cachedFailure ? "cached failure" : "failed", item.error.substr(0, item.error.find('\n')), item.cachePath);
                        if (item.deterministicFailure) shaders[item.pixel ? 1 : 0][item.hash].info = std::move(item.info);
                        ++failed;
                    } else {
                        auto& cache = shaders[item.pixel ? 1 : 0];
                        Shader& entry = cache[item.hash];
                        entry.info = std::move(item.info);
                        const auto moduleStarted = std::chrono::steady_clock::now();
                        entry.shader = device->createShader(item.bytecode.data(), item.bytecode.size(), "main", renderFormat);
                        moduleUs += std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - moduleStarted).count();
                        entry.valid = entry.shader != nullptr;
                        if (!entry.info.errors.empty())
                            LOG_WARNING("renderer: {} shader {:016x} notes: {}",
                                item.pixel ? "pixel" : "vertex", item.hash, entry.info.errors);
                        if (entry.valid) ++modulesReady;
                        else { ++modulesFailed; ++failed; initializationModuleFailure = true; bundleWriter.reset(); }
                    }
                    ++done;
                    video::SetShaderPreparationProgress(done, uint32_t(paths.size()));
                    video::PumpEvents();
                    return true;
                };

                xenos::preparation::QueueStats queueStats;
                try {
                    queueStats = xenos::preparation::RunBounded<PreparedSource>(paths.size(), workerCount,
                        readyCapacity, prepare, install, [] { video::PumpEvents(); });
                } catch (const std::exception& e) {
                    LOG_WARNING("renderer: shader preparation queue stopped after {} shaders: {}", done, e.what());
                }
                if (queueStats.startFailures)
                    LOG_WARNING("renderer: started only {} shader workers: {}",
                        queueStats.startedWorkers, queueStats.startError);
                video::SetShaderPreparationProgress(0, 0);
                if (bundleWriter && done == paths.size()) {
                    try {
                        if (sourcesBefore != snapshot(false) || resourcesBefore != snapshot(false, false))
                            throw std::runtime_error("source/resource inputs changed during preparation");
                        bundleWriter->Finish(snapshot());
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
            }

            Shader* GetShader(bool pixel, const uint32_t* words, uint32_t count)
            {
                uint64_t hash = Fnv1a(words, count * 4);
                auto& cache = shaders[pixel ? 1 : 0];
                auto it = cache.find(hash);
                if (it != cache.end())
                    return it->second.valid ? &it->second : nullptr;

                Shader& entry = cache[hash];
                if (!shaderCacheDir.empty()) {
                    const auto source = std::filesystem::path(shaderCacheDir) / "source";
                    std::error_code ec;
                    std::filesystem::create_directories(source, ec);
                    const auto path = source / fmt::format("{}_{:016x}.bin", pixel ? "ps" : "vs", hash);
                    if (!ec && !std::filesystem::exists(path, ec))
                        std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(words), size_t(count)*4);
                }
                ScopedTimer timer{ tShader };
                nShader++;
                std::vector<uint32_t> swapped(count);
                for (uint32_t i = 0; i < count; i++)
                    swapped[i] = ByteSwap(words[i]);
                entry.info = xenos::TranslateShader(swapped.data(), count, pixel);
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
                    if (present && dxil.empty()) LOG_WARNING("renderer: ignoring invalid/foreign shader cache {}", cachePath);
                }
                if (dxil.empty())
                {
                    const auto& compilerIdentity = xenos::DxcIdentity();
                    const auto failureKey = xenos::startup_cache::FailureKey(entry.info.hlsl, cacheIdentity, pixel);
                    const auto failurePath = cachePath + ".failed";
                    if (!cachePath.empty() && !compilerIdentity.empty() && !getenv("LO_SHADER_RETRY_FAILURES")) {
                        const auto failure = xenos::startup_cache::ReadFailure(failurePath, failureKey);
                        if (!failure.empty()) {
                            LOG_WARNING("renderer: {} shader {:016x} cached compiler failure: {} (diagnostic: {})",
                                pixel ? "pixel" : "vertex", hash, failure.substr(0, failure.find('\n')), failurePath);
                            return nullptr;
                        }
                    }
                    xenos::CompiledShader compiled = xenos::CompileHlsl(entry.info.hlsl, "main", pixel ? "ps_6_0" : "vs_6_0", binaryFormat);
                    if (!compiled.ok)
                    {
                        if (!cachePath.empty() && !compilerIdentity.empty())
                            xenos::startup_cache::WriteFailure(failurePath, failureKey, compiled.errors, compiled.deterministicFailure);
                        LOG_WARNING("renderer: {} shader {:016x} failed to compile:\n{}", pixel ? "pixel" : "vertex", hash, compiled.errors);
                        if (getenv("LO_SHADER_DUMP_DIR"))
                            std::ofstream(fmt::format("{}/{}_{:016x}.hlsl", getenv("LO_SHADER_DUMP_DIR"), pixel ? "ps" : "vs", hash)) << entry.info.hlsl;
                        return nullptr;
                    }
                    dxil = std::move(compiled.bytecode);
                    if (!cachePath.empty()) {
                        std::string error;
                        if (!xenos::cache::WriteBinary(cachePath, pixel, hash, cacheIdentity, dxil, &error))
                            LOG_WARNING("renderer: shader cache write failed: {}", error);
                    }
                }
                entry.shader = device->createShader(dxil.data(), dxil.size(), "main", renderFormat);
                entry.valid = entry.shader != nullptr;
                if (!entry.info.errors.empty())
                    LOG_WARNING("renderer: {} shader {:016x} notes: {}", pixel ? "pixel" : "vertex", hash, entry.info.errors);
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

            void ApplyInternalResolution()
            {
                if (resolutionConfigFrame == frame) return;
                const bool first = resolutionConfigFrame == ~0ull;
                resolutionConfigFrame = frame;
                const uint64_t output = outputSize.load(std::memory_order_relaxed);
                const auto config = settings::GetConfig();
                const auto requested = resolution::ResolveInternalSize(config.internalResolution, uint32_t(output >> 32), uint32_t(output));
                const bool requestChanged = requested != requestedInternalSize;
                if (requestChanged) {
                    requestedInternalSize = requested;
                    resolutionAllocationFailed = false;
                }
                const auto effective = resolveReadback || resolutionAllocationFailed ? resolution::Size{} : requested;
                if (effective == internalSize && !first && !requestChanged) return;
                // Resize only between renderer frames. Complete all references
                // before destroying framebuffer views and the resources they use.
                if (effective != internalSize) {
                    Flush();
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
                LOG_INFO("renderer: internal resolution f{} requested={}x{} effective={}x{} output={}x{} mode={} cpu_readback={} allocation_fallback={}",
                    frame, requested.width, requested.height, effective.width, effective.height,
                    uint32_t(output >> 32), uint32_t(output), config.internalResolution, resolveReadback, resolutionAllocationFailed);
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
                auto it = renderTargets.find(key);
                if (it != renderTargets.end())
                {
                    if (it->second->guestHeight >= height)
                        return it->second.get();
                    height = std::max(height, it->second->guestHeight);
                    const RenderTexture* old = it->second->texture.get();
                    for (auto fb = framebuffers.begin(); fb != framebuffers.end();)
                    {
                        if (fb->first.first == old || fb->first.second == old) fb = framebuffers.erase(fb);
                        else ++fb;
                    }
                    retiredTextures.push_back(std::move(it->second));
                    renderTargets.erase(it);
                }

                auto tex = std::make_unique<HostTexture>();
                tex->allocationSerial = ++nextTargetAllocation;
                tex->format = depth ? RenderFormat::D32_FLOAT_S8_UINT : ClassHostFormat(colorClass);
                tex->guestWidth = pitch;
                tex->guestHeight = height;
                tex->resolutionHeight = resolution::TargetHeight(pitch, height, internalSize.height);
                tex->width = std::max(1u, tex->Scale(pitch));
                tex->height = std::max(1u, tex->Scale(height));
                RenderTextureDesc desc = RenderTextureDesc::Texture2D(tex->width, tex->height, 1, tex->format, depth ? RenderTextureFlag::DEPTH_TARGET : RenderTextureFlag::RENDER_TARGET);
                tex->texture = device->createTexture(desc);
                tex->layout = RenderTextureLayout::UNKNOWN;
                if (!tex->texture) {
                    resolutionAllocationFailed = true;
                    LOG_ERROR("renderer: render target allocation failed guest={}x{} physical={}x{}; native resolution fallback next frame", pitch, height, tex->width, tex->height);
                    return nullptr;
                }
                LOG_INFO("renderer: new {} target base={:#x} fmt={} guest={}x{} physical={}x{} scale_height={}", depth ? "depth" : "color", base, format, pitch, height, tex->width, tex->height, tex->resolutionHeight);
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

            HostTexture* GetTexture(const uint32_t* fetch, uint32_t dimension)
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
                TextureKey key{ sourceAddress, format, width, height, (tiled ? 1u : 0u) | (endian << 1) | (pitch32 << 3) | (dimension << 12) | (uint32_t(packedMips) << 14) | (sourceMip << 15) };
                // LO_NO_DEPTH_FETCH=1: hand shaders a constant instead of the resolved
                // depth, to tell depth-driven artefacts from shading ones.
                static const bool noDepthFetch = getenv("LO_NO_DEPTH_FETCH") != nullptr;
                if (noDepthFetch && (format == 22 || format == 23))
                    return &dummyTexture2D;
                if (ResolvedSurface* rs = FindResolved(base, format))
                {
                    const uint32_t physicalWidth = std::max(1u, rs->tex->Scale(width));
                    const uint32_t physicalHeight = std::max(1u, rs->tex->Scale(height));
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
                            if (view) retiredTextures.push_back(std::move(view));
                            view = std::make_unique<HostTexture>();
                            view->format = rs->tex->format;
                            view->guestWidth = width;
                            view->guestHeight = height;
                            view->resolutionHeight = rs->tex->resolutionHeight;
                            view->width = physicalWidth;
                            view->height = physicalHeight;
                            view->texture = device->createTexture(RenderTextureDesc::Texture2D(physicalWidth, physicalHeight, 1, view->format));
                        }
                        if (!view->texture) {
                            resolutionAllocationFailed = true;
                            LOG_ERROR("renderer: fetch view allocation failed guest={}x{} physical={}x{}; native resolution fallback next frame", width, height, physicalWidth, physicalHeight);
                            return nullptr;
                        }
                        Transition(*rs->tex, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
                        Transition(*view, RenderTextureLayout::COPY_DEST, RenderBarrierStage::COPY);
                        RenderBox box{ 0, 0, int32_t(physicalWidth), int32_t(physicalHeight), 0, 1 };
                        commandList->copyTextureRegion(RenderTextureCopyLocation::Subresource(view->texture.get()),
                            RenderTextureCopyLocation::Subresource(rs->tex->texture.get()), 0, 0, 0, &box);
                        Transition(*view, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
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
                    if (!textureRevalidate || cached->checkedFrame == frame || cached->guestBytes == 0)
                        return cached;
                    cached->checkedFrame = frame;
                    const uint64_t now = SampleHash(Phys(cached->guestAddress), cached->guestBytes);
                    if (now == cached->guestHash)
                        return cached;
                    textureReuploads++;
                    retiredTextures.push_back(std::move(it->second));
                    textures.erase(it);
                }

                ScopedTimer timer{ tTexture };
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
                        RenderTextureCopyLocation::PlacedFootprint(uploadRing.get(), fi.host, texWidth, texHeight, 1, (rowPitch / hostBpp) * fi.blockWidth,
                            offset + uint64_t(f) * rowPitch * blocksY));
                Transition(*tex, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);

                HostTexture* result = tex.get();
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
                        retiredTextures.push_back(std::move(it->second));
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
                ScopedTimer timer{ tPipeline };
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
                        LOG_INFO("renderer: stencil pipeline vs={:016x} ps={:016x} ctl={:#x} refs={:#x}/{:#x} mask={:#x}", key.vs, key.ps, depthControl, key.stencilRefMask, key.stencilRefMaskBack, key.colorMask);
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

                return device->createGraphicsPipeline(desc);
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
                const uint64_t hash = SampleHash(guest, bytes);
                auto it = vertexCache.find(key);
                if (it != vertexCache.end())
                {
                    if (it->second.hash == hash)
                    {
                        it->second.lastFrame = frame;
                        return it->second.offset;
                    }
                    // Contents changed. Overwriting in place would corrupt draws
                    // already recorded into the open command list from this slot, so
                    // allocate a fresh one; the old bytes die with the arena reset.
                    vertexRevalidations++;
                    vertexCache.erase(it);
                }

                // Allocate (16-byte aligned, 16 bytes of slack for the shader's
                // last fetch); when the arena is full, drain the GPU and start over.
                const size_t needed = ((bytes + 16 + 15) & ~size_t(15));
                if (arenaOffset + needed > kVertexArenaSize)
                    return UINT64_MAX; // DrawImpl resets the arena between draws
                const uint64_t offset = arenaOffset;
                arenaOffset += needed;
                CopySwapped(arenaMapped + offset, guest, sizeDwords, endian);
                memset(arenaMapped + offset + bytes, 0, 16);
                vertexCache.emplace(key, VertexEntry{ offset, hash, frame });
                vertexUploads++;
                vertexBytesUploaded += bytes;
                return offset;
            }

            // ---- draw -------------------------------------------------------------------
            void Draw(const DrawInfo& info)
            {
                ScopedTimer timer{ tDraw };
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
                ApplyInternalResolution();
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
                const bool poolsFull = setPoolUsed[1] >= kMaxSetsPerKind;
                const bool ringLow = uploadOffset + kUploadHeadroom > kUploadRingSize;
                const bool arenaLow = arenaOffset + kArenaHeadroom > kVertexArenaSize;
                if (poolsFull || ringLow || arenaLow)
                {
                    Flush();
                    Begin();
                    if (arenaLow)
                    {
                        arenaOffset = 0;
                        vertexCache.clear();
                        LOG_INFO("renderer: vertex arena reset");
                    }
                }
                Begin();

                uint32_t modeControl = Reg(REG_RB_MODECONTROL) & 7;
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

                // Shaders come from the command processor's last IM_LOAD.
                uint32_t vsCount = 0, psCount = 0;
                const uint32_t* vsWords = g_commandProcessor.GetActiveShader(false, vsCount);
                const uint32_t* psWords = g_commandProcessor.GetActiveShader(true, psCount);
                if (!vsWords || vsCount == 0)
                {
                    drops.shader++;
                    return;
                }
                Shader* vs = GetShader(false, vsWords, vsCount);
                // RB_MODECONTROL=5 is depth-only: the last loaded pixel shader
                // is inactive, including its discard and depth exports. Running
                // a stale shadow-depth PS here corrupts stencil volume tests.
                Shader* ps = modeControl == 4 && psWords && psCount ? GetShader(true, psWords, psCount) : nullptr;
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
                HostTexture* color = AcquireColorTarget(colorInfo & 0xFFF, (colorInfo >> 16) & 0xF, pitch, rtHeight);
                HostTexture* depth = (depthControl & 3) ? GetRenderTarget(depthInfo & 0xFFF, (depthInfo >> 16) & 1, pitch, rtHeight, true) : nullptr;
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
                    color->aaProvenance.Invalidate(frame,color->allocationSerial,true);
                    if (loud)
                        LOG_INFO("renderer: frame {} wiped target base={:#x} fmt={} pitch={} {}x{}", frame, colorInfo & 0xFFF, (colorInfo >> 16) & 0xF, pitch, color->width, color->height);
                }

                // Pipeline.
                PipelineKey key{};
                key.vs = Fnv1a(vsWords, vsCount * 4);
                key.ps = ps ? Fnv1a(psWords, psCount * 4) : 0;
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
                float layerDepthOffset = 0.0f;
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
                RenderPipeline* pipeline = GetPipeline(key, vs, ps, color->format, depth ? depth->format : RenderFormat::UNKNOWN);
                if (!pipeline)
                {
                    drops.pipeline++;
                    drops.primMask |= 1u << (info.primitiveType & 31);
                    return;
                }

                // Constants.
                auto tConst0 = std::chrono::steady_clock::now();
                // Both halves of the ALU constant file are 256 vec4 wide
                // (0x4000-0x43FF for the vertex shader, 0x4400-0x47FF for the
                // pixel shader) and the translated HLSL declares float4 c[256]
                // for either stage. Uploading fewer left the tail reading back
                // as zero, which zeroed the light terms of every character
                // material - they index c[253..255].
                uint32_t vsConstants[256 * 4], psConstants[256 * 4];
                for (uint32_t i = 0; i < 256 * 4; i++) vsConstants[i] = Reg(REG_ALU_CONSTANTS + i);
                for (uint32_t i = 0; i < 256 * 4; i++) psConstants[i] = Reg(REG_ALU_CONSTANTS + 256 * 4 + i);
                // Diagnostic selection uses only GPU draw constants, not the CPU
                // presented-swap counter. Shader/layout recognition is deliberately
                // limited to the path verified in the captured Map2 scene.
                if(sceneAAConfigFrame!=frame) {
                    sceneAAConfigFrame=frame;const auto mode=settings::GetConfig().antialiasing;
                    if(sceneAAMode!=mode) {if(temporalHistory)temporalHistory->Reset();temporalSupportedFrame=~0ull;++temporalEpoch;}
                    sceneAAMode=mode;
                    const bool selected=mode==3&&!resolveReadback;
                    temporalExperiment=temporalForced||selected;
                    temporalAllowHistory=temporalForcedHistory||selected;
                    temporalJitter=temporalForcedJitter||(selected&&temporalSupportedFrame!=~0ull&&temporalSupportedFrame+1==frame);
                    temporalStableGrid=temporalForcedStable||selected;
                    if(temporalExperiment&&!temporalHistory&&!temporalInitFailed) {
                        temporalHistory=std::make_unique<temporal::HistoryOwner>();
                        if(!temporalHistory->Init(device)) {temporalHistory.reset();temporalInitFailed=true;LOG_ERROR("renderer: TAA initialization failed; SMAA fallback");}
                    }
                    if(!temporalHistory)temporalExperiment=false;
                }
                const bool trackTemporalScene = !debugCaptureDir.empty() || temporalExperiment || sceneAAEnabled;
                if (trackTemporalScene && temporalScene.Frame() != frame) {
                    temporalScene.Reset(frame);
                    if(temporalHistory&&std::chrono::steady_clock::now()-temporalFrameTime>std::chrono::milliseconds(250)) {
                        temporalHistory->Reset();temporalSupportedFrame=~0ull;temporalJitter=temporalForcedJitter;++temporalEpoch;
                    }
                }
                if(temporalExperiment&&temporalHistory) {
                    static const char* diagnosticStart=getenv("LO_TEMPORAL_LOG_START_FRAME");
                    static const uint64_t diagnosticFrame=diagnosticStart?strtoull(diagnosticStart,nullptr,10):0;
                    temporalHistory->BeginFrame(frame,temporalEpoch,diagnosticStart&&frame>=diagnosticFrame&&temporalFramesLogged<256);
                }
                std::optional<temporal::SceneResolve> temporalSceneCopy;
                bool sceneAARecorded=false,temporalAARecorded=false;
                std::optional<temporal::SceneAnchor> temporalDrawAnchor;

                SharedConstants shared{};
                for (uint32_t i = 0; i < 8; i++) shared.bools[i] = Reg(REG_BOOL_CONSTANTS + i);
                for (uint32_t i = 0; i < 32; i++) shared.loops[i] = Reg(REG_LOOP_CONSTANTS + i);

                uint32_t vte = Reg(REG_PA_CL_VTE_CNTL);
                float xs = RegF(REG_PA_CL_VPORT_XSCALE), xo = RegF(REG_PA_CL_VPORT_XSCALE + 1);
                float ys = RegF(REG_PA_CL_VPORT_XSCALE + 2), yo = RegF(REG_PA_CL_VPORT_XSCALE + 3);
                float zs = RegF(REG_PA_CL_VPORT_XSCALE + 4), zo = RegF(REG_PA_CL_VPORT_XSCALE + 5);
                RenderViewport viewport(0.0f, 0.0f, float(pitch), float(rtHeight));
                shared.ndcScale[0] = shared.ndcScale[1] = shared.ndcScale[2] = 1.0f;
                if (vte & 1) // viewport scale enabled: vertices are in NDC, use a real viewport
                {
                    float w = 2.0f * std::fabs(xs), h = 2.0f * std::fabs(ys);
                    viewport = RenderViewport(xo - std::fabs(xs), yo - std::fabs(ys), w > 0 ? w : float(pitch), h > 0 ? h : float(rtHeight));
                    if (ys > 0) shared.ndcScale[1] = -1.0f; // flipped viewport
                }
                else // screen-space vertices: map pixels to NDC ourselves
                {
                    shared.ndcScale[0] = 2.0f / float(pitch);
                    shared.ndcScale[1] = -2.0f / float(rtHeight);
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
                const double rasterScale = double(color->resolutionHeight) / 720.0;
                RenderViewport rasterViewport = viewport;
                rasterViewport.x *= rasterScale; rasterViewport.y *= rasterScale;
                rasterViewport.width *= rasterScale; rasterViewport.height *= rasterScale;
                const int temporalSlot=temporal::PositionVPSlot(key.vs);
                if((temporalExperiment||sceneAAEnabled)&&temporalSlot>=0&&temporalViewport&&depth&&(depthControl&4)) {
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
                if (temporalExperiment && temporalJitter && jitterShadowPair)
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
                const auto drawJitter = temporal::ApplyDrawJitter(key.vs, key.ps, frame,
                    temporalExperiment && temporalJitter, temporalViewport, jitterAnchor,
                    depth ? depth->allocationSerial : 0,
                    {rasterViewport.x, rasterViewport.y, rasterViewport.width, rasterViewport.height},
                    vsConstants, psConstants, &temporalScene.Depth(), jitterSampledDepth ? &*jitterSampledDepth : nullptr);
                if (drawJitter.applied) ++temporalJitterDraws;
                else if (temporalExperiment && temporalJitter && temporalSlot >= 0 && temporalViewport) ++temporalJitterMisses;
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
                    else if (cfmt >= 4) m = 65504.0f;           // 16_16(_16_16)(_FLOAT), 32_FLOAT
                    shared.colorMax[0] = shared.colorMax[1] = shared.colorMax[2] = m;
                    shared.colorMax[3] = (cfmt == 3 || cfmt == 12 || cfmt <= 2 || cfmt == 10) ? 1.0f : m;
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

                tConst += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tConst0).count();

                // Descriptor sets: vertex fetch buffers + samplers, textures.
                RenderDescriptorSet* set0 = staticSet0.get();
                RenderDescriptorSet* set1;
                RenderDescriptorSet* set2;
                RenderDescriptorSet* set3;
                {
                    ScopedTimer timer{ tSets };
                    set1 = AcquireSet(1);
                    set2 = AcquireSet(2);
                    set3 = AcquireSet(3);
                }
                auto tVertex0 = std::chrono::steady_clock::now();
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
                        drops.vfetchSkips++;
                        if (vfTrace)
                            vfTraceLine += fmt::format(" vf{}=SKIP({} d0={:#x} d1={:#x})", slot, skip, d0, d1);
                        continue;
                    }
                    shared.vfetchOffset[slot] = uint32_t(offset);
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
                    LOG_INFO("renderer: draw vfetch{} | indxOffset={} raw{}", vfTraceLine, int32_t(Reg(REG_VGT_INDX_OFFSET)), raw);
                }

                tVertex += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tVertex0).count();

                // Prove the actual destination coverage before replacing its source.
                // This shader fetches float4 positions from slot 95 with 32-byte stride.
                // Restrict to two triangles forming a rectangle; viewport size alone
                // cannot justify treating an arbitrary fullscreen-looking draw as a copy.
                uint32_t fullCopyReason=0;std::string fullCopyVertices;
                const bool fullSceneCopy = [&]() {
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
                    if(frame>=start&&frame-start<128)LOG_INFO("renderer scene AA guard f{} full={} reason={} mode={} jitter={} blend={:#x} mask={} vtx={} prim={} n={} cull={:#x} ctl={:#x} vp=({},{},{},{}) extent={}x{} fetch95={:08x},{:08x} quad={} ",frame,fullSceneCopy,fullCopyReason,sceneAAMode,temporalJitter,key.blend,key.colorMask,shared.vtxFmt,info.primitiveType,info.indexCount,key.modeCull,Reg(REG_RB_COLORCONTROL),viewport.x,viewport.y,viewport.width,viewport.height,pitch,rtHeight,Reg(REG_FETCH_CONSTANTS+190),Reg(REG_FETCH_CONSTANTS+191),fullCopyVertices);
                }
                // Textures used by the pixel and vertex shaders.
                auto tBind0 = std::chrono::steady_clock::now();
                auto bindTextures = [&](Shader* s)
                {
                    if (!s) return;
                    for (uint32_t slot = 0; slot < kTextureSlots; slot++)
                    {
                        if (!((s->info.textureSlotMask >> slot) & 1))
                            continue;
                        uint32_t fetch[6];
                        for (int i = 0; i < 6; i++) fetch[i] = Reg(REG_FETCH_CONSTANTS + slot * 6 + i);
                        const uint32_t declared = s->info.textureDimension[slot];
                        RenderDescriptorSet* set = declared == 2 ? set2 : declared == 3 ? set3 : set1;
                        HostTexture* dummy = declared == 2 ? &dummyTexture3D : declared == 3 ? &dummyTextureCube : &dummyTexture2D;
                        uint32_t dimension = (fetch[5] >> 9) & 3; // 0 1D, 1 2D, 2 3D, 3 cube
                        HostTexture* tex = (fetch[0] & 3) == 2 ? GetTexture(fetch, dimension) : nullptr;
                        if (!tex)
                        {
                            // Descriptor sets are pooled and reused, so a slot the
                            // shader reads must always be written: otherwise it keeps
                            // the texture some earlier draw left there.
                            set->setTexture(slot, dummy->texture.get(), RenderTextureLayout::SHADER_READ);
                            shared.samplerIndex[slot] = 0;
                            shared.textureInfo[slot] = 0x68800u;
                            dummyBindings++;
                            continue;
                        }
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
                        RenderTexture* temporalDisplay=nullptr;
                        if(temporalSceneCopy && fullSceneCopy && s==ps && slot==0 &&
                           rasterViewport.width==tex->width && rasterViewport.height==tex->height &&
                           rasterViewport.width==temporalScene.Anchor().viewport.width && rasterViewport.height==temporalScene.Anchor().viewport.height) {
                            temporalScene.ObserveColor(*temporalSceneCopy);
                            if(temporalExperiment && temporalHistory && temporalScene.Ready() && tex->format==RenderFormat::R8G8B8A8_UNORM) {
                                Transition(*tex,RenderTextureLayout::COPY_SOURCE,RenderBarrierStage::COPY);
                                const auto sample = temporal::FrameJitter(frame, rasterViewport.width, rasterViewport.height);
                                const double jx = temporalJitter ? sample.pixelX : 0, jy = temporalJitter ? sample.pixelY : 0;
                                temporalDisplay=temporalHistory->ResolveColor(commandList.get(),tex->texture.get(),temporalScene,jx,jy,temporalAllowHistory,temporalStableGrid,sceneAAMode==3);
                                Transition(*tex,RenderTextureLayout::SHADER_READ,RenderBarrierStage::GRAPHICS);
                                if(temporalDisplay) {
                                    temporalAARecorded=true;
                                    // Reserved diagnostic IDs (not guest addresses): same draw source,
                                    // reconstructed display, and owned current depth, copied before reuse.
                                    QueueResolveTrace(*tex,0xffff0001u);
                                    QueueResolveTrace(temporalDisplay,RenderFormat::R8G8B8A8_UNORM,tex->width,tex->height,RenderTextureLayout::SHADER_READ,0xffff0002u);
                                    QueueResolveTrace(temporalHistory->CurrentDepth(),RenderFormat::R32_FLOAT,tex->width,tex->height,RenderTextureLayout::SHADER_READ,0xffff0003u);
                                }
                            }
                            if(!temporalDisplay && sceneAAEnabled && sceneProcessor && !sceneAABusy &&
                               sceneAAAppliedFrame!=frame && temporalScene.Ready() &&
                               (sceneAAMode==1||sceneAAMode==2||sceneAAMode==3) && tex->format==RenderFormat::R8G8B8A8_UNORM) {
                                if(!sceneAAOutput||sceneAAWidth!=tex->width||sceneAAHeight!=tex->height) {
                                    sceneAAOutput=device->createTexture(RenderTextureDesc::Texture2D(tex->width,tex->height,1,
                                        RenderFormat::R8G8B8A8_UNORM,RenderTextureFlag::RENDER_TARGET));
                                    sceneAAWidth=tex->width;sceneAAHeight=tex->height;
                                }
                                if(sceneAAOutput && sceneProcessor->ProcessSceneColor(commandList.get(),tex->texture.get(),sceneAAOutput.get(),
                                    tex->width,tex->height,static_cast<gpu::Antialiasing>(sceneAAMode==3?2:sceneAAMode))) {
                                    tex->layout=RenderTextureLayout::SHADER_READ;
                                    temporalDisplay=sceneAAOutput.get();sceneAABusy=true;sceneAARecorded=true;
                                    QueueResolveTrace(*tex,0xffff0011u);
                                    QueueResolveTrace(temporalDisplay,RenderFormat::R8G8B8A8_UNORM,tex->width,tex->height,RenderTextureLayout::SHADER_READ,0xffff0012u);
                                }
                            }
                        }
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
                        shared.samplerIndex[slot] = GetSamplerIndex(samplerKey);
                        set->setTexture(slot, temporalDisplay?temporalDisplay:tex->texture.get(), RenderTextureLayout::SHADER_READ);
                    }
                };
                bindTextures(ps);
                bindTextures(vs);
                tBind += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tBind0).count();
                auto tIndex0 = std::chrono::steady_clock::now();

                uint64_t vsOffset = Upload(vsConstants, sizeof(vsConstants));
                uint64_t psOffset = Upload(psConstants, sizeof(psConstants));
                uint64_t sharedOffset = Upload(&shared, sizeof(shared));
                if (vsOffset == UINT64_MAX || psOffset == UINT64_MAX || sharedOffset == UINT64_MAX)
                {
                    drops.upload++;
                    return;
                }

                // Index buffer / primitive conversion.
                std::vector<uint32_t> indices;
                bool useIndices = false;
                RenderFormat indexFormat = RenderFormat::R32_UINT;
                uint32_t indexCount = info.indexCount;
                if (info.indexed)
                {
                    uint32_t count = std::min<uint32_t>(info.indexCount, info.indexBufferWords);
                    indices.resize(count);
                    const uint8_t* src = Phys(info.indexBase);
                    for (uint32_t i = 0; i < count; i++)
                    {
                        uint32_t v;
                        if (info.index32) { memcpy(&v, src + i * 4, 4); v = GpuSwap(v, info.indexEndian); }
                        else { uint16_t s16; memcpy(&s16, src + i * 2, 2); v = GpuSwap(s16, info.indexEndian) & 0xFFFF; }
                        indices[i] = v;
                    }
                    useIndices = true;
                }
                switch (info.primitiveType)
                {
                case 13: // quad list -> triangle list
                {
                    std::vector<uint32_t> out;
                    uint32_t quads = (useIndices ? uint32_t(indices.size()) : info.indexCount) / 4;
                    out.reserve(quads * 6);
                    for (uint32_t q = 0; q < quads; q++)
                    {
                        uint32_t v[4];
                        for (int k = 0; k < 4; k++) v[k] = useIndices ? indices[q * 4 + k] : q * 4 + k;
                        out.insert(out.end(), { v[0], v[1], v[2], v[0], v[2], v[3] });
                    }
                    indices = std::move(out);
                    useIndices = true;
                    break;
                }
                case 5: // triangle fan -> list
                {
                    std::vector<uint32_t> out;
                    uint32_t n = useIndices ? uint32_t(indices.size()) : info.indexCount;
                    for (uint32_t i = 2; i < n; i++)
                    {
                        uint32_t a = useIndices ? indices[0] : 0, b = useIndices ? indices[i - 1] : i - 1, c = useIndices ? indices[i] : i;
                        out.insert(out.end(), { a, b, c });
                    }
                    indices = std::move(out);
                    useIndices = true;
                    break;
                }
                default:
                    break;
                }
                if (useIndices)
                    indexCount = uint32_t(indices.size());
                if (indexCount == 0)
                {
                    drops.index++;
                    drops.primMask |= 1u << (info.primitiveType & 31);
                    return;
                }

                tIndex += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tIndex0).count();

                // Offline vertex replay: capture one frame of relative-addressed
                // draws with their constants, indices and current guest streams.
                // Stream files are swapped CPU snapshots, not upload-heap reads.
                const auto& captureEnvironment = GetHotCaptureEnvironment();
                if (captureEnvironment.geometryCaptureEnabled)
                {
                    const char* captureDir = captureEnvironment.geometryCaptureDir.c_str();
                    static const uint32_t captureFrame = getenv("LO_GEOMETRY_CAPTURE_FRAME")
                        ? strtoul(getenv("LO_GEOMETRY_CAPTURE_FRAME"), nullptr, 10) : 2400;
                    static uint32_t captureDraw = 0;
                    if (frame == captureFrame && vs->info.usesRelativeConstants)
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
                        for (uint32_t slot = 0; slot < kVertexFetchSlots; ++slot)
                        {
                            if (!((vs->info.vertexFetchSlotMask[slot >> 6] >> (slot & 63)) & 1)) continue;
                            uint32_t d0 = Reg(REG_FETCH_CONSTANTS + slot * 2), d1 = Reg(REG_FETCH_CONSTANTS + slot * 2 + 1);
                            uint32_t words = (d1 >> 2) & 0xFFFFFF;
                            std::vector<uint32_t> stream(words);
                            CopySwapped(stream.data(), Phys(d0 & ~3u), words, d1 & 3);
                            const std::string name = fmt::format("vb_{:016x}.bin", Fnv1a(stream.data(), stream.size() * 4));
                            const std::string path = std::string(captureDir) + "/" + name;
                            if (!std::filesystem::exists(path)) save(path, stream.data(), stream.size() * 4);
                            meta << fmt::format("fetch{}={} address={:#x} words={} endian={}\n", slot, name, d0 & ~3u, words, d1 & 3);
                        }
                    }
                }

                // Record.
                ScopedTimer recordTimer{ tRecord };
                RenderFramebuffer* framebuffer = GetFramebuffer(color, depth);
                commandList->setFramebuffer(framebuffer);
                commandList->setViewports(&rasterViewport, 1);
                RenderRect scissor(int32_t(scissorTl & 0x3FFF), int32_t((scissorTl >> 16) & 0x3FFF), int32_t(scissorBr & 0x3FFF), int32_t((scissorBr >> 16) & 0x3FFF));
                uint32_t windowOffset = Reg(REG_PA_SC_WINDOW_OFFSET);
                if (!(scissorTl & 0x80000000u) && windowOffset)
                {
                    int32_t ox = int32_t(windowOffset << 17) >> 17, oy = int32_t(windowOffset << 1) >> 17;
                    scissor.left += ox; scissor.right += ox; scissor.top += oy; scissor.bottom += oy;
                }
                scissor.left = std::clamp(scissor.left, 0, int32_t(pitch)); scissor.right = std::clamp(scissor.right, 0, int32_t(pitch));
                scissor.top = std::clamp(scissor.top, 0, int32_t(rtHeight)); scissor.bottom = std::clamp(scissor.bottom, 0, int32_t(rtHeight));
                scissor.left = int32_t(color->Scale(uint32_t(scissor.left))); scissor.right = int32_t(color->Scale(uint32_t(scissor.right)));
                scissor.top = int32_t(color->Scale(uint32_t(scissor.top))); scissor.bottom = int32_t(color->Scale(uint32_t(scissor.bottom)));
                // Height is a historical EDRAM allocation estimate. Attachments
                // may have different padding while covering the same draw; keep
                // that draw and constrain it to their common physical extent.
                const int32_t attachmentWidth = int32_t(depth ? std::min(color->width, depth->width) : color->width);
                const int32_t attachmentHeight = int32_t(depth ? std::min(color->height, depth->height) : color->height);
                scissor.left = std::min(scissor.left, attachmentWidth); scissor.right = std::min(scissor.right, attachmentWidth);
                scissor.top = std::min(scissor.top, attachmentHeight); scissor.bottom = std::min(scissor.bottom, attachmentHeight);
                if (scissor.right <= scissor.left || scissor.bottom <= scissor.top)
                {
                    drops.scissor++;
                    return;
                }
                commandList->setScissors(&scissor, 1);
                commandList->setPipeline(pipeline);
                commandList->setGraphicsPipelineLayout(pipelineLayout.get());
                SetConstantBuffer(vsOffset, 0);
                SetConstantBuffer(sharedOffset, 1);
                SetConstantBuffer(psOffset, 2);
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
                        LOG_INFO("renderer: ps trace f{} ps={:016x} indices={}{}", frame, key.ps, info.indexCount, values);
                    }
                }
                if (!debugCaptureDir.empty())
                {
                    debugTrace << fmt::format("shaders vs={:016x} ps={:016x}\n", key.vs, key.ps);
                    for (const auto& entry : {std::make_pair(key.vs, vs), std::make_pair(key.ps, ps)})
                    {
                        if (!entry.second) continue;
                        const auto path = debugCaptureRoot / "shaders" / fmt::format("{:016x}.hlsl", entry.first);
                        if (debugShaders.insert(entry.first).second)
                        {
                            std::ofstream out(path); out << entry.second->info.hlsl; out.close();
                            if (out.fail()) debugTrace.setstate(std::ios::failbit);
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
                        LOG_INFO("renderer: draw consts{}", consts);
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
                    LOG_INFO("renderer: draw detail{}", detail);
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
                        LOG_INFO("renderer: draw psconsts f{} ps={:016x} upload={:#x}{}", frame, key.ps, psOffset, values);
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
                    LOG_INFO("renderer: draw textures{}", texs);
                }
                if (drawLogs < 24 || (traceFrame && frame >= traceFrame && frame < traceFrame + traceCount))
                {
                    drawLogs++;
                    LOG_INFO("renderer: clip f{} vs={:016x} ps={:016x} control={:#x}", frame, key.vs, key.ps, Reg(0x2204));
                    LOG_INFO("renderer: draw f{} prim={} n={} idx={} vs={:016x} ps={:016x} rt={:#x}/{} {}x{} depth={:#x} dinfo={:#x} blend={:#x} mask={:#x} cull={:#x} colorctl={:#x} aref={:g} ring(vs={:#x} ps={:#x} sh={:#x}) vp=({},{} {}x{} z {}..{}) vte={:#x} scissor=({},{})-({},{}) ndc=({},{}) off=({},{}) mode={} c255=({:g},{:g},{:g},{:g})",
                        frame, info.primitiveType, indexCount, useIndices, key.vs, key.ps, colorInfo & 0xFFF, (colorInfo >> 16) & 0xF, pitch, rtHeight, depthControl, depthInfo,
                        key.blend, key.colorMask, key.modeCull, Reg(REG_RB_COLORCONTROL), RegF(REG_RB_ALPHA_REF), vsOffset, psOffset, sharedOffset,
                        viewport.x, viewport.y, viewport.width, viewport.height, viewport.minDepth, viewport.maxDepth, vte,
                        scissor.left, scissor.top, scissor.right, scissor.bottom, shared.ndcScale[0], shared.ndcScale[1], shared.ndcOffset[0], shared.ndcOffset[1], modeControl,
                        RegF(REG_ALU_CONSTANTS + 255 * 4), RegF(REG_ALU_CONSTANTS + 255 * 4 + 1), RegF(REG_ALU_CONSTANTS + 255 * 4 + 2), RegF(REG_ALU_CONSTANTS + 255 * 4 + 3));
                }
                if (useIndices)
                {
                    uint64_t offset = Upload(indices.data(), indices.size() * 4, 16);
                    if (offset == UINT64_MAX)
                        return;
                    RenderIndexBufferView view(RenderBufferReference(uploadRing.get(), offset), uint32_t(indices.size() * 4), RenderFormat::R32_UINT);
                    commandList->setIndexBuffer(&view);
                    commandList->drawIndexedInstanced(indexCount, 1, 0, baseVertex, 0);
                }
                else
                {
                    commandList->drawInstanced(indexCount, 1, uint32_t(baseVertex), 0);
                }
                drawsThisFrame++;
                // Opt-in, bounded diagnostics of constants actually uploaded for
                // a submitted draw. F1's guest register trace precedes these edits.
                static const uint64_t jitterLogStart = getenv("LO_TEMPORAL_DRAW_LOG_START_FRAME") ?
                    strtoull(getenv("LO_TEMPORAL_DRAW_LOG_START_FRAME"), nullptr, 10) : ~0ull;
                static const uint64_t jitterLogVs = getenv("LO_TEMPORAL_DRAW_LOG_VS") ?
                    strtoull(getenv("LO_TEMPORAL_DRAW_LOG_VS"), nullptr, 16) : 0;
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
                    key.vs == 0x400df7c5a60819f5ull || key.vs == 0x08dcef32bd434f8cull;
                const bool jitterLogSkinned = key.vs == 0x0eb223d33f8e8e0cull || key.vs == 0x1e9017d2b296f480ull;
                const bool jitterLogGeometry = !jitterLogIndexCount ||
                    (jitterLogStaticMesh ? info.indexCount == jitterLogIndexCount :
                        key.vs == 0x99c2b4b0960a9ccdull ? jitterShadowPair && jitterLoggedShadowFrame != frame :
                        key.vs == 0x3148f81d65d3b5f4ull ? jitterLoggedCharacterFrame != frame : true);
                if (((frame >= jitterLogStart && frame - jitterLogStart < 32) ||
                    (jitterLogWithResolveTrace && resolveTraceRemaining)) &&
                    jitterLogGeometry &&
                    (jitterLogVs ? key.vs == jitterLogVs :
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
                    LOG_INFO("renderer temporal draw f{} submitted={} vs={:016x} ps={:016x} slot={} log_slot={} indices={} index_base={:x} base_vertex={} fetch95={:08x},{:08x} world_c0_c3={:016x} enabled={} viewport={} applied={} shadow={} rejection={} phase={} ndc=({:.9g},{:.9g}) extent={}x{} depth={} layer_bias={:.9g} sampled_depth={:x}/{} scene_depth={:x}/{} vp_guest=[{}] vp_upload=[{}] ps_c2_c5_guest=[{}] ps_c2_c5_upload=[{}]",
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
                if(fullSceneCopy)color->aaProvenance.Invalidate(frame,color->allocationSerial,
                    rasterViewport.width>=color->aaValidWidth && rasterViewport.height>=color->aaValidHeight);
                if(sceneAARecorded||temporalAARecorded) {
                    color->aaProvenance.MarkFull(frame,color->allocationSerial);
                    color->aaValidWidth=uint32_t(rasterViewport.width);color->aaValidHeight=uint32_t(rasterViewport.height);
                    if(temporalAARecorded)temporalSupportedFrame=frame;
                    sceneAAAppliedFrame=frame;sceneAAAllocation=color->allocationSerial;
                    static const uint64_t logStart=getenv("LO_SCENE_AA_LOG_START_FRAME")?strtoull(getenv("LO_SCENE_AA_LOG_START_FRAME"),nullptr,10):~0ull;
                    if(frame>=logStart&&frame-logStart<128)LOG_INFO("renderer scene AA f{} mode={} temporal={} allocation={} full_copy=1 recorded=1",frame,sceneAAMode,temporalAARecorded,color->allocationSerial);
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
                            tex->aaProvenance.Invalidate(frame,tex->allocationSerial,true);
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
                            const auto mapped = MapDepthClear(pitch, (surfaceInfo >> 16) & 3, sourceRect,
                                k.pitch, target->guestHeight, target->depthMsaa);
                            std::vector<RenderRect> clearRects;
                            clearRects.reserve(mapped.size());
                            for (const auto& r : mapped) clearRects.push_back({int32_t(target->Scale(uint32_t(r.left))), int32_t(target->Scale(uint32_t(r.top))),
                                int32_t(target->Scale(uint32_t(r.right))), int32_t(target->Scale(uint32_t(r.bottom)))});
                            // A zero rectangle count means a whole-resource clear in
                            // the graphics API, so an empty mapping must be skipped.
                            if (!clearRects.empty())
                            {
                                if (getenv("LO_TRACE_CLEAR_CALL"))
                                    LOG_INFO("clear begin f{} base={} pitch={} size={}x{} count={} z={}", frame, k.base, k.pitch, target->width, target->height, clearRects.size(), rectZ);
                                commandList->clearDepthStencil(true, false, rectZ, 0, clearRects.data(), uint32_t(clearRects.size()));
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
                        mappedRows = target->Scale(std::clamp<uint32_t>(mappedRows, 1, target->guestHeight));
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
                        if (logged++ < 8)
                            LOG_INFO("renderer: colour clear rect (pitch {}, {} rows) replayed into base={:#x} pitch={} {}x{} rows", pitch, clearedRows, k.base, k.pitch, target->width, mappedRows);
                    }
                }

                // Debug snapshot goes last: it flushes, which would drop the
                // pipeline state the clear replay above still relies on.
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
                Begin();
                Transition(tex, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
                commandList->copyTextureRegion(
                    RenderTextureCopyLocation::PlacedFootprint(readback.get(), tex.format, tex.width, tex.height, 1, rowPitch / bpp, 0),
                    RenderTextureCopyLocation::Subresource(tex.texture.get(), 0));
                Flush();
                Begin();
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
                Flush();
                Begin();
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
            void ResolveDepthOnGpu(HostTexture& depth, uint32_t destBase, uint32_t destFormat, uint32_t destPitch, uint32_t destHeight,
                                   uint32_t x0, uint32_t y0, uint32_t w, uint32_t h)
            {
                Begin();
                const uint32_t guestW = std::clamp<uint32_t>(std::max(destPitch, x0 + w), 1, 8192);
                const uint32_t guestH = std::clamp<uint32_t>(std::max(destHeight, y0 + h), 1, 8192);
                const uint32_t texW = std::max(1u, depth.Scale(guestW)), texH = std::max(1u, depth.Scale(guestH));
                if (texW > 16384 || texH > 16384) {
                    resolutionAllocationFailed = true;
                    LOG_ERROR("renderer: depth resolve exceeds texture limit physical={}x{}; native resolution fallback next frame", texW, texH);
                    return;
                }
                w = depth.Scale(x0 + w) - depth.Scale(x0); h = depth.Scale(y0 + h) - depth.Scale(y0);
                x0 = depth.Scale(x0); y0 = depth.Scale(y0);
                ResolvedSurface& rs = ResolvedSlot(destBase, destFormat);
                if (!rs.tex || rs.tex->format != RenderFormat::R32_FLOAT || rs.tex->width != texW || rs.tex->height != texH)
                {
                    if (rs.tex) retiredTextures.push_back(std::move(rs.tex));
                    rs.writeOrdinal = 0; // The replacement allocation contains no previous resolve.
                    rs.writeWidth = rs.writeHeight = 0;
                    rs.tex = std::make_unique<HostTexture>();
                    rs.tex->format = RenderFormat::R32_FLOAT;
                    rs.tex->width = texW;
                    rs.tex->height = texH;
                    rs.tex->guestWidth = guestW; rs.tex->guestHeight = guestH;
                    rs.tex->resolutionHeight = depth.resolutionHeight;
                    rs.tex->texture = device->createTexture(RenderTextureDesc::Texture2D(texW, texH, 1, RenderFormat::R32_FLOAT,
                        vulkan ? RenderTextureFlag::RENDER_TARGET : RenderTextureFlag::NONE));
                    rs.tex->layout = RenderTextureLayout::UNKNOWN;
                    if (!rs.tex->texture)
                    {
                        resolutionAllocationFailed = true;
                        LOG_ERROR("renderer: depth resolve allocation failed guest={}x{} physical={}x{}; native resolution fallback next frame", guestW, guestH, texW, texH);
                        DropResolved(destBase, destFormat);
                        return;
                    }
                    static uint32_t created = 0;
                    if (created++ < 8)
                        LOG_INFO("renderer: resolved depth surface {:#x} guest={}x{} physical={}x{} dest fmt={}", destBase, guestW, guestH, texW, texH, destFormat & 0xFFF);
                }
                rs.destFormat = destFormat;
                rs.destPitch = destPitch;
                rs.swapRedBlue = false;
                if (x0 >= texW || y0 >= texH) return;
                w = std::min(w, texW - x0);
                h = std::min(h, texH - y0);
                if (w == 0 || h == 0)
                    return;
                if (vulkan) {
                    // Vulkan 1.2 image copies cannot reinterpret D32/S8 storage
                    // as an R32 color plane. Load the depth aspect and write the
                    // exact float value, retaining the destination rectangle.
                    if (!BlitRegion(depth, *rs.tex, x0, y0, w, h)) {
                        LOG_ERROR("renderer: Vulkan depth resolve blit failed");
                        return;
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
                if (!debugCaptureDir.empty() || temporalExperiment || sceneAAEnabled) {
                    temporalScene.ObserveDepth(depth.allocationSerial, {frame, rs.writeOrdinal, destBase, destFormat,
                        texW, texH, x0 == 0 && y0 == 0 && w == texW && h == texH});
                    if(temporalExperiment && temporalHistory && temporalScene.Depth().ordinal==rs.writeOrdinal) {
                        Transition(*rs.tex,RenderTextureLayout::COPY_SOURCE,RenderBarrierStage::COPY);
                        temporalHistory->CaptureDepth(commandList.get(),rs.tex->texture.get(),temporalScene);
                        Transition(*rs.tex,RenderTextureLayout::SHADER_READ,RenderBarrierStage::GRAPHICS);
                    }
                }
                DumpResolveStep(*rs.tex, destBase, rs.writeOrdinal, rs.destFormat);
            }

            // Copies the resolve rectangle into the host texture standing in for the
            // destination memory (destPitch x destHeight texels, rectangle placed at its
            // window position). No GPU sync, no tiling, no guest memory writes.
            void ResolveOnGpu(HostTexture& color, uint32_t destBase, uint32_t destFormat, uint32_t destPitch, uint32_t destHeight,
                              uint32_t x0, uint32_t y0, uint32_t w, uint32_t h)
            {
                Begin();
                const uint32_t guestW = std::clamp<uint32_t>(std::max(destPitch, x0 + w), 1, 8192);
                const uint32_t guestH = std::clamp<uint32_t>(std::max(destHeight, y0 + h), 1, 8192);
                const uint32_t texW = std::max(1u, color.Scale(guestW)), texH = std::max(1u, color.Scale(guestH));
                if (texW > 16384 || texH > 16384) {
                    resolutionAllocationFailed = true;
                    LOG_ERROR("renderer: color resolve exceeds texture limit physical={}x{}; native resolution fallback next frame", texW, texH);
                    return;
                }
                w = color.Scale(x0 + w) - color.Scale(x0); h = color.Scale(y0 + h) - color.Scale(y0);
                x0 = color.Scale(x0); y0 = color.Scale(y0);
                // The destination's own format decides what the surface holds, so the
                // frontbuffer stays 8888 even though EDRAM is kept in FP16.
                const RenderFormat destHost = (destFormat == 32 || destFormat == 7) ? RenderFormat::R16G16B16A16_FLOAT : RenderFormat::R8G8B8A8_UNORM;
                ResolvedSurface& rs = ResolvedSlot(destBase, destFormat);
                if (!rs.tex || rs.tex->format != destHost || rs.tex->width != texW || rs.tex->height != texH)
                {
                    if (rs.tex) retiredTextures.push_back(std::move(rs.tex));
                    rs.writeOrdinal = 0; // The replacement allocation contains no previous resolve.
                    rs.writeWidth = rs.writeHeight = 0;
                    rs.tex = std::make_unique<HostTexture>();
                    rs.tex->format = destHost;
                    rs.tex->width = texW;
                    rs.tex->height = texH;
                    rs.tex->guestWidth = guestW; rs.tex->guestHeight = guestH;
                    rs.tex->resolutionHeight = color.resolutionHeight;
                    rs.tex->texture = device->createTexture(RenderTextureDesc::Texture2D(texW, texH, 1, destHost, RenderTextureFlag::RENDER_TARGET));
                    rs.tex->layout = RenderTextureLayout::UNKNOWN;
                    if (!rs.tex->texture)
                    {
                        resolutionAllocationFailed = true;
                        LOG_ERROR("renderer: color resolve allocation failed guest={}x{} physical={}x{}; native resolution fallback next frame", guestW, guestH, texW, texH);
                        DropResolved(destBase, destFormat);
                        return;
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
                    static uint32_t created = 0;
                    if (created++ < 16)
                        LOG_INFO("renderer: resolved surface {:#x} guest={}x{} physical={}x{} host fmt={} dest fmt={}", destBase, guestW, guestH, texW, texH, uint32_t(color.format), destFormat);
                }
                rs.destFormat = destFormat;
                rs.destPitch = destPitch;
                rs.swapRedBlue = ((Reg(REG_RB_COPY_DEST_INFO) >> 24) & 1) != 0;
                if (x0 >= texW || y0 >= texH) return;
                w = std::min(w, texW - x0);
                h = std::min(h, texH - y0);
                if (w == 0 || h == 0)
                    return;
                if (rs.tex->format == color.format)
                {
                    Transition(color, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
                    Transition(*rs.tex, RenderTextureLayout::COPY_DEST, RenderBarrierStage::COPY);
                    RenderBox box{ int32_t(x0), int32_t(y0), int32_t(x0 + w), int32_t(y0 + h), 0, 1 };
                    commandList->copyTextureRegion(RenderTextureCopyLocation::Subresource(rs.tex->texture.get()),
                        RenderTextureCopyLocation::Subresource(color.texture.get()), x0, y0, 0, &box);
                }
                else if (!BlitRegion(color, *rs.tex, x0, y0, w, h))
                    return;
                Transition(*rs.tex, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
                rs.frame = frame;
                rs.writeOrdinal = ++resolveWriteOrdinal;
                rs.writeX = x0; rs.writeY = y0; rs.writeWidth = w; rs.writeHeight = h;
                auto sourceCoverage=color.aaProvenance.Get(frame,color.allocationSerial);
                if(sourceCoverage==scene_aa::Coverage::Full && (uint64_t(x0)+w>color.aaValidWidth || uint64_t(y0)+h>color.aaValidHeight))
                    sourceCoverage=scene_aa::Coverage::Mixed;
                rs.tex->aaProvenance.Resolve(frame,rs.tex->allocationSerial,sourceCoverage,
                    x0==0&&y0==0&&w==texW&&h==texH);
                DumpResolveStep(*rs.tex, destBase, rs.writeOrdinal, rs.destFormat);
            }

            void Resolve()
            {
                ScopedTimer timer{ tResolve };
                nResolve++;
                ResolveImpl();
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
                        ResolveDepthOnGpu(*depthRt, destBase, destFormat | kDepthResolveTag, destPitch, destHeight, x0, y0, copyWidth, copyHeight);
                        InvalidateRange(destBase, ((destPitch + 31) & ~31u) * copyHeight * 4);
                    }
                    else
                    {
                        static bool warned = false;
                        if (!warned) { LOG_WARNING("renderer: depth resolve readback not implemented"); warned = true; }
                    }
                    Begin();
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
                    ResolveOnGpu(*color, destBase, destFormat, destPitch, destHeight, x0, y0, copyWidth, copyHeight);
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
                Flush();

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

                Begin();
                if (copyControl & 0x100)
                {
                    uint32_t clear = Reg(REG_RB_COLOR_CLEAR);
                    RenderColor c(float((clear >> 16) & 0xFF) / 255.0f, float((clear >> 8) & 0xFF) / 255.0f, float(clear & 0xFF) / 255.0f, float(clear >> 24) / 255.0f);
                    Transition(*color, RenderTextureLayout::COLOR_WRITE, RenderBarrierStage::GRAPHICS);
                    commandList->setFramebuffer(GetFramebuffer(color, nullptr));
                    RenderRect rect{ int32_t(color->Scale(x0)), int32_t(color->Scale(y0)), int32_t(color->Scale(x1)), int32_t(color->Scale(y1)) };
                    commandList->clearColor(0, c, &rect, 1);
                    color->aaProvenance.Invalidate(frame,color->allocationSerial,x0==0&&y0==0&&x1==color->guestWidth&&y1==color->guestHeight);
                }
                if (copyControl & 0x200)
                    ClearDepthTarget(pitch, rtHeight);
            }

            void ClearDepthTarget(uint32_t pitch, uint32_t rtHeight)
            {
                uint32_t depthInfo = Reg(REG_RB_DEPTH_INFO);
                HostTexture* depth = GetRenderTarget(depthInfo & 0xFFF, (depthInfo >> 16) & 1, pitch, rtHeight, true);
                if (!depth || !depth->texture) return;
                Transition(*depth, RenderTextureLayout::DEPTH_WRITE, RenderBarrierStage::GRAPHICS);
                commandList->setFramebuffer(GetFramebuffer(nullptr, depth));
                uint32_t clear = Reg(REG_RB_DEPTH_CLEAR);
                commandList->clearDepthStencil(true, true, float(clear >> 8) / 16777215.0f, clear & 0xFF);
            }
        };

        Renderer* g_renderer = nullptr;
    }

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

    void FinishDebugCapture(uint32_t frontbuffer)
    {
        { std::lock_guard lock(captureMutex); UpdateCaptureArchive(); }
        if (!g_renderer || g_renderer->debugCaptureDir.empty()) return;
        auto& r = *g_renderer;
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
                         << ",\"resolution_height\":" << rs.tex->resolutionHeight
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
            std::vector<uint32_t> pixels;
            uint32_t width = 0, height = 0;
            if (ReadbackResolvedSurface(frontbuffer, pixels, width, height))
            {
                // A standard top-down 32-bit BMP can be opened directly on Windows.
                std::ofstream bmp(std::filesystem::path(r.debugCaptureDir) / "screenshot.bmp", std::ios::binary);
                auto put = [&](uint32_t value, int bytes) { for (int i = 0; i < bytes; ++i) bmp.put(char(value >> (8 * i))); };
                bmp.write("BM", 2); put(54 + width * height * 4, 4); put(0, 4); put(54, 4);
                put(40, 4); put(width, 4); put(uint32_t(-int32_t(height)), 4); put(1, 2); put(32, 2);
                put(0, 4); put(width * height * 4, 4); put(0, 4); put(0, 4); put(0, 4); put(0, 4);
                for (auto pixel : pixels) { bmp.put(char(pixel >> 16)); bmp.put(char(pixel >> 8)); bmp.put(char(pixel)); bmp.put(0); }
                bmp.close();
                ok = !bmp.fail();
            }
            r.debugTrace << fmt::format("end frame={} frontbuffer={:#x} size={}x{} submitted_draws={} screenshot={}\n",
                r.frame, frontbuffer, width, height, r.drawsThisFrame, ok);
            r.debugTrace << fmt::format("drops mode={} shader={} pitch={} pipeline={} upload={} index={} scissor={} dummy_bindings={}\n",
                r.drops.mode, r.drops.shader, r.drops.pitch, r.drops.pipeline, r.drops.upload, r.drops.index, r.drops.scissor, r.dummyBindings);
            r.debugTrace.close();
            ok = ok && !r.debugTrace.fail();
        }
        catch (const std::exception& e) { ok = false; LOG_ERROR("render capture finish: {}", e.what()); }
        LOG_INFO("render capture {}: {}", ok ? "saved" : "incomplete", FileSystem::PathUtf8(std::filesystem::path(r.debugCaptureDir)));
        if (ok) ++r.debugCaptureCompleted;
        try
        {
            std::ofstream manifest(r.debugCaptureRoot / "capture-info.txt");
            manifest << "requested_frames=" << r.debugCaptureFrameCount
                << "\ncompleted_frames=" << r.debugCaptureCompleted
                << "\nfirst_frame=" << r.debugCaptureFirstFrame
                << "\nlast_attempted_frame=" << r.frame
                << "\nstatus=" << (!ok ? "incomplete" : r.debugCaptureCompleted == r.debugCaptureFrameCount ? "complete" : "capturing")
                << "\nFrames are consecutive rendered frames. Capture readbacks may stall execution.\n"
                << "Each frame directory contains its own screenshot, register trace, resolves and metadata.\n"
                << "Shaders are deduplicated in shaders/. runtime.log is flushed after the last captured frame.\n"
                << "Default omissions: draw-step previews, duplicate screenshot.ppm and duplicate depth .f32.\n"
                << "Use LO_DEBUG_CAPTURE_DRAW_STEPS=1 to include draw-step previews.\n";
            manifest.close();
            if (manifest.fail()) throw std::runtime_error("Cannot write capture-info.txt");
        }
        catch (const std::exception& e) { ok = false; LOG_ERROR("render capture manifest: {}", e.what()); }
        if (ok && r.debugCaptureCompleted < r.debugCaptureFrameCount)
        {
            r.debugTrace.clear();
            r.debugCaptureDir.clear();
            r.captureFrame = 0;
            std::lock_guard lock(captureMutex);
            capturePending = true;
            captureStatus = L"等待下一帧 / Waiting for frame " + std::to_wstring(r.debugCaptureCompleted + 1) + L"/3";
            return;
        }
        // Snapshot the log owned by this process after frame export, before ZIP
        // creation. A disabled or unavailable log must not discard the capture.
        try
        {
            const auto directory = r.debugCaptureRoot;
            const auto error = os::logger::SnapshotFile(directory / "runtime.log");
            std::ofstream status(directory / "runtime-log-status.txt");
            status << "frame=" << r.frame << '\n';
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
        // Detach the completed capture from the renderer before starting the
        // worker. Subsequent frames cannot append to or use its source files.
        const auto directory = std::move(r.debugCaptureRoot);
        r.debugTrace.close();
        r.debugTrace.clear();
        r.debugCaptureDir.clear();
        r.debugCaptureRoot.clear();
        r.debugCaptureCompleted = 0;
        r.captureFrame = 0;
        std::lock_guard lock(captureMutex);
        if (ok)
        {
            try
            {
                captureArchive = os::StartCaptureArchive(directory);
                captureStatus = L"后台压缩 ZIP，可继续游戏 / Compressing ZIP in background";
                LOG_INFO("render capture ZIP started in background: {}", FileSystem::PathUtf8(directory));
                return;
            }
            catch (const std::exception& e) { LOG_ERROR("render capture ZIP start: {}", e.what()); }
        }
        captureStatus = (ok ? L"ZIP 失败，原始文件保留 / ZIP failed: " :
            L"导出不完整 / Incomplete: ") + directory.wstring();
        captureBusy = false;
    }

    void Flush()
    {
        if (g_renderer)
        {
            static const bool stats = getenv("LO_GPU_STATS") != nullptr;
            static auto lastFrame = std::chrono::steady_clock::now();
            g_renderer->Flush();
            if (stats)
            {
                auto now = std::chrono::steady_clock::now();
                double frameMs = std::chrono::duration<double, std::milli>(now - lastFrame).count();
                lastFrame = now;
                Renderer& r = *g_renderer;
                if (frameMs > 150.0 || (r.frame % 60) == 0)
                    LOG_INFO("renderer frame {}: {:.0f} ms, draws {} ({:.0f} ms: const {:.0f} sets {:.0f} vertex {:.0f} bind {:.0f} index {:.0f} record {:.0f}), shaders {} ({:.0f} ms), pipelines {} ({:.0f} ms), textures {} ({:.0f} ms, {} KB), vertex uploads {}+{} ({} KB, arena {} MB), resolves {} ({:.0f} ms), gpu wait {:.0f} ms",
                        r.frame, frameMs, r.drawsThisFrame, r.tDraw, r.tConst, r.tSets, r.tVertex, r.tBind, r.tIndex, r.tRecord, r.nShader, r.tShader, r.nPipeline, r.tPipeline, r.nTexture, r.tTexture, r.texBytes / 1024,
                        r.vertexUploads, r.vertexRevalidations, r.vertexBytesUploaded / 1024, r.arenaOffset >> 20, r.nResolve, r.tResolve, r.tFlush);
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
            g_renderer->FinishResolveTraceFrame();
            if(auto& owner=g_renderer->temporalHistory;owner&&g_renderer->temporalExperiment) {
                auto& r=*g_renderer;
                const auto now=std::chrono::steady_clock::now();
                const bool gap=now-r.temporalFrameTime>std::chrono::milliseconds(250);
                const bool complete=r.temporalScene.Frame()==r.frame&&r.temporalScene.Ready()&&owner->Completed()&&r.temporalSubmittedFrame==r.frame;
                static const uint32_t temporalLogStart=getenv("LO_TEMPORAL_LOG_START_FRAME")?strtoul(getenv("LO_TEMPORAL_LOG_START_FRAME"),nullptr,10):0;
                if(r.frame>=temporalLogStart&&r.temporalFramesLogged<256) {
                    LOG_INFO("renderer temporal f{} epoch={} ready={} completed={} reused={} reason={} depth={} color={} gap={} jitter_draws={} jitter_misses={}",r.frame,r.temporalEpoch,r.temporalScene.Ready(),complete,owner->Reused(),uint32_t(r.temporalScene.Reason()),r.temporalScene.Depth().ordinal,r.temporalScene.Color().ordinal,gap,r.temporalJitterDraws,r.temporalJitterMisses);
                    const auto& diagnostic=owner->Diagnostics();
                    if(diagnostic.captured) {
                        std::string reasons;
                        for(uint32_t bit=1;bit<=(1u<<9);bit<<=1)if(diagnostic.rejected&bit) {
                            if(!reasons.empty())reasons+='|';
                            reasons+=temporal::HistoryReuseRejectionName(temporal::HistoryReuseRejection(bit));
                        }
                        const auto& state=diagnostic.state;
                        LOG_INFO("renderer temporal gates f{} mask={:#x} reasons={} valid={} previous_completed={} stable={}/{} allow_history={} frames={}/{} epochs={}/{} allocations={}/{} camera_checks={}",
                            r.frame,diagnostic.rejected,reasons.empty()?"none":reasons,state.valid,state.previousCompleted,state.currentStable,state.previousStable,state.allowHistory,
                            state.currentFrame,state.previousFrame,state.currentEpoch,state.previousEpoch,state.currentAllocation,state.previousAllocation,diagnostic.cameraChecksAvailable);
                        auto logCamera=[&](const char* which,const std::optional<temporal::Camera>& camera) {
                            if(!camera) {LOG_INFO("renderer temporal camera f{} {} missing",r.frame,which);return;}
                            std::string vp;for(double value:camera->VP())vp+=fmt::format("{:.9g},",value);
                            const auto& v=camera->Raster();
                            LOG_INFO("renderer temporal camera f{} {} raster=({:.17g},{:.17g},{:.17g},{:.17g}) ndc_y={:.17g} half_pixel=({:.17g},{:.17g}) vp=[{}]",
                                r.frame,which,v.x,v.y,v.width,v.height,v.ndcYSign,v.halfPixelNdcX,v.halfPixelNdcY,vp);
                        };
                        logCamera("current",diagnostic.currentCamera);logCamera("previous",diagnostic.previousCamera);
                        if(diagnostic.cameraChecksAvailable)
                            LOG_INFO("renderer temporal depth_range f{} valid={} lower_bound={:.17g} far_world_w={:.17g} near_world_w={:.17g}",
                                r.frame,diagnostic.depthRange.valid,diagnostic.depthRange.lowerBound,diagnostic.depthRange.farWorldW,diagnostic.depthRange.nearWorldW);
                        if(diagnostic.cameraChecksAvailable&&diagnostic.depthRange.valid)for(const auto& probe:diagnostic.probes)
                            LOG_INFO("renderer temporal probe f{} depth={:.9g} rejection={} projected_valid={} projected=({:.17g},{:.17g},{:.17g}) delta_fraction=({:.17g},{:.17g}) quarter_screen_rejected={}",
                                r.frame,probe.depth,uint32_t(probe.rejection),probe.projectedValid,probe.projected.x,probe.projected.y,probe.projected.depth,probe.deltaXFraction,probe.deltaYFraction,probe.quarterScreenRejected);
                    }
                    ++r.temporalFramesLogged;
                }
                if(!complete||gap) {owner->Reset();r.temporalSupportedFrame=~0ull;++r.temporalEpoch;}
                r.temporalFrameTime=now;
                r.temporalJitterDraws=r.temporalJitterMisses=0;
            }
            g_renderer->SavePipelineRecipes();
            if (stats && g_renderer->frame % 600 == 0)
                LOG_INFO("renderer: pipeline reuse frame {}: {} prepared hits, {}/{} prepared keys used, {} runtime creates, {} recipes",
                    g_renderer->frame, g_renderer->preparedPipelineHits, g_renderer->usedPreparedPipelineKeys.size(),
                    g_renderer->preparedPipelineKeys.size(), g_renderer->runtimePipelineCreates, g_renderer->pipelineRecipes.size());
            g_renderer->PollPsTraceRequest();
            g_renderer->frame++;
            g_renderer->PollCaptureRequest();
            g_renderer->drawsThisFrame = 0;
            g_renderer->drops = {};
        }
    }

    void InvalidateGuestRange(uint32_t physicalAddress, uint32_t size)
    {
        if (g_renderer)
            g_renderer->InvalidateRange(physicalAddress, size);
    }

    plume::RenderTexture* AcquireResolvedSurface(uint32_t physicalAddress, uint32_t& width, uint32_t& height, uint32_t& format)
    {
        if (!g_renderer)
            return nullptr;
        auto* rs = g_renderer->NewestResolved(physicalAddress & 0x1FFFFFFF);
        if (!rs)
            return nullptr;
        HostTexture& tex = *rs->tex;
        g_renderer->Begin();
        g_renderer->Transition(tex, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
        g_renderer->Flush();
        width = tex.width;
        height = tex.height;
        format = uint32_t(tex.format);
        return tex.texture.get();
    }

    bool SceneAAApplied(uint32_t physicalAddress)
    {
        if(!g_renderer)return false;
        const auto* surface=g_renderer->NewestResolved(physicalAddress);
        if(!surface||!surface->tex||surface->frame+1!=g_renderer->frame)return false;
        auto coverage=surface->tex->aaProvenance.Get(surface->frame,surface->tex->allocationSerial);
        static const uint64_t start=getenv("LO_SCENE_AA_LOG_START_FRAME")?strtoull(getenv("LO_SCENE_AA_LOG_START_FRAME"),nullptr,10):~0ull;
        if(surface->frame>=start&&surface->frame-start<128)LOG_INFO("renderer scene AA present f{} address={:#x} coverage={} skip_final={}",surface->frame,physicalAddress,uint32_t(coverage),scene_aa::SkipFinalAA(coverage));
        return scene_aa::SkipFinalAA(coverage);
    }

    void ScaleResolvedSize(uint32_t physicalAddress, uint32_t& width, uint32_t& height)
    {
        if (!g_renderer) return;
        const auto* surface = g_renderer->NewestResolved(physicalAddress & 0x1FFFFFFF);
        if (!surface || !surface->tex) return;
        width = surface->tex->Scale(width);
        height = surface->tex->Scale(height);
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
        g_renderer->Begin();
        g_renderer->Transition(tex, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
        g_renderer->commandList->copyTextureRegion(
            RenderTextureCopyLocation::PlacedFootprint(g_renderer->readback.get(), copyFormat, tex.width, tex.height, 1, rowPitch / bpp, 0),
            RenderTextureCopyLocation::Subresource(tex.texture.get(), 0));
        g_renderer->Flush();
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
#else
    void FinishDebugCapture(uint32_t) {}
    bool Init() { return false; }
    void Shutdown() {}
    void ScaleResolvedSize(uint32_t, uint32_t&, uint32_t&) {}
    void Draw(const DrawInfo&) {}
    void Flush() {}
    void InvalidateGuestRange(uint32_t, uint32_t) {}
    bool SceneAAApplied(uint32_t) { return false; }
    plume::RenderTexture* AcquireResolvedSurface(uint32_t, uint32_t&, uint32_t&, uint32_t&) { return nullptr; }
    bool ReadbackResolvedSurface(uint32_t, std::vector<uint32_t>&, uint32_t&, uint32_t&) { return false; }
    std::vector<uint32_t> GetResolvedAddresses() { return {}; }
    void DumpRenderTargets(const char*) {}
#endif
    void SetOutputSize(uint32_t width, uint32_t height)
    {
        if (width && height) outputSize.store((uint64_t(width) << 32) | height, std::memory_order_relaxed);
    }
}
