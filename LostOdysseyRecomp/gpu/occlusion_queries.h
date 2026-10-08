#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

// Guest occlusion queries (PM4 EVENT_WRITE_ZPD) measured with host GPU queries.
//
// D3D on the console issues one ZPD event for a BEGIN record, the query's draws,
// then one event for the END record, and reads END - BEGIN of the ZPass counts.
// Its GetData (sub_823CF3F0) waits until the GPU fence has passed the query and
// END ZPass A and B no longer both hold the sentinel it stamped. Lost Odyssey
// reads its queries in the same frame, right after issuing them, so an exact
// result costs a GPU sync in the middle of every frame (Mode::Strict). Mode::Fast
// writes the last measured count of the same record instead, as Xenia's default
// "fast" mode does, and never culls. The default (Policy::Adaptive) runs Fast and
// switches to Strict while most queried objects read zero, where culling saves
// more than the sync costs (AdaptiveMode).
//
// The game hands out its query objects from a pool in allocation order every
// frame (sub_823CCCE8), so a record belongs to a different object whenever the
// set of queried objects changes; the sun flare (sub_823D5430) is allocated
// after every primitive query of the frame. Fast answers therefore follow the
// owner the allocation hook reports for a record (its call site and per-frame
// ordinal at that site), not the record itself.
//
// This file holds the bookkeeping only; the renderer owns the host queries and
// writes the records this class returns.
namespace gpu::occlusion
{
    // xe_gpu_depth_sample_counts: eight dwords, each count split into A and B.
    struct Record
    {
        uint32_t totalA = 0, totalB = 0;
        uint32_t zfailA = 0, zfailB = 0;
        uint32_t zpassA = 0, zpassB = 0;
        uint32_t stencilFailA = 0, stencilFailB = 0;
    };
    static_assert(sizeof(Record) == 32);

    // The END record sits at a 64-byte slot and the BEGIN record 32 bytes later.
    inline constexpr uint32_t kSlotBytes = 64;
    inline constexpr uint32_t kBeginOffset = 32;
    inline constexpr bool IsBeginRecord(uint32_t address) { return (address & (kSlotBytes - 1)) == kBeginOffset; }
    inline constexpr uint32_t SlotOf(uint32_t address) { return address & ~(kSlotBytes - 1); }

    // Samples reported for a query that was not measured: clearly visible.
    inline constexpr uint32_t kUnmeasuredSamples = 0x10000;

    enum class Mode
    {
        // The END record is written at once with the last measured count of
        // the same record, or 1 when that was zero or is not known yet. The
        // guest never waits and never culls on a stale count, while counts
        // used as magnitudes, such as lens flare visibility, follow the real
        // ones a frame or two late.
        Fast,
        // The END record is written once the host GPU has counted it. The
        // guest waits for exact results and culls with them, at the cost of a
        // GPU sync in the middle of every frame that issues queries.
        Strict,
    };

    // Which Mode answers the queries: one of them throughout, or AdaptiveMode's
    // choice frame by frame.
    enum class Policy { Adaptive, Fast, Strict };

    // LO_ZPD_MODE: unset or "host" selects Policy::Adaptive, "fast" and
    // "strict" pin one mode. The command processor's fake modes (grow, xenia,
    // begin0, none) turn host queries off.
    inline std::optional<Policy> HostPolicy(const char* mode)
    {
        if (!mode || !*mode || std::strcmp(mode, "host") == 0) return Policy::Adaptive;
        if (std::strcmp(mode, "fast") == 0) return Policy::Fast;
        if (std::strcmp(mode, "strict") == 0) return Policy::Strict;
        return std::nullopt;
    }

    // Identity of the guest object that issued a query, stable across frames:
    // `caller` is the pool allocation's return address and `ordinal` counts the
    // allocations from that call site since the pool was reset. Without an
    // owner a query is identified by its record slot.
    inline constexpr uint64_t OwnerKey(uint32_t caller, uint32_t ordinal) { return (uint64_t(caller) << 32) | ordinal; }

    // One host query around one guest draw. `scale` converts host samples to
    // guest samples (resolution scale and guest MSAA).
    struct Part
    {
        uint64_t batch = 0;
        uint32_t index = 0;
        double scale = 1.0;
    };

    // Rounds scaled host samples to a guest count. Any passing host sample
    // keeps the query visible; the cap only keeps END - BEGIN well defined.
    inline uint32_t GuestSamples(double samples)
    {
        if (!(samples > 0.0)) return 0;
        if (samples < 1.0) return 1;
        return samples >= double(0x40000000u) ? 0x40000000u : uint32_t(std::llround(samples));
    }

    // Guest samples per host sample for a target `hostWidth` x `hostHeight`
    // that stands for `guestWidth` x `guestHeight` guest pixels with
    // `guestMsaa` (RB_SURFACE_INFO MSAA field: 0, 1, 2 for 1, 2, 4 samples).
    inline double SampleScale(uint32_t guestWidth, uint32_t guestHeight, uint32_t hostWidth, uint32_t hostHeight, uint32_t guestMsaa)
    {
        if (!guestWidth || !guestHeight || !hostWidth || !hostHeight) return 1.0;
        const double samples = double(1u << (guestMsaa > 2 ? 2 : guestMsaa));
        return double(guestWidth) * double(guestHeight) * samples / (double(hostWidth) * double(hostHeight));
    }

    struct Write
    {
        uint32_t address = 0;
        Record record;
        uint32_t samples = 0;  // END - BEGIN this write completes
        bool measured = false; // counted by host queries
        bool apply = true;     // false: written at its END event already; reported for statistics
    };

    class Tracker
    {
    public:
        explicit Tracker(Mode mode = Mode::Fast) : mode_(mode) {}
        // Applies from the next END event; queries already waiting for host
        // results keep waiting until their batch completes.
        void SetMode(Mode mode) { mode_ = mode; }
        Mode CurrentMode() const { return mode_; }

        // BEGIN event: the record to write now. `owner` (OwnerKey) names the
        // guest object whose earlier counts answer this query in Mode::Fast;
        // 0 falls back to the record slot.
        Write Begin(uint32_t address, uint64_t owner = 0)
        {
            const uint32_t slot = SlotOf(address);
            active_ = Query{slot, ++generations_[slot], counter_};
            active_->owner = owner ? owner : slot;
            return {address, Counts(counter_)};
        }

        bool Active() const { return active_.has_value(); }

        // A guest draw inside the active query, measured or not.
        void DrawSeen() { if (active_) ++active_->drawsSeen; }
        void DrawMeasured(const Part& part) { if (active_) active_->parts.push_back(part); }

        // END event. Returns the record to write now, or nothing when it waits
        // for host results (Mode::Strict).
        std::optional<Write> End(uint32_t address)
        {
            const uint32_t slot = SlotOf(address);
            if (!active_ || active_->slot != slot) {
                // An END without its BEGIN cannot be measured.
                if (active_ && active_->slot != slot) active_.reset();
                return Unmeasured(address, counter_);
            }
            Query query = std::move(*active_);
            active_.reset();
            if (query.parts.empty() || query.parts.size() != query.drawsSeen)
                return Unmeasured(address, query.begin);
            query.end = address;
            std::optional<Write> write;
            if (mode_ == Mode::Fast) {
                const auto known = last_.find(query.owner);
                const uint32_t samples = known != last_.end() && known->second ? known->second : 1;
                write = Write{address, Counts(query.begin + samples), samples, false};
                counter_ = query.begin + samples;
                query.written = true;
            }
            else ++unwritten_;
            pending_.push_back(std::move(query));
            return write;
        }

        // A submitted batch completed; `results` holds its host query results
        // (UINT64_MAX when unavailable). Returns every query it finished.
        std::vector<Write> Complete(uint64_t batch, std::span<const uint64_t> results)
        {
            for (auto& query : pending_)
                for (auto& part : query.parts)
                    if (part.batch == batch && !query.failed) {
                        if (part.index >= results.size() || results[part.index] == ~0ull) query.failed = true;
                        else query.samples += double(results[part.index]) * part.scale;
                        part.batch = 0;
                    }
            return Collect();
        }

        // A batch that will not run: its queries read as visible.
        std::vector<Write> Abandon(uint64_t batch)
        {
            for (auto& query : pending_)
                for (auto& part : query.parts)
                    if (part.batch == batch) {
                        query.failed = true;
                        part.batch = 0;
                    }
            return Collect();
        }

        // A guest may be waiting on an END record that is not written yet.
        bool HasAwaited() const { return unwritten_ != 0; }

        // An unwritten END record waits on host queries in `batch`.
        bool Waiting(uint64_t batch) const
        {
            for (const auto& query : pending_)
                if (!query.written)
                    for (const auto& part : query.parts)
                        if (part.batch == batch) return true;
            return false;
        }

        bool HasPending() const { return !pending_.empty(); }

        // Device loss or shutdown: every unwritten END record reads as visible.
        std::vector<Write> AbandonAll()
        {
            for (auto& query : pending_) {
                query.failed = true;
                for (auto& part : query.parts) part.batch = 0;
            }
            active_.reset();
            return Collect();
        }

    private:
        struct Query
        {
            uint32_t slot = 0;
            uint32_t generation = 0;
            uint32_t begin = 0;
            uint32_t end = 0;
            uint32_t drawsSeen = 0;
            std::vector<Part> parts;
            double samples = 0.0; // guest samples measured so far
            bool failed = false;
            bool written = false; // END record written at its event (Mode::Fast)
            uint64_t owner = 0;   // key of the Fast answers (OwnerKey or slot)
        };

        static Record Counts(uint32_t zpass)
        {
            Record record;
            record.totalA = record.zpassA = zpass;
            return record;
        }

        Write Unmeasured(uint32_t address, uint32_t begin)
        {
            const uint32_t end = begin + kUnmeasuredSamples;
            counter_ = end;
            return {address, Counts(end), kUnmeasuredSamples, false};
        }

        // Finished queries leave in issue order. Each measured count becomes
        // the next Fast answer for its owner. An unwritten record whose slot
        // the guest has reissued since is dropped: it belongs to the new query.
        std::vector<Write> Collect()
        {
            std::vector<Write> writes;
            for (auto it = pending_.begin(); it != pending_.end();) {
                bool done = it->failed;
                if (!done) {
                    done = true;
                    for (const auto& part : it->parts) done &= part.batch == 0;
                }
                if (!done) { ++it; continue; }
                const uint32_t samples = it->failed ? kUnmeasuredSamples : GuestSamples(it->samples);
                if (!it->failed) last_[it->owner] = samples;
                const bool current = generations_[it->slot] == it->generation;
                if (it->written || current) {
                    writes.push_back({it->end, Counts(it->begin + samples), samples, !it->failed, !it->written});
                    if (!it->written && it->begin + samples - counter_ < 0x80000000u) counter_ = it->begin + samples;
                }
                if (!it->written) --unwritten_;
                it = pending_.erase(it);
            }
            return writes;
        }

        Mode mode_;
        uint32_t counter_ = 0;
        size_t unwritten_ = 0; // pending queries whose END record waits for host results
        std::optional<Query> active_;
        std::deque<Query> pending_;
        std::unordered_map<uint32_t, uint32_t> generations_;
        std::unordered_map<uint64_t, uint32_t> last_; // last measured count per owner
    };

    // Policy::Adaptive: the Mode for the next frame, from the zero share of the
    // queries that completed over a window of frames.
    //
    // Strict hides what the console hides, but each frame waits for the host GPU
    // to count the queries the guest reads mid-frame. That pays off only when
    // most queried objects are hidden: a room seen through a doorway, where
    // about 80% of a thousand queries read zero, drops about 60% of the draws
    // and renders faster, while a street with 15% zeros renders slower. With
    // Strict the guest tests hidden objects in batches, so the zero share falls
    // (about 80% in Fast reads as 35% in Strict in the same view); Strict
    // therefore leaves at a lower share than Fast enters at. A switch takes two
    // windows in a row, and frames without completed queries (menus, loading
    // screens, fades) do not count, so a scene change does not switch on its
    // transition frames. After leaving, Fast holds for a cooldown that doubles
    // each time Strict lasted only briefly, so a view on the edge settles in
    // Fast instead of switching back and forth.
    class AdaptiveMode
    {
    public:
        static constexpr uint32_t kWindowFrames = 32;
        static constexpr uint32_t kSwitchWindows = 2;
        static constexpr double kEnterZeroShare = 0.5;
        static constexpr uint32_t kEnterZerosPerFrame = 256;
        static constexpr double kLeaveZeroShare = 0.2;
        static constexpr uint32_t kCooldownFrames = 240;
        static constexpr uint32_t kMaxCooldownFrames = kCooldownFrames * 16;
        static constexpr uint32_t kBriefStrictFrames = kWindowFrames * kSwitchWindows * 2;

        Mode Current() const { return mode_; }
        // The last evaluated window, for logs.
        double ZeroShare() const { return share_; }
        double ZerosPerFrame() const { return zerosPerFrame_; }
        uint32_t Cooldown() const { return cooldown_; }

        // Queries that completed with host results, `zero` of them reading 0.
        void Add(uint32_t measured, uint32_t zero)
        {
            measured_ += measured;
            zero_ += zero;
        }

        // Once per guest frame. Returns true when the mode changed.
        bool EndFrame()
        {
            if (mode_ == Mode::Strict) ++strictFrames_;
            if (cooldown_) {
                --cooldown_;
                measured_ = zero_ = counted_ = 0;
                return false;
            }
            if (measured_ == counted_) return false; // no queries completed this frame
            counted_ = measured_;
            if (++frames_ < kWindowFrames) return false;
            share_ = double(zero_) / double(measured_);
            zerosPerFrame_ = double(zero_) / double(frames_);
            frames_ = 0;
            measured_ = zero_ = counted_ = 0;
            const bool other = mode_ == Mode::Fast
                ? share_ >= kEnterZeroShare && zerosPerFrame_ >= kEnterZerosPerFrame
                : share_ < kLeaveZeroShare;
            votes_ = other ? votes_ + 1 : 0;
            if (votes_ < kSwitchWindows) return false;
            votes_ = 0;
            if (mode_ == Mode::Fast) {
                mode_ = Mode::Strict;
                strictFrames_ = 0;
                return true;
            }
            mode_ = Mode::Fast;
            if (strictFrames_ >= kBriefStrictFrames) hold_ = kCooldownFrames;
            cooldown_ = hold_;
            hold_ = std::min(hold_ * 2, kMaxCooldownFrames);
            return true;
        }

    private:
        Mode mode_ = Mode::Fast;
        uint64_t measured_ = 0, zero_ = 0, counted_ = 0;
        uint32_t frames_ = 0, votes_ = 0, strictFrames_ = 0;
        uint32_t cooldown_ = 0, hold_ = kCooldownFrames;
        double share_ = 0.0, zerosPerFrame_ = 0.0;
    };
}
