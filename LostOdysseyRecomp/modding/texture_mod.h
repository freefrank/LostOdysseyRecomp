#pragma once
#include "mod_api.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <fstream>
#include <vector>

namespace modding {
// A validated LOTEX2 replacement (docs/wiki/Modding-API.md, Runtime
// textures). Levels are top first, rows without padding.
// Type 1: plain RGBA8 texels; the levels always form the full chain down to 1x1.
// Type 2: the DDS levels' 4x4 blocks as stored, exactly the payload's mip
// count; dxgiFormat names the block format (kTextureBc*).
struct TextureData {
    uint32_t width = 0, height = 0, scale = 1; // level 0; scale = payload / original size
    uint32_t type = 1, dxgiFormat = 0;         // payload type; 0 for type 1
    std::string key, modId;                    // key is informational, from the header
    std::filesystem::path path;
    std::vector<std::vector<uint8_t>> levels;
};
// Type-2 block formats, as DXGI_FORMAT values (all UNORM).
inline constexpr uint32_t kTextureBc1 = 71, kTextureBc3 = 77, kTextureBc4 = 80, kTextureBc7 = 98;
inline uint32_t TextureBcBytesPerBlock(uint32_t dxgiFormat) {
    return dxgiFormat == kTextureBc1 || dxgiFormat == kTextureBc4 ? 8 : 16;
}
// Bytes of one tightly packed block-compressed level.
inline uint64_t TextureBcLevelBytes(uint32_t width, uint32_t height, uint32_t dxgiFormat) {
    return uint64_t(std::max(1u, (width + 3) / 4)) * std::max(1u, (height + 3) / 4) * TextureBcBytesPerBlock(dxgiFormat);
}
inline const char* TexturePayloadName(const TextureData& data) {
    switch (data.dxgiFormat) {
    case kTextureBc1: return "BC1";
    case kTextureBc3: return "BC3";
    case kTextureBc4: return "BC4";
    case kTextureBc7: return "BC7";
    default: return "RGBA8";
    }
}
inline std::string TextureFingerprintKey(uint64_t fingerprint) {
    char key[17];
    std::snprintf(key, sizeof(key), "%016llx", static_cast<unsigned long long>(fingerprint));
    return key;
}
// The next level by a 2x2 box filter; an odd last row or column is dropped.
inline std::vector<uint8_t> HalveRgba8(const std::vector<uint8_t>& src, uint32_t w, uint32_t h) {
    const uint32_t nw = std::max(1u, w >> 1), nh = std::max(1u, h >> 1);
    std::vector<uint8_t> dst(size_t(nw) * nh * 4);
    for (uint32_t y = 0; y < nh; ++y) {
        const uint8_t* r0 = src.data() + size_t(std::min(2 * y, h - 1)) * w * 4;
        const uint8_t* r1 = src.data() + size_t(std::min(2 * y + 1, h - 1)) * w * 4;
        for (uint32_t x = 0; x < nw; ++x) {
            const size_t a = size_t(std::min(2 * x, w - 1)) * 4, b = size_t(std::min(2 * x + 1, w - 1)) * 4;
            for (size_t c = 0; c < 4; ++c)
                dst[(size_t(y) * nw + x) * 4 + c] = uint8_t((r0[a + c] + r0[b + c] + r1[a + c] + r1[b + c] + 2) / 4);
        }
    }
    return dst;
}
// Resolves {Texture, fingerprint} and validates the file against the upload
// it replaces. A missing replacement returns nullopt silently; an invalid one
// returns nullopt and reports once, through *error when given, else stderr.
// maxLevelBytes bounds level 0 (RGBA8 texels, or the type-2 blocks) before
// anything is allocated. blockCompressed false (a device that cannot sample BC)
// leaves type-2 files unread: nullopt, no error, *blockCompressedSkipped true.
inline std::optional<TextureData> ReadTextureReplacement(uint64_t fingerprint, uint32_t format,
    uint32_t width, uint32_t height, uint64_t maxLevelBytes, std::string* error = nullptr,
    bool blockCompressed = true, bool* blockCompressedSkipped = nullptr) noexcept {
    try {
        if (blockCompressedSkipped) *blockCompressedSkipped = false;
        const auto resolved = Resolve({{AssetKind::Texture, TextureFingerprintKey(fingerprint)}, {}});
        if (!resolved) return {};
        const char* reason = nullptr;
        bool skipped = false;
        auto decode = [&]() -> std::optional<TextureData> {
            std::ifstream in(resolved->path, std::ios::binary | std::ios::ate);
            if (!in) { reason = "cannot open"; return {}; }
            const uint64_t length = uint64_t(std::streamoff(in.tellg()));
            std::array<uint8_t, 64> header{};
            in.seekg(0);
            if (length < header.size() || !in.read(reinterpret_cast<char*>(header.data()), header.size())) { reason = "truncated header"; return {}; }
            const std::array<uint8_t, 8> magic{'L','O','T','E','X','2','\r','\n'};
            if (!std::equal(magic.begin(), magic.end(), header.begin())) { reason = "not LOTEX2"; return {}; }
            auto u32 = [&](size_t p) { return uint32_t(header[p]) | uint32_t(header[p+1]) << 8 |
                uint32_t(header[p+2]) << 16 | uint32_t(header[p+3]) << 24; };
            auto u64 = [&](size_t p) { return uint64_t(u32(p)) | uint64_t(u32(p + 4)) << 32; };
            const uint32_t headerSize = u32(8), type = u32(12), originalFormat = u32(24);
            const uint32_t w = u32(36), h = u32(40), mips = u32(44), keyLength = u32(56);
            const uint64_t payloadSize = u64(48);
            if (type != 1 && type != 2) { reason = "payload type is not 1 (RGBA8) or 2 (DDS)"; return {}; }
            if (type == 2 && !blockCompressed) { skipped = true; return {}; }
            if (keyLength > 4096 || headerSize != 64 + keyLength || u32(60) != 0) { reason = "bad header or key length"; return {}; }
            if (u64(16) != fingerprint || originalFormat != format || u32(28) != width || u32(32) != height) {
                reason = "fingerprint, format or original size differs from the game texture"; return {};
            }
            const uint32_t scale = width ? w / width : 0;
            if ((scale != 1 && scale != 2 && scale != 4 && scale != 8) || w != width * scale || h != height * scale ||
                w > 8192 || h > 8192) { reason = "payload size is not the original times 1, 2, 4 or 8"; return {}; }
            const uint32_t fullChain = uint32_t(std::bit_width(std::max(w, h)));
            if (!mips || mips > fullChain) { reason = "bad mip count"; return {}; }
            if (type == 2) {
                if (w % 4 || h % 4) { reason = "DDS level 0 is not a multiple of 4"; return {}; }
                TextureData data{w, h, scale, type, 0, std::string(keyLength, '\0'), resolved->modId, resolved->path, {}};
                if (!in.read(data.key.data(), keyLength)) { reason = "truncated key"; return {}; }
                // DDS_HEADER after the magic; DDS_HEADER_DXT10 follows for FourCC DX10.
                std::array<uint8_t, 148> dds{};
                if (payloadSize < 128 || !in.read(reinterpret_cast<char*>(dds.data()), 128)) { reason = "truncated DDS header"; return {}; }
                auto d32 = [&](size_t p) { return uint32_t(dds[p]) | uint32_t(dds[p+1]) << 8 |
                    uint32_t(dds[p+2]) << 16 | uint32_t(dds[p+3]) << 24; };
                constexpr auto fourCC = [](const char (&s)[5]) {
                    return uint32_t(uint8_t(s[0])) | uint32_t(uint8_t(s[1])) << 8 | uint32_t(uint8_t(s[2])) << 16 | uint32_t(uint8_t(s[3])) << 24;
                };
                // Every supported format sets DDPF_FOURCC (4) in the pixel format.
                if (d32(0) != fourCC("DDS ") || d32(4) != 124 || d32(76) != 32 || !(d32(80) & 4)) { reason = "not a DDS file with a FourCC format"; return {}; }
                // caps2 cube map (0x200) or volume (0x200000); DDSD_DEPTH (0x800000) in flags.
                if ((d32(112) & 0x200200) || ((d32(8) & 0x800000) && d32(24) > 1)) { reason = "DDS is a cube map or volume"; return {}; }
                uint64_t ddsBytes = 128;
                const uint32_t pixelFormat = d32(84);
                if (pixelFormat == fourCC("DXT1")) data.dxgiFormat = kTextureBc1;
                else if (pixelFormat == fourCC("DXT5")) data.dxgiFormat = kTextureBc3;
                else if (pixelFormat == fourCC("ATI1") || pixelFormat == fourCC("BC4U")) data.dxgiFormat = kTextureBc4;
                else if (pixelFormat == fourCC("DX10")) {
                    ddsBytes = 148;
                    if (payloadSize < ddsBytes || !in.read(reinterpret_cast<char*>(dds.data() + 128), 20)) { reason = "truncated DDS header"; return {}; }
                    data.dxgiFormat = d32(128);
                    // D3D10_RESOURCE_DIMENSION_TEXTURE2D (3); misc flag TEXTURECUBE (4).
                    if (d32(132) != 3 || (d32(136) & 4) || d32(140) != 1) { reason = "DDS is not one 2D texture"; return {}; }
                }
                if (data.dxgiFormat != kTextureBc1 && data.dxgiFormat != kTextureBc3 &&
                    data.dxgiFormat != kTextureBc4 && data.dxgiFormat != kTextureBc7) { reason = "DDS format is not BC1, BC3, BC4 or BC7 UNORM"; return {}; }
                // A mip count of 0 means one level, as DirectXTex reads it.
                if (d32(16) != w || d32(12) != h || std::max(1u, d32(28)) != mips) { reason = "DDS size or mip count differs from the LOTEX2 header"; return {}; }
                uint64_t expected = ddsBytes;
                for (uint32_t i = 0; i < mips; ++i) expected += TextureBcLevelBytes(std::max(1u, w >> i), std::max(1u, h >> i), data.dxgiFormat);
                if (payloadSize != expected || length != headerSize + payloadSize) { reason = "payload or file size is not exact"; return {}; }
                if (TextureBcLevelBytes(w, h, data.dxgiFormat) > maxLevelBytes) { reason = "level 0 is too large to upload"; return {}; }
                data.levels.reserve(mips);
                for (uint32_t i = 0; i < mips; ++i) {
                    auto& level = data.levels.emplace_back(size_t(TextureBcLevelBytes(std::max(1u, w >> i), std::max(1u, h >> i), data.dxgiFormat)));
                    if (!in.read(reinterpret_cast<char*>(level.data()), std::streamsize(level.size()))) { reason = "truncated payload"; return {}; }
                }
                return data;
            }
            uint64_t expected = 0;
            for (uint32_t i = 0; i < mips; ++i) expected += uint64_t(std::max(1u, w >> i)) * std::max(1u, h >> i) * 4;
            if (payloadSize != expected || length != headerSize + payloadSize) { reason = "payload or file size is not exact"; return {}; }
            if (uint64_t(w) * h * 4 > maxLevelBytes) { reason = "level 0 is too large to upload"; return {}; }
            TextureData data{w, h, scale, type, 0, std::string(keyLength, '\0'), resolved->modId, resolved->path, {}};
            if (!in.read(data.key.data(), keyLength)) { reason = "truncated key"; return {}; }
            data.levels.reserve(fullChain);
            for (uint32_t i = 0; i < fullChain; ++i) {
                const uint32_t lw = std::max(1u, w >> i), lh = std::max(1u, h >> i);
                if (i >= mips) { data.levels.push_back(HalveRgba8(data.levels.back(), std::max(1u, w >> (i - 1)), std::max(1u, h >> (i - 1)))); continue; }
                auto& level = data.levels.emplace_back(size_t(lw) * lh * 4);
                if (!in.read(reinterpret_cast<char*>(level.data()), std::streamsize(level.size()))) { reason = "truncated payload"; return {}; }
            }
            return data;
        };
        auto data = decode();
        if (skipped) {
            if (blockCompressedSkipped) *blockCompressedSkipped = true;
            return {};
        }
        if (!data) {
            const auto path = resolved->path.generic_u8string();
            std::string message = "invalid texture replacement, using original: " + std::string(path.begin(), path.end()) +
                " (" + (reason ? reason : "read failed") + ")";
            if (error) *error = std::move(message);
            else std::fprintf(stderr, "[mods] %s\n", message.c_str());
        }
        return data;
    } catch (...) { return {}; }
}
}
