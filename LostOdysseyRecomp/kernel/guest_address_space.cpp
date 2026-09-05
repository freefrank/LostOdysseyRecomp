#include "guest_address_space.h"
#include <cstddef>

#ifdef _WIN32
#include <windows.h>
#include <memoryapi.h>
#else
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace GuestAddressSpace
{
static constexpr size_t kSize = 0x100000000ull;
static constexpr size_t kBackingSize = 0xC0001000ull;
static constexpr size_t kStarts[] = {0, 0xA0000000, 0xC0000000, 0xE0000000};
static constexpr size_t kSizes[] = {0xA0000000, 0x20000000, 0x20000000, 0x20000000};
static constexpr size_t kOffsets[] = {0, 0xA0000000, 0xA0000000, 0xA0001000};

uint8_t* Allocate()
{
#ifdef _WIN32
    auto* base = static_cast<uint8_t*>(VirtualAlloc2(nullptr,
        reinterpret_cast<void*>(0x100000000ull), kSize,
        MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0));
    if (!base)
        base = static_cast<uint8_t*>(VirtualAlloc2(nullptr, nullptr, kSize,
            MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0));
    if (!base)
        return nullptr;

    // Split the reservation before replacing any placeholder. Keeping the
    // address range reserved prevents another thread from stealing a view.
    size_t split = 0;
    for (; split < 3; ++split)
    {
        if (!VirtualFree(base + kStarts[split], kSizes[split],
            MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
        {
            for (size_t i = 0; i <= split; ++i)
                VirtualFree(base + kStarts[i], 0, MEM_RELEASE);
            return nullptr;
        }
    }

    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
        PAGE_READWRITE, DWORD(kBackingSize >> 32), DWORD(kBackingSize), nullptr);
    size_t mapped = 0;
    if (section)
    {
        for (; mapped < 4; ++mapped)
        {
            if (!MapViewOfFile3(section, nullptr, base + kStarts[mapped],
                kOffsets[mapped], kSizes[mapped], MEM_REPLACE_PLACEHOLDER,
                PAGE_READWRITE, nullptr, 0))
                break;
        }
        CloseHandle(section);
    }
    if (mapped != 4)
    {
        for (size_t i = 0; i < mapped; ++i)
            UnmapViewOfFile(base + kStarts[i]);
        for (size_t i = mapped; i < 4; ++i)
            VirtualFree(base + kStarts[i], 0, MEM_RELEASE);
        return nullptr;
    }
    DWORD oldProtect;
    if (!VirtualProtect(base, 4096, PAGE_NOACCESS, &oldProtect))
    {
        Release(base);
        return nullptr;
    }
#else
    auto* base = static_cast<uint8_t*>(mmap(reinterpret_cast<void*>(0x100000000ull),
        kSize, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
    if (base == MAP_FAILED)
        return nullptr;
    int section = static_cast<int>(syscall(SYS_memfd_create, "lo-guest-memory", 0));
    if (section < 0)
    {
        munmap(base, kSize);
        return nullptr;
    }
    bool success = ftruncate(section, kBackingSize) == 0;
    for (size_t i = 0; success && i < 4; ++i)
        success = mmap(base + kStarts[i], kSizes[i], PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_FIXED, section, kOffsets[i]) != MAP_FAILED;
    close(section);
    if (!success || mprotect(base, 4096, PROT_NONE) != 0)
    {
        munmap(base, kSize);
        return nullptr;
    }
#endif
    return base;
}

void Release(uint8_t* base)
{
    if (!base)
        return;
#ifdef _WIN32
    for (size_t start : kStarts)
        UnmapViewOfFile(base + start);
#else
    munmap(base, kSize);
#endif
}
}
