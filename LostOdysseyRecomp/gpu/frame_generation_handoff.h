#pragma once

#include "frame_generation_snapshot.h"
#include <algorithm>
#include <atomic>
#include <limits>
#include <vector>

namespace gpu::frame_generation {

// Exact provenance of the selected resolve, not its guest address or a latest
// snapshot. The source is the promoted composite allocation that fed this
// resolve, not the SR scratch image (whose UI semantics remain unknown).
struct ResolveIdentity {
    uint64_t owner = 0, frame = 0, historyEpoch = 0;
    uint64_t sourceAllocation = 0, sourceGeneration = 0;
    uint64_t resolve = 0, allocation = 0, generation = 0;
    uint32_t width = 0, height = 0, x = 0, y = 0, writeWidth = 0, writeHeight = 0;
    uint32_t guestFormat = 0, pitch = 0;
    bool swapRedBlue = false;
    plume::RenderFormat format = plume::RenderFormat::UNKNOWN;
    frame_plan::FramePlan plan{};
    bool operator==(const ResolveIdentity&) const = default;
    bool Complete() const {
        return owner && sourceAllocation && sourceGeneration && resolve && allocation && generation &&
            width && height && !x && !y && writeWidth == width && writeHeight == height &&
            format != plume::RenderFormat::UNKNOWN && plan.cpuSerial && plan.deviceEpoch &&
            !plan.failed && !plan.requiresReadback && plan.output.width == width && plan.output.height == height &&
            plan.output.drawable.width && plan.output.drawable.height;
    }
};

enum class HandoffCancel : uint8_t {
    None, Superseded, HistoryReset, DisplayChange, AlternatePresent, SubmitFailure, DeviceLost, Shutdown
};

// Source and resolve copies can be submitted in DIFFERENT renderer batches.
// Presentation refers to the normal video graphics queue, not a provider queue.
// No field here reports DLSS-G/FSR-FG completion or HUDless/UI availability.
struct ResolvePacket {
    const ResolveIdentity identity;
    const std::shared_ptr<ProducerSnapshot> producer;
    const TextureLease finalColor;
    uint64_t resolveSerial = 0, presentSerial = 0;
    bool resolveCompleted = false, resolveDiscarded = false, resolveWaitFailed = false;
    bool selected = false, presentCompleted = false, presentWaitFailed = false;
    HandoffCancel canceled = HandoffCancel::None;

    bool Matches(const ResolveIdentity& selectedIdentity) const {
        if (!producer) return false;
        const auto& p = *producer;
        return canceled == HandoffCancel::None && !p.producerDiscarded && !p.lineageCanceled && identity.Complete() &&
            identity == selectedIdentity && identity.owner == p.lineageOwner &&
            identity.frame == p.inputs.renderFrameId && identity.historyEpoch == p.inputs.temporalEpoch &&
            identity.sourceAllocation == p.resolveSourceAllocation &&
            identity.sourceGeneration == p.resolveSourceGeneration && identity.plan == p.inputs.plan &&
            finalColor.Complete() && finalColor.region.width == identity.width && finalColor.region.height == identity.height;
    }
    bool ProducersComplete() const {
        return producer && producer->producerSerial && producer->producerCompleted && !producer->producerWaitFailed &&
            !producer->producerDiscarded && resolveSerial && resolveCompleted && !resolveDiscarded && !resolveWaitFailed;
    }
    bool DiagnosticReady() const {
        return Matches(identity) && selected && ProducersComplete() && presentSerial &&
            presentCompleted && !presentWaitFailed;
    }
    bool Settled() const {
        return producer && (producer->producerDiscarded || (producer->producerSerial && producer->producerCompleted)) &&
            (resolveDiscarded || (resolveSerial && resolveCompleted)) && (!presentSerial || presentCompleted);
    }
    void Cancel(HandoffCancel reason) {
        if (canceled == HandoffCancel::None) canceled = reason;
    }
    // Called only following checked completion on video's single ordered
    // DIRECT queue. Its monotonic serial spans renderer AND present submissions.
    // A failed wait never calls this. A future independent provider must supply
    // its own completion condition instead of borrowing this watermark.
    void CompleteOrderedGraphics(uint64_t deviceEpoch, uint64_t serial) {
        if (!producer || !serial || deviceEpoch != identity.plan.deviceEpoch) return;
        if (producer->producerSerial && producer->producerSerial <= serial) {
            producer->producerCompleted = true;
            producer->producerWaitFailed = false;
        }
        if (resolveSerial && resolveSerial <= serial) { resolveCompleted = true; resolveWaitFailed = false; }
        if (presentSerial && presentSerial <= serial) { presentCompleted = true; presentWaitFailed = false; }
    }
};

struct ResolvedHandoff {
    std::shared_ptr<ResolvePacket> packet;
    ResolveIdentity selected{};
    uint64_t afterPublicFlushFrame = 0, currentHistoryEpoch = 0;
    bool Matches() const {
        return packet && selected.frame != std::numeric_limits<uint64_t>::max() &&
            afterPublicFlushFrame == selected.frame + 1 && currentHistoryEpoch == selected.historyEpoch &&
            packet->Matches(selected) && packet->producer->producerSerial && packet->resolveSerial;
    }
};

// One renderer lifetime; only explicitly requested snapshots enter this pool.
// Weak accounting includes external consumers, so keeping a lease does not
// create an unbounded allocation loophole. GPU slots retain recorded packets
// before the first copy and release them only after checked completion.
class ResolveHandoffPool {
    inline static std::atomic<uint64_t> nextOwner_{1};
    static uint64_t NewOwner() {
        auto value = nextOwner_.load(std::memory_order_relaxed);
        while (value != UINT64_MAX) {
            if (nextOwner_.compare_exchange_weak(value, value + 1, std::memory_order_relaxed)) return value;
        }
        return 0; // exhausted, never reuse an owner identity
    }
    const uint64_t owner_ = NewOwner();
    std::vector<std::weak_ptr<ResolvePacket>> live_;
    std::vector<std::shared_ptr<ResolvePacket>> pending_;
public:
    static constexpr size_t Capacity = 4;
    ResolveHandoffPool() = default;
    ResolveHandoffPool(const ResolveHandoffPool&) = delete;
    ResolveHandoffPool& operator=(const ResolveHandoffPool&) = delete;
    uint64_t Owner() const { return owner_; }
    size_t LiveCount() {
        std::erase_if(live_, [](const auto& p) { return p.expired(); });
        return live_.size();
    }
    void Collect() {
        // A successful video selection owns its packet before selected is set.
        // Never release an unselected submitted packet just due to cancellation.
        std::erase_if(pending_, [](const auto& p) {
            return p->selected || (p->canceled != HandoffCancel::None && p->Settled());
        });
        LiveCount();
    }
    bool HasCapacity() { Collect(); return owner_ && live_.size() < Capacity; }
    bool Retain(const std::shared_ptr<ResolvePacket>& p) {
        if (!p || !HasCapacity() || p->identity.owner != owner_) return false;
        live_.push_back(p);
        try { pending_.push_back(p); }
        catch (...) { live_.pop_back(); throw; }
        return true;
    }
    void CancelAll(HandoffCancel reason) {
        for (auto& weak : live_) if (auto p = weak.lock()) p->Cancel(reason);
        Collect();
    }
    void Advance(uint64_t frame, uint64_t epoch, const frame_plan::FramePlan& plan) {
        for (auto& weak : live_) if (auto p = weak.lock()) {
            if (p->identity.historyEpoch != epoch || p->identity.plan.deviceEpoch != plan.deviceEpoch ||
                p->identity.plan.geometryEpoch != plan.geometryEpoch || p->identity.plan.output != plan.output ||
                p->identity.plan.requestSignature != plan.requestSignature)
                p->Cancel(HandoffCancel::HistoryReset);
            else if (!p->selected && p->identity.frame != frame)
                p->Cancel(HandoffCancel::Superseded);
        }
        Collect();
    }
};

} // namespace gpu::frame_generation
