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
struct FailureInfo
{
    FailureOperation operation;
    uint32_t error;
    uint32_t preferredReservationError;
    int32_t viewIndex;
    uintptr_t address;
    uint64_t size;
    uint64_t offset;
};
FailureInfo GetFailureInfo();
const char* FailureOperationName(FailureOperation operation);
// A and C alias the same 512 MiB; E starts one physical page later.
// Keep these OS mappings coherent even for direct recompiled base+address loads.
uint8_t* Allocate();
void Release(uint8_t* base);
}
