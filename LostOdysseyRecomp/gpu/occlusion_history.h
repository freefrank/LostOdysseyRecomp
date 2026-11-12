#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <ankerl/unordered_dense.h>
#include <utility>
#include <vector>

#include "occlusion_queries.h"

// Answers for the game's deferred occlusion reads (Policy::History).
//
// With the UE3 flag 0x833180B4 set, the game keeps each object's query in its
// view state (a map keyed by the primitive component, or by the projected
// shadow's light and primitive) and reads it at the start of the next frame:
// sub_823C0910 calls sub_823CE960 before the depth prepass and before it issues
// the frame's own queries. The host has not counted that query yet at this
// point: the command processor runs about a frame behind the guest and the GPU
// another frame behind it, so the newest counted result is the one issued three
// frames earlier, and waiting for the asked one costs what the same-frame read
// costs today. Each read is answered instead with the newest count measured for
// the same object, and a count of 0 culls the object as a fresh one would.
//
// A read answers visible when that count cannot stand for the current view:
//  - nothing was measured for the object yet, or the newest count is older
//    than kMaxAge frames;
//  - the camera cut (Cut) after the count was issued;
//  - the object was not read in the previous frame: it was out of view or not
//    queried, so its count predates the gap;
//  - with hysteresis K, it was measured visible within its K newest issues.
// A read whose count the host already wrote (GetData has it) takes that count.
// Reads in other scene renders (scene captures), and reads of a query object a
// scene capture issued last, answer visible: the game's query pool is shared.
namespace gpu::occlusion
{
    // The view matrix moved too far between two frames to reuse counts: an
    // axis turned by more than about 15 degrees or the origin moved by more
    // than kCutMove world units (walking moves under 100 a frame, a teleport
    // or a cut thousands). `prev` and `cur` are row-major 4x4 matrices.
    inline constexpr float kCutCos = 0.966f;
    inline constexpr float kCutMove = 300.0f;
    inline bool ViewCut(const float* prev, const float* cur)
    {
        for (int r = 0; r < 3; ++r) {
            const float dot = prev[r * 4] * cur[r * 4] + prev[r * 4 + 1] * cur[r * 4 + 1] + prev[r * 4 + 2] * cur[r * 4 + 2];
            if (!(dot >= kCutCos)) return true;
        }
        const float dx = cur[12] - prev[12], dy = cur[13] - prev[13], dz = cur[14] - prev[14];
        return !(dx * dx + dy * dy + dz * dz <= kCutMove * kCutMove);
    }

    class HistoryAnswers
    {
    public:
        enum class Kind : uint8_t { Primitive, Shadow };
        // Any non-zero count keeps an object drawn; the game uses no magnitude.
        static constexpr uint32_t kVisible = 1;
        // Newest count older than this (frames between its issue and the frame
        // it stands for) answers visible. Three frames covers the host pipeline.
        static constexpr uint32_t kMaxAge = 3;

        struct Stats
        {
            uint64_t reads = 0, ready = 0, measured = 0, zero = 0;
            uint64_t unknown = 0, stale = 0, cut = 0, gap = 0, hysteresis = 0, capture = 0;
        };

        explicit HistoryAnswers(uint32_t hysteresis = 0) : hysteresis_(hysteresis) {}

        // Start of a guest frame (the main scene render).
        void BeginFrame()
        {
            ++frame_;
            if (frame_ % 600 == 0)
                for (auto& map : entries_)
                    for (auto it = map.begin(); it != map.end();)
                        it = it->second.lastRead == kNone || frame_ - it->second.lastRead > 600 ? map.erase(it) : std::next(it);
        }
        uint32_t Frame() const { return frame_; }

        // The camera cut this frame: counts issued before it no longer apply.
        void Cut() { cutFrame_ = frame_; }

        // The pool handed out `query` in this frame from call site `caller`;
        // `main` is false in another scene render. Returns its owner.
        uint64_t Issue(uint32_t query, uint32_t caller, bool main)
        {
            const uint64_t owner = HistoryOwner(caller, serial_++ & ~kHistoryOwnerBit);
            queries_[query] = {owner, frame_, main};
            return owner;
        }

        // A deferred read of `query` for the object `key`. `ready`: GetData
        // returned the exact count `samples`. Returns the count for the game.
        uint32_t Read(Kind kind, uint64_t key, uint32_t query, bool main, bool ready, uint32_t samples)
        {
            ++stats_.reads;
            Entry& entry = entries_[size_t(kind)][key];
            const bool continuous = entry.lastRead != kNone && entry.lastRead + 1 == frame_;
            entry.lastRead = frame_;
            const auto issued = queries_.find(query);
            if (!main || issued == queries_.end() || !issued->second.main) {
                ++stats_.capture;
                return kVisible;
            }
            const uint32_t issue = issued->second.frame;
            if (ready) {
                ++stats_.ready;
                Store(entry, issue, samples);
                if (issue < cutFrame_) { ++stats_.cut; return kVisible; }
                if (!samples) ++stats_.zero;
                return samples;
            }
            Pending& pending = PendingFor(issued->second.owner);
            if (pending.owner != issued->second.owner) {
                Release(pending);
                pending.owner = issued->second.owner;
            }
            pending.frame = issue;
            AddKey(pending, kind, key);
            if (entry.issue == kNone) { ++stats_.unknown; return kVisible; }
            if (frame_ - entry.issue > kMaxAge + 1) { ++stats_.stale; return kVisible; }
            if (entry.issue < cutFrame_) { ++stats_.cut; return kVisible; }
            if (!continuous) { ++stats_.gap; return kVisible; }
            if (!entry.samples && hysteresis_ && entry.lastVisible != kNone && entry.issue - entry.lastVisible < hysteresis_) {
                ++stats_.hysteresis;
                return kVisible;
            }
            ++stats_.measured;
            if (!entry.samples) ++stats_.zero;
            return entry.samples;
        }

        // The host measured `samples` for the query of `owner`.
        void Completed(uint64_t owner, uint32_t samples)
        {
            Pending& pending = PendingFor(owner);
            if (pending.owner != owner) return;
            if (frame_ - pending.frame <= kPendingFrames) {
                for (uint32_t i = 0; i < pending.count && i < pending.keys.size(); ++i)
                    Store(entries_[size_t(pending.keys[i].first)][pending.keys[i].second], pending.frame, samples);
                for (uint32_t i = pending.more; i != kNone; i = overflow_[i].next)
                    Store(entries_[size_t(overflow_[i].kind)][overflow_[i].key], pending.frame, samples);
            }
            Release(pending);
        }

        const Stats& GetStats() const { return stats_; }
        // Keys after a read's first two, held for all pending reads together (the pool never shrinks).
        size_t OverflowCapacity() const { return overflow_.size(); }

    private:
        static constexpr uint32_t kNone = ~0u;
        static constexpr uint32_t kPendingFrames = 16; // a count later than this never arrives
        struct Entry
        {
            uint32_t samples = 0;
            uint32_t issue = kNone;       // issue frame of `samples`
            uint32_t lastVisible = kNone; // newest issue frame measured visible
            uint32_t lastRead = kNone;    // frame of the last read
        };
        struct Issued { uint64_t owner = 0; uint32_t frame = 0; bool main = false; };
        // Reads waiting for their query's count, in a ring by the issue serial in the owner (Issue): an owner is
        // never 0, and one kPendingSlots issues later takes the slot over. The map and vector this replaces allocated
        // and freed for about 700 reads a frame in the busy street, under newlib's malloc lock, on the game's
        // rendering thread.
        //
        // A query read for more than two objects keeps the others in overflow_, one pool for every slot, as a list
        // (`more`, newest first) that goes back to the pool's free list when the count arrives or the slot is taken
        // over. The pool holds the keys pending now. A vector per slot instead (until 2026-11-11) kept the largest
        // capacity it ever reached: in the first battle that grew the heap by about 2.8 KB/s at the start, 1.3 KB/s
        // after 25 minutes, towards 32,768 slots times the biggest overflow each had seen (the heap census, LO_HEAP_CENSUS).
        static constexpr uint32_t kPendingSlots = 1u << 15;
        struct Pending
        {
            uint64_t owner = 0;
            uint32_t frame = 0, count = 0;
            std::array<std::pair<Kind, uint64_t>, 2> keys{};
            uint32_t more = kNone; // overflow_ index of the newest key after the first two
        };
        struct Overflow
        {
            uint64_t key = 0;
            uint32_t next = kNone; // the pending read's next key, or the free list's next entry
            Kind kind = Kind::Primitive;
        };
        Pending& PendingFor(uint64_t owner) { return pending_[uint32_t(owner) & (kPendingSlots - 1)]; }

        void AddKey(Pending& pending, Kind kind, uint64_t key)
        {
            if (pending.count < pending.keys.size()) {
                pending.keys[pending.count++] = {kind, key};
                return;
            }
            uint32_t i = overflowFree_;
            if (i != kNone) overflowFree_ = overflow_[i].next;
            else {
                i = uint32_t(overflow_.size());
                overflow_.emplace_back();
            }
            overflow_[i] = {key, pending.more, kind};
            pending.more = i;
            ++pending.count;
        }

        // The slot's keys back to the pool; the slot is free.
        void Release(Pending& pending)
        {
            for (uint32_t i = pending.more; i != kNone;) {
                const uint32_t next = overflow_[i].next;
                overflow_[i].next = overflowFree_;
                overflowFree_ = i;
                i = next;
            }
            pending.owner = 0;
            pending.count = 0;
            pending.more = kNone;
        }

        static void Store(Entry& entry, uint32_t issue, uint32_t samples)
        {
            if (samples && (entry.lastVisible == kNone || issue > entry.lastVisible)) entry.lastVisible = issue;
            if (entry.issue == kNone || issue >= entry.issue) {
                entry.samples = samples;
                entry.issue = issue;
            }
        }

        uint32_t hysteresis_ = 0;
        uint32_t frame_ = 0;
        uint32_t cutFrame_ = 0;
        uint32_t serial_ = 1;
        // Flat tables: the lookups run for every deferred read and issue on the game's rendering thread.
        ankerl::unordered_dense::map<uint64_t, Entry> entries_[2];
        ankerl::unordered_dense::map<uint32_t, Issued> queries_;
        std::vector<Pending> pending_ = std::vector<Pending>(kPendingSlots);
        std::vector<Overflow> overflow_;
        uint32_t overflowFree_ = kNone;
        Stats stats_;
    };
}
