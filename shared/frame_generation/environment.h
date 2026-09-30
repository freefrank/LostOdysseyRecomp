#pragma once
#include "core.h"
#include <charconv>
#include <string_view>

namespace framegen {
struct EnvironmentSelection {
    Config config{};
    const char* error = nullptr;
    bool Enabled() const { return !error && config.provider != Provider::Off && config.mode != Mode::Off; }
};
// Explicit provider=off wins over the legacy opt-in. A malformed request never
// silently enables a different provider, multiplier, or fallback algorithm.
inline EnvironmentSelection ParseEnvironment(const char* provider, const char* mode,
    const char* multiplier, const char* target, const char* legacyDlss = nullptr) {
    EnvironmentSelection out;
    const std::string_view p = provider ? provider : (legacyDlss && std::string_view(legacyDlss) == "1" ? "dlss" : "off");
    if (p == "off" || p.empty()) return out;
    if (p == "dlss") out.config.provider = Provider::Dlss;
    else if (p == "fsr") out.config.provider = Provider::Fsr;
    else { out.error = "LO_FG_PROVIDER must be off, dlss, or fsr"; return out; }
    const std::string_view m = mode ? mode : "fixed";
    if (m == "off") { out.config = {}; return out; }
    if (m == "fixed") out.config.mode = Mode::Fixed;
    else if (m == "dynamic") out.config.mode = Mode::Dynamic;
    else { out.error = "LO_FG_MODE must be off, fixed, or dynamic"; return out; }
    if (multiplier) {
        uint32_t value = 0;
        const std::string_view text(multiplier);
        const auto result = std::from_chars(text.data(), text.data()+text.size(), value);
        if (result.ec != std::errc{} || result.ptr != text.data()+text.size() || value < 2 || value > kMaxMultiplier) {
            out.error = "LO_FG_MULTIPLIER must be an integer from 2 to 6"; return out;
        }
        out.config.generatedFrames = value - 1;
    }
    if (target) {
        const std::string_view text(target);
        const auto result = std::from_chars(text.data(), text.data()+text.size(), out.config.targetFrameRate);
        if (result.ec != std::errc{} || result.ptr != text.data()+text.size() ||
            !std::isfinite(out.config.targetFrameRate) || out.config.targetFrameRate < 0) {
            out.error = "LO_FG_TARGET_FPS must be finite and non-negative"; return out;
        }
    }
    if (out.config.provider == Provider::Fsr &&
        (out.config.mode != Mode::Fixed || out.config.generatedFrames != 1))
        out.error = "The pinned FSR FG adapter supports fixed 2x only";
    return out;
}
} // namespace framegen
