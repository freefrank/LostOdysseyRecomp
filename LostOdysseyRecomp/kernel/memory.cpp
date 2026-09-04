#include <stdafx.h>
#include "memory.h"
#include <os/logger.h>
#include <set>
#include <mutex>

Memory g_memory;
PageAllocator g_pageAllocator;

// Called through the function table for addresses without recompiled code.
static void MissingFunction(PPCContext& ctx, uint8_t* base)
{
    static std::mutex mutex;
    static std::set<uint32_t> seen;
    std::lock_guard lock(mutex);
    if (seen.insert(ctx.ctr.u32).second)
        LOG_ERROR("call to unrecompiled guest function ctr={:#x} lr={:#x} r3={:#x} r4={:#x}", ctx.ctr.u32, uint32_t(ctx.lr), ctx.r3.u32, ctx.r4.u32);
    ctx.r3.u64 = 0;
}

Memory::Memory()
{
#ifdef _WIN32
    base = (uint8_t*)VirtualAlloc((void*)0x100000000ull, PPC_MEMORY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (base == nullptr)
        base = (uint8_t*)VirtualAlloc(nullptr, PPC_MEMORY_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (base == nullptr)
        return;

    DWORD oldProtect;
    VirtualProtect(base, 4096, PAGE_NOACCESS, &oldProtect);
#else
    base = (uint8_t*)mmap((void*)0x100000000ull, PPC_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

    if (base == (uint8_t*)MAP_FAILED)
        base = (uint8_t*)mmap(NULL, PPC_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

    if (base == nullptr)
        return;

    mprotect(base, 4096, PROT_NONE);
#endif

    // Every code address the recompiler did not emit a function for gets a
    // logging stub instead of a null pointer, so a virtual call into a missed
    // function reports the guest address instead of jumping to host 0.
    for (uint32_t guest = PPC_CODE_BASE; guest < PPC_CODE_BASE + PPC_CODE_SIZE; guest += 4)
        InsertFunction(guest, MissingFunction);

    for (size_t i = 0; PPCFuncMappings[i].guest != 0; i++)
    {
        if (PPCFuncMappings[i].host != nullptr)
            InsertFunction(PPCFuncMappings[i].guest, PPCFuncMappings[i].host);
    }
}

void* MmGetHostAddress(uint32_t ptr)
{
    return g_memory.Translate(ptr);
}

// ---------------------------------------------------------------------------

void PageAllocator::Init()
{
    // Everything below the image and the function table stays free for the
    // guest's virtual allocations; the host-side heap (o1heap) takes the very
    // top of the virtual region, see heap.cpp.
    virtualRegion.begin = 0x00100000;
    virtualRegion.end = 0x7C000000;
    virtualRegion.used.assign((virtualRegion.end - virtualRegion.begin) / PAGE_SIZE, 0);

    physicalRegion.begin = 0xA0000000;
    physicalRegion.end = 0xFFF00000;
    physicalRegion.used.assign((physicalRegion.end - physicalRegion.begin) / PAGE_SIZE, 0);
}

uint32_t PageAllocator::Alloc(Region& region, uint32_t size, uint32_t alignment, uint32_t baseAddress)
{
    if (size == 0)
        size = PAGE_SIZE;

    size = RoundUp(size, PAGE_SIZE);
    alignment = std::max<uint32_t>(RoundUp(alignment, PAGE_SIZE), PAGE_SIZE);

    const uint32_t pageCount = size / PAGE_SIZE;
    const uint32_t alignPages = alignment / PAGE_SIZE;

    std::lock_guard lock(region.mutex);

    auto tryRange = [&](uint32_t firstPage) -> bool
    {
        if (firstPage + pageCount > region.used.size())
            return false;
        for (uint32_t i = 0; i < pageCount; i++)
            if (region.used[firstPage + i])
                return false;
        return true;
    };

    uint32_t page = UINT32_MAX;

    if (baseAddress != 0)
    {
        if (!Contains(region, baseAddress))
            return 0;
        uint32_t p = (baseAddress - region.begin) / PAGE_SIZE;
        if (tryRange(p))
            page = p;
        else
            return 0;
    }
    else
    {
        // First fit, scanning from the low end so the guest sees a compact heap.
        for (uint32_t p = 0; p + pageCount <= region.used.size(); p += alignPages)
        {
            if (tryRange(p))
            {
                page = p;
                break;
            }
        }
        if (page == UINT32_MAX)
            return 0;
    }

    for (uint32_t i = 0; i < pageCount; i++)
        region.used[page + i] = 1;

    uint32_t address = region.begin + page * PAGE_SIZE;
    {
        std::lock_guard sizeLock(m_sizesMutex);
        m_allocationSizes[address] = size;
    }
    memset(g_memory.Translate(address), 0, size);
    return address;
}

bool PageAllocator::Free(Region& region, uint32_t address, uint32_t size)
{
    if (!Contains(region, address))
        return false;

    if (size == 0)
    {
        std::lock_guard sizeLock(m_sizesMutex);
        auto it = m_allocationSizes.find(address);
        if (it == m_allocationSizes.end())
            return false;
        size = it->second;
        m_allocationSizes.erase(it);
    }
    else
    {
        std::lock_guard sizeLock(m_sizesMutex);
        m_allocationSizes.erase(address);
    }

    std::lock_guard lock(region.mutex);
    uint32_t first = (address - region.begin) / PAGE_SIZE;
    uint32_t count = RoundUp(size, PAGE_SIZE) / PAGE_SIZE;
    for (uint32_t i = 0; i < count && first + i < region.used.size(); i++)
        region.used[first + i] = 0;
    return true;
}

uint32_t PageAllocator::AllocationSize(Region& region, uint32_t address)
{
    std::lock_guard sizeLock(m_sizesMutex);
    auto it = m_allocationSizes.find(address);
    return it != m_allocationSizes.end() ? it->second : 0;
}

bool PageAllocator::FindAllocation(Region& region, uint32_t address, uint32_t& base, uint32_t& size)
{
    if (!Contains(region, address))
        return false;

    std::lock_guard sizeLock(m_sizesMutex);
    for (auto& [b, s] : m_allocationSizes)
    {
        if (address >= b && address < b + s)
        {
            base = b;
            size = s;
            return true;
        }
    }
    return false;
}
