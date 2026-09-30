#pragma once
// Provider-neutral frame-generation policy. No game, renderer, SDK or OS headers.
#include <cmath>
#include <cstdint>
#include <exception>
#include <memory>
#include <utility>

namespace framegen {
enum class Provider : uint8_t { Off, Dlss, Fsr };
enum class Mode : uint8_t { Off, Fixed, Dynamic };
enum class Api : uint8_t { D3D12, Vulkan };
struct Config {
    Provider provider = Provider::Off;
    Mode mode = Mode::Off;
    uint32_t generatedFrames = 1; // Generated frames, not the total multiplier.
    float targetFrameRate = 0;    // Dynamic mode: zero asks the SDK to use the display.
    bool operator==(const Config&) const = default;
};
// Largest total multiplier (rendered frame included) that settings and
// LO_FG_MULTIPLIER may request; DLSS multi-frame generation tops out at 6x.
// Select() still limits each request to the SDK-reported maximum.
constexpr uint32_t kMaxMultiplier = 6;
struct Capabilities {
    bool available = false;
    uint32_t maxGeneratedFrames = 0;
    bool dynamic = false;
};
enum class Rejection : uint8_t { None, InvalidConfig, Unavailable, Multiplier, DynamicUnsupported };
struct Selection {
    Config config{};
    Rejection rejection = Rejection::None;
    bool Enabled() const { return config.provider != Provider::Off && config.mode != Mode::Off; }
};
inline Selection Select(Config request, Capabilities caps) {
    if (request.provider == Provider::Off || request.mode == Mode::Off) return {};
    if ((request.provider != Provider::Dlss && request.provider != Provider::Fsr) ||
        (request.mode != Mode::Fixed && request.mode != Mode::Dynamic) ||
        !std::isfinite(request.targetFrameRate) || request.targetFrameRate < 0 ||
        !request.generatedFrames)
        return {{}, Rejection::InvalidConfig};
    if (!caps.available || !caps.maxGeneratedFrames) return {{}, Rejection::Unavailable};
    if (request.mode == Mode::Dynamic && !caps.dynamic) return {{}, Rejection::DynamicUnsupported};
    if (request.generatedFrames > caps.maxGeneratedFrames) return {{}, Rejection::Multiplier};
    return {request, Rejection::None};
}

// One swapchain has one owner. SR is deliberately absent from this contract.
class SwapchainOwner {
    Provider owner_ = Provider::Off;
public:
    bool Acquire(Provider requested) {
        if (requested == Provider::Off || owner_ != Provider::Off) return false;
        owner_ = requested; return true;
    }
    bool Release(Provider expected, bool drained) {
        if (!drained || expected == Provider::Off || expected != owner_) return false;
        owner_ = Provider::Off; return true;
    }
    Provider Get() const { return owner_; }
};

// A lease is released only after a checked completion, never from SDK counters.
// The application must keep this object alive (or terminate) on an unknown GPU
// completion. Destroying an in-flight context is not a recovery strategy.
class InputLease {
public:
    enum class Phase : uint8_t { Empty, Recorded, SubmitAttempted, Submitted, Presented };
private:
    Phase phase_ = Phase::Empty;
    std::shared_ptr<void> owner_;
    uint64_t serial_ = 0;
public:
    InputLease() = default;
    InputLease(const InputLease&) = delete;
    InputLease& operator=(const InputLease&) = delete;
    ~InputLease() { if (Pending()) std::terminate(); }
    bool Begin(std::shared_ptr<void> owner) {
        if (phase_ != Phase::Empty || !owner) return false;
        owner_ = std::move(owner); phase_ = Phase::Recorded; return true;
    }
    bool SubmitStart() {
        if (phase_ != Phase::Recorded) return false;
        phase_ = Phase::SubmitAttempted; return true;
    }
    bool Submitted(bool success, uint64_t serial) {
        if (phase_ != Phase::SubmitAttempted || !success || !serial) return false;
        serial_ = serial; phase_ = Phase::Submitted; return true;
    }
    bool Presented(bool accepted) {
        if (phase_ != Phase::Submitted || !accepted) return false;
        phase_ = Phase::Presented; return true;
    }
    bool Complete(uint64_t serial, bool checked) {
        if (phase_ != Phase::Presented || !checked || serial != serial_) return false;
        owner_.reset(); serial_ = 0; phase_ = Phase::Empty; return true;
    }
    bool CancelRecorded(bool commandsReset, bool tagsRevoked, bool producerCompleted) {
        if (phase_ != Phase::Recorded || !commandsReset || !tagsRevoked || !producerCompleted) return false;
        owner_.reset(); phase_ = Phase::Empty; return true;
    }
    bool Pending() const { return phase_ != Phase::Empty; }
    Phase GetPhase() const { return phase_; }
    uint64_t Serial() const { return serial_; }
};

struct HistoryKey {
    uint64_t deviceEpoch = 0, temporalEpoch = 0;
    uint32_t width = 0, height = 0, inputWidth = 0, inputHeight = 0, format = 0;
    Config config{};
    bool operator==(const HistoryKey&) const = default;
};
class History {
    HistoryKey key_{};
    uint64_t frame_ = 0;
    bool valid_ = false;
public:
    bool NeedsReset(uint64_t frame, const HistoryKey& key, bool explicitReset) const {
        return explicitReset || !valid_ || frame_ == UINT64_MAX || frame != frame_ + 1 || key != key_;
    }
    void Accepted(uint64_t frame, const HistoryKey& key) { frame_ = frame; key_ = key; valid_ = true; }
    void Reset() { valid_ = false; }
};

// Values are diagnostics, not physical display evidence. GetState deltas can
// span more than one Present after a failed/skipped query; do not call that MFG.
struct PresentStatistics {
    uint64_t actualPresents = 0, generatedIntervals = 0;
    bool contiguous = false;
    void RawPresent(bool accepted) {
        if (accepted) ++actualPresents;
        contiguous = false;
    }
    void Observe(bool queried, bool accepted, bool active, uint32_t delta) {
        if (queried && accepted) actualPresents += delta;
        if (contiguous && queried && accepted && active && delta > 1) ++generatedIntervals;
        contiguous = queried && accepted;
    }
};
} // namespace framegen
