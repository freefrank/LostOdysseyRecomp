#pragma once

#include "lo_semantics/crt_close_recursive_buffer_context.h"
#include "lo_semantics/float_triplet_transfer.h"

namespace lo::semantic::gpu::crt_reader_float61 {
using Registers = crt_close_recursive_buffer_context::Registers;

// Both services are borrowed for this call; guest allocation owns node/data
// storage. FP control remains an explicit host service for a later Rust port.
struct Dependencies {
    crt_close_recursive_buffer_context::GuestServices& guest;
    float_triplet_transfer::NativeServices& fp;
};

// 82BD10D8: r3 identifies a borrowed reader, f1 supplies the float value.
// Reader +0 is its current node and +16 is the last write address; node +0/+4/+8
// hold data, used bytes and capacity. Flush pending bits, grow if needed, append
// four bytes, and return the reader in r3. Guest callbacks can mutate all state,
// including the live f31 value used for the store. Only saved r31/f31 and the
// caller frame/LR are restored; no host allocation or ownership transfer occurs.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies deps, Registers& state);
}
