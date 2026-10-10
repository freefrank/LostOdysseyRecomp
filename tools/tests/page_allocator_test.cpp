#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <byteswap.h>
#include <mutex.h>

namespace ankerl::unordered_dense
{
template<class K, class V> using map = std::unordered_map<K, V>;
}

// These tests use a small virtual/physical range; the real allocator still
// performs page accounting and zeroing through the normal Translate call.
struct GuestStorage
{
    std::array<uint8_t, 0x50000> bytes{};
    void* Translate(size_t offset)
    {
        assert(offset < bytes.size());
        return bytes.data() + offset;
    }
} g_memory;

constexpr uint32_t STATUS_SUCCESS = 0;
constexpr uint32_t STATUS_INVALID_PARAMETER = 0xC000000D;
constexpr uint32_t STATUS_NO_MEMORY = 0xC0000017;
#define LOG_KERNEL(...) ((void)0)
#define LOG_ERROR(...) ((void)0)
// Desktop commit is a no-op (guest_address_space.h); only Switch backs pages here.
namespace GuestAddressSpace { inline bool Commit(uint32_t, uint32_t) { return true; } }

// PRODUCTION_DEFINITIONS

static void Check(bool value)
{
    if (!value)
    {
        std::fputs("guest allocation regression failed\n", stderr);
        std::abort();
    }
}

static void Initialize(PageAllocator::Region& region, uint32_t begin = 0x1000)
{
    region.begin = begin;
    region.end = begin + 0x20000;
    region.used.assign(32, 0);
}

static void CheckRejectedUnchanged(PageAllocator& allocator, PageAllocator::Region& region,
    uint32_t size, uint32_t alignment, uint32_t baseAddress = 0)
{
    const auto pages = region.used;
    const auto quarantine = region.quarantine;
    std::vector<uint32_t> sizes;
    for (uint32_t address = region.begin; address < region.end; address += PageAllocator::PAGE_SIZE)
        sizes.push_back(allocator.AllocationSize(region, address));
    Check(allocator.Alloc(region, size, alignment, baseAddress) == 0);
    Check(region.used == pages && region.quarantine == quarantine);
    size_t index = 0;
    for (uint32_t address = region.begin; address < region.end; address += PageAllocator::PAGE_SIZE)
        Check(allocator.AllocationSize(region, address) == sizes[index++]);
}

int main()
{
    PageAllocator allocator;
    auto& region = allocator.virtualRegion;
    Initialize(region);
    std::fill(g_memory.bytes.begin(), g_memory.bytes.end(), 0xA5);
    const auto first = allocator.Alloc(region, 0x1000, 0x1000);
    Check(first == region.begin && allocator.AllocationSize(region, first) == 0x1000);
    Check(g_memory.bytes[first] == 0 && g_memory.bytes[first + 0xFFF] == 0);
    Check(g_memory.bytes[first + 0x1000] == 0xA5);

    CheckRejectedUnchanged(allocator, region, 0xFFFFF001, 0x1000);
    CheckRejectedUnchanged(allocator, region, UINT32_MAX, 0x1000);
    CheckRejectedUnchanged(allocator, region, 0xFFFFF000, 0x1000);
    CheckRejectedUnchanged(allocator, region, 0x1000, UINT32_MAX);
    CheckRejectedUnchanged(allocator, region, 0x20001, 0x1000);
    CheckRejectedUnchanged(allocator, region, 0x1000, 0x1000, region.end);
    const auto second = allocator.Alloc(region, 0x1001, 0x1000);
    Check(second == first + 0x1000 && allocator.AllocationSize(region, second) == 0x2000);
    const auto zeroSize = allocator.Alloc(region, 0, 0);
    Check(zeroSize == second + 0x2000 && allocator.AllocationSize(region, zeroSize) == 0x1000);

    // An impossible request must not free quarantined pages as a side effect.
    std::fill(region.used.begin(), region.used.end(), 1);
    region.quarantine.push_back({region.begin + 0x1F000, 0x1000});
    CheckRejectedUnchanged(allocator, region, 0x20001, 0x1000);
    CheckRejectedUnchanged(allocator, region, 0xFFFFF001, 0x1000);
    CheckRejectedUnchanged(allocator, region, 0x1000, UINT32_MAX);

    Initialize(g_pageAllocator.virtualRegion);
    Initialize(g_pageAllocator.physicalRegion, 0x22000);
    be<uint32_t> base = 0x1FFF, size = 0xFFFFF002;
    const auto pagesBefore = g_pageAllocator.virtualRegion.used;
    Check(NtAllocateVirtualMemory(&base, &size, 0x3000, 0, 0) == STATUS_INVALID_PARAMETER);
    Check(uint32_t(base) == 0x1FFF && uint32_t(size) == 0xFFFFF002);
    Check(g_pageAllocator.virtualRegion.used == pagesBefore);
    Check(g_pageAllocator.AllocationSize(g_pageAllocator.virtualRegion, 0x1000) == 0);

    // A 64 KiB page can overflow at a different offset from a normal page.
    base = 0x1FFFF;
    size = 0xFFFF0002;
    Check(NtAllocateVirtualMemory(&base, &size, X_MEM_LARGE_PAGES, 0, 0) == STATUS_INVALID_PARAMETER);
    Check(uint32_t(base) == 0x1FFFF && uint32_t(size) == 0xFFFF0002);
    Check(g_pageAllocator.virtualRegion.used == pagesBefore);
    Check(NtAllocateVirtualMemory(nullptr, &size, 0x3000, 0, 0) == STATUS_INVALID_PARAMETER);
    Check(NtAllocateVirtualMemory(&base, nullptr, 0x3000, 0, 0) == STATUS_INVALID_PARAMETER);

    base = 0;
    size = 0x20001;
    Check(NtAllocateVirtualMemory(&base, &size, 0x3000, 0, 0) == STATUS_NO_MEMORY);
    Check(uint32_t(base) == 0 && uint32_t(size) == 0x20001);
    Check(g_pageAllocator.virtualRegion.used == pagesBefore);

    base = 0x1001;
    size = 0x1000;
    Check(NtAllocateVirtualMemory(&base, &size, 0x3000, 0, 0) == STATUS_SUCCESS);
    Check(uint32_t(base) == 0x1000 && uint32_t(size) == 0x2000);
    base = 0x1001;
    size = 0x100;
    Check(NtAllocateVirtualMemory(&base, &size, 0x1000, 0, 0) == STATUS_SUCCESS);
    Check(uint32_t(base) == 0x1000 && uint32_t(size) == 0x1000);
    base = 0;
    size = 0x1000;
    Check(NtAllocateVirtualMemory(&base, &size, 0x3000, 0, 0) == STATUS_SUCCESS);
    Check(uint32_t(base) == 0x3000 && uint32_t(size) == 0x1000);

    Check(MmAllocatePhysicalMemoryEx(0, 0xFFFFF001, 0, 0, UINT32_MAX, 0x1000) == 0);
    Check(MmAllocatePhysicalMemoryEx(0, 0x1000, 0, 0, UINT32_MAX, UINT32_MAX) == 0);
    const auto physical = MmAllocatePhysicalMemoryEx(0, 0x1000, 0, 0, UINT32_MAX, 0);
    Check(physical == 0x22000);
    Check(g_pageAllocator.AllocationSize(g_pageAllocator.physicalRegion, physical) == 0x1000);
    std::puts("PASS: production allocator/import overflow, capacity, failure atomicity, zeroing, rounding and non-overlap");
}
