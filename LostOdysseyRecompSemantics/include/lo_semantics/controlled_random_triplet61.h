#pragma once
#include "lo_semantics/crt_async_status_transfer.h"
#include "lo_semantics/float_triplet_transfer.h"
namespace lo::semantic::gpu::controlled_random_triplet61 {
using Registers=crt_async_status_transfer::Registers;
// E470/F40 read a borrowed object's flags at+160/+164 and write three floats
// to incoming r5. Three LCG steps update guest global8331367C before output.
// Generated signed-word products retain64-bit intermediates; the seed store
// narrows to32 bits. Mantissa bits yield fractional samples. Axis flag pairs
// select zero, sample, negated sample or scaled/offset sample via guest constants.
// Each later axis reloads flags after the preceding output store, including
// overlapping output/object storage. This is the original generator, without
// claims of uniformity/independence or thread safety; no RNG replacement.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,float_triplet_transfer::NativeServices&,Registers&);
}
