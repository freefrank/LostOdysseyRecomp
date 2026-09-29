#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace gpu::native_command
{
// Process-local native records share the guest ring/IB ordering and ownership.
// Only the discriminator is guest-endian; the payload is host-endian. These are
// NOT PM4 packets or an on-disk format. No host pointers or side-table tokens are
// stored, so an IB may be replayed and its memory may be recycled normally.
static_assert(std::endian::native == std::endian::little);
constexpr uint32_t kRegisters = 0x80004C52;
constexpr uint32_t kIndexedQuad = 0x80004C44;
constexpr uint32_t kRegisterHeaderWords = 4;
constexpr uint32_t kMaxRegisterWords = kRegisterHeaderWords + 64;
constexpr uint32_t kDrawWords = 4;
constexpr uint32_t kRegisterCount = 0x5003;

enum class Mode { Off, Registers, All };
extern const Mode mode;
const char* ModeName();

constexpr uint32_t GuestWord(uint32_t value)
{
    return ((value & 0x000000FFu) << 24) | ((value & 0x0000FF00u) << 8) |
        ((value & 0x00FF0000u) >> 8) | ((value & 0xFF000000u) >> 24);
}

// Bit 63 names first, bit 62 first+1, etc., matching sub_823C1BD8.
constexpr uint32_t ValueCount(uint64_t mask) { return std::popcount(mask); }
constexpr uint32_t RegisterWords(uint64_t mask)
{
    return kRegisterHeaderWords + ValueCount(mask);
}
constexpr uint32_t RegisterSpan(uint64_t mask)
{
    return mask ? 64u - uint32_t(std::countr_zero(mask)) : 0u;
}
constexpr bool RegisterRange(uint32_t first, uint64_t mask)
{
    // Every special WriteRegister side effect (scratch/status/coherence and
    // host-private control registers) is outside this ordinary state range.
    return mask && first >= 0x2000 && first < kRegisterCount &&
        RegisterSpan(mask) <= kRegisterCount - first;
}

template<class Visitor>
void VisitRuns(uint64_t mask, Visitor&& visit)
{
    uint32_t offset = 0, packed = 0;
    while (mask)
    {
        const auto skip = uint32_t(std::countl_zero(mask));
        offset += skip;
        mask <<= skip; // mask is nonzero, hence skip < 64.
        const auto count = uint32_t(std::countl_one(mask));
        visit(offset, count, packed);
        packed += count;
        offset += count;
        mask = count == 64 ? 0 : mask << count;
    }
}

inline uint32_t RunCount(uint64_t mask)
{
    uint32_t count = 0;
    VisitRuns(mask, [&](uint32_t, uint32_t, uint32_t) { ++count; });
    return count;
}

// SDK cursor denotes the last word written, not the next free word. Decline
// before any mutation if a native record would require SDK buffer rollover.
constexpr bool CanAppend(uint32_t cursor, uint32_t limit, uint32_t words)
{
    return cursor >= 0x10000 && !(cursor & 3) && !(limit & 3) && words &&
        uint64_t(cursor) + uint64_t(words) * 4 < uint64_t(limit);
}

// Reads selected guest values before the caller publishes the record/cursor.
// Output storage must not alias the source; the runtime uses a stack buffer.
template<class ReadGuestWord>
bool EncodeRegisters(uint32_t first, uint64_t mask, ReadGuestWord&& read,
    std::span<uint32_t> output)
{
    if (!RegisterRange(first, mask) || output.size() < RegisterWords(mask)) return false;
    output[0] = GuestWord(kRegisters);
    output[1] = first;
    output[2] = uint32_t(mask >> 32);
    output[3] = uint32_t(mask);
    VisitRuns(mask, [&](uint32_t offset, uint32_t count, uint32_t packed) {
        for (uint32_t i = 0; i < count; ++i)
            output[kRegisterHeaderWords + packed + i] = read(offset + i);
    });
    return true;
}

// Command-processor thread only. Check the full record before changing either
// image. Bulk runs avoid a virtual/address-translation/register dispatch per
// word while preserving ReadRegister's guest-MMIO mirror semantics.
inline bool ApplyRegisters(uint32_t first, uint64_t mask,
    std::span<const uint32_t> values, std::span<uint32_t> registers,
    std::span<uint32_t> guestMirror)
{
    if (!RegisterRange(first, mask) || values.size() != ValueCount(mask) ||
        registers.size() < first + RegisterSpan(mask) ||
        guestMirror.size() < first + RegisterSpan(mask)) return false;
    VisitRuns(mask, [&](uint32_t offset, uint32_t count, uint32_t packed) {
        const auto* source = values.data() + packed;
        auto* destination = registers.data() + first + offset;
        auto* mirror = guestMirror.data() + first + offset;
        std::copy_n(source, count, destination);
        for (uint32_t i = 0; i < count; ++i) mirror[i] = GuestWord(source[i]);
    });
    return true;
}

constexpr bool IndexedQuad(uint32_t initiator)
{
    return (initiator & 0x3Fu) == 5 && ((initiator >> 6) & 3u) == 0 &&
        (initiator >> 16) == 6;
}

inline bool EncodeIndexedQuad(uint32_t initiator, uint32_t dmaBase,
    uint32_t dmaSize, std::span<uint32_t> output)
{
    if (!IndexedQuad(initiator) || output.size() < kDrawWords) return false;
    // The removed PM4 DRAW_INDX header is predicated. The consumer must test
    // current bin select/mask BEFORE changing the three DMA registers.
    output[0] = GuestWord(kIndexedQuad);
    output[1] = initiator;
    output[2] = dmaBase;
    output[3] = dmaSize;
    return true;
}
}
