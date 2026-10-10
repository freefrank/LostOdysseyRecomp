#include "lo_semantics/battle_script_runtime61.h"
#include <bit>
#include "lo_semantics/battle_evaluation_effects61.h"
#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_evaluation_effects61 {
using recovery_abi::Address;
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82b21878) {
    auto old = Address(s.r[1]), owner = Address(s.r[3]),
         source = Address(s.r[4]), target = Address(s.r[5]);
    auto firstArg = Address(s.r[6]), secondArg = Address(s.r[7]),
         extra = Address(s.r[8]), flag = Address(s.r[9]);
    m.WriteU32(old - 8, Address(s.lr));
    recovery_abi::WriteU64(m, old - 16, s.r[31]);
    s.r[1] -= 96;
    m.WriteU32(Address(s.r[1]), old);
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto zero = double(std::bit_cast<float>(m.ReadU32(0x82000e50)));
    s.fpr_bits[0] = std::bit_cast<std::uint64_t>(zero);
    auto zeroBits = std::bit_cast<unsigned>(float(zero));
    m.WriteU8(owner + 45, flag);
    m.WriteU32(owner + 104, 0);
    for (auto offset : {4u, 12u})
      m.WriteU32(owner + offset, source);
    for (auto offset : {8u, 16u})
      m.WriteU32(owner + offset, target);
    m.WriteU32(owner + 28, firstArg);
    m.WriteU32(owner + 32, secondArg);
    m.WriteU32(owner + 40, extra);
    m.WriteU8(owner + 36, 0);
    m.WriteU8(owner + 44, 0);
    m.WriteU32(owner + 48, 0);
    auto secondary = m.ReadU32(0x832cb790);
    m.WriteU32(secondary + 36, zeroBits);
    m.WriteU32(secondary + 4, source);
    m.WriteU32(secondary + 12, firstArg);
    m.WriteU32(secondary + 20, secondArg);
    m.WriteU32(secondary + 24, 0);
    m.WriteU32(owner + 92, zeroBits);
    m.WriteU32(owner + 84, 0);
    m.WriteU32(owner + 88, 0);
    m.WriteU32(owner + 56, 1);
    for (auto offset : {120u, 64u, 65u})
      m.WriteU8(owner + offset, 0);
    m.WriteU8(owner + 108, m.ReadU32(secondArg) == 16);
    auto hp = double(std::bit_cast<float>(m.ReadU32(target + 2588)));
    s.fpr_bits[13] = std::bit_cast<std::uint64_t>(hp);
    m.WriteU32(owner + 96, std::bit_cast<unsigned>(float(hp)));
    for (auto offset : {116u, 112u, 72u, 76u})
      m.WriteU32(owner + offset, zeroBits);
    s.r[3] = 0x832c9c54;
    s.r[4] = m.ReadU32(owner + 4);
    (void)battle_script_runtime61::Apply(0x82a9bdb0, m, d, s);
    m.WriteU8(owner + 109, (m.ReadU32(Address(s.r[3]) + 64) >> 6) & 1);
    s.r[1] += 96;
    s.r[31] = recovery_abi::ReadU64(m, old - 16);
    s.lr = m.ReadU32(old - 8);
    return true;
  }
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
  (void)battle_evaluation_effects61::Apply(0x82b21878, m, d, s);
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
