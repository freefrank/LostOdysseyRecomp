#pragma once
#include "lo_semantics/crt_close_recursive_buffer_context.h"
namespace lo::semantic::gpu::mesh_attribute_reorder61 {
using Registers=crt_close_recursive_buffer_context::Registers;
using GuestServices=crt_close_recursive_buffer_context::GuestServices;
// BB3CF8 gathers mesh rows through borrowed uint32 indices r4. r3 points to
// the mesh pointer. Mesh+4 is row count; +12 owns packed 12-byte positions;
// +76/+32 optionally own uint16 columns; +80 owns/composes original row IDs;
// +36 optionally owns category IDs, byte-wide when mesh+28<256, else uint16.
// Each new column is allocated through global service slot8, filled in index
// order, then replaces its old slot after slot20 releases it. Every callback
// retains the complete mutable state and owner is reloaded before installation.
// Empty meshes allocate nothing. Existing absence/failure behavior is kept;
// no bounds guard, rollback or host ownership is introduced. r3 is not forced
// to a normalized result after the final allocator/free call.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,GuestServices&,Registers&);
}
