#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/battle_property_mutation61.h"
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
      e != 0x82b10618 && e != 0x82b09050 && e != 0x82b0fb08 &&
      e != 0x82b0f310 && e != 0x82b0ac68 && e != 0x82b0f6a8 && e != 0x82b11690)
    return false;
  unsigned frame = simple ? 96 : 128, first = simple ? 31 : 29;
  if (e == 0x82b09050)
    first = 28;
  if (e == 0x82b0fb08) {
    first = 25;
    frame = 176;
  }
  if (e == 0x82b11690) {
    first = 31;
    frame = 96;
  }
  bool literal = !simple && e != 0x82b11690;
  auto old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  if (literal)
    m.WriteU32(sp + 80, 0x8204a1d8);
  auto virtualCall = [&](unsigned slot) {
    s.r[3] = m.ReadU32(owner + 8);
    s.ctr = m.ReadU32(m.ReadU32(Address(s.r[3])) + slot);
    d.guest.CallIndirect(Address(s.ctr), m, s);
    return Address(s.r[3]);
  };
  bool result = false;
  if (e == 0x82b11690) {
    auto category = m.ReadU32(owner + 108);
    result = category != 0;
    bool manager = category == 1 || category == 4 ||
                   (category == 6 &&
                    (m.ReadU32(m.ReadU32(owner + 4) + 124) & 0x10000000u));
    if (manager) {
      d.guest.CallDirect(0x82380a18, m, s);
      d.guest.CallDirect(0x82389b78, m, s);
      result = (m.ReadU32(Address(s.r[3]) + 148) & 0x4000) == 0;
    }
  } else if (e == 0x82b0f6a8) {
    if (virtualCall(380) == 0) {
      auto source = m.ReadU32(owner + 4), target = m.ReadU32(owner + 8);
      bool same = m.ReadU32(owner + 20) == 3 && m.ReadU32(owner + 24) == 17 &&
                  m.ReadU32(source + 64) == m.ReadU32(target + 64);
      if (!same) {
        s.r[3] = target;
        s.r[4] = m.ReadU32(owner + 92);
        s.r[5] = m.ReadU32(owner + 100);
        s.r[6] = m.ReadU32(owner + 108);
        s.r[7] = m.ReadU32(owner + 112);
        s.r[8] = m.ReadU32(owner + 120);
        (void)battle_property_mutation61::Apply(0x82ac8ed8, m, d, s);
        result = (Address(s.r[3]) & 255) == 1;
        if (!result && m.ReadU32(owner + 92) == 7) {
          s.r[3] = 225;
          (void)battle_script_actions61::Apply(0x8238aab0, m, d, s);
          result = (Address(s.r[3]) & m.ReadU32(owner + 100)) &&
                   ((m.ReadU32(source + 124) ^ m.ReadU32(target + 124)) &
                    0x10000000u);
        }
      }
    }
  } else if (e == 0x82b09050) {
    result = true;
    for (unsigned id = 198; id <= 201; ++id) {
      s.r[3] = id;
      (void)battle_script_actions61::Apply(0x8238aab0, m, d, s);
      s.r[5] = s.r[3];
      s.r[3] = m.ReadU32(owner + 8);
      s.r[4] = m.ReadU32(owner + 92);
      s.r[6] = 0;
      (void)battle_property_mutation61::Apply(0x82ac8af8, m, d, s);
      if (!(Address(s.r[3]) & 255)) {
        result = false;
        break;
      }
    }
  } else if (e == 0x82b0fb08) {
    unsigned sum = 0;
    for (unsigned i = 0; i < 3; ++i) {
      m.WriteU32(sp + 88 + 4 * i, 0);
      auto bank = m.ReadU32(owner + 132 + 8 * i);
      if (bank != 255) {
        s.r[3] = m.ReadU32(owner + 8);
        s.r[4] = bank;
        s.r[5] = m.ReadU32(owner + 136 + 8 * i);
        (void)battle_property_mutation61::Apply(0x82ac85a0, m, d, s);
        m.WriteU32(sp + 88 + 4 * i, Address(s.r[3]));
      }
      sum += m.ReadU32(sp + 88 + 4 * i);
    }
    result = sum != 0;
  } else if (e == 0x82b0ac68) {
    result = virtualCall(380) == 0;
    if (!result) {
      for (unsigned i = 0; i < 2; ++i) {
        if (m.ReadU32(owner + 92 + 4 * i) != 0)
          continue;
        s.r[3] = 15;
        (void)battle_script_actions61::Apply(0x8238aab0, m, d, s);
        if (Address(s.r[3]) & m.ReadU32(owner + 100 + 4 * i)) {
          result = true;
          break;
        }
      }
    }
  } else if (e == 0x82b0f310) {
    if (virtualCall(380) == 0) {
      s.r[3] = m.ReadU32(owner + 8);
      s.r[4] = m.ReadU32(owner + 92);
      s.r[5] = m.ReadU32(owner + 100);
      s.r[6] = 0;
      (void)battle_property_mutation61::Apply(0x82ac8af8, m, d, s);
      result = (Address(s.r[3]) & 255) == 1;
      if (!result) {
        if (!m.ReadU32(owner + 104))
          result = true;
        else {
          s.r[3] = m.ReadU32(owner + 8);
          s.r[4] = m.ReadU32(owner + 96);
          s.r[5] = m.ReadU32(owner + 104);
          (void)battle_property_mutation61::Apply(0x82ac90e8, m, d, s);
          result = (Address(s.r[3]) & 255) == 1;
        }
      }
    }
  } else if (simple) {
    result = virtualCall(380) == 0;
    if (result && e == 0x82b08ab8)
      result = m.ReadU32(m.ReadU32(owner + 4) + 64) !=
               m.ReadU32(m.ReadU32(owner + 8) + 64);
  } else if (e == 0x82b102d8)
    result = virtualCall(292) != 0;
  else if (e == 0x82b10618) {
    if (virtualCall(380) == 0) {
      s.r[3] = owner;
      (void)battle_evaluation_gates61::Apply(0x82b09050, m, d, s);
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
        if (!battle_property_mutation61::Apply(call, m, d, s))
          d.guest.CallDirect(call, m, s);
        return (Address(s.r[3]) & 255) == 1;
      };
      result = query(0);
      if (!result && m.ReadU32(owner + 104) != 0)
        result = query(1);
    }
  }
  if (e == 0x82b09050)
    s.r[3] = result;
  else
    m.WriteU8(owner + 208, result);
  if (literal)
    m.WriteU32(sp + 80, 0x8204a1d8);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_evaluation_gates61
