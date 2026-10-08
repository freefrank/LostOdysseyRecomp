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
} // namespace gpu::dlss
