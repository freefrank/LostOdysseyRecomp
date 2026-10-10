#include "lo_semantics/battle_property_mutation61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_property_mutation61 {
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82ac9000) {
    s.r[5] = 1;
    return battle_property_mutation61::Apply(0x82ac8ee8, m, d, s);
  }
  if (e != 0x82ac8ee8)
    return false;
  using recovery_abi::Address;
  auto old = Address(s.r[1]), resource = Address(s.r[3]), id = Address(s.r[4]),
       mode = Address(s.r[5]) & 255;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = 29; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  unsigned result = 0;
  if (std::int32_t(id) <= 262) {
    auto n = std::int32_t(id), q = n / 32;
    auto p = 0x83213438u + 8 * unsigned(n - q * 32);
    auto bank = m.ReadU32(p) + unsigned(q), mask = m.ReadU32(p + 4),
         flags = resource + 272 * bank + 232;
    if (m.ReadU32(flags) & mask) {
      result = 1;
      if (mode == 1) {
        m.WriteU32(flags, m.ReadU32(flags) & ~mask);
        for (unsigned base : {59u, 91u}) {
          auto currentMask = m.ReadU32(p + 4);
          unsigned index = 0;
          for (; index < 31; ++index)
            if (currentMask & (1u << index))
              break;
          auto currentBank = m.ReadU32(p) + unsigned(q);
          m.WriteU32(resource + 4 * (68 * currentBank + index + base), 0);
        }
      }
    }
  }
  s.r[3] = result;
  for (unsigned i = 29; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_property_mutation61
