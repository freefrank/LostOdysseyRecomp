// Compile this fixture by itself: include the production allocator below while
// replacing only native calls. Cleanup deliberately overwrites GetLastError.
#include <windows.h>
#include <memoryapi.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <type_traits>

namespace
{
enum class Fault { None, Reserve, Split, Backing, Map, Protect };
Fault fault = Fault::Backing;
int faultIndex = 0;
bool failPreferred = false;
uint32_t failureCode = ERROR_COMMITMENT_LIMIT;
int reserveCalls = 0, splitCalls = 0, mapCalls = 0, freeCalls = 0, unmapCalls = 0, closeCalls = 0;
int checks = 0;
constexpr uintptr_t baseAddress = 0x100000000ull;
void Check(bool condition, const char* message)
{
    ++checks;
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
void Reset(Fault selected, int index = 0, bool preferred = true)
{
    fault = selected;
    faultIndex = index;
    failPreferred = preferred;
    failureCode = 0xD001u + uint32_t(selected) * 16 + uint32_t(index);
    reserveCalls = splitCalls = mapCalls = freeCalls = unmapCalls = closeCalls = 0;
    SetLastError(ERROR_SUCCESS);
}
void* FailPointer() { SetLastError(failureCode); return nullptr; }
BOOL FailBool() { SetLastError(failureCode); return FALSE; }
void* TestVirtualAlloc2(HANDLE, void* address, SIZE_T size, ULONG flags, ULONG protection,
                        MEM_EXTENDED_PARAMETER*, ULONG)
{
    ++reserveCalls;
    Check(size == 0x100000000ull && flags == (MEM_RESERVE | MEM_RESERVE_PLACEHOLDER) &&
          protection == PAGE_NOACCESS, "reservation contract changed");
    if (address && (failPreferred || fault == Fault::Reserve))
    {
        SetLastError(ERROR_INVALID_ADDRESS);
        return nullptr;
    }
    if (fault == Fault::Reserve) return FailPointer();
    return reinterpret_cast<void*>(baseAddress);
}
BOOL TestVirtualFree(void*, SIZE_T size, DWORD flags)
{
    if (flags == (MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
    {
        const int index = splitCalls++;
        if (fault == Fault::Split && faultIndex == index) return FailBool();
        return TRUE;
    }
    Check(flags == MEM_RELEASE && size == 0, "placeholder cleanup contract changed");
    ++freeCalls;
    SetLastError(ERROR_INVALID_HANDLE);
    return TRUE;
}
HANDLE TestCreateFileMappingW(HANDLE file, SECURITY_ATTRIBUTES*, DWORD protection, DWORD high, DWORD low, LPCWSTR)
{
    Check(file == INVALID_HANDLE_VALUE && protection == PAGE_READWRITE &&
          high == 0 && low == 0xC0001000u, "backing/commit contract changed");
    if (fault == Fault::Backing) return static_cast<HANDLE>(FailPointer());
    return reinterpret_cast<HANDLE>(1);
}
void* TestMapViewOfFile3(HANDLE, HANDLE, void* address, ULONG64 offset, SIZE_T size,
                        ULONG flags, ULONG protection, MEM_EXTENDED_PARAMETER*, ULONG)
{
    const int index = mapCalls++;
    constexpr uint64_t starts[] = {0, 0xA0000000, 0xC0000000, 0xE0000000};
    constexpr uint64_t offsets[] = {0, 0xA0000000, 0xA0000000, 0xA0001000};
    Check(index < 4 && reinterpret_cast<uintptr_t>(address) == baseAddress + starts[index] &&
          offset == offsets[index] && size == (index ? 0x20000000ull : 0xA0000000ull) &&
          flags == MEM_REPLACE_PLACEHOLDER && protection == PAGE_READWRITE, "alias map contract changed");
    if (fault == Fault::Map && faultIndex == index) return FailPointer();
    return address;
}
BOOL TestCloseHandle(HANDLE) { ++closeCalls; SetLastError(ERROR_INVALID_HANDLE); return TRUE; }
BOOL TestUnmapViewOfFile(const void*) { ++unmapCalls; SetLastError(ERROR_INVALID_HANDLE); return TRUE; }
BOOL TestVirtualProtect(void* address, SIZE_T size, DWORD protection, DWORD*)
{
    Check(reinterpret_cast<uintptr_t>(address) == baseAddress && size == 4096 && protection == PAGE_NOACCESS,
          "null protection contract changed");
    if (fault == Fault::Protect) return FailBool();
    return TRUE;
}
}
#define VirtualAlloc2 TestVirtualAlloc2
#define VirtualFree TestVirtualFree
#define CreateFileMappingW TestCreateFileMappingW
#define MapViewOfFile3 TestMapViewOfFile3
#define CloseHandle TestCloseHandle
#define UnmapViewOfFile TestUnmapViewOfFile
#define VirtualProtect TestVirtualProtect
#include <kernel/guest_address_space.cpp>
#undef VirtualAlloc2
#undef VirtualFree
#undef CreateFileMappingW
#undef MapViewOfFile3
#undef CloseHandle
#undef UnmapViewOfFile
#undef VirtualProtect

static_assert(std::is_trivial_v<GuestAddressSpace::FailureInfo>);
// Reproduce failure before main/logging exists, as with the real static Memory.
uint8_t* startupAllocation = GuestAddressSpace::Allocate();

int main()
{
    using namespace GuestAddressSpace;
    Check(!startupAllocation && GetFailureInfo().error == ERROR_COMMITMENT_LIMIT &&
          GetFailureInfo().operation == FailureOperation::CreateBacking, "static startup failure was lost");
    Check(GetLastError() == ERROR_INVALID_HANDLE, "cleanup did not overwrite OS error as negative control");
    for (Fault selected : {Fault::Reserve, Fault::Split, Fault::Backing, Fault::Map, Fault::Protect})
    {
        const int count = selected == Fault::Split ? 3 : selected == Fault::Map ? 4 : 1;
        for (int index = 0; index < count; ++index)
        {
            Reset(selected, index);
            Check(!Allocate(), "injected native failure must fail allocation");
            const auto info = GetFailureInfo();
            Check(info.error == failureCode, "cleanup overwrote captured native failure");
            Check(info.preferredReservationError == ERROR_INVALID_ADDRESS, "preferred reservation error was lost");
            const auto operation = selected == Fault::Reserve ? FailureOperation::ReserveAny :
                                   selected == Fault::Split ? FailureOperation::SplitReservation :
                                   selected == Fault::Backing ? FailureOperation::CreateBacking :
                                   selected == Fault::Map ? FailureOperation::MapView : FailureOperation::ProtectNull;
            Check(info.operation == operation, "failure operation mislabeled");
            Check(info.viewIndex == ((selected == Fault::Map || selected == Fault::Split) ? index : -1), "failure index mislabeled");
            const uint64_t start = index == 0 ? 0 : index == 1 ? 0xA0000000ull : index == 2 ? 0xC0000000ull : 0xE0000000ull;
            const uint64_t expectedSize = selected == Fault::Reserve ? 0x100000000ull :
                                          selected == Fault::Backing ? 0xC0001000ull :
                                          selected == Fault::Protect ? 4096 : index ? 0x20000000ull : 0xA0000000ull;
            Check(info.size == expectedSize, "requested size missing");
            Check(info.address == ((selected == Fault::Reserve || selected == Fault::Backing) ? 0 : baseAddress + start), "target address missing");
            Check(info.offset == (selected == Fault::Map && index ? index == 3 ? 0xA0001000ull : 0xA0000000ull : 0), "backing offset missing");
            if (selected == Fault::Split) Check(freeCalls == index + 1 && closeCalls == 0, "split failure cleanup incomplete");
            if (selected == Fault::Backing) Check(freeCalls == 4 && closeCalls == 0, "backing failure cleanup incomplete");
            if (selected == Fault::Map) Check(freeCalls == 4 - index && unmapCalls == index && closeCalls == 1, "partial-map cleanup incomplete");
            if (selected == Fault::Protect) Check(unmapCalls == 4 && closeCalls == 1, "protection failure cleanup incomplete");
            if (selected != Fault::Reserve) Check(GetLastError() == ERROR_INVALID_HANDLE, "cleanup negative control absent");
        }
    }
    Reset(Fault::None);
    auto* base = Allocate();
    Check(base && reserveCalls == 2, "preferred-reservation failure must still fall back");
    Check(GetFailureInfo().operation == FailureOperation::None && GetFailureInfo().error == 0,
          "successful fallback left stale failure info");
    Release(base);
    Check(unmapCalls == 4, "successful release must unmap all alias views");
    Reset(Fault::None, 0, false);
    base = Allocate();
    Check(base && reserveCalls == 1, "preferred reservation success changed");
    Release(base);
    std::printf("PASS: %d checks; static startup capture, 10 failure branches, cleanup error preservation, preferred fallback, unchanged mapping contracts.\n", checks);
}
