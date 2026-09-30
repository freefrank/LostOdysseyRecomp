#pragma once

#include "temporal_frame_inputs.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <utility>

namespace gpu::frame_generation {

// "Unavailable" is intentionally different from an explicitly UI-free frame.
// A provider must never guess that a composited game frame is HUD-less.
enum class UiSeparation : uint8_t { Unavailable = 0, NoUi = 1, ColorAndAlpha = 2 };

// Pins the producer-owned allocation while preserving the exact tagged region.
// The lifetime token may own the image directly or own a producer generation
// that retains it; a raw RenderTexture pointer by itself is never a lease.
struct TextureLease {
    temporal::TextureRegion region{};
    std::shared_ptr<void> lifetime{};
    uint64_t producerSerial = 0;

    bool Complete() const { return region.Complete() && bool(lifetime); }
    bool Matches(const temporal::TextureRegion& other) const {
        return Complete() && other.Complete() &&
            region.texture == other.texture &&
            region.allocation.width == other.allocation.width &&
            region.allocation.height == other.allocation.height &&
            region.x == other.x && region.y == other.y &&
            region.width == other.width && region.height == other.height;
    }
};

inline bool SameRegionGeometry(const TextureLease& a, const TextureLease& b) {
    return a.Complete() && b.Complete() &&
        a.region.x == b.region.x && a.region.y == b.region.y &&
        a.region.width == b.region.width && a.region.height == b.region.height;
}

inline bool FullAllocation(const TextureLease& value) {
    return value.Complete() && value.region.x == 0 && value.region.y == 0 &&
        value.region.width == value.region.allocation.width &&
        value.region.height == value.region.allocation.height;
}

// Provider-neutral real-frame packet. Generated frames may consume this packet,
// but they never advance renderFrameId/temporalEpoch or mutate TemporalFrameInputs.
struct PresentFrame {
    temporal::TemporalFrameInputs temporal{};
    TextureLease sourceColor{};
    TextureLease depth{};
    TextureLease motion{};
    TextureLease motionInvalidity{};
    TextureLease hudlessColor{};
    TextureLease uiColorAndAlpha{};
    TextureLease presentationColor{};
    UiSeparation ui = UiSeparation::Unavailable;

    bool ProviderReady() const {
        if (temporal.plan.frameGeneration == upscaling::FrameGeneration::Off ||
            !temporal.CompleteForConsumer() ||
            !sourceColor.Matches(temporal.color) ||
            !depth.Matches(temporal.depth))
            return false;

        if (upscaling::RequiresMotionDepth(temporal.plan.consumer, temporal.plan.frameGeneration) &&
            (!motion.Matches(temporal.motion) ||
             !motionInvalidity.Matches(temporal.motionInvalidity)))
            return false;

        if (!FullAllocation(hudlessColor) || !FullAllocation(presentationColor) ||
            !SameRegionGeometry(hudlessColor, presentationColor))
            return false;

        if (ui == UiSeparation::NoUi)
            return !uiColorAndAlpha.region.texture && !uiColorAndAlpha.lifetime;
        if (ui != UiSeparation::ColorAndAlpha || !FullAllocation(uiColorAndAlpha))
            return false;
        return SameRegionGeometry(hudlessColor, uiColorAndAlpha);
    }
};

struct PresentLease {
    uint64_t id = 0;
    PresentFrame frame{};
    // Zero means provider submission has not happened. Once nonzero, resources
    // remain pinned until the provider reports this serial complete.
    uint64_t completionSerial = 0;
};

// Presentation-thread owner. Publishing/cancelling never waits on the GPU.
// Resize/mode switches discard only unsubmitted packets. Submitted packets are
// retired by the provider completion watermark, or after an explicit provider
// drain during teardown.
class PresentLeaseQueue {
    uint64_t nextId_ = 1;
    std::deque<std::shared_ptr<PresentLease>> live_;

    bool Owns(const std::shared_ptr<PresentLease>& lease) const {
        if (!lease) return false;
        return std::any_of(live_.begin(), live_.end(),
            [&](const auto& item) { return item.get() == lease.get() && item->id == lease->id; });
    }

public:
    std::shared_ptr<PresentLease> PublishReady(PresentFrame frame) {
        if (!frame.ProviderReady()) return {};
        auto lease = std::make_shared<PresentLease>();
        lease->id = nextId_++;
        lease->frame = std::move(frame);
        live_.push_back(lease);
        return lease;
    }

    bool MarkSubmitted(const std::shared_ptr<PresentLease>& lease, uint64_t completionSerial) {
        if (!completionSerial || !Owns(lease) || lease->completionSerial) return false;
        lease->completionSerial = completionSerial;
        return true;
    }

    bool DiscardUnsubmitted(uint64_t id) {
        const auto before = live_.size();
        std::erase_if(live_, [&](const auto& lease) {
            return lease->id == id && lease->completionSerial == 0;
        });
        return live_.size() != before;
    }

    void CancelUnsubmitted() {
        std::erase_if(live_, [](const auto& lease) { return lease->completionSerial == 0; });
    }

    void RetireCompleted(uint64_t completedSerial) {
        if (!completedSerial) return;
        std::erase_if(live_, [&](const auto& lease) {
            return lease->completionSerial && lease->completionSerial <= completedSerial;
        });
    }

    // Caller must have stopped/drained the FG provider before this is used.
    void DrainAfterProvider() { live_.clear(); }

    size_t LiveCount() const { return live_.size(); }
    size_t InFlightCount() const {
        return size_t(std::count_if(live_.begin(), live_.end(),
            [](const auto& lease) { return lease->completionSerial != 0; }));
    }
};

} // namespace gpu::frame_generation
