#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>

// Guest memory writes that the renderer can otherwise only notice by re-reading
// memory: kernel file reads into physical memory, and vertex and index buffers
// the title's D3D runtime finishes writing (patches/resource_write_notes.cpp).
// Each 64 KiB block of the 512 MiB physical address space keeps the generation
// of the last such write; the texture, vertex and index caches compare them to
// decide whether cached guest bytes need another look.
namespace gpu::guest_writes
{
    inline constexpr uint32_t kPhysicalBase = 0xA0000000u; // PageAllocator physical region
    inline constexpr uint32_t kPhysicalSize = 0x20000000u;
    inline constexpr uint32_t kBlockShift = 16;
    inline constexpr uint32_t kBlocks = kPhysicalSize >> kBlockShift;

    inline std::atomic<uint32_t> g_counter{0};
    // Notes whose blocks are all stored. Read before the blocks: a reader that
    // saw this count also sees every block those notes wrote.
    inline std::atomic<uint32_t> g_published{0};
    inline std::atomic<uint32_t> g_blocks[kBlocks]{};

    // MmGetPhysicalAddress: the A, C and E views alias physical memory; E sits
    // one 4 KiB page later.
    inline uint32_t PhysicalOf(uint32_t guestAddress)
    {
        return (guestAddress & 0x1FFFFFFFu) + (guestAddress >= 0xE0000000u ? 0x1000u : 0u);
    }

    // After the bytes are in memory and before the guest can see the write
    // complete (or draw from it).
    inline void NotePhysical(uint32_t physical, uint32_t size)
    {
        if (!size || physical >= kPhysicalSize)
            return;
        const uint32_t last = uint32_t(std::min<uint64_t>(uint64_t(physical) + size - 1, kPhysicalSize - 1));
        const uint32_t generation = g_counter.fetch_add(1, std::memory_order_relaxed) + 1;
        // Concurrent notes may finish out of order; a block never goes back.
        for (uint32_t block = physical >> kBlockShift; block <= last >> kBlockShift; ++block) {
            uint32_t current = g_blocks[block].load(std::memory_order_relaxed);
            while (current < generation &&
                !g_blocks[block].compare_exchange_weak(current, generation, std::memory_order_release, std::memory_order_relaxed)) {}
        }
        g_published.fetch_add(1, std::memory_order_release);
    }

    // guestAddress is virtual; only the physical views count.
    inline void Note(uint32_t guestAddress, uint32_t size)
    {
        if (guestAddress >= kPhysicalBase)
            NotePhysical(PhysicalOf(guestAddress), size);
    }

    inline uint32_t Published() { return g_published.load(std::memory_order_acquire); }

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

    // Physical extent of the title's D3D command buffer, [begin, end) packed as
    // begin << 32 | end. DrawVerticesUP-style data is copied into it for each
    // draw without a note, so sources there are never cached.
    inline std::atomic<uint64_t> g_commandBuffer{0};

    inline void SetCommandBuffer(uint32_t physical, uint32_t size)
    {
        g_commandBuffer.store((uint64_t(physical) << 32) | (uint64_t(physical) + size), std::memory_order_release);
    }

    inline bool InCommandBuffer(uint32_t physical, uint32_t size)
    {
        const uint64_t extent = g_commandBuffer.load(std::memory_order_acquire);
        return uint64_t(physical) < uint32_t(extent) && uint64_t(physical) + size > (extent >> 32);
    }

    // Reads the extent once the title set its command buffer up
    // (patches/resource_write_notes.cpp).
    void CommandBufferSetUp(uint32_t device, uint8_t* base);
}
