// Deferred EDRAM clears (gpu/pending_clear.h): queue rules, where the queue
// runs, and a randomized check that deferring gives the same pixels as
// clearing at once on a model of the plume Vulkan pass rules.
#include "gpu/pending_clear.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <optional>
#include <random>
#include <string>
#include <vector>

using namespace gpu::pending_clear;

namespace {

Op Color(float v, std::vector<Rect> rects = {}) { const float c[4] = {v, v, v, 1.0f}; return ColorOp(c, std::move(rects)); }

void QueueRules()
{
    Queue q;
    q.Record(Color(0.25f), 16, 8);
    q.Record(Color(0.5f), 16, 8);
    assert(q.Size() == 1 && q.Ops()[0].color[0] == 0.5f && q.Ops()[0].Whole());

    // A later partial clear keeps its order after the whole one.
    q.Record(Color(0.75f, {{0, 0, 4, 4}}), 16, 8);
    assert(q.Size() == 2 && !q.Ops()[1].Whole());
    // The same value merges its rectangles into the last op.
    q.Record(Color(0.75f, {{8, 0, 12, 4}}), 16, 8);
    assert(q.Size() == 2 && q.Ops()[1].rects.size() == 2);
    // A partial clear of the value a whole clear already wrote is redundant.
    Queue same;
    same.Record(Color(0.5f), 16, 8);
    same.Record(Color(0.5f, {{1, 1, 2, 2}}), 16, 8);
    assert(same.Size() == 1 && same.Ops()[0].Whole());

    // A rectangle that covers the texture is a whole clear and drops the rest.
    q.Record(Color(1.0f, {{0, 0, 16, 8}}), 16, 8);
    assert(q.Size() == 1 && q.Ops()[0].Whole() && q.Ops()[0].color[0] == 1.0f);

    // A whole depth clear drops only the depth half of an older depth+stencil clear.
    Queue d;
    d.Record(DepthOp(true, true, 1.0f, 7), 16, 8);
    d.Record(DepthOp(true, false, 0.0f, 0), 16, 8);
    assert(d.Size() == 2);
    assert(d.Ops()[0].aspects == Stencil && d.Ops()[0].stencil == 7);
    assert(d.Ops()[1].aspects == Depth && d.Ops()[1].depth == 0.0f);
    d.Record(DepthOp(false, true, 0.0f, 3), 16, 8);
    assert(d.Size() == 2 && d.Ops()[0].aspects == Depth && d.Ops()[1].aspects == Stencil && d.Ops()[1].stencil == 3);
    d.Record(DepthOp(true, true, 0.5f, 1), 16, 8);
    assert(d.Size() == 1 && d.Ops()[0].aspects == (Depth | Stencil));
    // Equal values compare by bits: -0 and +0 are different clears.
    Queue z;
    z.Record(DepthOp(true, false, 0.0f, 0, {{0, 0, 2, 2}}), 16, 8);
    z.Record(DepthOp(true, false, -0.0f, 0, {{2, 0, 4, 2}}), 16, 8);
    assert(z.Size() == 2);

    // Fitting a pass: whole clears need the render area to be the texture;
    // rectangles must lie inside the render area.
    Queue f;
    f.Record(DepthOp(true, false, 1.0f, 0), 16, 8);
    assert(f.FitsPass(16, 8, 16, 8));
    assert(!f.FitsPass(16, 8, 16, 6));
    Queue r;
    r.Record(DepthOp(true, false, 1.0f, 0, {{0, 0, 16, 6}}), 16, 8);
    assert(r.FitsPass(16, 8, 16, 6) && !r.FitsPass(16, 8, 16, 4));
    auto taken = r.Take();
    assert(taken.size() == 1 && r.Empty());

    // Storage handed back after a run serves later clears; the rules hold.
    Queue s;
    s.Record(DepthOp(true, false, 1.0f, 0, {{0, 0, 4, 4}, {4, 0, 8, 4}}), 16, 8);
    auto ran = s.Take();
    s.Recycle(std::move(ran));
    assert(s.Empty());
    auto spare = s.SpareRects();
    assert(spare.empty() && spare.capacity() >= 2);
    spare.assign({{0, 0, 2, 2}});
    s.Record(DepthOp(true, false, 0.5f, 0, std::move(spare)), 16, 8);
    s.Record(DepthOp(true, false, 0.5f, 0, {{2, 0, 4, 2}}), 16, 8);
    assert(s.Size() == 1 && s.Ops()[0].rects.size() == 2 && s.Ops()[0].rects[1].left == 2);
    assert(s.SpareRects().capacity() >= 1); // the merged clear's list
    assert(s.SpareRects().capacity() == 0);
    s.Record(DepthOp(true, false, 0.25f, 0, {{0, 0, 16, 8}}), 16, 8);
    assert(s.Size() == 1 && s.Ops()[0].Whole() && s.Ops()[0].depth == 0.25f);
    std::puts("PASS: queue rules (whole replaces, order kept, merge, aspects, -0, fit)");
}

// ---- model of a texture and of the plume Vulkan pass rules ------------------

struct Texture {
    int id = 0;
    bool depth = false;
    uint32_t width = 0, height = 0;
    std::vector<uint32_t> value;   // colour or depth bits
    std::vector<uint32_t> stencil; // depth targets
    Queue pending;
    Texture(int i, bool d, uint32_t w, uint32_t h) : id(i), depth(d), width(w), height(h),
        value(size_t(w) * h, 0xDEADu + i), stencil(size_t(w) * h, 0x5Au + i) {}
};

uint32_t Bits(float v) { return std::bit_cast<uint32_t>(v); }

struct Gpu {
    Texture* color = nullptr;
    Texture* depth = nullptr;
    bool active = false;
    std::optional<uint32_t> startColor, startDepth, startStencil;
    std::vector<std::string> log;
    uint32_t passes = 0, clearCalls = 0;

    uint32_t AreaW() const { return color ? color->width : depth->width; }
    uint32_t AreaH() const { return color ? color->height : depth->height; }
    void Fill(Texture& t, std::vector<uint32_t>& plane, uint32_t v, const Rect& r)
    {
        for (int y = r.top; y < r.bottom; ++y)
            for (int x = r.left; x < r.right; ++x)
                plane[size_t(y) * t.width + x] = v;
    }
    void Begin()
    {
        if (active) return;
        ++passes;
        active = true;
        const Rect area{0, 0, int32_t(AreaW()), int32_t(AreaH())};
        std::string ops;
        if (startColor) { Fill(*color, color->value, *startColor, area); ops += " colorCLEAR"; }
        if (startDepth) { Fill(*depth, depth->value, *startDepth, area); ops += " depthCLEAR"; }
        if (startStencil) { Fill(*depth, depth->stencil, *startStencil, area); ops += " stencilCLEAR"; }
        startColor.reset(); startDepth.reset(); startStencil.reset();
        log.push_back("begin" + ops);
    }
    void End()
    {
        if (startColor || startDepth || startStencil) Begin(); // an unconsumed start clear still runs
        if (active) { log.push_back("end"); active = false; }
    }
    void SetFramebuffer(Texture* c, Texture* d)
    {
        if (c == color && d == depth) return;
        End();
        assert(!c || !d || (d->width >= c->width && d->height >= c->height)); // framebuffer extent rule
        color = c; depth = d;
    }
    bool Covers(const std::vector<Rect>& rects) const
    {
        return rects.empty() || (rects.size() == 1 && rects[0].left <= 0 && rects[0].top <= 0 &&
            rects[0].right >= int32_t(AreaW()) && rects[0].bottom >= int32_t(AreaH()));
    }
    void ClearRects(Texture& t, std::vector<uint32_t>& plane, uint32_t v, const std::vector<Rect>& rects)
    {
        if (rects.empty()) { Fill(t, plane, v, {0, 0, int32_t(AreaW()), int32_t(AreaH())}); return; }
        for (const auto& r : rects) {
            assert(r.left >= 0 && r.top >= 0 && r.right <= int32_t(AreaW()) && r.bottom <= int32_t(AreaH()));
            Fill(t, plane, v, r);
        }
    }
    void ClearColor(float v, const std::vector<Rect>& rects)
    {
        assert(color);
        ++clearCalls;
        if (!active && Covers(rects)) { startColor = Bits(v); return; }
        Begin();
        ClearRects(*color, color->value, Bits(v), rects);
        log.push_back("clearColor");
    }
    void ClearDepthStencil(bool d, bool s, float z, uint32_t sv, const std::vector<Rect>& rects)
    {
        assert(depth);
        ++clearCalls;
        if (!active && Covers(rects)) {
            if (d) startDepth = Bits(z);
            if (s) startStencil = sv;
            return;
        }
        Begin();
        if (d) ClearRects(*depth, depth->value, Bits(z), rects);
        if (s) ClearRects(*depth, depth->stencil, sv, rects);
        log.push_back("clearDepthStencil");
    }
    // A depth-tested draw: z test LESS, writes colour, depth and stencil.
    void Draw(const Rect& r, uint32_t c, float z, uint32_t s)
    {
        Begin();
        for (int y = r.top; y < r.bottom; ++y)
            for (int x = r.left; x < r.right; ++x) {
                if (depth) {
                    auto& stored = depth->value[size_t(y) * depth->width + x];
                    if (!(std::bit_cast<float>(stored) > z) && stored != 0xDEADu + depth->id) continue;
                    stored = Bits(z);
                    depth->stencil[size_t(y) * depth->width + x] = s + depth->stencil[size_t(y) * depth->width + x];
                }
                if (color) color->value[size_t(y) * color->width + x] = c;
            }
        log.push_back("draw");
    }
    // Reads end the pass (a barrier or copy outside it).
    std::vector<uint32_t> Read(const Texture& t)
    {
        End();
        auto out = t.value;
        if (t.depth) out.insert(out.end(), t.stencil.begin(), t.stencil.end());
        log.push_back("read" + std::to_string(t.id));
        return out;
    }
};

// ---- the renderer's two policies ---------------------------------------------

struct Renderer {
    bool lazy = false;
    Gpu gpu;
    std::vector<std::vector<uint32_t>> reads;

    void Replay(Texture& t, const std::vector<Op>& ops)
    {
        for (const auto& op : ops) {
            if (t.depth) gpu.ClearDepthStencil(op.aspects & Depth, op.aspects & Stencil, op.depth, op.stencil, op.rects);
            else gpu.ClearColor(op.color[0], op.rects);
        }
    }
    // RunPendingClears: the target's own framebuffer.
    void Run(Texture& t)
    {
        auto ops = t.pending.Take();
        gpu.SetFramebuffer(t.depth ? nullptr : &t, t.depth ? &t : nullptr);
        Replay(t, ops);
    }
    void Clear(Texture& t, Op op)
    {
        if (!lazy) {
            Queue once;
            once.Record(std::move(op), t.width, t.height);
            gpu.SetFramebuffer(t.depth ? nullptr : &t, t.depth ? &t : nullptr);
            Replay(t, once.Ops());
            return;
        }
        if (t.pending.Size() >= Queue::kMaxOps) Run(t);
        t.pending.Record(std::move(op), t.width, t.height);
    }
    // BindTargets.
    void Bind(Texture* c, Texture* d)
    {
        const uint32_t w = c ? c->width : d->width, h = c ? c->height : d->height;
        for (Texture* t : {c, d})
            if (t && !t->pending.Empty() && !t->pending.FitsPass(t->width, t->height, w, h)) Run(*t);
        gpu.SetFramebuffer(c, d);
        for (Texture* t : {c, d})
            if (t && !t->pending.Empty()) Replay(*t, t->pending.Take());
    }
    // Transition to a read layout.
    void Read(Texture& t)
    {
        if (!t.pending.Empty()) Run(t);
        reads.push_back(gpu.Read(t));
    }
    // TransferRegion: reads src, then draws over dst (all of it when the sizes match);
    // a transfer over the whole target drops the clears dst still owes.
    void Transfer(Texture& src, Texture& dst, Rect rect)
    {
        if (!src.pending.Empty()) Run(src);
        gpu.End(); // the read barrier
        uint32_t v = 2166136261u;
        for (uint32_t x : src.value) v = (v ^ x) * 16777619u;
        if (lazy && rect.left <= 0 && rect.top <= 0 && rect.right >= int32_t(dst.width) && rect.bottom >= int32_t(dst.height))
            dst.pending.Drop();
        Bind(&dst, nullptr);
        gpu.Draw(rect, v, 0.0f, 0);
    }
};

// Scripted execution points: whole clears become loadOp CLEAR of the next pass
// that binds the target, a newer whole clear drops the older one, and a read
// before any bind runs the clear in an empty pass of its own.
void ExecutionPoints()
{
    Renderer r; r.lazy = true;
    Texture color(1, false, 8, 4), depth(2, true, 8, 4), other(3, false, 8, 4);
    r.Clear(color, Color(0.25f));
    r.Clear(color, Color(0.5f));                    // drops 0.25
    r.Clear(depth, DepthOp(true, true, 1.0f, 0));
    r.Clear(other, Color(0.75f));
    assert(r.gpu.log.empty() && r.gpu.clearCalls == 0); // nothing recorded yet
    r.Bind(&color, &depth);
    r.gpu.Draw({0, 0, 4, 4}, 9, 0.5f, 1);
    assert((r.gpu.log == std::vector<std::string>{"begin colorCLEAR depthCLEAR stencilCLEAR", "draw"}));
    r.Read(other);                                  // runs other's clear alone, before the read
    assert((r.gpu.log == std::vector<std::string>{"begin colorCLEAR depthCLEAR stencilCLEAR", "draw", "end",
        "begin colorCLEAR", "end", "read3"}));
    assert(r.gpu.passes == 2 && r.gpu.clearCalls == 3);

    // A rectangle clear runs inside the bound pass, after the pass's own start clears.
    Renderer p; p.lazy = true;
    Texture c2(4, false, 8, 4), d2(5, true, 8, 4);
    p.Clear(d2, DepthOp(true, false, 1.0f, 0, {{0, 0, 8, 2}}));
    p.Bind(&c2, &d2);
    p.gpu.Draw({0, 0, 8, 4}, 3, 0.25f, 0);
    assert((p.gpu.log == std::vector<std::string>{"begin", "clearDepthStencil", "draw"}));

    // A depth texture taller than the colour attachment: its whole clear cannot
    // fold into a pass whose render area is the colour extent, so it runs first.
    Renderer m; m.lazy = true;
    Texture c3(6, false, 8, 4), d3(7, true, 8, 6);
    m.Clear(d3, DepthOp(true, true, 1.0f, 2));
    m.Bind(&c3, &d3);
    m.gpu.Draw({0, 0, 8, 4}, 3, 0.25f, 0);
    assert((m.gpu.log == std::vector<std::string>{"begin depthCLEAR stencilCLEAR", "end", "begin", "draw"}));
    std::puts("PASS: execution points (start clear, drop, read alone, in-pass rectangles, extent mismatch)");
}

std::vector<Rect> RandomRects(std::mt19937& rng, uint32_t w, uint32_t h)
{
    std::vector<Rect> rects;
    const int n = int(rng() % 4);
    for (int i = 0; i < n; ++i) {
        const int x0 = int(rng() % w), y0 = int(rng() % h);
        const int x1 = x0 + 1 + int(rng() % (w - x0)), y1 = y0 + 1 + int(rng() % (h - y0));
        rects.push_back({x0, y0, x1, y1});
    }
    if (rects.empty() && rng() % 2) rects.push_back({0, 0, int32_t(w), int32_t(h)});
    return rects;
}

// Same random sequence through both policies: identical reads and pixels.
void Randomized()
{
    uint64_t eagerPasses = 0, lazyPasses = 0, eagerClears = 0, lazyClears = 0;
    for (uint32_t seed = 1; seed <= 3000; ++seed) {
        Renderer runs[2];
        runs[1].lazy = true;
        std::vector<Texture> textures[2];
        for (auto& set : textures) {
            set.emplace_back(0, false, 8, 6); set.emplace_back(1, false, 8, 6); set.emplace_back(2, false, 4, 4);
            set.emplace_back(3, true, 8, 6); set.emplace_back(4, true, 8, 8); set.emplace_back(5, true, 4, 4);
        }
        for (int mode = 0; mode < 2; ++mode) {
            std::mt19937 rng(seed);
            auto& r = runs[mode];
            auto& t = textures[mode];
            const int steps = 4 + int(rng() % 40);
            for (int s = 0; s < steps; ++s) {
                switch (rng() % 7) {
                case 0: case 1: { // colour clear
                    Texture& c = t[rng() % 3];
                    const float v = float(rng() % 4) * 0.25f;
                    r.Clear(c, Color(v, RandomRects(rng, c.width, c.height)));
                    break;
                }
                case 2: { // depth/stencil clear
                    Texture& d = t[3 + rng() % 3];
                    const uint32_t aspects = 1 + rng() % 3;
                    const float z = float(rng() % 3) * 0.5f;
                    r.Clear(d, DepthOp(aspects & 1, aspects & 2, z, rng() % 4, RandomRects(rng, d.width, d.height)));
                    break;
                }
                case 3: case 4: { // draw: colour + depth (depth at least as large), colour only, depth only
                    const uint32_t kind = rng() % 3;
                    Texture* c = kind == 2 ? nullptr : &t[rng() % 3];
                    Texture* d = nullptr;
                    if (kind != 1) {
                        if (!c) d = &t[3 + rng() % 3];
                        else if (c->width == 4) d = &t[3 + rng() % 3];
                        else d = &t[3 + rng() % 2];
                    }
                    const uint32_t w = c ? c->width : d->width, h = c ? c->height : d->height;
                    const int x0 = int(rng() % w), y0 = int(rng() % h);
                    const Rect rect{x0, y0, x0 + 1 + int(rng() % (w - x0)), y0 + 1 + int(rng() % (h - y0))};
                    r.Bind(c, d);
                    r.gpu.Draw(rect, rng() % 100, float(rng() % 4) * 0.25f, rng() % 3);
                    break;
                }
                case 5: { // transfer between the two 8x6 colour views, whole or part of the target
                    const uint32_t src = rng() % 2;
                    const Rect rect = rng() % 2 ? Rect{0, 0, 8, 6} : Rect{0, 0, 1 + int(rng() % 8), 1 + int(rng() % 6)};
                    r.Transfer(t[src], t[1 - src], rect);
                    break;
                }
                default: // read (resolve, transfer source, readback)
                    r.Read(t[rng() % 6]);
                    break;
                }
            }
            for (auto& tex : t) r.Read(tex);
            r.gpu.End();
        }
        if (runs[0].reads != runs[1].reads) {
            std::fprintf(stderr, "FAIL: seed %u reads differ\n", seed);
            std::abort();
        }
        eagerPasses += runs[0].gpu.passes; lazyPasses += runs[1].gpu.passes;
        eagerClears += runs[0].gpu.clearCalls; lazyClears += runs[1].gpu.clearCalls;
    }
    assert(lazyPasses < eagerPasses && lazyClears <= eagerClears);
    std::printf("PASS: 3000 random sequences read identical pixels (passes %llu -> %llu, clear calls %llu -> %llu)\n",
        (unsigned long long)eagerPasses, (unsigned long long)lazyPasses,
        (unsigned long long)eagerClears, (unsigned long long)lazyClears);
}

} // namespace

int main()
{
    QueueRules();
    ExecutionPoints();
    Randomized();
    return 0;
}
