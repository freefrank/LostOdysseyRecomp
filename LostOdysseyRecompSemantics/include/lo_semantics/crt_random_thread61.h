#pragma once
#include "lo_semantics/crt_record_allocation_context.h"
#include "lo_semantics/crt_free_context.h"

namespace lo::semantic::gpu::crt_random_thread61 {
using Registers = crt_record_allocation_context::Registers;
// Borrowed thread/native services. Guest TLS owns the allocated record; no
// host allocation, synchronization or synthetic thread identity is introduced.
class Services {
public:
    virtual ~Services() = default;
    virtual void KeTlsGetValue(GuestMemory&, Registers&) = 0;
    virtual void KeTlsSetValue(GuestMemory&, Registers&) = 0;
    virtual void CallIndirect(GuestAddress target, GuestMemory&, Registers&) = 0;
    // Actual 82B7BED8 boundary with r3=16. A nonreturning implementation may
    // throw/terminate; a returning implementation leaves its live state intact.
    virtual void FatalRuntimeError(GuestMemory&, Registers&) = 0;
};
struct Dependencies {
    crt_record_allocation_context::Dependencies allocation;
    crt_free_context::LowerCalls& release;
    Services& services;
};
// Thread-record lookup/first allocation and its 15-bit LCG consumer. The
// native TLS, allocator selected lower ABI and fatal runtime remain boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory&, Dependencies, Registers&);
// Accepted direct lowers for the original-upper/shared-lower comparison.
[[nodiscard]] bool ApplyAcceptedLower(GuestAddress entry, GuestMemory&, Dependencies, Registers&);
}
