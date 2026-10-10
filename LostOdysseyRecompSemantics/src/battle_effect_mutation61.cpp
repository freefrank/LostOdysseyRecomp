#include "lo_semantics/battle_effect_mutation61.h"
#include "lo_semantics/battle_random_range61.h"
#include "lo_semantics/battle_action_eligibility61.h"
#include "lo_semantics/battle_evaluation_chance61.h"
#include "lo_semantics/battle_evaluation_gates61.h"
#include "lo_semantics/battle_property_mutation61.h"
#include "lo_semantics/battle_action_results61.h"
#include "lo_semantics/battle_result_application61.h"
#include "lo_semantics/battle_action_adjustments61.h"
#include "lo_semantics/battle_group_gauge61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <limits>
namespace lo::semantic::gpu::battle_effect_mutation61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_effect_mutation61::Apply(e, m, d, s) &&
      !battle_random_range61::Apply(e, m, d, s) &&
      !battle_action_eligibility61::Apply(e, m, d, s) &&
      !battle_evaluation_chance61::Apply(e, m, d, s) &&
      !battle_evaluation_gates61::Apply(e, m, d, s) &&
      !battle_property_mutation61::Apply(e, m, d, s) &&
      !battle_action_results61::Apply(e, m, d, s) &&
      !battle_result_application61::Apply(e, m, d, s) &&
      !battle_action_adjustments61::Apply(e, m, d, s) &&
      !battle_group_gauge61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  auto owner = Address(s.r[3]);
  if (e == 0x82b11660) {
    auto index = m.ReadU32(owner + 108);
    if (std::int32_t(index) < 7) {
      s.ctr = m.ReadU32(owner + 4 * (index + 215));
      d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    } else
      m.WriteU8(owner + 208, 0);
    return true;
  }
  unsigned frame = 112, first = 30, literal = 0;
  switch (e) {
  case 0x82b0ee40:
  case 0x82b0ec18:
  case 0x82b0f7d0:
  case 0x82b11758:
    frame = 128;
    first = 28;
    literal = 80;
    break;
  case 0x82b0fdb0:
    frame = 128;
    first = 29;
    literal = 80;
    break;
  case 0x82b104a8:
  case 0x82b0fcf0:
  case 0x82b0ea88:
    frame = 128;
    first = 29;
    literal = 80;
    break;
  case 0x82b10548:
    frame = 128;
    first = 28;
    literal = 84;
    break;
  case 0x82b0d378:
  case 0x82b0ddf8:
  case 0x82b10dd8:
  case 0x82b0dd20:
    break;
  default:
    return false;
  }
  auto old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  if (literal)
    m.WriteU32(sp + literal, 0x8204a1d8);
  auto fp = [&]() {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
  };
  auto trunc = [](float value) {
    return value > double(std::numeric_limits<std::int32_t>::max())
               ? std::numeric_limits<std::int32_t>::max()
           : !(value >= -2147483648.) ? std::numeric_limits<std::int32_t>::min()
                                      : std::int32_t(value);
  };
  auto chance = [&](unsigned method) {
    s.r[3] = owner;
    Call(method, m, d, s);
    return (Address(s.r[3]) & 255) != 0;
  };
  auto eligible = [&](unsigned kind) {
    s.r[3] = m.ReadU32(0x8324570c);
    s.r[4] = m.ReadU32(owner + 4);
    s.r[5] = m.ReadU32(owner + 8);
    s.r[6] = kind;
    s.r[7] = 1;
    s.r[8] = m.ReadU8(owner + 200);
    Call(0x82ace408, m, d, s);
    return (Address(s.r[3]) & 255) != 0;
  };
  auto mark = [&]() {
    s.r[3] = m.ReadU32(0x832cb790);
    Call(0x82b2b248, m, d, s);
  };
  auto property = [&](unsigned method, unsigned bank, unsigned mask) {
    s.r[3] = m.ReadU32(owner + 8);
    s.r[4] = m.ReadU32(owner + bank);
    s.r[5] = m.ReadU32(owner + mask);
    s.r[6] = 0;
    Call(method, m, d, s);
  };
  if (e == 0x82b0ee40 || e == 0x82b0ec18 || e == 0x82b0f7d0) {
    bool removing = e == 0x82b0ee40;
    bool allowed = eligible(2) || m.ReadU32(owner + 92) != (removing ? 2u : 1u);
    if (allowed)
      allowed = eligible(1) || m.ReadU32(owner + 92) != (removing ? 1u : 2u);
    if (!allowed)
      m.WriteU8(owner + 208, 0);
    else if (m.ReadU32(owner + 92) != (removing ? 1u : 2u) ||
             chance(0x82b08ea8)) {
      mark();
      if (removing) {
        property(0x82ac91d8, 92, 100);
        if (m.ReadU32(owner + 104))
          property(0x82ac91d8, 96, 104);
      } else {
        auto add = [&](unsigned bank, unsigned mask, unsigned value,
                       unsigned option) {
          s.r[3] = m.ReadU32(owner + 8);
          s.r[4] = m.ReadU32(owner + bank);
          s.r[5] = m.ReadU32(owner + mask);
          s.r[6] = m.ReadU32(owner + value);
          s.r[7] = option;
          Call(0x82ac8968, m, d, s);
        };
        add(92, 100, 108, e == 0x82b0ec18 && m.ReadU32(owner + 120) != 0);
        if (e == 0x82b0ec18) {
          if (m.ReadU32(owner + 104))
            add(96, 104, 112, m.ReadU32(owner + 120) != 0);
        } else {
          property(0x82ac8ae8, 96, 104);
          s.r[3] = m.ReadU32(owner + 8);
          s.r[4] = m.ReadU32(owner + 96) == 5 ? 6 : 5;
          s.r[5] = m.ReadU32(owner + 104);
          Call(0x82ac91d8, m, d, s);
        }
      }
    }
  } else if (e == 0x82b11758) {
    if (!eligible(1))
      m.WriteU8(owner + 208, 0);
    else {
      if (chance(0x82b08e28)) {
        mark();
        s.r[3] = m.ReadU32(0x83264558);
        s.r[4] = 1;
        s.r[5] = m.ReadU32(owner + 108);
        s.r[6] = 97;
        s.r[7] = m.ReadU32(m.ReadU32(owner + 4) + 64);
        Call(0x82aa0740, m, d, s);
        s.r[6] = s.r[3];
        s.r[3] = m.ReadU32(owner + 8);
        s.r[4] = m.ReadU32(owner + 92);
        s.r[5] = m.ReadU32(owner + 100);
        s.r[7] = 0;
        s.r[8] = m.ReadU32(owner + 120);
        Call(0x82ac8ec8, m, d, s);
        auto source = m.ReadU32(owner + 4);
        s.r[3] = m.ReadU32(owner + 100);
        Call(0x82ac84b8, m, d, s);
        m.WriteU32(m.ReadU32(owner + 8) +
                       4 * (68 * m.ReadU32(owner + 92) + Address(s.r[3]) + 91),
                   m.ReadU32(source + 64));
      }
      m.WriteU8(owner + 208, 1);
    }
  } else if (e == 0x82b0fdb0) {
    if (!eligible(1))
      m.WriteU8(owner + 208, 0);
    else {
      mark();
      for (unsigned id : {32u, 33u, 34u, 35u, 36u, 37u, 38u, 160u, 161u, 162u,
                          163u, 164u, 165u, 224u, 228u, 229u}) {
        s.r[3] = m.ReadU32(owner + 8);
        s.r[4] = id;
        Call(0x82ac9000, m, d, s);
      }
    }
  } else if (e == 0x82b104a8) {
    auto source = m.ReadU32(owner + 4);
    m.WriteU32(source + 124, m.ReadU32(source + 124) | 0x40000);
    if (chance(0x82b08ea8)) {
      mark();
      s.r[3] = m.ReadU32(owner + 8);
      s.r[4] = 0;
      s.r[5] = 1;
      s.r[6] = 1;
      Call(0x82ac9be0, m, d, s);
    }
  } else if (e == 0x82b0fcf0) {
    if (!eligible(2))
      m.WriteU8(owner + 208, 0);
    else {
      mark();
      property(0x82aca6f0, 92, 100);
      property(0x82ac91d8, 96, 104);
    }
  } else if (e == 0x82b0ea88) {
    if (!eligible(2) && !m.ReadU32(owner + 92) && !m.ReadU32(owner + 116))
      m.WriteU8(owner + 208, 0);
    else {
      mark();
      property(0x82ac91d8, 92, 100);
      if (m.ReadU32(owner + 104))
        property(0x82ac91d8, 96, 104);
    }
  } else if (e == 0x82b0d378) {
    if (!eligible(1))
      m.WriteU8(owner + 208, 0);
    else {
      auto index = m.ReadU32(owner + 112);
      if (std::int32_t(index) < 21) {
        s.r[3] = owner;
        s.ctr = m.ReadU32(owner + 4 * (index + 194));
        d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
      } else
        m.WriteU8(owner + 208, 0);
      m.WriteU8(owner + 202, 0);
    }
  } else if (e == 0x82b10548) {
    if (!chance(0x82b09050))
      m.WriteU32(owner + 124, 0);
    else {
      mark();
      property(0x82ac8ae8, 92, 100);
      fp();
      auto value = unsigned(trunc(std::bit_cast<float>(m.ReadU32(owner + 80))));
      m.WriteU32(sp + 80, value);
      s.r[3] = m.ReadU32(owner + 100);
      Call(0x82ac84b8, m, d, s);
      auto destination =
          m.ReadU32(owner + 8) +
          4 * (68 * m.ReadU32(owner + 92) + Address(s.r[3]) + 59);
      m.WriteU32(destination, value);
    }
  } else if (e == 0x82b10dd8) {
    mark();
    s.r[3] = m.ReadU32(0x832aeb00);
    s.r[4] = m.ReadU32(owner + 8);
    Call(0x82ac7550, m, d, s);
    if ((Address(s.r[3]) & 255) == 1) {
      auto payload = m.ReadU32(owner + 120), resource = m.ReadU32(owner + 8);
      if (!payload)
        m.WriteU32(resource + 188, 0);
      else {
        recovery_abi::WriteU64(
            m, sp + 80, std::uint64_t(std::int64_t(std::int32_t(payload))));
        fp();
        auto scaled = float(std::bit_cast<float>(m.ReadU32(resource + 2592)) *
                            float(std::int32_t(payload)));
        auto amount = unsigned(
            trunc(float(scaled * std::bit_cast<float>(m.ReadU32(0x82000d7c)))));
        m.WriteU32(sp + 80, amount);
        s.r[3] = m.ReadU32(0x832aeb00);
        s.r[4] = resource;
        s.r[5] = amount;
        Call(0x82ac7178, m, d, s);
      }
    }
  } else {
    bool allowed = true;
    if (e == 0x82b0dd20 && !eligible(1)) {
      m.WriteU8(owner + 208, 0);
      allowed = false;
    }
    if (allowed && chance(0x82b08e28) && m.ReadU32(owner + 184) != 1) {
      mark();
      auto result = m.ReadU32(0x832cb790);
      fp();
      float input;
      if (e == 0x82b0ddf8) {
        auto integer = std::int32_t(m.ReadU32(owner + 108));
        recovery_abi::WriteU64(m, sp + 80,
                               std::uint64_t(std::int64_t(integer)));
        input = float(integer);
      } else
        input = std::bit_cast<float>(m.ReadU32(0x82000e50));
      s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(input));
      s.r[3] = result;
      s.r[4] = m.ReadU32(owner + 8);
      s.r[6] = e == 0x82b0ddf8 ? 7 : 6;
      Call(0x82b2b9e0, m, d, s);
      fp();
      auto index = 116 * m.ReadU32(result + 12) + m.ReadU32(result + 24) + 3726;
      m.WriteU32(
          m.ReadU32(result + 20) + 4 * index,
          std::bit_cast<unsigned>(float(std::bit_cast<double>(s.fpr_bits[1]))));
      m.WriteU32(owner + 172, m.ReadU32(owner + 32));
    }
  }
  if (literal)
    m.WriteU32(sp + literal, 0x8204a1d8);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_effect_mutation61
