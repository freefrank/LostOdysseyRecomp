#pragma once

#include <charconv>
#include <chrono>
#include <cstdint>
#include <string_view>

namespace gpu::render_batch
{
    // D3D12 shares a 65536-view heap. Unused 3D/cube banks reuse one dummy set,
    // so unique 2D combinations can use most of that heap. Vulkan owns
    // independent descriptor pools. 500 is the historical per-draw split.
    inline constexpr uint32_t kD3D12DescriptorLimit = 1800;
    inline constexpr uint32_t kVulkanDescriptorLimit = 2048;
    inline constexpr uint32_t kMinDescriptorLimit = 500;

    inline uint32_t DescriptorLimit(bool vulkan, std::string_view overrideValue = {})
    {
        const uint32_t fallback = vulkan ? kVulkanDescriptorLimit : kD3D12DescriptorLimit;
        if (!vulkan) return fallback;
        uint32_t value = 0;
        if (!overrideValue.empty()) {
            const auto result = std::from_chars(overrideValue.data(), overrideValue.data() + overrideValue.size(), value);
            if (result.ec == std::errc{} && result.ptr == overrideValue.data() + overrideValue.size() &&
                value >= kMinDescriptorLimit && value <= kVulkanDescriptorLimit) return value;
        }
        return fallback;
    }

    // Disabled instrumentation must not read the clock, even when a scope ends.
    // Clock injection lets the fixture verify this without timing noise.
    template<class Clock = std::chrono::steady_clock>
    class CpuTimer
    {
        bool enabled;
        typename Clock::time_point start{};
    public:
        explicit CpuTimer(bool active) : enabled(active) { if (enabled) start = Clock::now(); }
        void AddTo(double& accumulator) const {
            if (enabled) accumulator += std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        }
    };
}
