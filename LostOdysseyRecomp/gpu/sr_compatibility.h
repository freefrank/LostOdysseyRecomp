#pragma once

#include <cstdlib>

namespace gpu::sr_compatibility {
// Quality-only concessions for bringing up temporal upscalers. This does not
// relax image bounds, device ownership, frame identity, or GPU synchronization.
// Launch with LO_SR_COMPAT=0 to restore the strict reference path.
inline bool Enabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("LO_SR_COMPAT");
        return !value || value[0] != '0' || value[1] != '\0';
    }();
    return enabled;
}
}
