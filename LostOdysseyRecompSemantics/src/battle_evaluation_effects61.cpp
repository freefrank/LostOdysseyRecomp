#include "lo_semantics/battle_evaluation_effects61.h"
#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_evaluation_effects61 {
using recovery_abi::Address;
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned mode = 0, frame = 96, first = 31;
  bool literal = false, late = false;
  switch (e) {
  case 0x82b0ce98:
    mode = 1;
    frame = 128;
    first = 29;
    literal = true;
    break;
  case 0x82b0cf20:
    mode = 2;
    break;
  case 0x82b0cf78:
    mode = 3;
    break;
  case 0x82b0cfd0:
    mode = 4;
    break;
  case 0x82b0d028:
    break;
  case 0x82b0d148:
    mode = 5;
    break;
  case 0x82b0d1a0:
    mode = 6;
    frame = 112;
    first = 30;
    late = true;
    break;
  case 0x82b0d088:
    mode = 8;
    frame = 128;
    first = 28;
    literal = true;
    late = true;
    break;
  case 0x82b0d210:
    frame = 128;
    first = 28;
    literal = true;
    break;
  case 0x82b0d2c8:
    mode = 9;
    frame = 128;
    first = 28;
    literal = true;
    break;
  default:
    return false;
  }
  auto owner = Address(s.r[3]), old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  if (literal)
    m.WriteU32(sp + 80, 0x8204a1d8);
  if (!late)
    m.WriteU32(owner + 64, mode);
  s.r[7] = m.ReadU32(owner + 40);
  s.r[6] = m.ReadU32(owner + 36);
  s.r[5] = m.ReadU32(owner + 8);
  s.r[4] = m.ReadU32(owner + 4);
  s.r[8] = 0;
  s.r[9] = 1;
  s.r[3] = m.ReadU32(0x832ca0cc);
  d.guest.CallDirect(0x82b21878, m, s);
  if (late)
    m.WriteU32(owner + 64, mode);
  s.r[3] = m.ReadU32(0x832ca0cc);
  d.guest.CallDirect(0x82b22948, m, s);
  if (e == 0x82b0d088) {
    m.WriteU32(m.ReadU32(owner + 4) + 188, 0);
    auto payload = m.ReadU32(owner + 120);
    s.r[3] = 111;
    (void)battle_script_actions61::Apply(0x8238aab0, m, d, s);
    s.r[5] = s.r[3];
    s.r[3] = m.ReadU32(owner + 4);
    s.r[4] = 3;
    s.r[6] = payload;
    s.r[7] = 0;
    s.r[8] = 1;
    d.guest.CallDirect(0x82ac8ec8, m, s);
  } else if (e == 0x82b0d210 || e == 0x82b0d2c8) {
    auto entry = m.ReadU32(owner + 108) == 0 ? 0x82ac71e8u : 0x82ac80b8u;
    s.r[3] = m.ReadU32(0x832aeb00);
    s.r[4] = m.ReadU32(owner + 8);
    s.r[5] = m.ReadU32(owner + 120);
    d.guest.CallDirect(entry, m, s);
  }
  if (literal)
    m.WriteU32(sp + 80, 0x8204a1d8);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_evaluation_effects61
