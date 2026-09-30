#pragma once
#include <cstring>
#include <optional>

namespace probe {
struct ProbeOptions {
    bool noActivate{};
    bool explicitInputWait{};
};

inline std::optional<ProbeOptions> ParseProbeOptions(int argc, char* const argv[]) {
    ProbeOptions options{};
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--no-activate") == 0 && !options.noActivate)
            options.noActivate = true;
        else if (std::strcmp(argv[i], "--explicit-input-wait") == 0 && !options.explicitInputWait)
            options.explicitInputWait = true;
        else
            return std::nullopt;
    }
    return options;
}
}
