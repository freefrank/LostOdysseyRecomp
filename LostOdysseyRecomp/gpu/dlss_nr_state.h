#pragma once

#include <atomic>
#include <cstdint>

namespace gpu::dlss {
// DLSS 5 Neural Rendering outcome. The NGX controller writes it on the render
// thread; the settings menu reads it for the help line.
enum class NeuralRenderingState : uint8_t {
    Off,
    Active,
    // nvngx_dlssnr.dll is missing or is not a usable copy.
    MissingRuntime,
    // The GPU, driver or device extensions cannot run it.
    Unsupported,
    Failed,
};
inline std::atomic<NeuralRenderingState> g_neuralRenderingState{NeuralRenderingState::Off};

// The model controls every evaluate reads (310.8.0.0 honours them without a
// feature rebuild). Strengths use the model's 0..2 scale; skin below 0 follows
// structure. Style: 0 Default, 1 Natural, 2 Cinematic.
struct NeuralRenderingTuning {
    uint32_t style = 0;
    float intensity = 1.0f, globalTone = 1.0f, localTone = 1.0f, structure = 1.0f, skin = -1.0f;
    bool autoMask = true;
    bool operator==(const NeuralRenderingTuning&) const = default;
};
} // namespace gpu::dlss
