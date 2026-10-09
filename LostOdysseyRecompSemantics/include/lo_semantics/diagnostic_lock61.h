#pragma once
#include "lo_semantics/crt_async_status_transfer.h"

namespace lo::semantic::gpu::diagnostic_lock61 {
using Registers = crt_async_status_transfer::Registers;

// Local state omitted from the common selected register ABI. reserved_bits
// retains the generated context's little-endian-host 32-bit snapshot in its
// low half; the high half survives lwarx. This matches the selected x64
// generation/execution environment, not an arbitrary host endian contract.
// This is the generated CAS reservation
// model, not a claim to emulate a PPC exclusive-monitor address/granule.
struct MachineState {
    std::uint32_t msr = 0u;
    std::uint64_t reserved_bits = 0u;
};

class SynchronizationServices {
public:
    virtual ~SynchronizationServices() = default;
    // All values use guest word order at this API. The borrowed service owns
    // the synchronized backing storage and real atomic compare/exchange.
    // A RAM read/check/write sequence is not a valid implementation of CAS.
    virtual std::uint32_t LoadReservedWord(GuestAddress address, GuestMemory& memory) = 0;
    virtual bool CompareExchangeWord(GuestAddress address, std::uint32_t expected,
        std::uint32_t desired, GuestMemory& memory) = 0;
    // Native critical-section calls share live ordinary and machine state.
    virtual void EnterCriticalSection(GuestMemory& memory, Registers& registers, MachineState& machine) = 0;
    virtual void LeaveCriticalSection(GuestMemory& memory, Registers& registers, MachineState& machine) = 0;
};

// 822B29A0 enters the critical section, attempts flag 0->1 at target+28,
// then records the current TLS thread identity at target+32. 822B3438
// attempts flag 1->0 and leaves the section. r3 points to a borrowed target
// pointer. A compare mismatch performs the original observed->observed CAS;
// only a failed matching CAS retries. Both return 1 and preserve their frames.
// MSR mask 0x8020 transitions and reservation snapshot remain explicit;
// host/PPC hardware interrupt and reservation-granule equivalence is untested.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    SynchronizationServices& synchronization, Registers& registers, MachineState& machine);
}
