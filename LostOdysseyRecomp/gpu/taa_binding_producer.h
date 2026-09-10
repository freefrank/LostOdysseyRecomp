#pragma once
#include "taa_binding_evidence.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace gpu::taa_collection::binding {
inline int32_t RelativeAge(uint64_t now, uint64_t before) noexcept {
    return before <= now && now - before <= 255 ? int32_t(now - before) : -1;
}

inline bool ProvenTransform(const Transform& value) noexcept {
    if (value.slot < 0 || value.slot > 252 || value.phase > 32) return false;
    for (auto bits : value.guestVP) if (!std::isfinite(std::bit_cast<float>(bits))) return false;
    for (auto bits : value.uploadedVP) if (!std::isfinite(std::bit_cast<float>(bits))) return false;
    for (auto bits : value.viewport) if (!std::isfinite(std::bit_cast<float>(bits))) return false;
    for (auto bits : value.jitterNdc) if (!std::isfinite(std::bit_cast<float>(bits))) return false;
    return std::bit_cast<float>(value.viewport[2]) > 0 && std::bit_cast<float>(value.viewport[3]) > 0 &&
        (!value.applied || value.phase != 0);
}

inline bool SameProducerTransform(Transform a, Transform b) noexcept {
    // Different VS constant slots can contain the exact same camera matrix.
    a.slot = b.slot = -1;
    return a == b;
}

// CPU record provenance, not GPU completion or per-pixel coverage. A clear is
// phase-independent background. Unobserved/partial retained pixels never become
// a proven uniform producer merely because the last draw had a known matrix.
struct Producer {
    uint64_t epoch = 0, frame = 0;
    ProducerState state = ProducerState::Unknown;
    uint32_t draws = 0;
    bool initialized = false, cleared = false, drawCountOverflowed = false;
    Transform transform{};

    void Unknown(uint64_t generation, uint64_t atFrame) noexcept {
        *this = {};
        epoch = generation; frame = atFrame;
    }
    void Mixed(uint64_t generation, uint64_t atFrame) noexcept {
        const auto count = epoch == generation ? draws : 0;
        const bool overflowed = epoch == generation && drawCountOverflowed;
        Unknown(generation, atFrame);
        initialized = true; state = ProducerState::Mixed; draws = count;
        drawCountOverflowed = overflowed;
    }
    void Clear(uint64_t generation, uint64_t atFrame, bool full) noexcept {
        if (!full) { Mixed(generation, atFrame); return; }
        Unknown(generation, atFrame);
        initialized = cleared = true;
    }
    void Draw(uint64_t generation, uint64_t atFrame, const Transform& input) noexcept {
        if (epoch != generation) Unknown(generation, atFrame);
        if (draws == 65535) drawCountOverflowed = true;
        if (drawCountOverflowed) {
            // Zero means unknown; never report a clamped count as exact evidence.
            draws = 0; initialized = true; cleared = false;
            state = ProducerState::Unknown; transform = {}; frame = atFrame;
            return;
        }
        ++draws;
        if (!initialized) { frame = atFrame; return; }
        // Retained geometry from an earlier frame is not current-frame coverage.
        if (!cleared && frame != atFrame) { Mixed(generation, atFrame); return; }
        const bool proven = ProvenTransform(input);
        Transform normalized = input;
        if (!normalized.applied) { normalized.phase = 0; normalized.jitterNdc = {}; }
        if (cleared) {
            cleared = false; frame = atFrame;
            if (!proven) { state = ProducerState::Unknown; transform = {}; return; }
            transform = normalized;
            state = input.applied ? ProducerState::UniformJittered : ProducerState::UniformUnjittered;
        } else if (!proven || state == ProducerState::Unknown || state == ProducerState::Mixed ||
            !SameProducerTransform(transform, normalized)) {
            Mixed(generation, atFrame);
        }
    }
    void Copy(const Producer& source, uint64_t generation, uint64_t atFrame, bool fullDestination) noexcept {
        const Producer input = source; // Also valid when source and destination alias.
        if (input.epoch != generation || !input.initialized) {
            if (fullDestination) Unknown(generation, atFrame);
            else Mixed(generation, atFrame);
            return;
        }
        if (fullDestination || (epoch == generation && initialized && cleared)) {
            *this = input;
            return; // Preserve the producer frame; the resolve has its own frame.
        }
        // Partial copies may retain pixels from a different pass or old frame.
        Mixed(generation, atFrame);
    }
    void Describe(Texture& output, uint64_t generation, uint64_t now) const noexcept {
        if (epoch != generation || !initialized) return;
        output.producerFrameAge = RelativeAge(now, frame);
        output.producerState = state;
        output.producerDraws = draws;
        if (state == ProducerState::UniformJittered || state == ProducerState::UniformUnjittered)
            output.producer = transform;
    }
};
}
