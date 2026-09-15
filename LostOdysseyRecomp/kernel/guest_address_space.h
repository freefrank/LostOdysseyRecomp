#pragma once

#include <cstdint>

namespace GuestAddressSpace
{
enum class FailureOperation : uint32_t
{
    None, ReservePreferred, ReserveAny, SplitReservation, CreateBacking,
    ResizeBacking, MapView, ProtectNull
};
// POD storage is available during static Memory construction, before logging.
struct FailureMemoryStatus
{
    uint32_t valid;
    uint32_t error;
    uint32_t loadPercent;
    uint64_t totalPhysical;
    uint64_t availablePhysical;
    uint64_t totalCommit;
    uint64_t availableCommit;
    uint64_t totalVirtual;
    uint64_t availableVirtual;
};
struct FailureInfo
{
    FailureOperation operation;
    uint32_t error;
    uint32_t preferredReservationError;
    int32_t viewIndex;
    uintptr_t address;
    uint64_t size;
    uint64_t offset;
    // Captured at failure, before cleanup or main/logging initialization.
    // utcFileTime uses Windows FILETIME units (100 ns since 1601-01-01 UTC).
    uint64_t utcFileTime;
    uint64_t uptimeMilliseconds;
    uint32_t threadId;
    uint32_t flags;
    uint32_t protection;
    uintptr_t processHandle;
    uintptr_t backingHandle;
    FailureMemoryStatus memory;
};
FailureInfo GetFailureInfo();
const char* FailureOperationName(FailureOperation operation);
const char* FailureApiName(FailureOperation operation);
// "placeholder", "legacy", or "none". Valid after Allocate returns.
const char* MappingMethodName();
// Placeholder maps E at file offset +4 KiB. Win7 MapViewOfFileEx cannot, so
// legacy maps E onto the same pages as A/C and this returns false.
bool EWindowHasPageOffset();
// A and C alias the same 512 MiB; E starts one physical page later when the
// host supports page-granular section offsets. Keep OS mappings coherent even
// for direct recompiled base+address loads.
uint8_t* Allocate();
void Release(uint8_t* base);
}
