// Deferred EDRAM clears on the GPU: one script of clears, draws, transfers and
// resolves runs three ways and every resolve must read back the same bytes.
//   reference: clear at once, whole clears split in two rectangles so plume
//              records them inside a pass (vkCmdClearAttachments only);
//   eager:     clear at once (plume Vulkan turns whole clears into loadOp CLEAR);
//   lazy:      gpu/pending_clear.h with the renderer's execution points (and the
//              drop of owed clears under a transfer over the whole target).
// Usage: LoLazyClearGpuTest [--vulkan] [seeds]
#include <gpu/pending_clear.h>
#include <gpu/shader/dxc_compiler.h>
#include <plume_render_interface.h>
#include <plume_render_interface_builders.h>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace plume {
#ifdef _WIN32
std::unique_ptr<RenderInterface> CreateD3D12Interface();
#endif
std::unique_ptr<RenderInterface> CreateVulkanInterface();
}

namespace {
using namespace plume;
namespace pc = gpu::pending_clear;

enum class Mode { Reference, Eager, Lazy };

struct Target {
    std::unique_ptr<RenderTexture> texture;
    RenderFormat format = RenderFormat::UNKNOWN;
    uint32_t width = 0, height = 0;
    bool depth = false;
    RenderTextureLayout layout = RenderTextureLayout::UNKNOWN;
    pc::Queue pending;
    uint32_t Bytes() const { return format == RenderFormat::R16G16B16A16_FLOAT ? 8 : 4; }
};

struct Shaders {
    std::unique_ptr<RenderShader> vs, draw, blit;
    std::unique_ptr<RenderPipelineLayout> layout;
    RenderDescriptorSetBuilder textures;
};

class Harness {
public:
    Harness(RenderDevice& device, RenderCommandQueue& queue, Shaders& shaders, Mode mode)
        : device_(device), queue_(queue), shaders_(shaders), mode_(mode)
    {
        commands_ = queue_.createCommandList();
        fence_ = device_.createCommandFence();
        if (!commands_ || !fence_) throw std::runtime_error("command list");
        commands_->begin();
    }

    Target& Add(RenderFormat format, uint32_t w, uint32_t h)
    {
        auto t = std::make_unique<Target>();
        t->format = format; t->width = w; t->height = h;
        t->depth = format == RenderFormat::D32_FLOAT_S8_UINT;
        t->texture = device_.createTexture(RenderTextureDesc::Texture2D(w, h, 1, format,
            t->depth ? RenderTextureFlag::DEPTH_TARGET : RenderTextureFlag::RENDER_TARGET));
        if (!t->texture) throw std::runtime_error("texture");
        targets_.push_back(std::move(t));
        return *targets_.back();
    }
    std::vector<std::unique_ptr<Target>>& Targets() { return targets_; }

    // ---- the renderer's entry points ----------------------------------------
    void Transition(Target& t, RenderTextureLayout layout)
    {
        if (!t.pending.Empty() && layout != RenderTextureLayout::COLOR_WRITE && layout != RenderTextureLayout::DEPTH_WRITE)
            Run(t);
        Barrier(t, layout);
    }
    void Clear(Target& t, pc::Op op)
    {
        if (mode_ == Mode::Lazy) {
            if (t.pending.Size() >= pc::Queue::kMaxOps) Run(t);
            t.pending.Record(std::move(op), t.width, t.height);
            return;
        }
        if (mode_ == Mode::Reference && op.Whole()) {
            const int32_t half = int32_t(t.height / 2);
            op.rects = {{0, 0, int32_t(t.width), half}, {0, half, int32_t(t.width), int32_t(t.height)}};
        }
        Barrier(t, t.depth ? RenderTextureLayout::DEPTH_WRITE : RenderTextureLayout::COLOR_WRITE);
        commands_->setFramebuffer(Framebuffer(t.depth ? nullptr : &t, t.depth ? &t : nullptr));
        Replay(t, {op});
    }
    void Bind(Target* color, Target* depth)
    {
        const Target* area = color ? color : depth;
        for (Target* t : {color, depth})
            if (t && !t->pending.Empty() && !t->pending.FitsPass(t->width, t->height, area->width, area->height)) Run(*t);
        commands_->setFramebuffer(Framebuffer(color, depth));
        if (color && !color->pending.Empty()) Replay(*color, color->pending.Take());
        if (depth && !depth->pending.Empty()) Replay(*depth, depth->pending.Take());
    }

    // ---- script operations ----------------------------------------------------
    // kind 0: depth test LESS_EQUAL + write, stencil INCR_WRAP; kind 1..3: stencil
    // EQUAL probe with reference kind-1, no depth. Colour shows depth and position.
    void Draw(Target* color, Target* depth, uint32_t kind, RenderRect rect, float minDepth)
    {
        if (color) Transition(*color, RenderTextureLayout::COLOR_WRITE);
        if (depth) Transition(*depth, RenderTextureLayout::DEPTH_WRITE);
        Bind(color, depth);
        const Target& area = color ? *color : *depth;
        RenderViewport viewport(0, 0, float(area.width), float(area.height), minDepth, minDepth + 0.25f);
        commands_->setViewports(&viewport, 1);
        commands_->setScissors(&rect, 1);
        commands_->setGraphicsPipelineLayout(shaders_.layout.get());
        commands_->setPipeline(DrawPipeline(color ? color->format : RenderFormat::UNKNOWN, depth != nullptr, kind));
        commands_->drawInstanced(3, 1, 0, 0);
    }
    // TransferRegion/BlitRegion: a draw that samples src into dst.
    void Blit(Target& src, Target& dst, RenderRect rect)
    {
        Transition(src, RenderTextureLayout::SHADER_READ);
        Transition(dst, RenderTextureLayout::COLOR_WRITE);
        auto set = shaders_.textures.create(&device_);
        set->setTexture(0, src.texture.get(), RenderTextureLayout::SHADER_READ);
        // TransferRegion: a transfer over the whole target replaces every pixel.
        if (mode_ == Mode::Lazy && rect.left <= 0 && rect.top <= 0 &&
            rect.right >= int32_t(dst.width) && rect.bottom >= int32_t(dst.height)) dst.pending.Drop();
        Bind(&dst, nullptr);
        RenderViewport viewport(0, 0, float(dst.width), float(dst.height));
        commands_->setViewports(&viewport, 1);
        commands_->setScissors(&rect, 1);
        commands_->setGraphicsPipelineLayout(shaders_.layout.get());
        commands_->setPipeline(BlitPipeline(dst.format));
        commands_->setGraphicsDescriptorSet(set.get(), 1);
        commands_->drawInstanced(3, 1, 0, 0);
        keep_.push_back(std::move(set));
    }
    // A resolve or readback: copy outside any pass.
    void Resolve(Target& t)
    {
        Transition(t, RenderTextureLayout::COPY_SOURCE);
        const uint32_t pitch = (t.width * t.Bytes() + 255) & ~255u;
        auto buffer = device_.createBuffer(RenderBufferDesc::ReadbackBuffer(uint64_t(pitch) * t.height));
        if (!buffer) throw std::runtime_error("readback buffer");
        commands_->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(buffer.get(),
            t.depth ? RenderFormat::R32_FLOAT : t.format, t.width, t.height, 1, pitch / t.Bytes(), 0),
            RenderTextureCopyLocation::Subresource(t.texture.get(), 0));
        readbacks_.push_back({std::move(buffer), pitch, t.width * t.Bytes(), t.height});
    }
    void Submit()
    {
        commands_->end();
        const RenderCommandList* lists[] = {commands_.get()};
        queue_.executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence_.get());
        queue_.waitForCommandFence(fence_.get());
        for (auto& r : readbacks_) {
            const auto* bytes = static_cast<const uint8_t*>(r.buffer->map());
            for (uint32_t y = 0; y < r.rows; ++y)
                bytes_.insert(bytes_.end(), bytes + size_t(y) * r.pitch, bytes + size_t(y) * r.pitch + r.rowBytes);
            r.buffer->unmap();
        }
        readbacks_.clear();
        keep_.clear();
        commands_->begin();
        // A new command list: layouts persist, bindings do not.
    }
    const std::vector<uint8_t>& Bytes() const { return bytes_; }

private:
    struct Readback { std::unique_ptr<RenderBuffer> buffer; uint32_t pitch, rowBytes, rows; };

    void Barrier(Target& t, RenderTextureLayout layout)
    {
        if (t.layout == layout) return;
        const auto stages = layout == RenderTextureLayout::COPY_SOURCE ? RenderBarrierStage::COPY : RenderBarrierStage::GRAPHICS;
        commands_->barriers(stages, RenderTextureBarrier(t.texture.get(), layout));
        t.layout = layout;
    }
    void Run(Target& t)
    {
        const auto ops = t.pending.Take();
        Barrier(t, t.depth ? RenderTextureLayout::DEPTH_WRITE : RenderTextureLayout::COLOR_WRITE);
        commands_->setFramebuffer(Framebuffer(t.depth ? nullptr : &t, t.depth ? &t : nullptr));
        Replay(t, ops);
    }
    void Replay(const Target& t, const std::vector<pc::Op>& ops)
    {
        for (const auto& op : ops) {
            std::vector<RenderRect> rects;
            for (const auto& r : op.rects) rects.push_back({r.left, r.top, r.right, r.bottom});
            const RenderRect* data = rects.empty() ? nullptr : rects.data();
            if (t.depth) commands_->clearDepthStencil(op.aspects & pc::Depth, op.aspects & pc::Stencil, op.depth, op.stencil, data, uint32_t(rects.size()));
            else commands_->clearColor(0, RenderColor(op.color[0], op.color[1], op.color[2], op.color[3]), data, uint32_t(rects.size()));
        }
    }
    RenderFramebuffer* Framebuffer(Target* color, Target* depth)
    {
        auto& slot = framebuffers_[{color, depth}];
        if (!slot) {
            const RenderTexture* colors[] = {color ? color->texture.get() : nullptr};
            slot = device_.createFramebuffer(RenderFramebufferDesc(colors, color ? 1u : 0u, depth ? depth->texture.get() : nullptr));
            if (!slot) throw std::runtime_error("framebuffer");
        }
        return slot.get();
    }
    RenderPipeline* DrawPipeline(RenderFormat colorFormat, bool depth, uint32_t kind)
    {
        auto& slot = drawPipelines_[{uint32_t(colorFormat), depth, kind}];
        if (slot) return slot.get();
        RenderGraphicsPipelineDesc desc;
        desc.pipelineLayout = shaders_.layout.get();
        desc.vertexShader = shaders_.vs.get();
        desc.pixelShader = colorFormat != RenderFormat::UNKNOWN ? shaders_.draw.get() : nullptr;
        desc.cullMode = RenderCullMode::NONE;
        if (colorFormat != RenderFormat::UNKNOWN) {
            desc.renderTargetCount = 1;
            desc.renderTargetFormat[0] = colorFormat;
            desc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        }
        if (depth) {
            desc.depthTargetFormat = RenderFormat::D32_FLOAT_S8_UINT;
            desc.depthEnabled = kind == 0;
            desc.depthWriteEnabled = kind == 0;
            desc.depthFunction = kind == 0 ? RenderComparisonFunction::LESS_EQUAL : RenderComparisonFunction::ALWAYS;
            desc.stencilEnabled = true;
            desc.stencilReference = kind == 0 ? 0 : kind - 1;
            auto& face = desc.stencilFrontFace;
            face.compareFunction = kind == 0 ? RenderComparisonFunction::ALWAYS : RenderComparisonFunction::EQUAL;
            face.passOp = kind == 0 ? RenderStencilOp::INCREMENT_AND_WRAP : RenderStencilOp::KEEP;
            desc.stencilBackFace = face;
        }
        slot = device_.createGraphicsPipeline(desc);
        if (!slot) throw std::runtime_error("draw pipeline");
        return slot.get();
    }
    RenderPipeline* BlitPipeline(RenderFormat format)
    {
        auto& slot = blitPipelines_[uint32_t(format)];
        if (slot) return slot.get();
        RenderGraphicsPipelineDesc desc;
        desc.pipelineLayout = shaders_.layout.get();
        desc.vertexShader = shaders_.vs.get();
        desc.pixelShader = shaders_.blit.get();
        desc.cullMode = RenderCullMode::NONE;
        desc.renderTargetCount = 1;
        desc.renderTargetFormat[0] = format;
        desc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        slot = device_.createGraphicsPipeline(desc);
        if (!slot) throw std::runtime_error("blit pipeline");
        return slot.get();
    }

    RenderDevice& device_;
    RenderCommandQueue& queue_;
    Shaders& shaders_;
    Mode mode_;
    std::unique_ptr<RenderCommandList> commands_;
    std::unique_ptr<RenderCommandFence> fence_;
    std::vector<std::unique_ptr<Target>> targets_;
    std::map<std::pair<Target*, Target*>, std::unique_ptr<RenderFramebuffer>> framebuffers_;
    std::map<std::tuple<uint32_t, bool, uint32_t>, std::unique_ptr<RenderPipeline>> drawPipelines_;
    std::map<uint32_t, std::unique_ptr<RenderPipeline>> blitPipelines_;
    std::vector<Readback> readbacks_;
    std::vector<std::unique_ptr<RenderDescriptorSet>> keep_;
    std::vector<uint8_t> bytes_;
};

// colour 0..2: 64x48 (FP16, RGBA8, FP16), 3: 32x32 FP16; depth 4: 64x48,
// 5: 64x64 (taller than the colour targets it pairs with), 6: 32x32.
void AddTargets(Harness& h)
{
    h.Add(RenderFormat::R16G16B16A16_FLOAT, 64, 48);
    h.Add(RenderFormat::R8G8B8A8_UNORM, 64, 48);
    h.Add(RenderFormat::R16G16B16A16_FLOAT, 64, 48);
    h.Add(RenderFormat::R16G16B16A16_FLOAT, 32, 32);
    h.Add(RenderFormat::D32_FLOAT_S8_UINT, 64, 48);
    h.Add(RenderFormat::D32_FLOAT_S8_UINT, 64, 64);
    h.Add(RenderFormat::D32_FLOAT_S8_UINT, 32, 32);
}

RenderRect RandomRect(std::mt19937& rng, uint32_t w, uint32_t h)
{
    const int x0 = int(rng() % w), y0 = int(rng() % h);
    return {x0, y0, x0 + 1 + int(rng() % (w - x0)), y0 + 1 + int(rng() % (h - y0))};
}

std::vector<pc::Rect> RandomClearRects(std::mt19937& rng, uint32_t w, uint32_t h)
{
    std::vector<pc::Rect> rects;
    switch (rng() % 4) {
    case 0: return rects; // whole
    case 1: rects.push_back({0, 0, int32_t(w), int32_t(h)}); return rects; // whole as one rectangle
    default:
        for (uint32_t i = 0, n = 1 + rng() % 5; i < n; ++i) {
            const auto r = RandomRect(rng, w, h);
            rects.push_back({r.left, r.top, r.right, r.bottom});
        }
        return rects;
    }
}

// The same seed gives the same operations in every mode.
void RunScript(Harness& h, uint32_t seed)
{
    std::mt19937 rng(seed);
    auto& t = h.Targets();
    // Defined contents first, so no step reads uninitialised memory.
    const float zero[4] = {0, 0, 0, 0};
    for (uint32_t i = 0; i < 4; ++i) h.Clear(*t[i], pc::ColorOp(zero));
    for (uint32_t i = 4; i < 7; ++i) h.Clear(*t[i], pc::DepthOp(true, true, 1.0f, 0));
    const uint32_t steps = 30 + rng() % 60;
    for (uint32_t s = 0; s < steps; ++s) {
        switch (rng() % 10) {
        case 0: case 1: { // colour clear
            Target& c = *t[rng() % 4];
            const float v = float(rng() % 5) * 0.25f;
            const float rgba[4] = {v, 1.0f - v, 0.5f, v * 0.5f};
            h.Clear(c, pc::ColorOp(rgba, RandomClearRects(rng, c.width, c.height)));
            break;
        }
        case 2: case 3: { // depth/stencil clear, any aspect combination
            Target& d = *t[4 + rng() % 3];
            const uint32_t aspects = 1 + rng() % 3;
            h.Clear(d, pc::DepthOp(aspects & 1, aspects & 2, float(rng() % 5) * 0.25f, rng() % 3,
                RandomClearRects(rng, d.width, d.height)));
            break;
        }
        case 4: case 5: { // colour + depth draw
            const uint32_t c = rng() % 4;
            const uint32_t d = c == 3 ? 6 : 4 + rng() % 2;
            const uint32_t kind = rng() % 4;
            h.Draw(t[c].get(), t[d].get(), kind, RandomRect(rng, t[c]->width, t[c]->height), float(rng() % 4) * 0.25f);
            break;
        }
        case 6: { // colour-only or depth-only draw
            if (rng() % 2) {
                Target& c = *t[rng() % 4];
                h.Draw(&c, nullptr, 0, RandomRect(rng, c.width, c.height), 0.0f);
            } else {
                Target& d = *t[4 + rng() % 3];
                h.Draw(nullptr, &d, 0, RandomRect(rng, d.width, d.height), float(rng() % 4) * 0.25f);
            }
            break;
        }
        case 7: { // transfer between the 64x48 colour views
            const uint32_t src = rng() % 3, dst = (src + 1 + rng() % 2) % 3;
            h.Blit(*t[src], *t[dst], rng() % 2 ? RenderRect{0, 0, 64, 48} : RandomRect(rng, 64, 48));
            break;
        }
        case 8: // resolve / readback
            h.Resolve(*t[rng() % 7]);
            break;
        default: // command list boundary
            if (rng() % 3 == 0) h.Submit();
            break;
        }
    }
    for (auto& target : t) h.Resolve(*target);
    h.Submit();
}

} // namespace

int main(int argc, char** argv)
{
    try {
#ifdef _WIN32
        const bool vulkan = argc > 1 && std::string(argv[1]) == "--vulkan";
#else
        const bool vulkan = true;
#endif
        const int seedArg = argc > 1 && std::string(argv[1]) == "--vulkan" ? 2 : 1;
        const uint32_t seeds = argc > seedArg ? uint32_t(std::stoul(argv[seedArg])) : 200;
#ifdef _WIN32
        auto api = vulkan ? CreateVulkanInterface() : CreateD3D12Interface();
#else
        auto api = CreateVulkanInterface();
#endif
        if (!api) throw std::runtime_error("render interface");
        auto device = api->createDevice();
        if (!device) throw std::runtime_error("render device");
        std::printf("Backend: %s on %s\n", vulkan ? "Vulkan" : "D3D12", device->getDescription().name.c_str());
        auto queue = device->createCommandQueue(RenderCommandListType::DIRECT);
        const auto binaryFormat = vulkan ? xenos::ShaderBinaryFormat::Spirv : xenos::ShaderBinaryFormat::Dxil;
        const auto shaderFormat = vulkan ? RenderShaderFormat::SPIRV : RenderShaderFormat::DXIL;
        const char* vs = "float4 main(uint id:SV_VertexID):SV_Position { float2 uv=float2((id<<1)&2,id&2); return float4(uv*float2(2,-2)+float2(-1,1),0.5,1); }";
        const char* draw = "float4 main(float4 p:SV_Position):SV_Target { return float4(frac(p.x*0.137), frac(p.y*0.311), p.z, 1); }";
        const char* blit = "Texture2D<float4> src : register(t0, space1);\n"
            "float4 main(float4 p:SV_Position):SV_Target { return src.Load(int3(int2(p.xy),0))*0.5+float4(0.25,0.125,0.0625,0.5); }";
        Shaders shaders;
        for (auto [source, profile, out] : {std::tuple{vs, "vs_6_0", &shaders.vs}, std::tuple{draw, "ps_6_0", &shaders.draw},
                                            std::tuple{blit, "ps_6_0", &shaders.blit}}) {
            auto compiled = xenos::CompileCachedHlsl(source, "main", profile, binaryFormat);
            if (!compiled.ok) throw std::runtime_error(compiled.errors);
            *out = device->createShader(compiled.bytecode.data(), compiled.bytecode.size(), "main", shaderFormat);
        }
        RenderDescriptorSetBuilder empty;
        empty.begin(); empty.end();
        shaders.textures.begin(); shaders.textures.addTexture(0); shaders.textures.end();
        RenderPipelineLayoutBuilder layout;
        layout.begin(false, false); layout.addDescriptorSet(empty); layout.addDescriptorSet(shaders.textures); layout.end();
        shaders.layout = layout.create(device.get());

        size_t compared = 0;
        for (uint32_t seed = 1; seed <= seeds; ++seed) {
            std::vector<uint8_t> bytes[3];
            for (int mode = 0; mode < 3; ++mode) {
                Harness h(*device, *queue, shaders, Mode(mode));
                AddTargets(h);
                RunScript(h, seed);
                bytes[mode] = h.Bytes();
            }
            for (int mode = 1; mode < 3; ++mode) {
                if (bytes[mode] != bytes[0]) {
                    size_t first = 0;
                    while (first < std::min(bytes[0].size(), bytes[mode].size()) && bytes[0][first] == bytes[mode][first]) ++first;
                    std::printf("FAIL: seed %u %s differs from reference at byte %zu (sizes %zu/%zu)\n",
                        seed, mode == 1 ? "eager" : "lazy", first, bytes[0].size(), bytes[mode].size());
                    return 1;
                }
            }
            compared += bytes[0].size();
        }
        std::printf("PASS: %u scripts, reference / eager (loadOp CLEAR) / lazy readbacks byte-identical (%zu bytes each)\n",
            seeds, compared);
        return 0;
    } catch (const std::exception& e) {
        std::printf("FAIL: %s\n", e.what());
        return 2;
    }
}
