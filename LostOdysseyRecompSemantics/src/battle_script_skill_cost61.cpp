#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/battle_script_skill_cost61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::battle_script_skill_cost61 {
namespace {
using recovery_abi::Address;
void Run(GuestMemory &m, Dependencies d, Registers &s, unsigned resource) {
  auto category = Address(s.r[4]), id = Address(s.r[5]), sp = Address(s.r[1]);
  m.WriteU32(sp + 80, 0x8204a1d8);
  unsigned result = 1;
  if (category == 2 || category == 3) {
    unsigned key;
    bool blocked = false;
    if (category == 2) {
      auto bank = m.ReadU32(0x83213468), mask = m.ReadU32(0x8321346c);
      blocked = (m.ReadU32(resource + 272 * bank + 232) & mask) != 0;
      auto record = m.ReadU32(0x83264984) + 96 * id;
      key = blocked ? 0 : m.ReadU32(record + 16);
    } else
      key = m.ReadU32(m.ReadU32(0x832649c0) + 104 * id + 24);
    if (blocked)
      result = 0;
    else {
      s.r[3] = m.ReadU32(0x83291dc0);
      s.r[4] = resource;
      s.r[5] = key;
      (void)battle_script_skill_cost61::Apply(0x82ac1af0, m, d, s);
      auto cost = std::int32_t(Address(s.r[3]));
      recovery_abi::WriteU64(m, sp + 88, std::uint64_t(std::int64_t(cost)));
      if (s.cached_fp_control & 0x8040) {
        s.cached_fp_control &= ~0x8040u;
        d.fp.SetHostFpControl(s.cached_fp_control);
      }
      auto mp = std::bit_cast<float>(m.ReadU32(resource + 2616));
      auto number = float(cost);
      s.fpr_bits[0] = std::bit_cast<std::uint64_t>(double(mp));
      s.fpr_bits[13] = std::bit_cast<std::uint64_t>(double(number));
      result = category == 2 ? number <= mp : !(number > mp);
    }
  }
  s.r[3] = result;
  m.WriteU32(sp + 80, 0x8204a1d8);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82b08b80) {
    auto id = std::int32_t(Address(s.r[4]));
    s.r[3] = id >= 150 ? 9 : id >= 100 ? 8 : id >= 50 ? 7 : 6;
    return true;
  }
  if (e != 0x82ac9aa8 && e != 0x82ac1af0)
    return false;
  unsigned first = 28, frame = e == 0x82ac1af0 ? 128 : 144;
  auto owner = Address(s.r[3]), old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  if (e == 0x82ac1af0) {
    auto resource = Address(s.r[4]), cost = Address(s.r[5]),
         sp = Address(s.r[1]);
    m.WriteU32(sp + 80, 0x8204a1d8);
    s.r[3] = resource;
    s.r[4] = 236;
    (void)battle_action_readiness61::Apply(0x8238e368, m, d, s);
    if (Address(s.r[3]) & 255)
      cost -= unsigned(std::int32_t(cost) / 4);
    s.r[3] = resource;
    s.r[4] = 97;
    (void)battle_action_readiness61::Apply(0x8238e368, m, d, s);
    s.r[3] = (Address(s.r[3]) & 255) ? 0 : cost;
    m.WriteU32(sp + 80, 0x8204a1d8);
  } else
    Run(m, d, s, owner);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_skill_cost61
