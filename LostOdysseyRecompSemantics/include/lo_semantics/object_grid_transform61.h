#pragma once
#include "lo_semantics/grid_transform_support61.h"

namespace lo::semantic::gpu::object_grid_transform61 {
using Registers = grid_transform_support61::Registers;
using NativeServices = grid_transform_support61::NativeServices;

// 82BB06D8 walks the borrowed cubic grid in r3. +88 is side length, +92
// plane stride and +108 the word buffer. For each positive eight-corner
// neighborhood containing an unmarked word, transform its top-bit-marked
// corners to physical coordinates and replace those words with the visit
// record's result, retaining the top bit. Return qualifying neighborhoods.
// Coordinate fields: origin +28..36, bias +40..48, scale +76..84.
// r4 is borrowed optional visit metadata, copied by fixed leaf 82BD78E8;
// no dynamic callback or ownership transfer is inferred from that leaf.
// Integer spills, single-precision stages, guest frames and scratch state
// remain observable. Ordinary finite inputs are the validation boundary;
// grid object, word buffer and metadata do not overlap the 608-byte frame.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);
}
