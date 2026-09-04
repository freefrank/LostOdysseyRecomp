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
            std::unordered_map<TextureKey, std::unique_ptr<HostTexture>, TextureKeyHash> textures;
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
                readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(kReadbackSize));
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
                    staticSet0->setBuffer(vfetchDescriptorBase + i, uploadRing.get(), kUploadRingSize);
                RenderSampler* defaultSampler = GetSampler(0x2 | (0x2 << 2) | (0x1 << 4)); // linear, wrap
                for (uint32_t i = 0; i < kSamplerPalette; i++)
                    staticSet0->setSampler(samplerDescriptorBase + i, defaultSampler);

                CreateDummyTexture(dummyTexture2D, RenderTextureDimension::TEXTURE_2D, 0);
                CreateDummyTexture(dummyTexture3D, RenderTextureDimension::TEXTURE_3D, 0);
                CreateDummyTexture(dummyTextureCube, RenderTextureDimension::TEXTURE_2D, RenderTextureFlag::CUBE);

                if (const char* dir = getenv("LO_SHADER_CACHE_DIR"))
                    shaderCacheDir = dir;

                CompileRectListGs();
                LOG_INFO("renderer: initialised");
                return true;
            }

            void CreateDummyTexture(HostTexture& tex, RenderTextureDimension dim, RenderTextureFlags flags)
            {
                RenderTextureDesc desc = RenderTextureDesc::Texture(dim, 1, 1, 1, 1, flags & RenderTextureFlag::CUBE ? 6 : 1, RenderFormat::R8G8B8A8_UNORM, flags);
                tex.texture = device->createTexture(desc);
                tex.format = RenderFormat::R8G8B8A8_UNORM;
                tex.width = tex.height = 1;
                tex.layout = RenderTextureLayout::UNKNOWN;
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
                queue->waitForCommandFence(fence.get());
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
                std::vector<uint32_t> swapped(count);
                for (uint32_t i = 0; i < count; i++)
                    swapped[i] = ByteSwap(words[i]);
                entry.info = xenos::TranslateShader(swapped.data(), count, pixel);

                std::vector<uint8_t> dxil;
                std::string cachePath;
                if (!shaderCacheDir.empty())
                {
                    cachePath = fmt::format("{}/{}_{:016x}.dxil", shaderCacheDir, pixel ? "ps" : "vs", hash);
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
                RenderTargetKey key{ base, format, pitch, height, depth };
                auto it = renderTargets.find(key);
                if (it != renderTargets.end())
                    return it->second.get();

                auto tex = std::make_unique<HostTexture>();
                tex->format = depth ? RenderFormat::D32_FLOAT_S8_UINT : ColorFormat(format);
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
                case 10: out = { RenderFormat::R8G8_UNORM, 1, 1, 2, false }; return true;       // k_8_8
                case 15: out = { RenderFormat::R8G8B8A8_UNORM, 1, 1, 2, true }; return true;    // k_4_4_4_4
                case 18: out = { RenderFormat::BC1_UNORM, 4, 4, 8, false }; return true;        // DXT1
                case 19: out = { RenderFormat::BC2_UNORM, 4, 4, 16, false }; return true;       // DXT2/3
                case 20: out = { RenderFormat::BC3_UNORM, 4, 4, 16, false }; return true;       // DXT4/5
                case 32: out = { RenderFormat::R16G16B16A16_FLOAT, 1, 1, 8, false }; return true;
                case 36: out = { RenderFormat::R32_FLOAT, 1, 1, 4, false }; return true;
                default: return false;
                }
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
                auto it = textures.find(key);
                if (it != textures.end())
                    return it->second.get();

                TextureFormatInfo fi;
                if (!GetTextureFormat(format, fi) || base == 0)
                {
                    if (loggedFormats.insert(format).second)
                        LOG_WARNING("renderer: unsupported texture format {} ({}x{} at {:#x})", format, width, height, base);
                    return nullptr;
                }

                // Guest layout: blocks, pitch in blocks aligned to the 32-block macro tile.
                uint32_t blocksX = (width + fi.blockWidth - 1) / fi.blockWidth;
                uint32_t blocksY = (height + fi.blockHeight - 1) / fi.blockHeight;
                uint32_t pitchBlocks = std::max<uint32_t>((pitch32 * 32) / fi.blockWidth, blocksX);
                pitchBlocks = (pitchBlocks + 31) & ~31u;
                uint32_t bpbLog2 = fi.bytesPerBlock == 1 ? 0 : fi.bytesPerBlock == 2 ? 1 : fi.bytesPerBlock == 4 ? 2 : fi.bytesPerBlock == 8 ? 3 : 4;

                const uint8_t* src = Phys(base);
                uint32_t hostBpp = fi.convertToRgba8 ? 4 : fi.bytesPerBlock;
                uint32_t rowBytes = blocksX * hostBpp;
                uint32_t rowPitch = (rowBytes + 255) & ~255u;
                std::vector<uint8_t> staging(size_t(rowPitch) * blocksY);
                std::vector<uint8_t> block(fi.bytesPerBlock);
                for (uint32_t by = 0; by < blocksY; by++)
                {
                    uint8_t* dstRow = staging.data() + size_t(by) * rowPitch;
                    for (uint32_t bx = 0; bx < blocksX; bx++)
                    {
                        uint32_t offset = tiled ? video::TiledOffset2D(bx, by, pitchBlocks, bpbLog2) : (by * pitchBlocks + bx) * fi.bytesPerBlock;
                        memcpy(block.data(), src + offset, fi.bytesPerBlock);
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
                            }
                        }
                    }
                }

                auto tex = std::make_unique<HostTexture>();
                tex->format = fi.host;
                tex->width = width;
                tex->height = height;
                uint32_t texWidth = fi.blockWidth > 1 ? blocksX * fi.blockWidth : width;
                uint32_t texHeight = fi.blockHeight > 1 ? blocksY * fi.blockHeight : height;
                tex->texture = device->createTexture(RenderTextureDesc::Texture2D(texWidth, texHeight, 1, fi.host));
                tex->layout = RenderTextureLayout::UNKNOWN;
                if (!tex->texture)
                {
                    LOG_WARNING("renderer: texture creation failed fmt={} {}x{}", format, texWidth, texHeight);
                    return nullptr;
                }

                uint64_t offset = Upload(staging.data(), staging.size(), 512);
                if (offset == UINT64_MAX)
                    return nullptr;
                Transition(*tex, RenderTextureLayout::COPY_DEST, RenderBarrierStage::COPY);
                commandList->copyTextureRegion(
                    RenderTextureCopyLocation::Subresource(tex->texture.get()),
                    RenderTextureCopyLocation::PlacedFootprint(uploadRing.get(), fi.host, texWidth, texHeight, 1, (rowPitch / hostBpp) * fi.blockWidth, offset));
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
                    if (overlap) it = textures.erase(it); else ++it;
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
                uint32_t vfetchOffset[96];
                uint32_t samplerIndex[32];
            };

            void Draw(const DrawInfo& info)
            {
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
                if (vte & 0x10) { viewport.minDepth = std::clamp(zo, 0.0f, 1.0f); viewport.maxDepth = std::clamp(zo + zs, 0.0f, 1.0f); }
                if (viewport.maxDepth <= viewport.minDepth) { viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f; }
                shared.vtxFmt = (vte >> 8) & 7;
                if ((Reg(REG_PA_SU_VTX_CNTL) & 1) == 0)
                {
                    shared.halfPixel[0] = 1.0f / viewport.width;
                    shared.halfPixel[1] = -1.0f / viewport.height;
                }
                uint32_t colorControl = Reg(REG_RB_COLORCONTROL);
                if (colorControl & 8)
                {
                    shared.flags |= 1;
                    shared.alphaTest[0] = RegF(REG_RB_ALPHA_REF);
                    shared.alphaTest[1] = float(colorControl & 7);
                }

                uint64_t vsOffset = Upload(vsConstants, sizeof(vsConstants));
                uint64_t psOffset = Upload(psConstants, sizeof(psConstants));
                if (vsOffset == UINT64_MAX || psOffset == UINT64_MAX)
                    return;

                // Descriptor sets: vertex fetch buffers + samplers, textures.
                RenderDescriptorSet* set0 = staticSet0.get();
                RenderDescriptorSet* set1 = AcquireSet(1);
                RenderDescriptorSet* set2 = AcquireSet(2);
                RenderDescriptorSet* set3 = AcquireSet(3);
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
                    size_t bytes = size_t(sizeDwords) * 4 + 16;
                    uint64_t offset = Upload(nullptr, bytes, 16);
                    if (offset == UINT64_MAX)
                        continue;
                    memcpy(uploadMapped + offset, Phys(address), bytes);
                    SwapBuffer(reinterpret_cast<uint32_t*>(uploadMapped + offset), bytes / 4, d1 & 3);
                    shared.vfetchOffset[slot] = uint32_t(offset);
                }

                // Textures used by the pixel and vertex shaders.
                auto bindTextures = [&](Shader* s)
                {
                    if (!s) return;
                    for (uint32_t slot = 0; slot < kTextureSlots; slot++)
                    {
                        if (!((s->info.textureSlotMask >> slot) & 1))
                            continue;
                        uint32_t fetch[6];
                        for (int i = 0; i < 6; i++) fetch[i] = Reg(REG_FETCH_CONSTANTS + slot * 6 + i);
                        if ((fetch[0] & 3) != 2)
                            continue;
                        uint32_t dimension = (fetch[5] >> 9) & 3; // 0 1D, 1 2D, 2 3D, 3 cube
                        HostTexture* tex = GetTexture(fetch, dimension);
                        if (!tex)
                            continue;
                        uint32_t d3 = fetch[3];
                        uint64_t samplerKey = ((d3 >> 19) & 3) | (((d3 >> 21) & 3) << 2) | (((d3 >> 23) & 3) << 4)
                            | (((fetch[0] >> 10) & 7) << 6) | (((fetch[0] >> 13) & 7) << 9) | (((fetch[0] >> 16) & 7) << 12);
                        shared.samplerIndex[slot] = GetSamplerIndex(samplerKey);
                        RenderDescriptorSet* target = s->info.textureDimension[slot] == 2 ? set2 : s->info.textureDimension[slot] == 3 ? set3 : set1;
                        target->setTexture(slot, tex->texture.get(), RenderTextureLayout::SHADER_READ);
                    }
                };
                bindTextures(ps);
                bindTextures(vs);

                uint64_t sharedOffset = Upload(&shared, sizeof(shared));
                if (sharedOffset == UINT64_MAX)
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

                // Record.
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
                if (drawLogs < 24)
                {
                    drawLogs++;
                    LOG_INFO("renderer: draw prim={} n={} idx={} vs={:016x} ps={:016x} rt={:#x}/{} {}x{} vp=({},{} {}x{} z {}..{}) vte={:#x} scissor=({},{})-({},{}) ndc=({},{}) off=({},{}) mode={} c255=({:g},{:g},{:g},{:g})",
                        info.primitiveType, indexCount, useIndices, key.vs, key.ps, colorInfo & 0xFFF, (colorInfo >> 16) & 0xF, pitch, rtHeight,
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
            }

            // ---- resolve --------------------------------------------------------------------
            void Resolve()
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
                    static bool warned = false;
                    if (!warned) { LOG_WARNING("renderer: depth resolve not implemented"); warned = true; }
                    if (copyControl & 0x200) ClearDepthTarget(pitch, rtHeight);
                    return;
                }
                if (destFormat != 6 && destFormat != 32)
                {
                    if (loggedFormats.insert(0x100 + destFormat).second)
                        LOG_WARNING("renderer: unsupported resolve destination format {}", destFormat);
                    return;
                }

                uint32_t colorInfo = Reg(REG_RB_COLOR_INFO + (srcSelect < 4 ? (srcSelect == 0 ? 0 : 2 + (srcSelect - 1)) : 0));
                HostTexture* color = GetRenderTarget(colorInfo & 0xFFF, (colorInfo >> 16) & 0xF, pitch, rtHeight, false);
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
                if (resolveLogs < 12)
                {
                    resolveLogs++;
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
                InvalidateRange(destBase, pitchBlocks * copyHeight * (destFormat == 6 ? 4 : 8));

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
            g_renderer->Flush();
            g_renderer->frame++;
            g_renderer->drawsThisFrame = 0;
        }
    }

    void InvalidateGuestRange(uint32_t physicalAddress, uint32_t size)
    {
        if (g_renderer)
            g_renderer->InvalidateRange(physicalAddress, size);
    }
#else
    bool Init() { return false; }
    void Shutdown() {}
    void Draw(const DrawInfo&) {}
    void Flush() {}
    void InvalidateGuestRange(uint32_t, uint32_t) {}
#endif
}
