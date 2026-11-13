#pragma once
#include <algorithm>
#include <bit>
#include <cstdint>
#include <vector>

// EDRAM clears owed by one render target. The renderer records a clear here
// instead of recording GPU commands at once, and runs the queue right before
// the texture is next used: at the start of the pass that binds it, or on its
// own framebuffer before a read or an out-of-pass write. A newer whole-target
// clear drops the older ones it covers. Values and per-texture order never
// change, so the result is the same as clearing at once.
namespace gpu::pending_clear {

struct Rect { int32_t left = 0, top = 0, right = 0, bottom = 0; };

inline bool Contains(const Rect& outer, const Rect& inner)
{
    return outer.left <= inner.left && outer.top <= inner.top &&
        outer.right >= inner.right && outer.bottom >= inner.bottom;
}

enum Aspect : uint8_t { Color = 1, Depth = 2, Stencil = 4 };

struct Op {
    uint8_t aspects = 0;
    float color[4]{};
    float depth = 0.0f;
    uint32_t stencil = 0;
    std::vector<Rect> rects; // empty: the whole texture

    bool Whole() const { return rects.empty(); }
    bool SameValue(const Op& other) const
    {
        if (aspects != other.aspects) return false;
        if (aspects & Color)
            for (int i = 0; i < 4; ++i)
                if (std::bit_cast<uint32_t>(color[i]) != std::bit_cast<uint32_t>(other.color[i])) return false;
        if ((aspects & Depth) && std::bit_cast<uint32_t>(depth) != std::bit_cast<uint32_t>(other.depth)) return false;
        if ((aspects & Stencil) && stencil != other.stencil) return false;
        return true;
    }
};

inline Op ColorOp(const float rgba[4], std::vector<Rect> rects = {})
{
    Op op;
    op.aspects = Color;
    for (int i = 0; i < 4; ++i) op.color[i] = rgba[i];
    op.rects = std::move(rects);
    return op;
}

inline Op DepthOp(bool depth, bool stencil, float depthValue, uint32_t stencilValue, std::vector<Rect> rects = {})
{
    Op op;
    op.aspects = uint8_t((depth ? Depth : 0) | (stencil ? Stencil : 0));
    op.depth = depthValue;
    op.stencil = stencilValue;
    op.rects = std::move(rects);
    return op;
}

class Queue {
public:
    // More than this runs the queue first (the renderer's choice); in practice
    // equal values merge and whole clears drop older ones.
    static constexpr size_t kMaxOps = 8;

    bool Empty() const { return ops_.empty(); }
    size_t Size() const { return ops_.size(); }
    const std::vector<Op>& Ops() const { return ops_; }

    // width/height: the texture extent, to recognise a rectangle that covers it.
    void Record(Op op, uint32_t width, uint32_t height)
    {
        if (!op.aspects) return;
        if (op.rects.size() == 1 && op.rects[0].left <= 0 && op.rects[0].top <= 0 &&
            op.rects[0].right >= int32_t(width) && op.rects[0].bottom >= int32_t(height))
            op.rects.clear();
        if (op.Whole() || op.rects.size() == 1) {
            // A newer rectangular clear also kills older clears wholly inside
            // it, independently for color/depth/stencil. Do not approximate the
            // union of rectangles or split a partially covered aspect.
            for (auto& old : ops_) {
                const bool covered = op.Whole() || (!old.Whole() && old.rects.size() <= kCoverageRects &&
                    std::all_of(old.rects.begin(), old.rects.end(),
                        [&](const Rect& r) { return Contains(op.rects[0], r); }));
                if (covered) old.aspects &= uint8_t(~op.aspects);
            }
            std::erase_if(ops_, [](const Op& old) { return old.aspects == 0; });
        }
        if (!ops_.empty() && ops_.back().SameValue(op)) {
            Op& last = ops_.back();
            // A whole last clear already covers these pixels with this value.
            if (!last.Whole()) {
                if (op.Whole()) last.rects.clear();
                else if (op.rects.size() == 1 && last.rects.size() <= kCoverageRects) {
                    const Rect& incoming = op.rects[0];
                    // Equal-value rectangles that are already covered need no
                    // replay. Replacing contained rectangles also avoids
                    // repeatedly clearing their overlap on the GPU.
                    if (!std::any_of(last.rects.begin(), last.rects.end(),
                        [&](const Rect& r) { return Contains(r, incoming); })) {
                        std::erase_if(last.rects, [&](const Rect& r) { return Contains(incoming, r); });
                        last.rects.push_back(incoming);
                    }
                }
                else last.rects.insert(last.rects.end(), op.rects.begin(), op.rects.end());
            }
            Keep(op.rects);
            return;
        }
        ops_.push_back(std::move(op));
    }

    std::vector<Op> Take()
    {
        std::vector<Op> ops;
        ops.swap(ops_);
        return ops;
    }

    // Hands back the ops of a Take() after they ran. Their storage serves
    // later clears of this texture: SpareRects() and the op list itself.
    void Recycle(std::vector<Op>&& ops)
    {
        for (auto& op : ops) Keep(op.rects);
        if (ops_.empty() && ops_.capacity() < ops.capacity()) {
            ops.clear();
            ops_.swap(ops);
        }
    }

    // Empty rectangle storage for a clear about to be recorded here.
    std::vector<Rect> SpareRects()
    {
        if (spareRects_.empty()) return {};
        std::vector<Rect> rects = std::move(spareRects_.back());
        spareRects_.pop_back();
        return rects;
    }

    void Drop() { ops_.clear(); }

    // Whether the queue can run inside a pass whose render area is the
    // framebuffer extent (areaWidth x areaHeight). A whole-texture clear there
    // covers only the render area, and clear rectangles must lie inside it.
    bool FitsPass(uint32_t width, uint32_t height, uint32_t areaWidth, uint32_t areaHeight) const
    {
        for (const auto& op : ops_) {
            if (op.Whole()) {
                if (width != areaWidth || height != areaHeight) return false;
                continue;
            }
            for (const auto& r : op.rects)
                if (r.left < 0 || r.top < 0 || r.right > int32_t(areaWidth) || r.bottom > int32_t(areaHeight))
                    return false;
        }
        return true;
    }

private:
    // Bound the added CPU work even when equal disjoint clears accumulate.
    static constexpr size_t kCoverageRects = 32;

    void Keep(std::vector<Rect>& rects)
    {
        if (rects.capacity() && spareRects_.size() < kMaxOps) {
            rects.clear();
            spareRects_.push_back(std::move(rects));
        }
    }

    std::vector<Op> ops_;
    std::vector<std::vector<Rect>> spareRects_;
};

} // namespace gpu::pending_clear
