#pragma once
#include "lo_semantics/diagnostic_lock61.h"
#include "lo_semantics/crt_narrow_formatter61.h"

namespace lo::semantic::gpu::diagnostic_format_routes61 {
using Registers = diagnostic_lock61::Registers;
struct Dependencies {
    crt_narrow_formatter61::Dependencies formatter;
    diagnostic_lock61::SynchronizationServices& synchronization;
    diagnostic_lock61::MachineState& machine;
};

// BC8B78 owns the scoped diagnostic lock but borrows the sink, format, varargs
// and output buffers. Modes 107/208 select special sink slots; other modes
// use the general diagnostic slot. Formatting starts in the guest frame;
// truncation grows through the guest allocator up to the original limit.
// B9C298 builds the vararg view; B9D328 supplies diagnostic mode 2;
// BD18C0 sets two nonzero format fields or sends the existing diagnostic.
// Virtual sink/allocation calls retain the full mutable register state.
// Lock MSR/reservation state stays local to these explicitly composed calls.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
}
