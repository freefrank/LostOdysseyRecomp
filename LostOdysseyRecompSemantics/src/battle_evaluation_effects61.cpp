#include "lo_semantics/battle_action_eligibility61.h"
#include "lo_semantics/battle_action_results61.h"
#include "lo_semantics/battle_random_range61.h"
#include "lo_semantics/battle_evaluation_chance61.h"
#include "lo_semantics/battle_property_mutation61.h"
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
  if (e == 0x82b11248 || e == 0x82b11350 || e == 0x82b114b8) {
    auto old = Address(s.r[1]), owner = Address(s.r[3]);
    unsigned first = e == 0x82b11248 ? 28 : 30,
             frame = e == 0x82b11248 ? 128 : 112;
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= frame;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    if (e == 0x82b11248)
      m.WriteU32(sp + 80, 0x8204a1d8);
    unsigned actor = 0;
    if (e == 0x82b11248) {
      s.r[3] = 0x832c9c54;
      s.r[4] = m.ReadU32(owner + 8);
      (void)battle_script_runtime61::Apply(0x82a9bdb0, m, d, s);
      actor = Address(s.r[3]);
    }
    s.r[3] = m.ReadU32(0x8324570c);
    s.r[4] = m.ReadU32(owner + 4);
    s.r[5] = m.ReadU32(owner + 8);
    s.r[6] = e == 0x82b114b8 ? 2 : 1;
    s.r[7] = e == 0x82b114b8 ? 0 : 1;
    s.r[8] = m.ReadU8(owner + 200);
    (void)battle_action_eligibility61::Apply(0x82ace408, m, d, s);
    if (!(Address(s.r[3]) & 255))
      m.WriteU8(owner + 208, 0);
    else {
      bool apply = true;
      if (e == 0x82b11248) {
        apply = (m.ReadU32(actor + 64) & 512) == 0;
        if (apply) {
          s.r[3] = owner;
          (void)battle_evaluation_chance61::Apply(0x82b08ea8, m, d, s);
          apply = (Address(s.r[3]) & 255) != 0;
        }
        if (apply)
          apply = (m.ReadU32(m.ReadU32(owner + 8) + 4876) & 1) == 0;
      } else if (e == 0x82b11350) {
        if (m.ReadU32(owner + 36))
          apply = m.ReadU32(m.ReadU32(owner + 40) + 14888) == 1;
        else {
          bool blocked = m.ReadU32(m.ReadU32(owner + 8) + 68) == 52 ||
                         (m.ReadU32(m.ReadU32(0x832c9c54 + 44) + 28) & 0x4000);
          unsigned code = 250;
          if (!blocked) {
            s.r[3] = owner;
            (void)battle_evaluation_chance61::Apply(0x82b08e28, m, d, s);
            blocked = (Address(s.r[3]) & 255) == 0;
            if (!blocked)
              code = 15;
          }
          auto report = m.ReadU32(0x832cb798);
          m.WriteU32(report + 20, 2);
          m.WriteU32(m.ReadU32(report + 16) + 264, 2);
          m.WriteU32(report + 24, code);
          m.WriteU32(m.ReadU32(report + 16) + 268, code);
          if (blocked)
            apply = false;
        }
      } else if (m.ReadU32(m.ReadU32(owner + 4) + 124) & 0x10000000u) {
        if (m.ReadU32(owner + 36))
          apply = m.ReadU32(m.ReadU32(owner + 40) + 14888) == 1;
        else {
          if (m.ReadU32(owner + 20) == 5) {
            auto scene = m.ReadU32(0x832ca0e0 + 5784);
            bool boosted = scene == 62 || scene == 63 || scene == 64 ||
                           scene == 194 || (scene >= 272 && scene <= 275);
            s.r[3] = m.ReadU32(0x83264558);
            s.r[4] = boosted ? 80 : 30;
            s.r[5] = 2;
            s.r[6] = m.ReadU32(m.ReadU32(owner + 4) + 64);
            (void)battle_random_range61::Apply(0x82aa0838, m, d, s);
          } else {
            s.r[3] = owner;
            (void)battle_evaluation_chance61::Apply(0x82b08e28, m, d, s);
          }
          apply = (Address(s.r[3]) & 255) != 0;
          if (apply) {
            d.guest.CallDirect(0x82380a18, m, s);
            d.guest.CallDirect(0x82389b78, m, s);
            auto address = Address(s.r[3]) + 148, flags = m.ReadU32(address);
            m.WriteU32(address,
                       (flags & ~0x3000u) | ((flags + 0x1000u) & 0x3000u));
          }
        }
      }
      if (apply) {
        s.r[3] = m.ReadU32(0x832cb790);
        (void)battle_action_results61::Apply(0x82b2b248, m, d, s);
        auto target = m.ReadU32(owner + 8);
        if (e == 0x82b11248) {
          m.WriteU32(target + 124, m.ReadU32(target + 124) | 0x04000000u);
          s.r[3] = m.ReadU32(owner + 8);
          s.r[4] = 0;
          s.r[5] = 1;
          s.r[6] = 1;
          d.guest.CallDirect(0x82ac9be0, m, s);
        } else {
          m.WriteU32(target + 132, 0);
          m.WriteU32(target + 124,
                     m.ReadU32(target + 124) |
                         (e == 0x82b11350 ? 0x02200000u : 0x00200000u));
        }
      }
    }
    if (e == 0x82b11248)
      m.WriteU32(sp + 80, 0x8204a1d8);
    s.r[1] += frame;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  unsigned mode = 0, frame = 96, first = 31;
  bool literal = false, late = false;
  switch (e) {
  case 0x82b0cd88:
    frame = 128;
    first = 28;
    literal = true;
    break;
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
  if (e == 0x82b0cd88) {
    s.r[3] = owner;
    (void)battle_evaluation_chance61::Apply(0x82b08ea8, m, d, s);
    if (Address(s.r[3]) & 255) {
      auto mask = m.ReadU32(owner + 100), bank = m.ReadU32(owner + 92);
      if (mask && (bank == 0 || bank == 5 || bank == 6)) {
        s.r[3] = m.ReadU32(owner + 8);
        s.r[4] = bank;
        s.r[5] = mask;
        s.r[6] = 0;
        (void)battle_property_mutation61::Apply(
            bank == 0 ? 0x82aca6f0 : 0x82ac8ae8, m, d, s);
        if (bank == 0)
          m.WriteU32(owner + 196, m.ReadU32(owner + 100));
        else {
          s.r[3] = m.ReadU32(owner + 8);
          s.r[4] = m.ReadU32(owner + 96);
          s.r[5] = m.ReadU32(owner + 104);
          (void)battle_property_mutation61::Apply(0x82ac91d8, m, d, s);
        }
      }
      if (s.cached_fp_control & 0x8040) {
        s.cached_fp_control &= ~0x8040u;
        d.fp.SetHostFpControl(s.cached_fp_control);
      }
      auto value =
          double(std::bit_cast<float>(m.ReadU32(m.ReadU32(0x832ca0cc) + 24)));
      s.fpr_bits[0] = std::bit_cast<std::uint64_t>(value);
      m.WriteU32(owner + 172, std::bit_cast<unsigned>(float(value)));
    }
  } else if (e == 0x82b0d088) {
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
    (void)battle_property_mutation61::Apply(0x82ac8ec8, m, d, s);
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
