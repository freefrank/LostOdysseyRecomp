#include "lo_semantics/battle_evaluation_gates61.h"
#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_evaluation_gates61 {
using recovery_abi::Address;
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  auto owner = Address(s.r[3]);
  if (e == 0x82acf878 || e == 0x82b08b20) {
    m.WriteU8(owner + 208, e == 0x82b08b20);
    return true;
  }
  bool simple = e == 0x82b08b30 || e == 0x82b08ab8;
  bool paired =
      e == 0x82b0eb68 || e == 0x82b0c430 || e == 0x82b0e9d0 || e == 0x82b0ed68;
  if (!simple && !paired && e != 0x82b0b0f0 && e != 0x82b102d8 &&
      e != 0x82b10618)
    return false;
  unsigned frame = simple ? 96 : 128, first = simple ? 31 : 29;
  auto old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  if (!simple)
    m.WriteU32(sp + 80, 0x8204a1d8);
  auto virtualCall = [&](unsigned slot) {
    s.r[3] = m.ReadU32(owner + 8);
    s.ctr = m.ReadU32(m.ReadU32(Address(s.r[3])) + slot);
    d.guest.CallIndirect(Address(s.ctr), m, s);
    return Address(s.r[3]);
  };
  bool result = false;
  if (simple) {
    result = virtualCall(380) == 0;
    if (result && e == 0x82b08ab8)
      result = m.ReadU32(m.ReadU32(owner + 4) + 64) !=
               m.ReadU32(m.ReadU32(owner + 8) + 64);
  } else if (e == 0x82b102d8)
    result = virtualCall(292) != 0;
  else if (e == 0x82b10618) {
    if (virtualCall(380) == 0) {
      s.r[3] = owner;
      d.guest.CallDirect(0x82b09050, m, s);
      result = (Address(s.r[3]) & 255) != 0;
    }
  } else if (e == 0x82b0b0f0) {
    m.WriteU8(owner + 208, 1);
    s.r[3] = m.ReadU32(owner + 4);
    s.r[4] = 114;
    (void)battle_action_readiness61::Apply(0x8238e368, m, d, s);
    result = (Address(s.r[3]) & 255) != 1;
  } else {
    bool eligible = e != 0x82b0ed68 || virtualCall(380) == 0;
    if (eligible) {
      auto query = [&](unsigned second) {
        s.r[3] = m.ReadU32(owner + 8);
        s.r[4] = m.ReadU32(owner + 92 + second * 4);
        s.r[5] = m.ReadU32(owner + 100 + second * 4);
        unsigned call = 0x82ac90e8;
        if (e == 0x82b0c430 || e == 0x82b0ed68) {
          s.r[6] = m.ReadU32(owner + 108 + second * 4);
          call = 0x82ac8978;
        }
        if (e == 0x82b0e9d0) {
          s.r[6] = 0;
          call = 0x82aca700;
        }
        d.guest.CallDirect(call, m, s);
        return (Address(s.r[3]) & 255) == 1;
      };
      result = query(0);
      if (!result && m.ReadU32(owner + 104) != 0)
        result = query(1);
    }
  }
  m.WriteU8(owner + 208, result);
  if (!simple)
    m.WriteU32(sp + 80, 0x8204a1d8);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_evaluation_gates61
