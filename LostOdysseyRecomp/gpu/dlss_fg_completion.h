#pragma once

#include <cstdint>
#include <limits>

namespace gpu::dlss_fg {

// Game-path ownership for eBlockPresentingClientQueue only. These serials
// describe successful post-Present queue markers, not renderer submissions,
// SDK frame counts, or the standalone probe's explicit timeline mode.
class PresentQueueCompletion {
public:
    bool Pending() const { return submitted_ != completed_; }
    bool CanSubmit() const { return !failed_ && !Pending(); }
    uint64_t SubmittedSerial() const { return submitted_; }
    uint64_t CompletedSerial() const { return completed_; }

    bool Submitted(bool success) {
        if (!CanSubmit() || !success || submitted_ == std::numeric_limits<uint64_t>::max()) {
            failed_ = true;
            return false;
        }
        ++submitted_;
        return true;
    }
    bool Completed(uint64_t serial, bool success) {
        if (failed_ || !Pending() || serial != submitted_ || !success) return false;
        completed_ = serial;
        return true;
    }
private:
    uint64_t submitted_ = 0, completed_ = 0;
    bool failed_ = false;
};

} // namespace gpu::dlss_fg
