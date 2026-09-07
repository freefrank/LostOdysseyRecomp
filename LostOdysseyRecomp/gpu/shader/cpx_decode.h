#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

namespace xenos::resources::cpx {

// The largest package in the four-disc inventory decodes to 41,909,681 bytes.
inline constexpr size_t kMaxDecodedSize = 128 * 1024 * 1024;
inline constexpr size_t kMaxStoredSize = 128 * 1024 * 1024;
inline constexpr size_t kBlockSize = 65536;

struct Header {
    uint32_t storedSize = 0;
    uint32_t decodedSize = 0;
    uint16_t blockCount = 0;
    size_t headerSize = 0;
    unsigned bitWidth = 0;
};

namespace detail {
inline uint32_t ReadLE32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
inline void Require(bool value) {
    if (!value) throw std::runtime_error("invalid CPX stream");
}

struct Bits {
    std::span<const uint8_t> data;
    size_t pos = 0;
    unsigned width, left = 0, valid = 0, value = 0;

    Bits(std::span<const uint8_t> input, unsigned bits) : data(input), width(bits) { Refill(); }

    void Refill() {
        value = 0;
        valid = 0;
        left = width;
        for (unsigned i = 0; i < width / 8; ++i) {
            value <<= 8;
            if (pos < data.size()) {
                value |= data[pos];
                valid += 8;
            }
            ++pos;
        }
    }

    unsigned Get(unsigned count) {
        Require(count <= 16);
        unsigned result = 0;
        while (count) {
            const auto take = std::min(count, left);
            Require(width - left + take <= valid);
            left -= take;
            result = (result << take) | ((value >> left) & ((1u << take) - 1));
            count -= take;
            // The original codec prefetches immediately, including after the
            // last requested bit. DirectWord must start beyond that prefetch.
            if (!left) Refill();
        }
        return result;
    }

    unsigned DirectWord() {
        Require(pos <= data.size() && data.size() - pos >= 2);
        const auto result = unsigned(data[pos]) << 8 | data[pos + 1];
        pos += 2;
        return result;
    }
};

inline void DecodeBlock(std::span<const uint8_t> data, unsigned width, std::span<uint8_t> output) {
    if (data[0] == 255) {
        std::memcpy(output.data(), data.data() + 4, output.size());
        return;
    }
    const unsigned lengthMode = data[0] >> 6;
    const unsigned offsetMode = data[0] >> 4 & 3;
    unsigned widths[] = {unsigned(data[1] & 15), unsigned(data[1] >> 4), 0};
    if (offsetMode < 3) widths[offsetMode] = 16;
    constexpr unsigned orders[][3] = {{0, 1, 2}, {1, 0, 2}, {2, 0, 1}, {2, 2, 2}};
    const auto* order = orders[data[0] >> 2 & 3];
    Bits bits(data.subspan(4), width);
    size_t pos = 0;
    while (pos < output.size()) {
        if (!bits.Get(1)) {
            output[pos++] = uint8_t(bits.Get(8));
            continue;
        }
        unsigned offset = 0;
        if (offsetMode == 0) {
            offset = width == 16 ? bits.DirectWord() : bits.Get(16);
        } else if (offsetMode == 1) {
            offset = bits.Get(widths[bits.Get(1)]);
        } else if (offsetMode == 2) {
            const auto choice = !bits.Get(1) ? order[0] : order[1 + bits.Get(1)];
            offset = bits.Get(widths[choice]);
        }
        unsigned lengthBits = 9;
        if (lengthMode == 1 && !bits.Get(1)) lengthBits = 2;
        else if (lengthMode == 2 && !bits.Get(1)) lengthBits = 3;
        const auto length = bits.Get(lengthBits) + 3;
        Require(offset + 1 <= pos && length <= output.size() - pos);
        // Overlap is intentional: repeated output is valid LZ input.
        for (unsigned i = 0; i < length; ++i, ++pos) output[pos] = output[pos - offset - 1];
    }
}
} // namespace detail

// Accepts the fixed 16-byte prefix for bounded file reads. Decode additionally
// checks the complete offset table and every block before allocating output.
// Byte 3 is a loader reserve-size field, so it is not required to be NUL.
inline bool ReadHeader(std::span<const uint8_t> data, Header& output) noexcept {
    output = {};
    if (data.size() < 16 || std::memcmp(data.data(), "cpx", 3) != 0) return false;
    Header header;
    header.storedSize = detail::ReadLE32(data.data() + 8);
    header.decodedSize = detail::ReadLE32(data.data() + 12);
    header.blockCount = uint16_t(data[6]) | uint16_t(data[7]) << 8;
    header.headerSize = 16 + 4 * size_t(header.blockCount);
    header.bitWidth = (data[4] & 0xf0) == 0x10 ? 8 : 16;
    if (!header.decodedSize || header.decodedSize > kMaxDecodedSize || !header.blockCount ||
        header.blockCount != (uint64_t(header.decodedSize) + kBlockSize - 1) / kBlockSize ||
        header.storedSize > kMaxStoredSize ||
        header.storedSize < header.headerSize + 4 * size_t(header.blockCount)) return false;
    output = header;
    return true;
}

// Failure clears output. Input may alias output: decoding is transactional and
// swaps in the complete result only after every block has succeeded.
inline bool Decode(std::span<const uint8_t> data, std::vector<uint8_t>& output) noexcept {
    try {
        Header header;
        detail::Require(ReadHeader(data, header) && header.storedSize == data.size());
        detail::Require(header.headerSize <= data.size() &&
                        detail::ReadLE32(data.data() + 16) == header.headerSize);
        size_t decoded = 0;
        for (size_t i = 0; i < header.blockCount; ++i) {
            const size_t begin = detail::ReadLE32(data.data() + 16 + 4 * i);
            const size_t end = i + 1 < header.blockCount ?
                detail::ReadLE32(data.data() + 20 + 4 * i) : header.storedSize;
            detail::Require(begin >= header.headerSize && begin < end && end <= data.size() && end - begin >= 4);
            const auto* block = data.data() + begin;
            const size_t count = (unsigned(block[2]) | unsigned(block[3]) << 8) + 1;
            detail::Require(count == std::min(kBlockSize, size_t(header.decodedSize) - decoded));
            if (block[0] == 255) detail::Require(block[1] == 0 && end - begin - 4 >= count);
            decoded += count;
        }
        detail::Require(decoded == header.decodedSize);
        std::vector<uint8_t> result(header.decodedSize);
        decoded = 0;
        for (size_t i = 0; i < header.blockCount; ++i) {
            const size_t begin = detail::ReadLE32(data.data() + 16 + 4 * i);
            const size_t end = i + 1 < header.blockCount ?
                detail::ReadLE32(data.data() + 20 + 4 * i) : header.storedSize;
            const size_t count = std::min(kBlockSize, size_t(header.decodedSize) - decoded);
            detail::DecodeBlock(data.subspan(begin, end - begin), header.bitWidth,
                                std::span<uint8_t>(result).subspan(decoded, count));
            decoded += count;
        }
        output.swap(result);
        return true;
    } catch (...) {
        output.clear();
        return false;
    }
}

} // namespace xenos::resources::cpx
