// Graphics-boundary model for the exact production TransferRegion body.
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>
#include "gpu/edram_transfer.h"
#include "gpu/pending_clear.h"
#include "gpu/render_resolution.h"

namespace pending_clear = gpu::pending_clear;
namespace edram_transfer = gpu::edram_transfer;
namespace resolution = gpu::resolution;
using Pixel = std::array<uint16_t, 4>;
using Rgba = std::array<float, 4>;
using Rect = pending_clear::Rect;
static size_t checks = 0, scenarios = 0, omitted = 0;
static void Check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
static Pixel Half(const float* c) {
    Pixel p{};
    for (int i = 0; i < 4; ++i) p[i] = edram_transfer::FloatToHalf(c[i], edram_transfer::Round::NearestEven);
    return p;
}
enum class RenderFormat { R16G16B16A16_FLOAT, R8G8B8A8_UNORM };
enum class RenderTextureLayout { SHADER_READ, COLOR_WRITE };
enum class RenderBarrierStage { GRAPHICS };
enum class SdrLossTrace { TileTransfer };
struct Texture { uint32_t width, height; std::vector<Pixel> pixels; };
struct Loss {
    uint64_t vs = 0;
    bool Lost(uint64_t, SdrLossTrace, uint32_t) { return true; }
};
struct HostTexture {
    uint64_t allocationSerial = 123, sdrProducerFrame = 5;
    bool hdrValid = true;
    Loss sdrLoss;
    RenderFormat format = RenderFormat::R16G16B16A16_FLOAT;
    uint32_t width, height, guestWidth, guestHeight;
    resolution::Size resolutionSize;
    pending_clear::Queue pendingClear;
    std::shared_ptr<Texture> texture;
    uint32_t ScaleX(uint32_t n) const { return resolution::ScaleX(n, resolutionSize.width); }
    uint32_t ScaleY(uint32_t n) const { return resolution::Scale(n, resolutionSize.height); }
    HostTexture(uint32_t w, uint32_t h, resolution::Size scale) : guestWidth(w), guestHeight(h), resolutionSize(scale) {
        width = ScaleX(w); height = ScaleY(h);
        texture = std::make_shared<Texture>(Texture{width, height, std::vector<Pixel>(size_t(width) * height)});
        for (size_t i = 0; i < texture->pixels.size(); ++i) {
            const float c[] = {float((i * 7) % 32) / 32, float((i * 11) % 64) / 64, 0.25f, 1.0f};
            texture->pixels[i] = Half(c);
        }
    }
};
static HostTexture Clone(const HostTexture& src) {
    HostTexture copy = src;
    copy.texture = std::make_shared<Texture>(*src.texture);
    return copy;
}
static void Apply(HostTexture& t) {
    for (const auto& op : t.pendingClear.Ops()) {
        if (!(op.aspects & pending_clear::Color)) continue;
        const Pixel value = Half(op.color);
        for (uint32_t y = 0; y < t.height; ++y) for (uint32_t x = 0; x < t.width; ++x) {
            const bool covered = op.Whole() || std::any_of(op.rects.begin(), op.rects.end(), [&](const Rect& r) {
                return int32_t(x) >= r.left && int32_t(x) < r.right && int32_t(y) >= r.top && int32_t(y) < r.bottom;
            });
            if (covered) t.texture->pixels[size_t(y) * t.width + x] = value;
        }
    }
    t.pendingClear.Drop();
}
static void Shader(Texture& src, Texture& dst, uint32_t w, uint32_t h, const uint32_t* constants) {
    const float sx = std::bit_cast<float>(constants[2]), sy = std::bit_cast<float>(constants[3]);
    for (uint32_t y = 0; y < std::min(h, dst.height); ++y) for (uint32_t x = 0; x < std::min(w, dst.width); ++x) {
        const uint32_t tx = uint32_t((float(x) + 0.5f) * sx), ty = uint32_t((float(y) + 0.5f) * sy);
        Rgba value{}, out{};
        if (tx < src.width && ty < src.height) {
            const auto& p = src.pixels[size_t(ty) * src.width + tx];
            for (int i = 0; i < 4; ++i) value[i] = edram_transfer::HalfToFloat(p[i]);
        }
        // Non-finite clears are decision-only cases: avoid undefined float-to-uint conversions.
        for (auto& f : value) if (!std::isfinite(f)) f = 0;
        edram_transfer::UnpackGuest(edram_transfer::PackGuest(value.data(), constants[0], false), constants[1], out.data());
        dst.pixels[size_t(y) * dst.width + x] = Half(out.data());
    }
}
struct SharedConstants { uint32_t transfer[4]{}; };
struct RenderPipeline {};
struct RenderViewport { RenderViewport(float, float, float, float) {} };
using RenderRect = Rect;
struct RenderDescriptorSet {
    Texture* image = nullptr;
    void setTexture(uint32_t, Texture* t, RenderTextureLayout) { image = t; }
};
struct Harness;
struct Commands {
    Harness* owner;
    RenderRect rect{};
    Texture* image = nullptr;
    void setViewports(const RenderViewport*, uint32_t) {}
    void setScissors(const RenderRect* r, uint32_t) { rect = *r; }
    void setPipeline(RenderPipeline*) {}
    void setGraphicsPipelineLayout(int*) {}
    void setGraphicsDescriptorSet(RenderDescriptorSet* s, uint32_t which) { if (which == 1) image = s->image; }
    void drawInstanced(uint32_t, uint32_t, uint32_t, uint32_t);
};
struct Harness {
    static constexpr uint32_t kClass7e3 = 2;
    uint64_t frame = 8;
    bool vulkan = true;
    uint32_t transfers = 0, draws = 0, queued = 0, flushes = 0, alpha = 0, uploads = 0;
    struct { uint32_t invalidations = 0; void Invalidate() { ++invalidations; } } consecutiveResolveCopies;
    SharedConstants uploaded;
    HostTexture* target = nullptr;
    std::function<void()> replay;
    std::unique_ptr<int> pipelineLayout = std::make_unique<int>();
    std::unique_ptr<RenderDescriptorSet> staticSet0 = std::make_unique<RenderDescriptorSet>();
    std::unique_ptr<RenderDescriptorSet> staticSamplerSet = std::make_unique<RenderDescriptorSet>();
    RenderDescriptorSet sets[4];
    RenderPipeline pipeline;
    Commands commands{this};
    Commands* commandList = &commands;
    void FlushMotionReplayQueue() { ++flushes; if (replay) replay(); }
    void QueueClear(HostTexture& dst, pending_clear::Op op) { ++queued; dst.pendingClear.Record(std::move(op), dst.width, dst.height); }
    void HandleFsrAlphaRgbWriter(HostTexture&, const char*) { ++alpha; }
    RenderPipeline* GetTransferPipeline(RenderFormat) { return &pipeline; }
    uint64_t Upload(const SharedConstants* c, size_t) { ++uploads; uploaded = *c; return 0; }
    bool Begin() { return true; }
    void Transition(HostTexture& t, RenderTextureLayout layout, RenderBarrierStage) { if (layout == RenderTextureLayout::SHADER_READ) Apply(t); }
    RenderDescriptorSet* AcquireSet(uint32_t which) { return &sets[which]; }
    void BindTargets(HostTexture* dst, void*) { target = dst; Apply(*dst); }
    void SetConstantBuffer(uint64_t, uint32_t) {}
    /* PRODUCTION_TRANSFER_REGION */
};
void Commands::drawInstanced(uint32_t, uint32_t, uint32_t, uint32_t) {
    ++owner->draws;
    Shader(*image, *owner->target->texture, uint32_t(rect.right), uint32_t(rect.bottom), owner->uploaded.transfer);
}
static void Run(HostTexture src, HostTexture dst, uint32_t sc, uint32_t dc, int expectClear, bool replayWrite = false) {
    ++scenarios;
    auto wantSrc = Clone(src), wantDst = Clone(dst);
    Apply(wantSrc); Apply(wantDst);
    if (replayWrite) {
        const float changed[] = {0.5f, 0.25f, 0.75f, 1};
        std::fill(wantSrc.texture->pixels.begin(), wantSrc.texture->pixels.end(), Half(changed));
    }
    const uint32_t w = dst.ScaleX(std::min(src.guestWidth, dst.guestWidth));
    const uint32_t h = dst.ScaleY(std::min(src.guestHeight, dst.guestHeight));
    const uint32_t constants[] = {sc, dc,
        std::bit_cast<uint32_t>(float(src.resolutionSize.width) / dst.resolutionSize.width),
        std::bit_cast<uint32_t>(float(src.resolutionSize.height) / dst.resolutionSize.height)};
    Shader(*wantSrc.texture, *wantDst.texture, w, h, constants);
    const auto beforePixels = src.texture->pixels;
    const auto beforeOps = src.pendingClear.Ops();
    Harness gpu;
    if (replayWrite) gpu.replay = [&] {
        Apply(src);
        const float changed[] = {0.5f, 0.25f, 0.75f, 1};
        std::fill(src.texture->pixels.begin(), src.texture->pixels.end(), Half(changed));
    };
    gpu.TransferRegion(src, dst, sc, dc);
    Check(gpu.flushes == 1 && gpu.consecutiveResolveCopies.invalidations == 1, "lost replay flush or resolve invalidation");
    Check(dst.sdrProducerFrame == ~0ull && !dst.hdrValid && dst.sdrLoss.vs == src.allocationSerial, "lost SDR/HDR bookkeeping");
    Check(gpu.transfers == 1 && gpu.alpha == 1, "lost transfer count or FSR-alpha notification");
    bool enforce = expectClear >= 0;
#ifdef LO_TEST_BASELINE
    enforce = expectClear == 0;
#endif
    if (enforce) {
        if (gpu.queued != uint32_t(expectClear))
            std::cerr << "scenario " << scenarios << ": class " << sc << " -> " << dc
                      << ", expected clear " << expectClear << ", got " << gpu.queued << '\n';
        Check(gpu.queued == uint32_t(expectClear), "unexpected transfer clear decision");
    }
    Check(gpu.draws + gpu.queued == 1, "transfer lost or emitted twice");
    if (gpu.queued) {
        ++omitted;
        Check(gpu.uploads == 0 && gpu.draws == 0, "constant transfer still recorded native work");
        Check(src.texture->pixels == beforePixels && src.pendingClear.Size() == beforeOps.size(), "source clear executed or consumed");
        for (size_t i = 0; i < beforeOps.size(); ++i) {
            Check(src.pendingClear.Ops()[i].SameValue(beforeOps[i]), "source clear value changed");
            const auto& a = src.pendingClear.Ops()[i].rects;
            const auto& b = beforeOps[i].rects;
            Check(a.size() == b.size(), "source clear rectangles changed");
            for (size_t j = 0; j < a.size(); ++j)
                Check(a[j].left == b[j].left && a[j].top == b[j].top && a[j].right == b[j].right && a[j].bottom == b[j].bottom,
                      "source clear region changed");
        }
    }
    Apply(src); Apply(dst); // Intermediate reads must observe all deferred clears.
    Check(src.texture->pixels == wantSrc.texture->pixels, "logical source contents changed");
    Check(dst.texture->pixels.size() == wantDst.texture->pixels.size(), "destination allocation changed");
    for (size_t i = 0; i < dst.texture->pixels.size(); ++i)
        Check(dst.texture->pixels[i] == wantDst.texture->pixels[i], "destination pixel differs from eager transfer");
}
int main() try {
    const Rgba zero{0, 0, 0, 0}, one{1, 1, 1, 1};
    for (auto scale : {resolution::Size{1280,720}, {1920,1080}, {2403,1080}, {3204,1440}, {3840,2160}}) {
        for (uint32_t sc = 0; sc < 3; ++sc) for (uint32_t dc = 0; dc < 3; ++dc) {
            Rgba guestOnes{};
            edram_transfer::UnpackGuest(0xFFFFFFFFu, sc, guestOnes.data());
            for (const auto& value : {zero, guestOnes}) {
                HostTexture src(12, 10, scale), dst(8, 6, scale);
                const int32_t w = int32_t(dst.width), h = int32_t(dst.height);
                src.pendingClear.Record(pending_clear::ColorOp(value.data(), {{0,0,w,h}}), src.width, src.height);
                Run(Clone(src), Clone(dst), sc, dc, 1); // Fails with unmodified upstream.
                // Each edge, and a bounding box made of disjoint rectangles, must fall back.
                for (const std::vector<Rect>& rects : {std::vector<Rect>{{1,0,w,h}}, {{0,1,w,h}}, {{0,0,w-1,h}},
                        {{0,0,w,h-1}}, {{0,0,w/2,h},{w/2+1,0,w,h}}, {{0,0,w,h/2},{0,h/2,w,h}}}) {
                    auto partial = Clone(src); partial.pendingClear.Drop();
                    partial.pendingClear.Record(pending_clear::ColorOp(value.data(), rects), partial.width, partial.height);
                    Run(std::move(partial), Clone(dst), sc, dc, 0);
                }
                auto merged = Clone(src);
                merged.pendingClear.Record(pending_clear::ColorOp(value.data(), {{w,0,w+1,1}}), merged.width, merged.height);
                Run(std::move(merged), Clone(dst), sc, dc, 1);
                auto large = Clone(src); large.pendingClear.Drop();
                std::vector<Rect> rects{{0,0,w,h}};
                const int32_t tail = int32_t(large.width) - w;
                for (int32_t j = 0; j < 32; ++j) {
                    const int32_t x = w + j % tail, y = j / tail;
                    rects.push_back({x,y,x+1,y+1});
                }
                large.pendingClear.Record(pending_clear::ColorOp(value.data(), std::move(rects)), large.width, large.height);
                Run(std::move(large), Clone(dst), sc, dc, 0);
                Run(Clone(src), Clone(dst), sc, dc, 0, true); // Replay writes must precede the decision.
                auto whole = Clone(src); whole.pendingClear.Drop();
                whole.pendingClear.Record(pending_clear::ColorOp(value.data()), whole.width, whole.height);
                HostTexture otherScale(8,6,{1280,1080});
                Run(std::move(whole), Clone(otherScale), sc, dc, 1); // Preserve the existing whole-clear path.
                Run(Clone(src), std::move(otherScale), sc, dc, scale == resolution::Size{1280,1080} ? 1 : 0);
                auto queued = Clone(src);
                const float different[] = {0.25f, 0.5f, 0.75f, 1};
                queued.pendingClear.Record(pending_clear::ColorOp(different, {{0,0,1,1}}), queued.width, queued.height);
                Run(std::move(queued), Clone(dst), sc, dc, 0);
                auto otherFormat = Clone(src); otherFormat.format = RenderFormat::R8G8B8A8_UNORM;
                Run(std::move(otherFormat), Clone(dst), sc, dc, 0);
                dst.format = RenderFormat::R8G8B8A8_UNORM;
                Run(Clone(src), std::move(dst), sc, dc, 0);
            }
        }
    }
    // A transfer covers only part of the destination: pending writes outside it survive.
    for (uint32_t sc = 0; sc < 3; ++sc) for (uint32_t dc = 0; dc < 3; ++dc) {
        HostTexture src(12,4,{}), dst(8,6,{});
        src.pendingClear.Record(pending_clear::ColorOp(zero.data(), {{0,0,8,4}}), src.width, src.height);
        dst.pendingClear.Record(pending_clear::ColorOp(one.data()), dst.width, dst.height);
        Run(std::move(src), std::move(dst), sc, dc, 1);
    }
    // Seeded queues, random clear values, gaps and read boundaries. Pixel oracle
    // checks accepted AND rejected conversions without duplicating the selection rule.
    std::mt19937 rng(0xEDEDA123u);
    for (uint32_t i = 0; i < 2500; ++i) {
        HostTexture src(12,10,{}), dst(8,6,{});
        Rgba value{};
        for (auto& c : value) c = float(rng() % 1025) / 1024;
        if (i % 3 == 0) value = zero;
        const int32_t edge = int32_t(7 + rng() % 4);
        src.pendingClear.Record(pending_clear::ColorOp(value.data(), {{0,0,edge,6}}), src.width, src.height);
        if (i % 5 == 0) src.pendingClear.Record(pending_clear::ColorOp(one.data(), {{2,1,3,2}}), src.width, src.height);
        dst.pendingClear.Record(pending_clear::ColorOp(one.data(), {{0,4,4,6}}), dst.width, dst.height);
        Run(std::move(src), std::move(dst), rng() % 3, rng() % 3, -1);
    }
    // Empty queues, non-color clears, conversion ambiguity and signed zero.
    {
        HostTexture src(12,10,{}), dst(8,6,{});
        Run(Clone(src), Clone(dst), 0, 0, 0);
        auto mixed = pending_clear::ColorOp(zero.data(), {{0,0,8,6}});
        mixed.aspects |= pending_clear::Depth;
        src.pendingClear.Record(std::move(mixed), src.width, src.height);
        Run(Clone(src), Clone(dst), 0, 0, 0);
        src.pendingClear.Drop();
        const float ambiguous[] = {0.1f, 0.2f, 0.3f, 0.4f};
        src.pendingClear.Record(pending_clear::ColorOp(ambiguous, {{0,0,8,6}}), src.width, src.height);
        Run(Clone(src), Clone(dst), 0, 0, 0);
        src.pendingClear.Drop();
        const float signedZero[] = {-0.0f, 0.0f, -0.0f, 0.0f};
        src.pendingClear.Record(pending_clear::ColorOp(signedZero, {{0,0,8,6}}), src.width, src.height);
        Run(Clone(src), Clone(dst), 0, 0, 1);
        src.pendingClear.Drop();
        std::vector<Rect> rects{{0,0,8,6}};
        for (int32_t j = 0; j < 31; ++j) {
            const int32_t x = 8 + j % 4, y = j / 4;
            rects.push_back({x,y,x+1,y+1});
        }
        src.pendingClear.Record(pending_clear::ColorOp(zero.data(), std::move(rects)), src.width, src.height);
        Run(Clone(src), Clone(dst), 0, 0, 1);
    }
    std::cout << "PASS: " << checks << " checks, " << scenarios << " transfers, " << omitted
              << " synthetic transfers used clears; no hardware timing measured\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << '\n'; return 1;
}
