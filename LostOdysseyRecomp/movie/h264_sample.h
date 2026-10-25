#pragma once
#include "mp4_demux.h"
#include <span>

namespace movie {
inline void AppendH264ParameterSets(const std::vector<std::vector<uint8_t>>& sets, std::vector<uint8_t>& out) {
    for (const auto& nal : sets) {
        out.insert(out.end(), {0, 0, 0, 1});
        out.insert(out.end(), nal.begin(), nal.end());
    }
}

// Append one complete MP4 access unit, preserving its NAL order (including
// avc3's in-band parameter sets). Codec configuration is supplied separately.
inline bool AppendH264Sample(std::span<const uint8_t> raw, uint8_t lengthBytes,
                             std::vector<uint8_t>& out, std::string& error) {
    if (lengthBytes < 1 || lengthBytes > 4) { error = "invalid H.264 NAL length size"; return false; }
    for (size_t pos = 0; pos < raw.size();) {
        if (raw.size() - pos < lengthBytes) { error = "truncated NAL length in a sample"; return false; }
        size_t n = 0;
        for (uint8_t i = 0; i < lengthBytes; ++i) n = (n << 8) | raw[pos++];
        if (n > raw.size() - pos) { error = "NAL length runs past its sample"; return false; }
        out.insert(out.end(), {0, 0, 0, 1});
        out.insert(out.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
    }
    return true;
}
}
