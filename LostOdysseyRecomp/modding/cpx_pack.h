#pragma once
#include <algorithm>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

// Packing rebuilt archive members as CPX, the format gpu/shader/cpx_decode.h
// reads and the game decodes. Each 64 KiB block is LZ-compressed with the
// header modes the discs use for some blocks (lengths of 2 or 9 bits, offsets
// of 8 or 16 bits) or stored when that is not smaller. Greedy matching over a
// hash of 3 bytes with a short chain.
//
// The game decodes in place: the stream sits at buffer + decoded + reserve -
// stored (header byte 3 is the reserve in sectors; sub_8284E6B8, sub_828571B8)
// and the output grows from the buffer's start a block at a time, so a block's
// output must not reach the next block's bytes. The discs' streams need no more
// than that (their reserves cover it and nothing finer), and packing sets the
// reserve to cover it.
namespace modding::cpx_pack
{
constexpr size_t kBlockSize = 65536;

namespace detail
{
// Big-endian words of `width` bits, as cpx_decode's Bits reads them.
struct BitWriter
{
    unsigned width;
    std::vector<uint8_t> &out;
    uint32_t word = 0;
    unsigned used = 0;
    void Put(uint32_t value, unsigned count)
    {
        while (count--)
        {
            word = word << 1 | (value >> count & 1);
            if (++used == width)
            {
                if (width == 16) out.push_back(uint8_t(word >> 8));
                out.push_back(uint8_t(word));
                word = 0;
                used = 0;
            }
        }
    }
    void Finish() { if (used) Put(0, width - used); }
};
}

// A compressed block: 4 header bytes, then the bit stream. width: the file's
// bit width (header byte 4; 16 on every disc file).
inline std::vector<uint8_t> PackBlock(std::span<const uint8_t> data, unsigned width)
{
    constexpr unsigned kShort = 8, kMaxLength = 514, kChain = 16;
    if (data.empty() || data.size() > kBlockSize || (width != 8 && width != 16)) throw std::runtime_error("bad CPX block");
    // Length mode 1, offset mode 1, order 0; the short offset width in the low nibble.
    std::vector<uint8_t> out{uint8_t(1 << 6 | 1 << 4), uint8_t(kShort), uint8_t((data.size() - 1) & 0xff),
                             uint8_t((data.size() - 1) >> 8)};
    out.reserve(data.size() + data.size() / 8 + 8);
    detail::BitWriter bits{width, out};
    std::vector<int32_t> head(1 << 15, -1), previous(data.size(), -1);
    const auto hash = [&](size_t at) {
        return (uint32_t(data[at]) << 16 | uint32_t(data[at + 1]) << 8 | data[at + 2]) * 2654435761u >> 17;
    };
    const auto insert = [&](size_t at) {
        if (at + 3 > data.size()) return;
        const auto h = hash(at);
        previous[at] = head[h];
        head[h] = int32_t(at);
    };
    size_t at = 0;
    while (at < data.size())
    {
        size_t length = 0, offset = 0;
        if (at + 3 <= data.size())
        {
            const size_t limit = std::min<size_t>(kMaxLength, data.size() - at);
            int32_t candidate = head[hash(at)];
            for (unsigned chain = 0; candidate >= 0 && chain < kChain; ++chain, candidate = previous[size_t(candidate)])
            {
                size_t n = 0;
                while (n < limit && data[size_t(candidate) + n] == data[at + n]) ++n;
                if (n > length)
                {
                    length = n;
                    offset = at - size_t(candidate) - 1; // within a block: at most 65535
                    if (n == limit) break;
                }
            }
        }
        if (length >= 3)
        {
            bits.Put(1, 1);
            if (offset < (1u << kShort)) { bits.Put(0, 1); bits.Put(uint32_t(offset), kShort); }
            else { bits.Put(1, 1); bits.Put(uint32_t(offset), 16); }
            if (length - 3 < 4) { bits.Put(0, 1); bits.Put(uint32_t(length - 3), 2); }
            else { bits.Put(1, 1); bits.Put(uint32_t(length - 3), 9); }
            for (size_t i = 0; i < length; ++i) insert(at + i);
            at += length;
        }
        else
        {
            bits.Put(0, 1);
            bits.Put(data[at], 8);
            insert(at);
            ++at;
        }
    }
    bits.Finish();
    return out;
}

// payload as a CPX file. Header bytes 4-5 (bit width) come from the original
// member; byte 3, the reserve, is the original's or what in-place decoding
// needs, whichever is larger. compress false stores every block, as the discs
// store incompressible data.
inline std::vector<uint8_t> Pack(std::span<const uint8_t> payload, std::span<const uint8_t> original, uint64_t sector,
                                 bool compress = true)
{
    const size_t blocks = (payload.size() + kBlockSize - 1) / kBlockSize;
    if (!blocks || blocks > 0xffff || original.size() < 16) throw std::runtime_error("file too large for CPX");
    std::vector<uint8_t> out(16 + 4 * blocks, 0);
    std::copy_n("cpx", 3, out.begin());
    std::copy_n(original.begin() + 3, 3, out.begin() + 3);
    out[6] = uint8_t(blocks);
    out[7] = uint8_t(blocks >> 8);
    const unsigned width = (original[4] & 0xf0) == 0x10 ? 8 : 16;
    auto put32 = [&](size_t at, uint32_t value) { for (int i = 0; i < 4; ++i) out[at + size_t(i)] = uint8_t(value >> (8 * i)); };
    for (size_t i = 0; i < blocks; ++i)
    {
        const auto chunk = payload.subspan(i * kBlockSize, std::min(kBlockSize, payload.size() - i * kBlockSize));
        put32(16 + 4 * i, uint32_t(out.size()));
        auto packed = compress ? PackBlock(chunk, width) : std::vector<uint8_t>{};
        if (compress && packed.size() < chunk.size() + 4)
            out.insert(out.end(), packed.begin(), packed.end());
        else
        {
            out.insert(out.end(), {0xff, 0x00, uint8_t((chunk.size() - 1) & 0xff), uint8_t((chunk.size() - 1) >> 8)});
            out.insert(out.end(), chunk.begin(), chunk.end());
        }
    }
    put32(8, uint32_t(out.size()));
    put32(12, uint32_t(payload.size()));
    // How far a block's output end passes the start of the next block's bytes
    // (the stream's end after the last), with the stream at decoded - stored.
    int64_t ahead = 0;
    for (size_t i = 0; i < blocks; ++i)
    {
        const auto end = int64_t(std::min((i + 1) * kBlockSize, payload.size()));
        const auto next = int64_t(i + 1 < blocks ? uint32_t(out[20 + 4 * i]) | uint32_t(out[21 + 4 * i]) << 8 |
                                                       uint32_t(out[22 + 4 * i]) << 16 | uint32_t(out[23 + 4 * i]) << 24
                                                 : uint32_t(out.size()));
        ahead = std::max(ahead, end - next);
    }
    // One sector more than that.
    const int64_t need = ahead - int64_t(payload.size()) + int64_t(out.size());
    const auto reserve = uint64_t(std::max<int64_t>(need, 0) + int64_t(sector) - 1) / sector + 1;
    if (reserve > 0xff)
    {
        if (compress) return Pack(payload, original, sector, false);
        throw std::runtime_error("file too large for CPX");
    }
    out[3] = std::max(out[3], uint8_t(reserve));
    return out;
}
}
