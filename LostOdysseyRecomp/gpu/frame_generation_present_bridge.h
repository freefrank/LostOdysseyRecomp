#pragma once
#include "frame_generation_handoff.h"

namespace gpu::video {

// Production video boundary shared with the asset-free GPU fixture. Selection
// stages ONLY an exact identity. No texture/input view becomes consumable until
// checked source/resolve/present completion. Ordinary presentation never depends
// on this diagnostic object's return values.
class FgPresentBridge {
    using Packet = frame_generation::ResolvePacket;
    using Cancel = frame_generation::HandoffCancel;
    std::vector<std::shared_ptr<Packet>> pending_;
    std::shared_ptr<Packet> recording_;
public:
    size_t LiveCount() const { return pending_.size(); }
    void Collect() {
        std::erase_if(pending_, [](const auto& p) {
            return p->Settled() && (p->canceled != Cancel::None || p->presentCompleted);
        });
    }
    bool Select(const frame_generation::ResolvedHandoff& handoff, uint64_t deviceEpoch,
        uint32_t sourceWidth, uint32_t sourceHeight, uint32_t outputWidth, uint32_t outputHeight) {
        CancelRecording(); Collect();
        if (!handoff.Matches() || handoff.packet->selected ||
            pending_.size() >= frame_generation::ResolveHandoffPool::Capacity) return false;
        const auto& key = handoff.selected;
        const auto& output = key.plan.output;
        // This first slice supports an exact, unscaled resolved output region.
        // Other crops/transforms retain ordinary present, without guessing which
        // pixels would correspond to the snapshot. No selector is changed.
        if (deviceEpoch != key.plan.deviceEpoch || sourceWidth != key.width || sourceHeight != key.height ||
            output.drawable.width != outputWidth || output.drawable.height != outputHeight ||
            sourceWidth > outputWidth || sourceHeight > outputHeight ||
            (sourceWidth != outputWidth && sourceHeight != outputHeight) ||
            output.x != (outputWidth - sourceWidth) / 2 || output.y != (outputHeight - sourceHeight) / 2)
            return false;
        try { pending_.push_back(handoff.packet); }
        catch (const std::bad_alloc&) { return false; }
        recording_ = handoff.packet;
        recording_->selected = true;
        return true;
    }
    void Submitted(uint64_t deviceEpoch, uint64_t serial) {
        if (!recording_) return;
        auto p = std::move(recording_);
        if (deviceEpoch != p->identity.plan.deviceEpoch || !serial || serial <= p->resolveSerial ||
            serial <= p->producer->producerSerial) {
            p->Cancel(Cancel::SubmitFailure); return;
        }
        p->presentSerial = serial;
    }
    void CancelRecording() {
        if (recording_) { recording_->Cancel(Cancel::SubmitFailure); recording_.reset(); }
        Collect();
    }
    void WaitFailed() {
        for (auto& p : pending_) if (p->presentSerial && !p->presentCompleted) p->presentWaitFailed = true;
    }
    size_t Completed(uint64_t deviceEpoch, uint64_t serial) {
        size_t ready = 0;
        for (auto& p : pending_) {
            const bool wasReady = p->DiagnosticReady();
            // This observer is a real present fence on the same ordered queue.
            // It cannot repair any future provider's completion state.
            if (p->presentSerial && p->presentSerial <= serial)
                p->CompleteOrderedGraphics(deviceEpoch, serial);
            ready += !wasReady && p->DiagnosticReady();
        }
        Collect(); return ready;
    }
    void CancelAll(Cancel reason) {
        recording_.reset();
        for (auto& p : pending_) p->Cancel(reason);
        Collect();
    }
};

} // namespace gpu::video
