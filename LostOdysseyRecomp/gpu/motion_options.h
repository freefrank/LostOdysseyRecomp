#pragma once
#include <cstdlib>
#include <string_view>
#include "upscaling_plan.h"
namespace gpu::temporal {
// Geometric motion is the default for TAA. Renderer work is gated by active
// temporal rendering; LO_MV_ENABLE=0 remains an exact comparison switch.
struct MotionOptions {
    bool enabled = false, replay = false, consume = false, debug = false, log = false, timing = false;
    static MotionOptions Environment() {
        auto on = [](const char* n) { const auto* p = std::getenv(n); return p && std::string_view(p) == "1"; };
        auto notOff = [](const char* n) { const auto* p = std::getenv(n); return !p || std::string_view(p) != "0"; };
        MotionOptions out; out.enabled = notOff("LO_MV_ENABLE");
        out.replay = out.enabled && notOff("LO_MV_REPLAY");
        out.consume = out.replay && notOff("LO_MV_CONSUME");
        out.debug = out.consume && on("LO_MV_DEBUG"); out.log = on("LO_MV_LOG");
        out.timing = on("LO_MV_TIMING"); // TAA baseline A can be measured with MV disabled.
        return out;
    }
    bool ExplicitlyDisabled() const { return !enabled || !replay || !consume; }
    bool Supports(upscaling::TemporalConsumer consumer) const {
        return consumer != upscaling::TemporalConsumer::DlssInputs || !ExplicitlyDisabled();
    }
    const char* Failure(upscaling::TemporalConsumer consumer) const {
        if (consumer != upscaling::TemporalConsumer::DlssInputs) return "";
        if (!enabled) return "LO_MV_ENABLE=0 disables required DLSS geometry motion";
        if (!replay) return "LO_MV_REPLAY=0 disables required DLSS geometry motion";
        if (!consume) return "LO_MV_CONSUME=0 disables required DLSS geometry motion";
        return "";
    }
};
}
