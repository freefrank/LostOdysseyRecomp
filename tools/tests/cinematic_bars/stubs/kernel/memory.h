#pragma once
#include <cstdint>

// Sparse guest bytes avoid reserving/committing a 4 GiB address space in a
// CPU-only fixture. Guest loads retain their big-endian production semantics.
namespace fixture
{
uint8_t Load8(uint32_t address);
uint32_t Load32(uint32_t address);
}
#define PPC_LOAD_U8(address) fixture::Load8(uint32_t(address))
#define PPC_LOAD_U32(address) fixture::Load32(uint32_t(address))
