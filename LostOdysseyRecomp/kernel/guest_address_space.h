#pragma once

#include <cstddef>
#include <cstdint>
#include <os/platform.h>

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
// A and C alias the same 512 MiB; E starts one physical page later.
// Keep these OS mappings coherent even for direct recompiled base+address loads.
uint8_t* Allocate();
void Release(uint8_t* base);
#if LO_PLATFORM_SWITCH
// Horizon has no lazy commit: guest pages must be backed before first use.
// Commits [address, address + size) of the virtual or A view (A also backs
// its C and E aliases). Returns false when the console is out of memory.
bool Commit(uint32_t address, uint32_t size);
size_t CommittedBytes();
// For a guest pointer the game reads although nothing was allocated there
// (the desktop builds read zeros from such pages; the Switch would fault):
// backs [address, address + size) with zeroed memory. C and E addresses are
// committed through their A pages. Cheap when already backed.
// Repaired: the chunk was committed but the kernel no longer had the page
// mapped there; the view was mapped again (see LastRepair).
enum class Backing { Failed, AlreadyBacked, NewlyBacked, Repaired };
Backing EnsureBacked(uint32_t address, uint32_t size);
struct RepairInfo
{
    uint32_t count;           // repairs so far
    uint32_t address;         // guest address that was not mapped
    uint32_t type, attribute, permission; // what svcQueryMemory reported there
    uint64_t regionStart, regionSize;     // that memory block (guest offsets)
    uint32_t aliasType, aliasPermission;  // the same page through C
    uint32_t result;          // svcMapProcessMemory result of the repair
    uint64_t source;          // the chunk's code-memory source (0: none recorded)
    uint32_t sourceType, sourcePermission;
    uint64_t mappedStart, mappedSize;     // what the repair mapped (guest offsets)
    char layout[320];         // the chunk's memory blocks: "start-end tTYPE pPERM, ..."
};
RepairInfo LastRepair();
// Logs LastRepair() as an error (patches/unbacked_guest_reads.cpp); the first few only.
void LogRepair(const char* who);
#else
// Desktop hosts commit on first touch.
inline bool Commit(uint32_t, uint32_t) { return true; }
enum class Backing { Failed, AlreadyBacked, NewlyBacked, Repaired };
inline Backing EnsureBacked(uint32_t address, uint32_t) { return address >= 0x1000 ? Backing::AlreadyBacked : Backing::Failed; }
#endif
}
