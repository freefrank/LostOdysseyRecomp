#include "guest_address_space.h"
#include <cstddef>
#include <cerrno>

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
static constexpr size_t kLegacyEOffset = 0xA0000000ull;
static constinit FailureInfo failure{};
static constinit bool usingLegacyApis = false;
static constinit bool eWindowPageOffset = true;
static constinit const char* mappingMethod = "none";

FailureInfo GetFailureInfo() { return failure; }
const char* MappingMethodName() { return mappingMethod; }
bool EWindowHasPageOffset() { return eWindowPageOffset; }

const char* FailureOperationName(FailureOperation operation)
{
    switch (operation)
    {
    case FailureOperation::None: return "none";
    case FailureOperation::ReservePreferred: return "reserve preferred address";
    case FailureOperation::ReserveAny: return "reserve any address";
    case FailureOperation::SplitReservation: return "split reservation placeholder";
    case FailureOperation::CreateBacking: return "create shared backing";
    case FailureOperation::ResizeBacking: return "resize shared backing";
    case FailureOperation::MapView: return "map alias view";
    case FailureOperation::ProtectNull: return "protect null page";
    }
    return "unknown";
}

const char* FailureApiName(FailureOperation operation)
{
    switch (operation)
    {
    case FailureOperation::None: return "none";
#ifdef _WIN32
    case FailureOperation::ReservePreferred:
    case FailureOperation::ReserveAny: return usingLegacyApis ? "VirtualAlloc" : "VirtualAlloc2";
    case FailureOperation::SplitReservation: return "VirtualFree";
    case FailureOperation::CreateBacking: return "CreateFileMappingW";
    case FailureOperation::MapView: return usingLegacyApis ? "MapViewOfFileEx" : "MapViewOfFile3";
    case FailureOperation::ProtectNull: return "VirtualProtect";
#else
    case FailureOperation::ReservePreferred:
    case FailureOperation::ReserveAny:
    case FailureOperation::MapView: return "mmap";
    case FailureOperation::CreateBacking: return "memfd_create";
    case FailureOperation::ResizeBacking: return "ftruncate";
    case FailureOperation::ProtectNull: return "mprotect";
#endif
    default: return "unknown";
    }
}

static void RecordFailure(FailureOperation operation, uint32_t error, int32_t viewIndex,
                          const void* address, size_t size, size_t offset = 0,
                          uint32_t preferredReservationError = 0, uintptr_t backingHandle = 0)
{
    failure = {operation, error, preferredReservationError, viewIndex,
               reinterpret_cast<uintptr_t>(address), size, offset};
#ifdef _WIN32
    // This path also runs during global Memory construction. Keep it allocation
    // free, and retain the caller's original error before any diagnostic API.
    FILETIME timestamp{};
    GetSystemTimeAsFileTime(&timestamp);
    failure.utcFileTime = (uint64_t(timestamp.dwHighDateTime) << 32) | timestamp.dwLowDateTime;
    failure.uptimeMilliseconds = GetTickCount64();
    failure.threadId = GetCurrentThreadId();
    failure.backingHandle = backingHandle;
    switch (operation)
    {
    case FailureOperation::ReservePreferred:
    case FailureOperation::ReserveAny:
        failure.flags = MEM_RESERVE | (usingLegacyApis ? 0 : MEM_RESERVE_PLACEHOLDER);
        failure.protection = PAGE_NOACCESS;
        break;
    case FailureOperation::SplitReservation:
        failure.flags = MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER;
        break;
    case FailureOperation::CreateBacking:
        failure.protection = PAGE_READWRITE;
        failure.backingHandle = reinterpret_cast<uintptr_t>(INVALID_HANDLE_VALUE);
        break;
    case FailureOperation::MapView:
        failure.flags = usingLegacyApis ? 0 : MEM_REPLACE_PLACEHOLDER;
        failure.protection = PAGE_READWRITE;
        break;
    case FailureOperation::ProtectNull:
        failure.protection = PAGE_NOACCESS;
        break;
    default: break;
    }
    // VirtualAlloc2 and MapViewOfFile3 both receive a null process handle,
    // which explicitly selects the calling process.
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory))
    {
        failure.memory = {1, 0, memory.dwMemoryLoad,
            memory.ullTotalPhys, memory.ullAvailPhys,
            memory.ullTotalPageFile, memory.ullAvailPageFile,
            memory.ullTotalVirtual, memory.ullAvailVirtual};
    }
    else
    {
        failure.memory.error = GetLastError();
    }
#endif
}

#ifdef _WIN32
#ifndef LO_GUEST_MEMORY_API_OVERRIDE
using VirtualAlloc2Fn = PVOID (WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
using MapViewOfFile3Fn = PVOID (WINAPI*)(HANDLE, HANDLE, PVOID, ULONG64, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);

static FARPROC LoadKernelProc(const char* name)
{
    if (HMODULE module = GetModuleHandleW(L"kernelbase.dll"))
        if (FARPROC proc = GetProcAddress(module, name))
            return proc;
    if (HMODULE module = GetModuleHandleW(L"kernel32.dll"))
        return GetProcAddress(module, name);
    return nullptr;
}

static void* CallVirtualAlloc2(void* address)
{
    static const VirtualAlloc2Fn fn = reinterpret_cast<VirtualAlloc2Fn>(LoadKernelProc("VirtualAlloc2"));
    if (!fn)
    {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return nullptr;
    }
    return fn(nullptr, address, kSize, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0);
}

static void* CallMapViewOfFile3(HANDLE section, void* address, ULONG64 offset, SIZE_T size)
{
    static const MapViewOfFile3Fn fn = reinterpret_cast<MapViewOfFile3Fn>(LoadKernelProc("MapViewOfFile3"));
    if (!fn)
    {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return nullptr;
    }
    return fn(section, nullptr, address, offset, size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0);
}

static bool LegacyRequested()
{
    char text[8]{};
    return GetEnvironmentVariableA("LO_GUEST_MEMORY_LEGACY", text, DWORD(sizeof(text))) &&
        text[0] && text[0] != '0';
}

static bool ShouldFallback(uint32_t error)
{
    switch (error)
    {
    case ERROR_INVALID_HANDLE:
    case ERROR_INVALID_PARAMETER:
    case ERROR_NOT_SUPPORTED:
    case ERROR_CALL_NOT_IMPLEMENTED:
    case ERROR_PROC_NOT_FOUND:
    case ERROR_INVALID_FUNCTION:
    case ERROR_MOD_NOT_FOUND:
        return true;
    default:
        return false;
    }
}
#else
static void* CallVirtualAlloc2(void* address)
{
    return VirtualAlloc2(nullptr, address, kSize, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0);
}

static void* CallMapViewOfFile3(HANDLE section, void* address, ULONG64 offset, SIZE_T size)
{
    return MapViewOfFile3(section, nullptr, address, offset, size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0);
}

static bool LegacyRequested() { return false; }
static bool ShouldFallback(uint32_t) { return false; }
#endif

static uint8_t* FinishMappedBase(uint8_t* base, uint32_t preferredError)
{
    DWORD oldProtect;
    if (!VirtualProtect(base, 4096, PAGE_NOACCESS, &oldProtect))
    {
        RecordFailure(FailureOperation::ProtectNull, GetLastError(), -1, base, 4096, 0, preferredError);
        Release(base);
        return nullptr;
    }
    return base;
}

static uint8_t* AllocatePlaceholder(uint32_t& preferredError)
{
    usingLegacyApis = false;
    auto* base = static_cast<uint8_t*>(CallVirtualAlloc2(reinterpret_cast<void*>(0x100000000ull)));
    if (!base)
    {
        preferredError = GetLastError();
        base = static_cast<uint8_t*>(CallVirtualAlloc2(nullptr));
    }
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
            RecordFailure(FailureOperation::SplitReservation, GetLastError(), int32_t(split),
                          base + kStarts[split], kSizes[split], 0, preferredError);
            for (size_t i = 0; i <= split; ++i)
                VirtualFree(base + kStarts[i], 0, MEM_RELEASE);
            return nullptr;
        }
    }

    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
        PAGE_READWRITE, DWORD(kBackingSize >> 32), DWORD(kBackingSize), nullptr);
    if (!section)
        RecordFailure(FailureOperation::CreateBacking, GetLastError(), -1, nullptr, kBackingSize, 0, preferredError);
    size_t mapped = 0;
    if (section)
    {
        for (; mapped < 4; ++mapped)
        {
            if (!CallMapViewOfFile3(section, base + kStarts[mapped], kOffsets[mapped], kSizes[mapped]))
            {
                RecordFailure(FailureOperation::MapView, GetLastError(), int32_t(mapped),
                              base + kStarts[mapped], kSizes[mapped], kOffsets[mapped], preferredError,
                              reinterpret_cast<uintptr_t>(section));
                break;
            }
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
    return FinishMappedBase(base, preferredError);
}

static uint8_t* AllocateLegacy(uint32_t preferredError)
{
    usingLegacyApis = true;
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
        PAGE_READWRITE, DWORD(kBackingSize >> 32), DWORD(kBackingSize), nullptr);
    if (!section)
    {
        RecordFailure(FailureOperation::CreateBacking, GetLastError(), -1, nullptr, kBackingSize, 0, preferredError);
        return nullptr;
    }

    uint8_t* base = nullptr;
    uint32_t lastError = 0;
    int32_t lastView = -1;
    size_t lastOffset = 0;
    size_t lastSize = kSize;
    for (int attempt = 0; attempt < 8 && !base; ++attempt)
    {
        void* probe = VirtualAlloc(reinterpret_cast<void*>(0x100000000ull), kSize, MEM_RESERVE, PAGE_NOACCESS);
        if (!probe)
        {
            if (!preferredError)
                preferredError = GetLastError();
            probe = VirtualAlloc(nullptr, kSize, MEM_RESERVE, PAGE_NOACCESS);
        }
        if (!probe)
        {
            RecordFailure(FailureOperation::ReserveAny, GetLastError(), -1, nullptr, kSize, 0, preferredError);
            CloseHandle(section);
            return nullptr;
        }
        auto* candidate = static_cast<uint8_t*>(probe);
        if (!VirtualFree(probe, 0, MEM_RELEASE))
        {
            RecordFailure(FailureOperation::ReserveAny, GetLastError(), -1, candidate, kSize, 0, preferredError);
            CloseHandle(section);
            return nullptr;
        }

        size_t mapped = 0;
        for (; mapped < 4; ++mapped)
        {
            const size_t offset = mapped == 3 ? kLegacyEOffset : kOffsets[mapped];
            if (!MapViewOfFileEx(section, FILE_MAP_READ | FILE_MAP_WRITE,
                DWORD(offset >> 32), DWORD(offset), kSizes[mapped], candidate + kStarts[mapped]))
            {
                lastError = GetLastError();
                lastView = int32_t(mapped);
                lastOffset = offset;
                lastSize = kSizes[mapped];
                break;
            }
        }
        if (mapped == 4)
        {
            base = candidate;
            break;
        }
        for (size_t i = 0; i < mapped; ++i)
            UnmapViewOfFile(candidate + kStarts[i]);
    }
    CloseHandle(section);
    if (!base)
    {
        RecordFailure(FailureOperation::MapView, lastError, lastView,
                      lastView >= 0 ? reinterpret_cast<void*>(kStarts[lastView]) : nullptr,
                      lastSize, lastOffset, preferredError);
        return nullptr;
    }
    return FinishMappedBase(base, preferredError);
}
#endif

uint8_t* Allocate()
{
    failure = {};
    mappingMethod = "none";
    eWindowPageOffset = true;
#ifdef _WIN32
    usingLegacyApis = false;
    uint32_t preferredError = 0;
    uint8_t* base = nullptr;
    if (!LegacyRequested())
    {
        base = AllocatePlaceholder(preferredError);
        if (base)
        {
            mappingMethod = "placeholder";
            eWindowPageOffset = true;
            return base;
        }
        if (failure.operation != FailureOperation::None)
            return nullptr;
        const uint32_t anyError = GetLastError();
        if (!ShouldFallback(anyError) && !ShouldFallback(preferredError))
        {
            RecordFailure(FailureOperation::ReserveAny, anyError, -1, nullptr, kSize, 0, preferredError);
            return nullptr;
        }
        if (!preferredError)
            preferredError = anyError;
    }
    base = AllocateLegacy(preferredError);
    if (base)
    {
        failure = {};
        mappingMethod = "legacy";
        eWindowPageOffset = false;
    }
    return base;
#else
    auto* base = static_cast<uint8_t*>(mmap(reinterpret_cast<void*>(0x100000000ull),
        kSize, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
    if (base == MAP_FAILED)
    {
        RecordFailure(FailureOperation::ReservePreferred, uint32_t(errno), -1,
                      reinterpret_cast<void*>(0x100000000ull), kSize);
        return nullptr;
    }
    int section = static_cast<int>(syscall(SYS_memfd_create, "lo-guest-memory", 0));
    if (section < 0)
    {
        RecordFailure(FailureOperation::CreateBacking, uint32_t(errno), -1, nullptr, kBackingSize);
        munmap(base, kSize);
        return nullptr;
    }
    bool success = ftruncate(section, kBackingSize) == 0;
    if (!success)
        RecordFailure(FailureOperation::ResizeBacking, uint32_t(errno), -1, nullptr, kBackingSize);
    for (size_t i = 0; success && i < 4; ++i)
    {
        success = mmap(base + kStarts[i], kSizes[i], PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_FIXED, section, kOffsets[i]) != MAP_FAILED;
        if (!success)
            RecordFailure(FailureOperation::MapView, uint32_t(errno), int32_t(i),
                          base + kStarts[i], kSizes[i], kOffsets[i]);
    }
    close(section);
    if (success && mprotect(base, 4096, PROT_NONE) != 0)
    {
        RecordFailure(FailureOperation::ProtectNull, uint32_t(errno), -1, base, 4096);
        success = false;
    }
    if (!success)
    {
        munmap(base, kSize);
        return nullptr;
    }
    mappingMethod = "placeholder";
    eWindowPageOffset = true;
    return base;
#endif
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
