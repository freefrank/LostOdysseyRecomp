#pragma once
#include <cstdint>
#include <limits>

namespace gpu::submission {
// Command-worker owned. A native failure is terminal for this device lifetime,
// not a request-level DLSS fallback. In particular a failed submit has no fence
// to wait for. Only an explicit device replacement creates a fresh state.
class VulkanState {
public:
    static constexpr int32_t Success = 0, DeviceLost = -4, InvalidState = -13;
    bool Stopped() const { return firstFailure_ != Success; }
    bool Lost() const { return lost_; }
    int32_t Failure() const { return firstFailure_; }
    uint64_t LastSubmission() const { return serial_; }
    void Stop(int32_t error) {
        if (error == Success) error = InvalidState;
        if (!Stopped()) firstFailure_ = error;
        lost_ |= error == DeviceLost;
    }
    template<class Reset, class Submit>
    bool SubmitBatch(Reset&& reset, Submit&& submit, uint64_t& serial, int32_t& result) {
        serial = 0;
        if (Stopped()) { result = Failure(); return false; }
        if (serial_ == std::numeric_limits<uint64_t>::max()) {
            Stop(InvalidState); result = Failure(); return false;
        }
        result = reset();
        if (result == Success) result = submit();
        if (result != Success) { Stop(result); return false; }
        serial = ++serial_;
        return true;
    }
    template<class Wait>
    bool WaitSubmitted(Wait&& wait) {
        // Previously successful submissions still need draining after another
        // submit failed. Do not reset the failure latch on a successful wait.
        const int32_t result = wait();
        if (result != Success) Stop(result);
        return result == Success;
    }
private:
    int32_t firstFailure_ = Success;
    uint64_t serial_ = 0;
    bool lost_ = false;
};
} // namespace gpu::submission
