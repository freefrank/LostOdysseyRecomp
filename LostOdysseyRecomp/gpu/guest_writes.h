#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>

// Guest memory writes made outside the command stream that the renderer can
// otherwise only notice by re-reading memory: kernel file reads into physical
// memory. Each 64 KiB block of the 512 MiB physical address space keeps the
// generation of the last such write; the texture cache compares them to decide
// whether a cached texture's guest bytes need another look.
namespace gpu::guest_writes
{
    inline constexpr uint32_t kPhysicalBase = 0xA0000000u; // PageAllocator physical region
    inline constexpr uint32_t kPhysicalSize = 0x20000000u;
    inline constexpr uint32_t kBlockShift = 16;
    inline constexpr uint32_t kBlocks = kPhysicalSize >> kBlockShift;

    inline std::atomic<uint32_t> g_counter{0};
    inline std::atomic<uint32_t> g_blocks[kBlocks]{};

    // After the bytes are in memory and before the guest can see the write
    // complete. guestAddress is virtual; only the physical region counts.
    inline void Note(uint32_t guestAddress, uint32_t size)
    {
        if (!size || guestAddress < kPhysicalBase || guestAddress - kPhysicalBase >= kPhysicalSize)
            return;
        const uint32_t first = guestAddress - kPhysicalBase;
        const uint32_t last = uint32_t(std::min<uint64_t>(uint64_t(first) + size - 1, kPhysicalSize - 1));
        const uint32_t generation = g_counter.fetch_add(1, std::memory_order_relaxed) + 1;
        for (uint32_t block = first >> kBlockShift; block <= last >> kBlockShift; ++block)
            g_blocks[block].store(generation, std::memory_order_release);
    }

    // Newest write generation in [physical, physical + size), 0 if none.
    inline uint32_t Newest(uint32_t physical, uint32_t size)
    {
        if (!size || physical >= kPhysicalSize)
            return 0;
        const uint32_t last = uint32_t(std::min<uint64_t>(uint64_t(physical) + size - 1, kPhysicalSize - 1));
        uint32_t newest = 0;
        for (uint32_t block = physical >> kBlockShift; block <= last >> kBlockShift; ++block)
            newest = std::max(newest, g_blocks[block].load(std::memory_order_acquire));
        return newest;
    }
}
