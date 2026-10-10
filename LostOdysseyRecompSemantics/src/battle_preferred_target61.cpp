#include "lo_semantics/battle_preferred_target61.h"
#include "lo_semantics/battle_random_range61.h"
#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::battle_preferred_target61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_random_range61::Apply(e, m, d, s) &&
      !battle_action_readiness61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
unsigned List(GuestMemory &m, Dependencies d, Registers &s) {
  Call(0x82380a18, m, d, s);
  Call(0x8238e2f8, m, d, s);
  return Address(s.r[3]);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82ac8228)
    return false;
  auto old = Address(s.r[1]), owner = Address(s.r[3]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = 19; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  recovery_abi::WriteU64(m, old - 120, s.fpr_bits[31]);
  if (s.cached_fp_control & 0x8040) {
    s.cached_fp_control &= ~0x8040u;
    d.fp.SetHostFpControl(s.cached_fp_control);
  }
  s.r[1] -= 304;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  m.WriteU32(sp + 80, 0x8204a1d8);
  unsigned countA = 0, countB = 0, countAll = 0;
  auto list = List(m, d, s);
  auto zero = std::bit_cast<float>(m.ReadU32(0x82000e50));
  s.fpr_bits[31] = std::bit_cast<std::uint64_t>(double(zero));
  for (unsigned i = 0;
       std::int32_t(i) < std::int32_t(m.ReadU32(List(m, d, s) + 4)); ++i) {
    auto resource = m.ReadU32(m.ReadU32(list) + 4 * i);
    if (!(m.ReadU32(resource + 124) & 0x10000000u))
      continue;
    s.r[3] = resource;
    s.r[4] = 0;
    Call(0x8238e368, m, d, s);
    if (Address(s.r[3]))
      continue;
    auto hp = std::bit_cast<float>(m.ReadU32(resource + 2588));
    s.fpr_bits[0] = std::bit_cast<std::uint64_t>(double(hp));
    if (!(hp > zero) || !m.ReadU32(resource + 132))
      continue;
    auto id = m.ReadU32(resource + 64);
    if (m.ReadU32(resource + 124) & 0x40000000u)
      m.WriteU32(sp + 128 + 4 * countA++, id);
    else
      m.WriteU32(sp + 96 + 4 * countB++, id);
    m.WriteU32(sp + 160 + 4 * countAll++, id);
  }
  unsigned pool = sp + 160, count = countAll, tag = 62;
  if (countA && countB) {
    s.r[3] = m.ReadU32(0x83264558);
    s.r[4] = m.ReadU32(0x83213428 + 4 * m.ReadU32(owner + 32));
    s.r[5] = 63;
    s.r[6] = 0;
    Call(0x82aa0838, m, d, s);
    if ((Address(s.r[3]) & 255) == 1) {
      pool = sp + 96;
      count = countB;
      tag = 64;
    } else {
      pool = sp + 128;
      count = countA;
      tag = 65;
    }
  }
  s.r[3] = m.ReadU32(0x83264558);
  s.r[4] = 0;
  s.r[5] = count - 1;
  s.r[6] = tag;
  s.r[7] = 0;
  Call(0x82aa0740, m, d, s);
  s.r[3] = m.ReadU32(pool + 4 * Address(s.r[3]));
  m.WriteU32(sp + 80, 0x8204a1d8);
  s.r[1] += 304;
  s.fpr_bits[31] = recovery_abi::ReadU64(m, old - 120);
  for (unsigned i = 19; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_preferred_target61
