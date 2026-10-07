#pragma once
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace gpu {
// Match ReadRegister's zero-value MMIO fallback. Callers run on the command
// processor thread and keep the same register order; this is not a cache.
template<class MmioValue>
inline void CopyRegisterSnapshot(std::span<const uint32_t> registers,
    uint32_t first, std::span<uint32_t> output, const MmioValue* mmio)
{
    const size_t count = first < registers.size()
        ? std::min(output.size(), registers.size() - first) : 0;
    const auto* source = count ? registers.data() + first : nullptr;
    for (size_t i = 0; i < count; ++i)
        output[i] = source[i] ? source[i] : uint32_t(mmio[i]);
    std::fill(output.begin() + count, output.end(), 0);
}

constexpr uint32_t SwapGuestWord(uint32_t word)
{
    return (word >> 24) | ((word >> 8) & 0xFF00u) | ((word << 8) & 0xFF0000u) | (word << 24);
}

// Renderer copy of one ALU constant bank with ReadRegister's zero-value MMIO
// fallback. Command writes mark 16-word blocks dirty; between them only the
// zero-register words can change, through direct guest MMIO stores.
struct ConstantBankSnapshot
{
    static constexpr size_t kWords = 1024, kBlockWords = 16;
    uint32_t values[kWords]{};
    uint32_t mmio[kWords]{}; // guest-endian MMIO image at the last check
    uint64_t zero[kWords / 64]{};
    uint64_t zeroBlocks = 0;
    bool valid = false;
};

// Re-reads the dirty blocks (all of them for a new snapshot), then the
// zero-register words whose MMIO word changed. Returns the blocks re-read or
// changed. `registers` and guest-endian `mmio` cover the bank.
inline uint64_t UpdateConstantBankSnapshot(ConstantBankSnapshot& s, const uint32_t* registers,
    const uint32_t* mmio, uint64_t dirty)
{
    constexpr size_t kBlock = ConstantBankSnapshot::kBlockWords;
    if (!s.valid) { dirty = ~0ull; s.valid = true; }
    uint64_t changed = dirty;
    for (; dirty; dirty &= dirty - 1) {
        const size_t block = size_t(std::countr_zero(dirty)), first = block * kBlock;
        uint64_t zero = 0;
        for (size_t i = 0; i < kBlock; ++i) {
            const uint32_t value = registers[first + i], word = mmio[first + i];
            s.values[first + i] = value ? value : SwapGuestWord(word);
            s.mmio[first + i] = word;
            zero |= uint64_t(value == 0) << i;
        }
        uint64_t& bits = s.zero[first / 64];
        bits = (bits & ~(uint64_t(0xFFFF) << (first % 64))) | (zero << (first % 64));
        s.zeroBlocks = zero ? s.zeroBlocks | (1ull << block) : s.zeroBlocks & ~(1ull << block);
    }
    for (uint64_t blocks = s.zeroBlocks & ~changed; blocks; blocks &= blocks - 1) {
        const size_t block = size_t(std::countr_zero(blocks)), first = block * kBlock;
        uint32_t diff = 0;
        for (size_t i = first; i < first + kBlock; ++i) diff |= s.mmio[i] ^ mmio[i];
        if (!diff) continue;
        for (size_t i = first; i < first + kBlock; ++i) {
            if (s.mmio[i] == mmio[i]) continue;
            s.mmio[i] = mmio[i];
            const uint32_t value = SwapGuestWord(mmio[i]);
            if (((s.zero[i / 64] >> (i % 64)) & 1) && s.values[i] != value) {
                s.values[i] = value;
                changed |= 1ull << block;
            }
        }
    }
    return changed;
}

inline bool CanReuseUploadedConstants(uint64_t uploadedOffset, uint64_t uploadedVersion,
    uint64_t snapshotVersion)
{
    return snapshotVersion != 0 && uploadedOffset != UINT64_MAX && uploadedVersion == snapshotVersion;
}
}
