#pragma once

#ifndef _WIN32
#define MEM_COMMIT  0x00001000
#define MEM_RESERVE 0x00002000
#endif

// The whole 4 GiB guest address space is one contiguous host range. Guest pointers are
// offsets from `base`; the recompiled function lookup table lives right after the
// XEX image (see PPC_LOOKUP_FUNC in ppc_context.h).
struct Memory
{
    uint8_t* base{};

    Memory();

    bool IsInMemoryRange(const void* host) const noexcept
    {
        return host >= base && host < (base + PPC_MEMORY_SIZE);
    }

    void* Translate(size_t offset) const noexcept
    {
        if (offset)
            assert(offset < PPC_MEMORY_SIZE);

        return base + offset;
    }

    uint32_t MapVirtual(const void* host) const noexcept
    {
        if (host)
            assert(IsInMemoryRange(host));

        return static_cast<uint32_t>(static_cast<const uint8_t*>(host) - base);
    }

    PPCFunc* FindFunction(uint32_t guest) const noexcept
    {
        return PPC_LOOKUP_FUNC(base, guest);
    }

    void InstallFunctionTracers();
    void InsertFunction(uint32_t guest, PPCFunc* host)
    {
        PPC_LOOKUP_FUNC(base, guest) = host;
    }
};

extern "C" void* MmGetHostAddress(uint32_t ptr);
extern Memory g_memory;

// Page-granular allocator over guest address ranges, used to back
// NtAllocateVirtualMemory / MmAllocatePhysicalMemoryEx so the game's own
// heap implementation (statically linked xapilib/libcmt) runs unmodified.
struct PageAllocator
{
    static constexpr uint32_t PAGE_SIZE = 0x1000;

    struct Region
    {
        uint32_t begin;
        uint32_t end;
        std::vector<uint8_t> used;
        std::vector<std::pair<uint32_t, uint32_t>> quarantine; // freed (address, size) awaiting reuse // one byte per page
        Mutex mutex;
    };

    Region virtualRegion;   // 0x00100000 .. 0x7F000000
    Region physicalRegion;  // 0xA0000000 .. 0xC0000000

    void Init();

    // Returns guest address or 0. `baseAddress` is a hint; when non-zero the
    // exact address is required (honouring the guest's expectations).
    uint32_t Alloc(Region& region, uint32_t size, uint32_t alignment, uint32_t baseAddress = 0);
    bool Free(Region& region, uint32_t address, uint32_t size = 0);
    uint32_t AllocationSize(Region& region, uint32_t address);

    // Locate the allocation containing `address`. Returns false when the
    // address is not inside any live allocation.
    bool FindAllocation(Region& region, uint32_t address, uint32_t& base, uint32_t& size);
    bool Contains(const Region& region, uint32_t address) const
    {
        return address >= region.begin && address < region.end;
    }

private:
    ankerl::unordered_dense::map<uint32_t, uint32_t> m_allocationSizes; // address -> byte size
    Mutex m_sizesMutex;
};

extern PageAllocator g_pageAllocator;
