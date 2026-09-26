#pragma once
#include "mod_api.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <vector>

namespace modding {
struct ImageData {
    uint32_t width = 0, height = 0;
    // Native menu pixels are 0xAARRGGBB, independent of host byte order.
    std::vector<uint32_t> pixels;
};
// LOTEX1 is an uncompressed, identity-bearing RGBA8 interchange format.
// Native atlas consumers must pass their original dimensions to preserve UVs.
inline std::optional<ImageData> ReadImageReplacement(const AssetRequest& request,
                                                    uint32_t expectedWidth, uint32_t expectedHeight) noexcept {
    try {
        const auto resolved = Resolve(request);
        if (!resolved) return {};
        auto decode = [&]() -> std::optional<ImageData> {
            std::ifstream in(resolved->path, std::ios::binary | std::ios::ate);
            if (!in) return {};
            const auto length = in.tellg();
            if (length < 24 || length > std::streamoff(24 + 4096 + 64 * 1024 * 1024)) return {};
            in.seekg(0);
            std::array<uint8_t, 24> header{};
            if (!in.read(reinterpret_cast<char*>(header.data()), header.size())) return {};
            const std::array<uint8_t, 8> magic{'L','O','T','E','X','1','\r','\n'};
            if (!std::equal(magic.begin(), magic.end(), header.begin())) return {};
            auto u32 = [&](size_t p) { return uint32_t(header[p]) | uint32_t(header[p+1]) << 8 |
                uint32_t(header[p+2]) << 16 | uint32_t(header[p+3]) << 24; };
            const auto w = u32(8), h = u32(12), keyLength = u32(16), format = u32(20);
            const uint64_t count = uint64_t(w) * h;
            if (!w || !h || w > 8192 || h > 8192 || count > 16 * 1024 * 1024 ||
                w != expectedWidth || h != expectedHeight || !keyLength || keyLength > 4096 || format != 1 ||
                uint64_t(length) != 24 + keyLength + count * 4) return {};
            std::string key(keyLength, '\0');
            if (!in.read(key.data(), key.size()) || key != resolved->id.key) return {};
            ImageData image{w, h, std::vector<uint32_t>(size_t(count))};
            std::vector<uint8_t> row(size_t(w) * 4);
            for (uint32_t y = 0; y < h; ++y) {
                if (!in.read(reinterpret_cast<char*>(row.data()), row.size())) return {};
                for (uint32_t x = 0; x < w; ++x) {
                    const auto* p = row.data() + size_t(x) * 4;
                    image.pixels[size_t(y) * w + x] = uint32_t(p[3]) << 24 | uint32_t(p[0]) << 16 | uint32_t(p[1]) << 8 | p[2];
                }
            }
            return image;
        };
        auto image = decode();
        if (!image) std::fprintf(stderr, "[mods] invalid image, using original: %s\n", resolved->path.string().c_str());
        return image;
    } catch (...) { return {}; }
}
}
