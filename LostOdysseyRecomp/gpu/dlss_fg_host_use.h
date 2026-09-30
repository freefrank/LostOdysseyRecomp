#pragma once
#include <cstdint>

namespace gpu::dlss_fg {
// One presentation-thread input recording. A native submit attempt is an
// irreversible boundary: even an error cannot authorize command cancellation.
// Post-Present fence serials are tracked separately by PresentQueueCompletion.
class HostInputUse {
public:
    bool Begin(const void* commands) {
        if (!commands || commands_) return false;
        commands_ = commands; attempted_ = false; serial_ = 0;
        return true;
    }
    bool SubmissionStarted() {
        if (!commands_) return true; // FG-off host frame owns no input recording.
        if (attempted_) return false;
        attempted_ = true;
        return true;
    }
    bool Submitted(bool success, uint64_t serial) {
        if (!success || !serial) return false;
        if (!commands_) return true;
        if (!attempted_ || serial_) return false;
        serial_ = serial;
        return true;
    }
    bool Matches(const void* commands) const { return commands_ && commands_ == commands; }
    bool CanCancel(const void* commands) const { return Matches(commands) && !attempted_; }
    bool Canceled(const void* commands, bool reset, bool tagsRevoked, bool producerDrained) {
        if (!CanCancel(commands) || !reset || !tagsRevoked || !producerDrained) return false;
        Clear();
        return true;
    }
    bool Completed() {
        if (commands_ && (!attempted_ || !serial_)) return false;
        Clear();
        return true;
    }
    bool Pending() const { return commands_ != nullptr; }
    uint64_t Serial() const { return serial_; }
private:
    void Clear() { commands_ = nullptr; attempted_ = false; serial_ = 0; }
    const void* commands_ = nullptr;
    bool attempted_ = false;
    uint64_t serial_ = 0;
};
} // namespace gpu::dlss_fg
