#include <stdafx.h>
#include "renderer.h"
#include "video.h"
#include "command_processor.h"
#include "shader/xenos_translator.h"
#include "shader/dxc_compiler.h"
#include <kernel/memory.h>
#include <os/logger.h>

#ifdef LO_GPU_PLUME
#include <plume_render_interface.h>
#include <plume_render_interface_builders.h>
#endif

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace gpu::renderer
{
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
        constexpr uint32_t kReadbackSize = 32u << 20;
        constexpr uint32_t kVertexFetchSlots = 96;
        constexpr uint32_t kTextureSlots = 32;

        uint32_t Reg(uint32_t index) { return g_commandProcessor.ReadRegister(index); }
        float RegF(uint32_t index) { uint32_t v = Reg(index); float f; memcpy(&f, &v, 4); return f; }
        uint8_t* Phys(uint32_t physicalAddress) { return static_cast<uint8_t*>(g_memory.Translate(0xA0000000u + (physicalAddress & 0x1FFFFFFF))); }

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
            std::unique_ptr<RenderTexture> texture;
            RenderTextureLayout layout = RenderTextureLayout::UNKNOWN;
            RenderFormat format = RenderFormat::UNKNOWN;
            uint32_t width = 0, height = 0;
            // Guest-memory footprint and a sampled hash of it, so a texture the
            // title streams in after we first uploaded it is noticed and re-read.
            uint32_t guestAddress = 0, guestBytes = 0;
            uint64_t guestHash = 0;
            uint64_t checkedFrame = ~0ull;
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

        struct PipelineKey
        {
            uint64_t vs, ps;
            uint32_t blend, depthControl, modeCull, colorMask, prim, rtFormat, depthFormat, flags;
            bool operator==(const PipelineKey& o) const { return memcmp(this, &o, sizeof(o)) == 0; }
        };
        struct PipelineKeyHash { size_t operator()(const PipelineKey& k) const { return size_t(Fnv1a(&k, sizeof(k))); } };

        struct Renderer
        {
            RenderDevice* device = nullptr;
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
            RenderDescriptorSetBuilder setBuilders[4];
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
            std::unordered_map<RenderTargetKey, std::unique_ptr<HostTexture>, RenderTargetKeyHash> renderTargets;
            std::vector<std::unique_ptr<HostTexture>> retiredTextures; // replaced targets, freed after the next Flush
            std::unordered_map<TextureKey, std::unique_ptr<HostTexture>, TextureKeyHash> textures;

            // Resolve results kept on the GPU, keyed by guest physical address.
            // The game samples them through fetch constants pointing at the
            // same address, and the presenter copies the frontbuffer from here.
            struct ResolvedSurface
            {
                std::unique_ptr<HostTexture> tex;
                uint32_t destFormat = 0, destPitch = 0;
                uint64_t frame = 0;
            };
            std::unordered_map<uint32_t, ResolvedSurface> resolved;
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
            uint32_t frame = 0;
            std::set<uint32_t> loggedFormats;

            // ---- lifecycle -----------------------------------------------------
            bool Init()
            {
                device = video::GetDevice();
                queue = video::GetQueue();
                if (!device || !queue)
                    return false;

                commandList = queue->createCommandList();
                fence = device->createCommandFence();
                uploadRing = device->createBuffer(RenderBufferDesc::UploadBuffer(kUploadRingSize));
                uploadMapped = static_cast<uint8_t*>(uploadRing->map());
                vertexArena = device->createBuffer(RenderBufferDesc::UploadBuffer(kVertexArenaSize));
                arenaMapped = static_cast<uint8_t*>(vertexArena->map());
                readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(kReadbackSize));
                resolveReadback = getenv("LO_RESOLVE_READBACK") != nullptr;
                textureRevalidate = getenv("LO_TEXTURE_STATIC") == nullptr;
                dummyBuffer = device->createBuffer(RenderBufferDesc::DefaultBuffer(256));

                // Layout: root CBVs b0 (VS constants) b1 (shared) b2 (PS constants) in space0;
                // set0 = vertex fetch buffers t0-95 + samplers s0-31 (space0),
                // set1..3 = 2D / 3D / cube textures t0-31 (space1..3).
                RenderPipelineLayoutBuilder layout;
                layout.begin(false, false);
                layout.addRootDescriptor(0, 0, RenderRootDescriptorType::CONSTANT_BUFFER);
                layout.addRootDescriptor(1, 0, RenderRootDescriptorType::CONSTANT_BUFFER);
                layout.addRootDescriptor(2, 0, RenderRootDescriptorType::CONSTANT_BUFFER);
                setBuilders[0].begin();
                vfetchDescriptorBase = setBuilders[0].addByteAddressBuffer(0, kVertexFetchSlots);
                samplerDescriptorBase = setBuilders[0].addSampler(0, kSamplerPalette);
                setBuilders[0].end();
                for (int i = 1; i < 4; i++)
                {
                    setBuilders[i].begin();
                    setBuilders[i].addTexture(0, kTextureSlots);
                    setBuilders[i].end();
                }
                for (int i = 0; i < 4; i++)
                    layout.addDescriptorSet(setBuilders[i]);
                layout.end();
                pipelineLayout = layout.create(device);

                staticSet0 = setBuilders[0].create(device);
                for (uint32_t i = 0; i < kVertexFetchSlots; i++)
                    staticSet0->setBuffer(vfetchDescriptorBase + i, vertexArena.get(), kVertexArenaSize);
                RenderSampler* defaultSampler = GetSampler(0x2 | (0x2 << 2) | (0x1 << 4)); // linear, wrap
                for (uint32_t i = 0; i < kSamplerPalette; i++)
                    staticSet0->setSampler(samplerDescriptorBase + i, defaultSampler);

                CreateDummyTexture(dummyTexture2D, RenderTextureDimension::TEXTURE_2D, 0);
                CreateDummyTexture(dummyTexture3D, RenderTextureDimension::TEXTURE_3D, 0);
                CreateDummyTexture(dummyTextureCube, RenderTextureDimension::TEXTURE_2D, RenderTextureFlag::CUBE);

                if (const char* dir = getenv("LO_SHADER_CACHE_DIR"))
                    shaderCacheDir = dir;

                CompileRectListGs();
                CompileBlitShaders();
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
                xenos::CompiledShader v = xenos::CompileHlsl(vsSrc, "main", "vs_6_0");
                xenos::CompiledShader f = xenos::CompileHlsl(psSrc, "main", "ps_6_0");
                if (!v.ok || !f.ok)
                {
                    LOG_WARNING("renderer: blit shader compilation failed: {}{}", v.errors, f.errors);
                    return;
                }
                blitVs = device->createShader(v.dxil.data(), v.dxil.size(), "main", RenderShaderFormat::DXIL);
                blitPs = device->createShader(f.dxil.data(), f.dxil.size(), "main", RenderShaderFormat::DXIL);
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
                commandList->setGraphicsRootDescriptor(RenderBufferReference(uploadRing.get(), 0), 0);
                commandList->setGraphicsRootDescriptor(RenderBufferReference(uploadRing.get(), 0), 1);
                commandList->setGraphicsRootDescriptor(RenderBufferReference(uploadRing.get(), 0), 2);
                commandList->setGraphicsDescriptorSet(staticSet0.get(), 0);
                commandList->setGraphicsDescriptorSet(set1, 1);
                commandList->setGraphicsDescriptorSet(AcquireSet(2), 2);
                commandList->setGraphicsDescriptorSet(AcquireSet(3), 3);
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
                xenos::CompiledShader gs = xenos::CompileHlsl(src, "main", "gs_6_0");
                if (!gs.ok)
                {
                    LOG_WARNING("renderer: rect list GS failed: {}", gs.errors);
                    return;
                }
                rectListGs = device->createShader(gs.dxil.data(), gs.dxil.size(), "main", RenderShaderFormat::DXIL);
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
                retiredTextures.clear();
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
                    return 0;
                uint32_t index = uint32_t(samplerPalette.size());
                samplerPalette.emplace(key, index);
                staticSet0->setSampler(samplerDescriptorBase + index, GetSampler(key));
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
            Shader* GetShader(bool pixel, const uint32_t* words, uint32_t count)
            {
                uint64_t hash = Fnv1a(words, count * 4);
                auto& cache = shaders[pixel ? 1 : 0];
                auto it = cache.find(hash);
                if (it != cache.end())
                    return it->second.valid ? &it->second : nullptr;

                Shader& entry = cache[hash];
                ScopedTimer timer{ tShader };
                nShader++;
                std::vector<uint32_t> swapped(count);
                for (uint32_t i = 0; i < count; i++)
                    swapped[i] = ByteSwap(words[i]);
                entry.info = xenos::TranslateShader(swapped.data(), count, pixel);

                std::vector<uint8_t> dxil;
                std::string cachePath;
                if (!shaderCacheDir.empty())
                {
                    // The cache name carries a translator version so changes to the
                    // generated HLSL don't resurrect stale DXIL.
                    cachePath = fmt::format("{}/{}_{:016x}_v14.dxil", shaderCacheDir, pixel ? "ps" : "vs", hash);
                    std::ifstream in(cachePath, std::ios::binary);
                    if (in)
                        dxil.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
                }
                if (dxil.empty())
                {
                    xenos::CompiledShader compiled = xenos::CompileHlsl(entry.info.hlsl, "main", pixel ? "ps_6_0" : "vs_6_0");
                    if (!compiled.ok)
                    {
                        LOG_WARNING("renderer: {} shader {:016x} failed to compile:\n{}", pixel ? "pixel" : "vertex", hash, compiled.errors);
                        if (getenv("LO_SHADER_DUMP_DIR"))
                            std::ofstream(fmt::format("{}/{}_{:016x}.hlsl", getenv("LO_SHADER_DUMP_DIR"), pixel ? "ps" : "vs", hash)) << entry.info.hlsl;
                        return nullptr;
                    }
                    dxil = std::move(compiled.dxil);
                    if (!cachePath.empty())
                        std::ofstream(cachePath, std::ios::binary).write(reinterpret_cast<const char*>(dxil.data()), dxil.size());
                }
                entry.shader = device->createShader(dxil.data(), dxil.size(), "main", RenderShaderFormat::DXIL);
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

            HostTexture* GetRenderTarget(uint32_t base, uint32_t format, uint32_t pitch, uint32_t height, bool depth)
            {
                // EDRAM targets have no intrinsic height and ours is only guessed
                // from the scissor, so draws and resolves may disagree on it: key
                // by tile base, format and pitch only and grow the texture when a
                // taller extent shows up.
                height = std::clamp<uint32_t>((height + 31) & ~31u, 32, 2048);
                // Depth formats (D24S8 / D24FS8) alias the same tiles and share our
                // host format, so they are one target.
                // Every colour format is a view of the same EDRAM tiles: this title
                // draws the base pass through the 8_8_8_8 view and resolves it through
                // the 2_10_10_10 view in the same frame. Keeping one host texture per
                // (base, pitch) is what makes those views agree; the format is the
                // widest one so the HDR passes keep their range.
                RenderTargetKey key{ base, 0, pitch, 0, depth };
                auto it = renderTargets.find(key);
                if (it != renderTargets.end())
                {
                    if (it->second->height >= height)
                        return it->second.get();
                    height = std::max(height, it->second->height);
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
                tex->format = depth ? RenderFormat::D32_FLOAT_S8_UINT : RenderFormat::R16G16B16A16_FLOAT;
                tex->width = pitch;
                tex->height = height;
                RenderTextureDesc desc = RenderTextureDesc::Texture2D(pitch, height, 1, tex->format, depth ? RenderTextureFlag::DEPTH_TARGET : RenderTextureFlag::RENDER_TARGET);
                tex->texture = device->createTexture(desc);
                tex->layout = RenderTextureLayout::UNKNOWN;
                if (!tex->texture)
                    LOG_WARNING("renderer: render target creation failed");
                LOG_INFO("renderer: new {} target base={:#x} fmt={} {}x{}", depth ? "depth" : "color", base, format, pitch, height);
                HostTexture* result = tex.get();
                renderTargets.emplace(key, std::move(tex));
                return result;
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

                TextureKey key{ base, format, width, height, (tiled ? 1u : 0u) | (endian << 1) | (pitch32 << 3) | (dimension << 12) };
                if (auto rit = resolved.find(base); rit != resolved.end() && rit->second.tex && ResolveFormatMatches(format, rit->second.destFormat))
                {
                    Transition(*rit->second.tex, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
                    return rit->second.tex.get();
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
                uint32_t sourceAddress = base;
                if (sourceAddress == 0)
                {
                    const uint32_t mipAddress = (fetch[5] >> 12) << 12;
                    const uint32_t mipMin = std::max<uint32_t>(1, (fetch[4] >> 2) & 0xF);
                    if (mipAddress == 0)
                        return nullptr;
                    sourceAddress = mipAddress;
                    width = std::max<uint32_t>(1, width >> mipMin);
                    height = std::max<uint32_t>(1, height >> mipMin);
                    pitch32 = std::max<uint32_t>(1, pitch32 >> mipMin);
                }

                // Guest layout: blocks, pitch in blocks aligned to the 32-block macro tile.
                uint32_t blocksX = (width + fi.blockWidth - 1) / fi.blockWidth;
                uint32_t blocksY = (height + fi.blockHeight - 1) / fi.blockHeight;
                uint32_t pitchBlocks = std::max<uint32_t>((pitch32 * 32) / fi.blockWidth, blocksX);
                pitchBlocks = (pitchBlocks + 31) & ~31u;
                uint32_t bpbLog2 = fi.bytesPerBlock == 1 ? 0 : fi.bytesPerBlock == 2 ? 1 : fi.bytesPerBlock == 4 ? 2 : fi.bytesPerBlock == 8 ? 3 : 4;

                const uint8_t* src = Phys(sourceAddress);
                uint32_t hostBpp = fi.convertToRgba8 ? 4 : fi.bytesPerBlock;
                uint32_t rowBytes = blocksX * hostBpp;
                uint32_t rowPitch = (rowBytes + 255) & ~255u;
                // Cube faces sit back to back, each face's tiled image padded to the
                // 4 KB subresource alignment (xenos.h kTextureSubresourceAlignment).
                const uint32_t faces = dimension == 3 ? 6u : 1u;
                const uint32_t blocksYAligned = (blocksY + 31) & ~31u;
                const uint32_t faceStride = ((pitchBlocks * blocksYAligned * fi.bytesPerBlock) + 4095u) & ~4095u;
                std::vector<uint8_t> staging(size_t(rowPitch) * blocksY * faces);
                std::vector<uint8_t> block(fi.bytesPerBlock);
                for (uint32_t f = 0; f < faces; f++)
                for (uint32_t by = 0; by < blocksY; by++)
                {
                    uint8_t* dstRow = staging.data() + (size_t(f) * blocksY + by) * rowPitch;
                    for (uint32_t bx = 0; bx < blocksX; bx++)
                    {
                        uint32_t offset = tiled ? video::TiledOffset2D(bx, by, pitchBlocks, bpbLog2) : (by * pitchBlocks + bx) * fi.bytesPerBlock;
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
                tex->guestBytes = uint32_t(std::min<uint64_t>(uint64_t(pitchBlocks) * blocksY * fi.bytesPerBlock * faces, 64u << 20));
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

            RenderPipeline* GetPipeline(const PipelineKey& key, Shader* vs, Shader* ps, RenderFormat rtFormat, RenderFormat depthFormat)
            {
                auto it = pipelines.find(key);
                if (it != pipelines.end())
                    return it->second.get();
                ScopedTimer timer{ tPipeline };
                nPipeline++;

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

                auto pipeline = device->createGraphicsPipeline(desc);
                RenderPipeline* result = pipeline.get();
                pipelines.emplace(key, std::move(pipeline));
                return result;
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
                uint32_t vfetchOffset[96];
                uint32_t samplerIndex[32];
            };

            void Draw(const DrawInfo& info)
            {
                ScopedTimer timer{ tDraw };
                DrawImpl(info);
            }

            void DrawImpl(const DrawInfo& info)
            {
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
                    return;

                // Shaders come from the command processor's last IM_LOAD.
                uint32_t vsCount = 0, psCount = 0;
                const uint32_t* vsWords = g_commandProcessor.GetActiveShader(false, vsCount);
                const uint32_t* psWords = g_commandProcessor.GetActiveShader(true, psCount);
                if (!vsWords || vsCount == 0)
                    return;
                Shader* vs = GetShader(false, vsWords, vsCount);
                Shader* ps = psWords && psCount ? GetShader(true, psWords, psCount) : nullptr;
                if (!vs)
                    return;

                // Render targets.
                uint32_t surfaceInfo = Reg(REG_RB_SURFACE_INFO);
                uint32_t pitch = surfaceInfo & 0x3FFF;
                if (pitch == 0)
                    return;
                uint32_t scissorBr = Reg(REG_PA_SC_WINDOW_SCISSOR_BR);
                uint32_t scissorTl = Reg(REG_PA_SC_WINDOW_SCISSOR_TL);
                uint32_t rtHeight = GuessTargetHeight(pitch, (scissorBr >> 16) & 0x3FFF);

                uint32_t colorInfo = Reg(REG_RB_COLOR_INFO);
                uint32_t depthInfo = Reg(REG_RB_DEPTH_INFO);
                uint32_t depthControl = Reg(REG_RB_DEPTHCONTROL);
                bool colorWrites = modeControl == 4;
                HostTexture* color = GetRenderTarget(colorInfo & 0xFFF, (colorInfo >> 16) & 0xF, pitch, rtHeight, false);
                HostTexture* depth = (depthControl & 3) ? GetRenderTarget(depthInfo & 0xFFF, (depthInfo >> 16) & 1, pitch, rtHeight, true) : nullptr;

                Transition(*color, RenderTextureLayout::COLOR_WRITE, RenderBarrierStage::GRAPHICS);
                if (depth)
                    Transition(*depth, RenderTextureLayout::DEPTH_WRITE, RenderBarrierStage::GRAPHICS);

                // Pipeline.
                PipelineKey key{};
                key.vs = Fnv1a(vsWords, vsCount * 4);
                key.ps = ps ? Fnv1a(psWords, psCount * 4) : 0;
                key.blend = Reg(REG_RB_BLENDCONTROL0);
                key.depthControl = depthControl & 0x7F;
                key.modeCull = Reg(REG_PA_SU_SC_MODE_CNTL) & 7;
                key.colorMask = colorWrites ? (Reg(REG_RB_COLOR_MASK) & 0xF) : 0;
                key.prim = info.primitiveType;
                key.rtFormat = uint32_t(color->format);
                key.depthFormat = depth ? uint32_t(depth->format) : 0;
                RenderPipeline* pipeline = GetPipeline(key, vs, ps, color->format, depth ? depth->format : RenderFormat::UNKNOWN);
                if (!pipeline)
                    return;

                // Constants.
                auto tConst0 = std::chrono::steady_clock::now();
                uint32_t vsConstants[256 * 4], psConstants[224 * 4];
                for (uint32_t i = 0; i < 256 * 4; i++) vsConstants[i] = Reg(REG_ALU_CONSTANTS + i);
                for (uint32_t i = 0; i < 224 * 4; i++) psConstants[i] = Reg(REG_ALU_CONSTANTS + 256 * 4 + i);

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
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                shared.vtxFmt = (vte >> 8) & 7;
                if ((Reg(REG_PA_SU_VTX_CNTL) & 1) == 0)
                {
                    shared.halfPixel[0] = 1.0f / viewport.width;
                    shared.halfPixel[1] = -1.0f / viewport.height;
                }
                // Range of the bound colour format, clamped in the shader epilogue.
                {
                    const uint32_t cfmt = (colorInfo >> 16) & 0xF;
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
                for (uint32_t slot = 0; slot < kVertexFetchSlots; slot++)
                {
                    if (!((vs->info.vertexFetchSlotMask[slot >> 6] >> (slot & 63)) & 1))
                        continue;
                    uint32_t d0 = Reg(REG_FETCH_CONSTANTS + slot * 2);
                    uint32_t d1 = Reg(REG_FETCH_CONSTANTS + slot * 2 + 1);
                    if ((d0 & 3) != 3)
                        continue;
                    uint32_t address = d0 & ~3u;
                    uint32_t sizeDwords = (d1 >> 2) & 0xFFFFFF;
                    if (sizeDwords == 0 || sizeDwords > (16u << 20))
                        continue;
                    uint64_t offset = GetVertexBuffer(address, sizeDwords, d1 & 3);
                    if (offset == UINT64_MAX)
                        continue;
                    shared.vfetchOffset[slot] = uint32_t(offset);
                }

                tVertex += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tVertex0).count();

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
                            dummyBindings++;
                            continue;
                        }
                        uint32_t d3 = fetch[3];
                        uint64_t samplerKey = ((d3 >> 19) & 3) | (((d3 >> 21) & 3) << 2) | (((d3 >> 23) & 3) << 4)
                            | (((fetch[0] >> 10) & 7) << 6) | (((fetch[0] >> 13) & 7) << 9) | (((fetch[0] >> 16) & 7) << 12);
                        shared.samplerIndex[slot] = GetSamplerIndex(samplerKey);
                        set->setTexture(slot, tex->texture.get(), RenderTextureLayout::SHADER_READ);
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
                    return;

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
                    return;

                tIndex += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tIndex0).count();

                // Record.
                ScopedTimer recordTimer{ tRecord };
                RenderFramebuffer* framebuffer = GetFramebuffer(color, depth);
                commandList->setFramebuffer(framebuffer);
                commandList->setViewports(&viewport, 1);
                RenderRect scissor(int32_t(scissorTl & 0x3FFF), int32_t((scissorTl >> 16) & 0x3FFF), int32_t(scissorBr & 0x3FFF), int32_t((scissorBr >> 16) & 0x3FFF));
                uint32_t windowOffset = Reg(REG_PA_SC_WINDOW_OFFSET);
                if (!(scissorTl & 0x80000000u) && windowOffset)
                {
                    int32_t ox = int32_t(windowOffset << 17) >> 17, oy = int32_t(windowOffset << 1) >> 17;
                    scissor.left += ox; scissor.right += ox; scissor.top += oy; scissor.bottom += oy;
                }
                scissor.left = std::clamp(scissor.left, 0, int32_t(pitch)); scissor.right = std::clamp(scissor.right, 0, int32_t(pitch));
                scissor.top = std::clamp(scissor.top, 0, int32_t(rtHeight)); scissor.bottom = std::clamp(scissor.bottom, 0, int32_t(rtHeight));
                if (scissor.right <= scissor.left || scissor.bottom <= scissor.top)
                    return;
                commandList->setScissors(&scissor, 1);
                commandList->setPipeline(pipeline);
                commandList->setGraphicsPipelineLayout(pipelineLayout.get());
                commandList->setGraphicsRootDescriptor(RenderBufferReference(uploadRing.get(), vsOffset), 0);
                commandList->setGraphicsRootDescriptor(RenderBufferReference(uploadRing.get(), sharedOffset), 1);
                commandList->setGraphicsRootDescriptor(RenderBufferReference(uploadRing.get(), psOffset), 2);
                commandList->setGraphicsDescriptorSet(set0, 0);
                commandList->setGraphicsDescriptorSet(set1, 1);
                commandList->setGraphicsDescriptorSet(set2, 2);
                commandList->setGraphicsDescriptorSet(set3, 3);

                int32_t baseVertex = int32_t(Reg(REG_VGT_INDX_OFFSET));
                static uint32_t drawLogs = 0;
                static const uint32_t traceFrame = getenv("LO_DRAW_TRACE") ? strtoul(getenv("LO_DRAW_TRACE"), nullptr, 10) : 0;
                static const uint32_t traceCount = getenv("LO_DRAW_TRACE_COUNT") ? strtoul(getenv("LO_DRAW_TRACE_COUNT"), nullptr, 10) : 1;
                if (traceFrame && frame >= traceFrame && frame < traceFrame + traceCount && (info.indexCount >= 200 || info.primitiveType == 8))
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
                        for (int i = 0; i < 10; i++)
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
                            bool fromResolve = resolved.count(base) != 0;
                            texs += fmt::format(" t{}=[fmt {} {}x{} at {:#x}{}]", slot, f1 & 0x3F, (f2 & 0x1FFF) + 1, ((f2 >> 13) & 0x1FFF) + 1, base, fromResolve ? " resolved" : "");
                        }
                    }
                    LOG_INFO("renderer: draw textures{}", texs);
                }
                if (drawLogs < 24 || (traceFrame && frame >= traceFrame && frame < traceFrame + traceCount))
                {
                    drawLogs++;
                    LOG_INFO("renderer: draw f{} prim={} n={} idx={} vs={:016x} ps={:016x} rt={:#x}/{} {}x{} depth={:#x} dinfo={:#x} vp=({},{} {}x{} z {}..{}) vte={:#x} scissor=({},{})-({},{}) ndc=({},{}) off=({},{}) mode={} c255=({:g},{:g},{:g},{:g})",
                        frame, info.primitiveType, indexCount, useIndices, key.vs, key.ps, colorInfo & 0xFFF, (colorInfo >> 16) & 0xF, pitch, rtHeight, depthControl, depthInfo,
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
                            commandList->clearDepthStencil(true, false, rectZ, 0);
                            if (logged++ < 8)
                                LOG_INFO("renderer: depth clear rect (pitch {}, {}x{} .. {}x{}) -> cleared depth base={:#x} pitch={} to {}", pitch, minX, minY, maxX, maxY, k.base, k.pitch, rectZ);
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
                        mappedRows = std::clamp<uint32_t>(mappedRows, 1, target->height);
                        HostTexture* otherDepth = depth ? GetRenderTarget(depthInfo & 0xFFF, (depthInfo >> 16) & 1, k.pitch, target->height, true) : nullptr;
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
            }

            // LO_DUMP_RESOLVE_SEQ=<frame>: dump every resolve of that frame in order.
            uint32_t resolveSeq = 0;
            void DumpResolveStep(HostTexture& tex, uint32_t destBase)
            {
                static const uint32_t dumpFrame = getenv("LO_DUMP_RESOLVE_SEQ") ? strtoul(getenv("LO_DUMP_RESOLVE_SEQ"), nullptr, 10) : 0;
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
                const char* dir = getenv("LO_DUMP_RESOLVE_DIR");
                std::string path = fmt::format("{}/seq{:02}_{:x}.ppm", dir ? dir : ".", resolveSeq++, destBase);
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
                const uint32_t texW = std::clamp<uint32_t>(std::max(destPitch, x0 + w), 1, 8192);
                const uint32_t texH = std::clamp<uint32_t>(std::max(destHeight, y0 + h), 1, 8192);
                ResolvedSurface& rs = resolved[destBase];
                if (!rs.tex || rs.tex->format != RenderFormat::R32_FLOAT || rs.tex->width != texW || rs.tex->height != texH)
                {
                    if (rs.tex) retiredTextures.push_back(std::move(rs.tex));
                    rs.tex = std::make_unique<HostTexture>();
                    rs.tex->format = RenderFormat::R32_FLOAT;
                    rs.tex->width = texW;
                    rs.tex->height = texH;
                    rs.tex->texture = device->createTexture(RenderTextureDesc::Texture2D(texW, texH, 1, RenderFormat::R32_FLOAT));
                    rs.tex->layout = RenderTextureLayout::UNKNOWN;
                    if (!rs.tex->texture)
                    {
                        resolved.erase(destBase);
                        return;
                    }
                    static uint32_t created = 0;
                    if (created++ < 8)
                        LOG_INFO("renderer: resolved depth surface {:#x} {}x{} dest fmt={}", destBase, texW, texH, destFormat & 0xFFF);
                }
                rs.destFormat = destFormat;
                rs.destPitch = destPitch;
                rs.frame = frame;
                w = std::min(w, texW - x0);
                h = std::min(h, texH - y0);
                if (w == 0 || h == 0)
                    return;
                Transition(depth, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
                Transition(*rs.tex, RenderTextureLayout::COPY_DEST, RenderBarrierStage::COPY);
                RenderBox box{ int32_t(x0), int32_t(y0), int32_t(x0 + w), int32_t(y0 + h), 0, 1 };
                commandList->copyTextureRegion(RenderTextureCopyLocation::Subresource(rs.tex->texture.get()),
                    RenderTextureCopyLocation::Subresource(depth.texture.get(), 0), x0, y0, 0, &box);
                Transition(*rs.tex, RenderTextureLayout::SHADER_READ, RenderBarrierStage::GRAPHICS);
                DumpResolveStep(*rs.tex, destBase);
            }

            // Copies the resolve rectangle into the host texture standing in for the
            // destination memory (destPitch x destHeight texels, rectangle placed at its
            // window position). No GPU sync, no tiling, no guest memory writes.
            void ResolveOnGpu(HostTexture& color, uint32_t destBase, uint32_t destFormat, uint32_t destPitch, uint32_t destHeight,
                              uint32_t x0, uint32_t y0, uint32_t w, uint32_t h)
            {
                Begin();
                const uint32_t texW = std::clamp<uint32_t>(std::max(destPitch, x0 + w), 1, 8192);
                const uint32_t texH = std::clamp<uint32_t>(std::max(destHeight, y0 + h), 1, 8192);
                // The destination's own format decides what the surface holds, so the
                // frontbuffer stays 8888 even though EDRAM is kept in FP16.
                const RenderFormat destHost = (destFormat == 32 || destFormat == 7) ? RenderFormat::R16G16B16A16_FLOAT : RenderFormat::R8G8B8A8_UNORM;
                ResolvedSurface& rs = resolved[destBase];
                if (!rs.tex || rs.tex->format != destHost || rs.tex->width != texW || rs.tex->height != texH)
                {
                    if (rs.tex) retiredTextures.push_back(std::move(rs.tex));
                    rs.tex = std::make_unique<HostTexture>();
                    rs.tex->format = destHost;
                    rs.tex->width = texW;
                    rs.tex->height = texH;
                    rs.tex->texture = device->createTexture(RenderTextureDesc::Texture2D(texW, texH, 1, destHost, RenderTextureFlag::RENDER_TARGET));
                    rs.tex->layout = RenderTextureLayout::UNKNOWN;
                    if (!rs.tex->texture)
                    {
                        LOG_WARNING("renderer: resolved surface creation failed ({}x{})", texW, texH);
                        resolved.erase(destBase);
                        return;
                    }
                    static uint32_t created = 0;
                    if (created++ < 16)
                        LOG_INFO("renderer: resolved surface {:#x} {}x{} host fmt={} dest fmt={}", destBase, texW, texH, uint32_t(color.format), destFormat);
                }
                rs.destFormat = destFormat;
                rs.destPitch = destPitch;
                rs.frame = frame;
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
                DumpResolveStep(*rs.tex, destBase);
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
                    static const uint32_t traceFrame = getenv("LO_DRAW_TRACE") ? strtoul(getenv("LO_DRAW_TRACE"), nullptr, 10) : 0;
                    static const uint32_t traceCount = getenv("LO_DRAW_TRACE_COUNT") ? strtoul(getenv("LO_DRAW_TRACE_COUNT"), nullptr, 10) : 1;
                    if (traceFrame && frame >= traceFrame && frame < traceFrame + traceCount)
                    {
                        const bool existed = renderTargets.count(RenderTargetKey{ colorInfo & 0xFFF, (colorInfo >> 16) & 0xF, pitch, 0, false }) != 0;
                        LOG_INFO("renderer: resolve f{} src sel={} base={:#x} fmt={} pitch={} h={} existed={} -> {:#x} destfmt={} rect ({},{}) {}x{} destPitch={} destHeight={} clear={:#x}",
                            frame, srcSelect, colorInfo & 0xFFF, (colorInfo >> 16) & 0xF, pitch, rtHeight, existed, destBase, destFormat, x0, y0, copyWidth, copyHeight, destPitch, destHeight, copyControl & 0x300);
                    }
                }
                HostTexture* color = GetRenderTarget(colorInfo & 0xFFF, (colorInfo >> 16) & 0xF, pitch, rtHeight, false);
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
                static const uint32_t traceFrame = getenv("LO_DRAW_TRACE") ? strtoul(getenv("LO_DRAW_TRACE"), nullptr, 10) : 0;
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
                    RenderRect rect{ int32_t(x0), int32_t(y0), int32_t(x1), int32_t(y1) };
                    commandList->clearColor(0, c, &rect, 1);
                }
                if (copyControl & 0x200)
                    ClearDepthTarget(pitch, rtHeight);
            }

            void ClearDepthTarget(uint32_t pitch, uint32_t rtHeight)
            {
                uint32_t depthInfo = Reg(REG_RB_DEPTH_INFO);
                HostTexture* depth = GetRenderTarget(depthInfo & 0xFFF, (depthInfo >> 16) & 1, pitch, rtHeight, true);
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
        auto* r = new Renderer();
        if (!r->Init())
        {
            delete r;
            return false;
        }
        g_renderer = r;
        return true;
    }

    void Shutdown()
    {
        if (g_renderer)
        {
            g_renderer->Flush();
            delete g_renderer;
            g_renderer = nullptr;
        }
    }

    void Draw(const DrawInfo& info)
    {
        if (g_renderer)
            g_renderer->Draw(info);
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
                if (stats && r.dummyBindings)
                    LOG_INFO("renderer frame {}: {} texture slots fell back to the dummy", r.frame, r.dummyBindings);
                r.dummyBindings = 0;
                if (stats && r.textureReuploads)
                    LOG_INFO("renderer frame {}: {} textures re-uploaded after a guest write", r.frame, r.textureReuploads);
                r.textureReuploads = 0;
                r.ResetTimers();
                r.vertexUploads = r.vertexRevalidations = 0;
                r.vertexBytesUploaded = 0;
            }
            g_renderer->frame++;
            g_renderer->drawsThisFrame = 0;
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
        auto it = g_renderer->resolved.find(physicalAddress & 0x1FFFFFFF);
        if (it == g_renderer->resolved.end() || !it->second.tex)
            return nullptr;
        HostTexture& tex = *it->second.tex;
        g_renderer->Begin();
        g_renderer->Transition(tex, RenderTextureLayout::COPY_SOURCE, RenderBarrierStage::COPY);
        g_renderer->Flush();
        width = tex.width;
        height = tex.height;
        format = uint32_t(tex.format);
        return tex.texture.get();
    }

    std::vector<uint32_t> GetResolvedAddresses()
    {
        std::vector<uint32_t> out;
        if (g_renderer)
            for (auto& [address, surface] : g_renderer->resolved)
                if (surface.tex)
                    out.push_back(address);
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
        auto it = g_renderer->resolved.find(physicalAddress & 0x1FFFFFFF);
        if (it == g_renderer->resolved.end() || !it->second.tex)
            return false;
        return ReadbackTexture(*it->second.tex, pixels, width, height);
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
    bool Init() { return false; }
    void Shutdown() {}
    void Draw(const DrawInfo&) {}
    void Flush() {}
    void InvalidateGuestRange(uint32_t, uint32_t) {}
    plume::RenderTexture* AcquireResolvedSurface(uint32_t, uint32_t&, uint32_t&, uint32_t&) { return nullptr; }
    bool ReadbackResolvedSurface(uint32_t, std::vector<uint32_t>&, uint32_t&, uint32_t&) { return false; }
    std::vector<uint32_t> GetResolvedAddresses() { return {}; }
    void DumpRenderTargets(const char*) {}
#endif
}
