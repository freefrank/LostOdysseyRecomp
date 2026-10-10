#include "lo_semantics/battle_manager_access61.h"
#include "lo_semantics/battle_resource_stats61.h"
#include "lo_semantics/battle_resource_growth61.h"
#include "lo_semantics/battle_script_runtime61.h"
#include "lo_semantics/battle_effect_mutation61.h"
#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/battle_effect_scaling61.h"
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
      !battle_script_runtime61::Apply(e, m, d, s) &&
      !battle_manager_access61::Apply(e, m, d, s) &&
      !battle_resource_growth61::Apply(e, m, d, s) &&
      !battle_resource_stats61::Apply(e, m, d, s) &&
      !battle_script_actions61::Apply(e, m, d, s) &&
      !battle_action_readiness61::Apply(e, m, d, s) &&
      !battle_effect_scaling61::Apply(e, m, d, s) &&
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
  case 0x82b0f3e0:
  case 0x82b0fbd0:
  case 0x82b10368:
    frame = 128;
    first = 28;
    literal = 80;
    break;
  case 0x82b0d7f0:
    frame = 160;
    first = 28;
    break;
  case 0x82b0d418:
  case 0x82b0c9e0:
  case 0x82b0c4e8:
    frame = 160;
    first = 27;
    break;
  case 0x82b10aa8:
    frame = 176;
    first = 27;
    literal = 80;
    break;
  case 0x82b12a98:
    frame = 160;
    first = 27;
    break;
  case 0x82b0ad38:
    frame = 128;
    first = 29;
    break;
  case 0x82b0e798:
  case 0x82b10e98:
    frame = 144;
    first = 27;
    literal = 80;
    break;
  case 0x82b0f920:
    frame = 192;
    first = 24;
    literal = 80;
    break;
  case 0x82b0db98:
    frame = 144;
    first = 28;
    break;
  case 0x82b0c880:
    frame = 144;
    first = 28;
    literal = 80;
    break;
  case 0x82b0af88:
    frame = 144;
    first = 27;
    literal = 80;
    break;
  case 0x82b110b8:
    frame = 160;
    first = 24;
    literal = 80;
    break;
  case 0x82b0aa70:
    frame = 160;
    first = 27;
    literal = 80;
    break;
  case 0x82b13220:
  case 0x82b0a568:
  case 0x82b0a698:
    break;
  case 0x82b0a928:
    frame = 128;
    first = 28;
    literal = 80;
    break;
  case 0x82b0a7a0:
    frame = 128;
    first = 29;
    break;
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
  unsigned savedFp = e == 0x82b0d7f0   ? 48
                     : e == 0x82b0a7a0 ? 40
                     : (e == 0x82b0aa70 || e == 0x82b12a98 || e == 0x82b10aa8 ||
                        e == 0x82b0c4e8 || e == 0x82b0c9e0 || e == 0x82b0d418)
                         ? 56
                         : 0;
  if (savedFp)
    recovery_abi::WriteU64(m, old - savedFp, s.fpr_bits[31]);
  unsigned savedFp30 = e == 0x82b0d7f0 ? 56
                       : (e == 0x82b10aa8 || e == 0x82b0c4e8 ||
                          e == 0x82b0c9e0 || e == 0x82b0d418)
                           ? 64
                           : 0;
  if (savedFp30)
    recovery_abi::WriteU64(m, old - savedFp30, s.fpr_bits[30]);
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
  auto eligible = [&](unsigned kind, unsigned detail = 1) {
    s.r[3] = m.ReadU32(0x8324570c);
    s.r[4] = m.ReadU32(owner + 4);
    s.r[5] = m.ReadU32(owner + 8);
    s.r[6] = kind;
    s.r[7] = detail;
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
  if (e == 0x82b0c4e8 || e == 0x82b0c9e0 || e == 0x82b0d7f0 ||
      e == 0x82b0d418) {
    if ((e == 0x82b0d7f0 || e == 0x82b0d418) && !eligible(1))
      m.WriteU8(owner + 208, 0);
    else {
      mark();
      bool missing = false;
      if (e == 0x82b0d7f0) {
        missing = true;
        s.r[3] = m.ReadU32(0x83291dc0);
        s.r[4] = m.ReadU32(owner + 4);
        Call(0x82abff20, m, d, s);
        auto level = std::int32_t(Address(s.r[3]));
        recovery_abi::WriteU64(m, sp + 88, std::uint64_t(std::int64_t(level)));
        fp();
        auto high = trunc(
            float(float(level) * std::bit_cast<float>(m.ReadU32(owner + 80))));
        m.WriteU32(sp + 80, unsigned(high));
        s.r[3] = m.ReadU32(0x83264558);
        s.r[4] = 1;
        s.r[5] = unsigned(high);
        s.r[6] = 10;
        s.r[7] = m.ReadU32(m.ReadU32(owner + 4) + 64);
        Call(0x82aa0740, m, d, s);
        auto value = std::int32_t(Address(s.r[3]));
        recovery_abi::WriteU64(m, sp + 88, std::uint64_t(std::int64_t(value)));
        fp();
        m.WriteU32(owner + 28, std::bit_cast<unsigned>(float(value)));
      }
      if (e == 0x82b0c9e0) {
        auto source = m.ReadU32(owner + 4);
        fp();
        auto maximum = std::bit_cast<float>(m.ReadU32(source + 2592)),
             current = std::bit_cast<float>(m.ReadU32(source + 2588));
        missing = !m.ReadU32(owner + 120);
        if (!missing) {
          auto divisor = std::int32_t(m.ReadU32(owner + 116));
          recovery_abi::WriteU64(m, sp + 88,
                                 std::uint64_t(std::int64_t(divisor)));
          missing = !(current > float(maximum / float(divisor)));
        }
        if (missing)
          m.WriteU32(owner + 28,
                     std::bit_cast<unsigned>(float(maximum - current)));
      }
      if (e == 0x82b0d418) {
        s.r[3] = m.ReadU32(0x83291dc0);
        s.r[4] = m.ReadU32(owner + 4);
        Call(0x82abff20, m, d, s);
        auto repetitions = std::int32_t(Address(s.r[3])) /
                           std::int32_t(m.ReadU32(owner + 108));
        fp();
        float total = std::bit_cast<float>(m.ReadU32(0x82000e50));
        for (std::int32_t i = 0; i < repetitions; ++i) {
          s.r[3] = owner;
          Call(0x82b09a48, m, d, s);
          fp();
          total = float(float(std::bit_cast<double>(s.fpr_bits[1])) + total);
        }
        if (!(m.ReadU32(m.ReadU32(owner + 4) + 124) & 0x10000000))
          total = float(total * std::bit_cast<float>(m.ReadU32(0x821baa74)));
        m.WriteU32(owner + 28, std::bit_cast<unsigned>(total));
      }
      if (!missing) {
        for (unsigned method :
             {0x82b09a48u, 0x82b09ca0u, 0x82b0a0d0u, 0x82b0a260u}) {
          if (e == 0x82b0d418 && (method == 0x82b09a48 || method == 0x82b09ca0))
            continue;
          if (e == 0x82b0c9e0 && method == 0x82b0a0d0)
            continue;
          s.r[3] = owner;
          Call(method, m, d, s);
          fp();
          m.WriteU32(owner + 28, std::bit_cast<unsigned>(float(
                                     std::bit_cast<double>(s.fpr_bits[1]))));
        }
        if (e == 0x82b0c9e0) {
          s.r[3] = owner;
          Call(0x82b098b0, m, d, s);
        } else {
          m.WriteU8(owner + 79, 0);
          if (!m.ReadU8(owner + 77)) {
            auto source = m.ReadU32(owner + 4);
            s.r[3] = m.ReadU32(0x83264558);
            s.r[4] = m.ReadU32(source + 5096);
            s.r[5] = 112;
            s.r[6] = m.ReadU32(source + 64);
            Call(0x82aa0838, m, d, s);
            if ((Address(s.r[3]) & 255) == 1)
              m.WriteU8(owner + 79, 1);
          }
        }
        fp();
        if (m.ReadU8(owner + 79) == 1)
          m.WriteU32(owner + 28,
                     std::bit_cast<unsigned>(
                         float(std::bit_cast<float>(m.ReadU32(owner + 28)) *
                               std::bit_cast<float>(m.ReadU32(0x82000e1c)))));
        if (e != 0x82b0d418) {
          s.r[3] = owner;
          Call(0x82b0a188, m, d, s);
          fp();
          m.WriteU32(owner + 28, std::bit_cast<unsigned>(float(
                                     std::bit_cast<double>(s.fpr_bits[1]))));
        }
        s.r[3] = owner;
        Call(0x82b0a3b0, m, d, s);
      }
      if (e == 0x82b0c9e0 || e == 0x82b0d7f0) {
        s.r[3] = owner;
        Call(0x82b0a3b0, m, d, s);
      }
      auto mode = m.ReadU32(owner + 184);
      auto getAmount = [&]() {
        return std::bit_cast<float>(m.ReadU32(owner + 32));
      };
      auto putAmount = [&](float value) {
        m.WriteU32(owner + 32, std::bit_cast<unsigned>(value));
      };
      auto apply = [&](unsigned kind, unsigned offset, float amount) {
        auto result = m.ReadU32(0x832cb790);
        s.r[3] = result;
        s.r[4] = m.ReadU32(owner + 8);
        s.r[6] = kind;
        fp();
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(amount));
        Call(0x82b2b9e0, m, d, s);
        fp();
        auto index =
            116 * m.ReadU32(result + 12) + m.ReadU32(result + 24) + offset;
        m.WriteU32(m.ReadU32(result + 20) + 4 * index,
                   std::bit_cast<unsigned>(
                       float(std::bit_cast<double>(s.fpr_bits[1]))));
      };
      fp();
      auto zero = std::bit_cast<float>(m.ReadU32(0x82000e50));
      if (mode == 1) {
        putAmount(zero);
        apply(0, 3726, zero);
      } else if (mode == 2 || mode == 8) {
        apply(1, 3730, getAmount());
        putAmount(zero);
      } else if (mode == 3) {
        auto mp =
            trunc(std::bit_cast<float>(m.ReadU32(m.ReadU32(owner + 8) + 2616)));
        m.WriteU32(sp + 88, unsigned(mp));
        recovery_abi::WriteU64(m, sp + 88, std::uint64_t(std::int64_t(mp)));
        auto available = float(mp), remaining = float(available - getAmount());
        if (!(remaining < zero)) {
          apply(2, 3734, getAmount());
          putAmount(zero);
        } else {
          apply(2, 3734, available);
          putAmount(float(getAmount() - available));
          apply(0, 3726, getAmount());
        }
      } else {
        if (mode == 6 && getAmount() != zero) {
          auto one = std::bit_cast<float>(m.ReadU32(0x82007784));
          auto rounded =
              trunc(float(float(getAmount() + one) *
                          std::bit_cast<float>(m.ReadU32(0x8201f9f0))));
          m.WriteU32(sp + 80, unsigned(rounded));
          recovery_abi::WriteU64(m, sp + 88,
                                 std::uint64_t(std::int64_t(rounded)));
          putAmount(float(rounded) < one ? one : float(rounded));
        }
        if (mode == 7)
          putAmount(zero);
        m.WriteU32(sp + 88, 0x8204a1d8);
        s.r[3] = m.ReadU32(owner + 8);
        s.r[5] = sp + 80;
        s.r[6] = sp + 84;
        fp();
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(getAmount()));
        Call(0x82ac9618, m, d, s);
        fp();
        putAmount(std::bit_cast<float>(m.ReadU32(sp + 80)));
        if (std::bit_cast<float>(m.ReadU32(sp + 84)) != zero) {
          auto result = m.ReadU32(0x832cb790),
               index =
                   116 * m.ReadU32(result + 12) + m.ReadU32(result + 24) + 3742;
          m.WriteU32(m.ReadU32(result + 20) + 4 * index, m.ReadU32(sp + 84));
        }
        m.WriteU32(sp + 88, 0x8204a1d8);
        apply(0, 3726, getAmount());
      }
      if (e == 0x82b0c9e0 || e == 0x82b0d7f0 || e == 0x82b0d418)
        m.WriteU32(owner + 172, m.ReadU32(owner + 32));
      else {
        auto target = m.ReadU32(owner + 8);
        m.WriteU32(target + 124, m.ReadU32(target + 124) | 0x40000000);
      }
    }
  } else if (e == 0x82b10aa8) {
    bool special = m.ReadU32(owner + 108) != 0, allowed = true;
    if (!special) {
      s.r[3] = m.ReadU32(owner + 8);
      s.r[4] = 142;
      Call(0x8238e368, m, d, s);
      allowed = (Address(s.r[3]) & 255) != 1;
    }
    if (allowed) {
      mark();
      fp();
      auto zero = std::bit_cast<float>(m.ReadU32(0x82000e50));
      auto multiplier = std::bit_cast<float>(m.ReadU32(0x82000e1c));
      auto calculate = [&](float amount, bool skipStat, bool decide) {
        fp();
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(amount));
        s.r[3] = owner;
        s.r[5] = skipStat;
        Call(0x82b09b30, m, d, s);
        fp();
        m.WriteU32(owner + 28, std::bit_cast<unsigned>(float(
                                   std::bit_cast<double>(s.fpr_bits[1]))));
        if (decide) {
          s.r[3] = owner;
          Call(0x82b097a0, m, d, s);
        }
        if (m.ReadU8(owner + 79) == 1) {
          fp();
          auto value = std::bit_cast<float>(m.ReadU32(owner + 28));
          m.WriteU32(owner + 28,
                     std::bit_cast<unsigned>(float(value * multiplier)));
        }
        s.r[3] = owner;
        Call(0x82b0a188, m, d, s);
        fp();
        m.WriteU32(owner + 28, std::bit_cast<unsigned>(float(
                                   std::bit_cast<double>(s.fpr_bits[1]))));
        s.r[3] = owner;
        Call(0x82b0a3b0, m, d, s);
      };
      auto apply = [&](unsigned resourceOffset, unsigned mode,
                       unsigned offset) {
        auto result = m.ReadU32(0x832cb790);
        s.r[3] = result;
        s.r[4] = m.ReadU32(owner + resourceOffset);
        s.r[6] = mode;
        fp();
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(
            double(std::bit_cast<float>(m.ReadU32(owner + 32))));
        Call(0x82b2b9e0, m, d, s);
        fp();
        auto index =
            116 * m.ReadU32(result + 12) + m.ReadU32(result + 24) + offset;
        m.WriteU32(m.ReadU32(result + 20) + 4 * index,
                   std::bit_cast<unsigned>(
                       float(std::bit_cast<double>(s.fpr_bits[1]))));
      };
      if (!special) {
        auto input = std::bit_cast<float>(m.ReadU32(owner + 80));
        if (input != zero) {
          calculate(input, false, true);
          apply(4, 1, 18);
          m.WriteU32(sp + 96, 0x8204a1d8);
          s.r[3] = m.ReadU32(owner + 8);
          s.r[5] = sp + 84;
          s.r[6] = sp + 88;
          fp();
          s.fpr_bits[1] = std::bit_cast<std::uint64_t>(
              double(std::bit_cast<float>(m.ReadU32(owner + 32))));
          Call(0x82ac9618, m, d, s);
          fp();
          m.WriteU32(owner + 32, m.ReadU32(sp + 84));
          if (std::bit_cast<float>(m.ReadU32(sp + 88)) != zero) {
            auto result = m.ReadU32(0x832cb790);
            auto index =
                116 * m.ReadU32(result + 12) + m.ReadU32(result + 24) + 3742;
            m.WriteU32(m.ReadU32(result + 20) + 4 * index, m.ReadU32(sp + 88));
          }
          m.WriteU32(sp + 96, 0x8204a1d8);
          apply(8, 0, 3726);
          m.WriteU32(owner + 172, m.ReadU32(owner + 32));
        }
      }
      auto amount = std::int32_t(m.ReadU32(owner + 120));
      if (special || amount) {
        recovery_abi::WriteU64(m, sp + 96, std::uint64_t(std::int64_t(amount)));
        fp();
        calculate(float(amount), true, false);
        if (!special)
          apply(4, 3, 26);
        apply(8, 2, 3734);
      }
    }
  } else if (e == 0x82b0f3e0) {
    bool allowed = eligible(2) || m.ReadU32(owner + 116) != 0;
    if (allowed)
      allowed = eligible(1) || m.ReadU32(owner + 116) != 1;
    if (!allowed)
      m.WriteU8(owner + 208, 0);
    else {
      if (m.ReadU32(owner + 20) == 3) {
        auto kind = m.ReadU32(owner + 24), bit = kind == 19  ? 2048u
                                                 : kind == 5 ? 1024u
                                                             : 0u;
        if (bit) {
          auto target = m.ReadU32(owner + 8), flags = m.ReadU32(target + 124);
          if (flags & bit)
            allowed = false;
          else
            m.WriteU32(target + 124, flags | bit);
        }
      }
      if (allowed && m.ReadU32(owner + 20) == 10 &&
          m.ReadU32(owner + 24) == 166) {
        auto target = m.ReadU32(owner + 8);
        if (m.ReadU32(target + 4876) == 0x7ff7c) {
          s.r[3] = m.ReadU32(0x832cb790);
          Call(0x82b2b298, m, d, s);
          allowed = false;
        } else if ((m.ReadU32(target + 60) & 255) == 2) {
          for (unsigned i = 0;
               std::int32_t(i) <
               std::int32_t(m.ReadU32(m.ReadU32(owner + 8) + 14660));
               ++i) {
            target = m.ReadU32(owner + 8);
            auto action = m.ReadU32(target + 14656) + 124208 * i;
            m.WriteU32(target + 76368, 0);
            m.WriteU32(action, 13);
            m.WriteU32(action + 4, 0);
            target = m.ReadU32(owner + 8);
            m.WriteU32(target + 124, m.ReadU32(target + 124) & ~512u);
          }
        }
      }
      if (allowed) {
        s.r[3] = m.ReadU32(owner + 8);
        s.r[4] = m.ReadU32(owner + 92);
        s.r[5] = m.ReadU32(owner + 100);
        s.r[6] = m.ReadU32(owner + 108);
        s.r[7] = m.ReadU32(owner + 112);
        s.r[8] = m.ReadU32(owner + 120);
        Call(0x82ac8ec8, m, d, s);
        if ((Address(s.r[3]) & 255) == 1)
          mark();
        if (m.ReadU32(owner + 100) & 0x2000000) {
          m.WriteU32(m.ReadU32(owner + 8) + 4952, 2);
          for (unsigned method : {0x82ac0588u, 0x82ac25e8u}) {
            s.r[3] = m.ReadU32(0x83291dc0);
            s.r[4] = m.ReadU32(owner + 8);
            Call(method, m, d, s);
          }
          auto target = m.ReadU32(owner + 8), stats = m.ReadU32(0x83291dc0);
          m.WriteU32(stats + 4, m.ReadU32(target + 5108));
          for (unsigned i = 0; i < 5; ++i)
            m.WriteU32(stats + 12 + 4 * i, m.ReadU32(target + 5116 + 4 * i));
          for (unsigned method : {0x82ac0888u, 0x82ac2468u, 0x82ac0620u}) {
            s.r[3] = m.ReadU32(0x83291dc0);
            s.r[4] = m.ReadU32(owner + 8);
            if (method == 0x82ac0620)
              s.r[5] = (m.ReadU32(Address(s.r[4]) + 124) >> 28) & 1;
            Call(method, m, d, s);
          }
        }
      }
    }
  } else if (e == 0x82b0fbd0) {
    if (!eligible(3) || chance(0x82b08ea8)) {
      s.r[3] = 7;
      Call(0x8238aab0, m, d, s);
      auto target = m.ReadU32(owner + 8);
      bool allowed = !(m.ReadU32(target + 4876) & Address(s.r[3]));
      if (!allowed) {
        s.r[3] = target;
        s.r[4] = 7;
        Call(0x8238e368, m, d, s);
        allowed = (Address(s.r[3]) & 255) != 0;
      }
      if (allowed) {
        mark();
        property(0x82ac9548, 92, 100);
        s.r[3] = m.ReadU32(owner + 100);
        Call(0x82ac84b8, m, d, s);
        if (!m.ReadU32(owner + 92))
          m.WriteU32(owner + 168, Address(s.r[3]));
        if (m.ReadU32(owner + 104))
          property(0x82ac9548, 96, 104);
      }
    }
  } else if (e == 0x82b0e798) {
    if (!eligible(1) && !m.ReadU32(owner + 92))
      m.WriteU8(owner + 208, 0);
    else if (chance(0x82b08ea8)) {
      bool proceed = true;
      if (m.ReadU32(owner + 20) == 3 && m.ReadU32(owner + 24) == 18) {
        auto target = m.ReadU32(owner + 8), flags = m.ReadU32(target + 124);
        if (flags & 4096)
          proceed = false;
        else
          m.WriteU32(target + 124, flags | 4096);
      }
      if (proceed) {
        auto mask = m.ReadU32(owner + 100);
        unsigned count = std::popcount(mask);
        if (count > 1) {
          s.r[3] = m.ReadU32(0x83264558);
          s.r[4] = 1;
          s.r[5] = count;
          s.r[6] = 11;
          s.r[7] = m.ReadU32(m.ReadU32(owner + 4) + 64);
          Call(0x82aa0740, m, d, s);
          auto ordinal = Address(s.r[3]);
          for (unsigned bit = 0; bit < 32; ++bit)
            if (mask & (1u << bit)) {
              if (!--ordinal) {
                mask = 1u << bit;
                break;
              }
            }
        }
        m.WriteU32(owner + 196, 0);
        auto add = [&](unsigned bank, unsigned bits) {
          s.r[3] = m.ReadU32(owner + 8);
          s.r[4] = bank;
          s.r[5] = bits;
          s.r[6] = m.ReadU32(owner + 120) != 0;
          Call(0x82aca6f0, m, d, s);
          return (Address(s.r[3]) & 255) == 1;
        };
        if (add(m.ReadU32(owner + 92), mask)) {
          s.r[3] = mask;
          Call(0x82ac84b8, m, d, s);
          if (!m.ReadU32(owner + 92))
            m.WriteU32(owner + 168, Address(s.r[3]));
          mark();
          if (!m.ReadU32(owner + 92))
            m.WriteU32(owner + 196, m.ReadU32(owner + 100));
        }
        auto secondary = m.ReadU32(owner + 104);
        if (secondary && add(m.ReadU32(owner + 96), secondary)) {
          mark();
          if (!m.ReadU32(owner + 96))
            m.WriteU32(owner + 196,
                       m.ReadU32(owner + 196) | m.ReadU32(owner + 100));
        }
      }
    }
  } else if (e == 0x82b0ad38) {
    m.WriteU8(owner + 203, 0);
    if (!eligible(2, 0))
      m.WriteU8(owner + 208, 0);
    else {
      auto source = m.ReadU32(owner + 4), target = m.ReadU32(owner + 8);
      if (m.ReadU32(source + 64) != m.ReadU32(target + 64)) {
        bool hp = m.ReadU32(owner + 108) != 0;
        fp();
        auto available =
            trunc(std::bit_cast<float>(m.ReadU32(source + (hp ? 2588 : 2616))));
        m.WriteU32(sp + 80, unsigned(available));
        if (available) {
          auto divisor = trunc(std::bit_cast<float>(m.ReadU32(owner + 80)));
          m.WriteU32(sp + 80, unsigned(divisor));
          auto quotient = available / divisor;
          recovery_abi::WriteU64(m, sp + 80,
                                 std::uint64_t(std::int64_t(quotient)));
          float amount = float(quotient);
          if (!(amount > std::bit_cast<float>(m.ReadU32(0x82000e50)))) {
            recovery_abi::WriteU64(m, sp + 80,
                                   std::uint64_t(std::int64_t(available)));
            amount = float(available);
          }
          m.WriteU32(owner + 32, std::bit_cast<unsigned>(amount));
          auto apply = [&](unsigned resource, unsigned mode, unsigned offset) {
            auto result = m.ReadU32(0x832cb790);
            s.r[3] = result;
            s.r[4] = resource;
            s.r[6] = mode;
            fp();
            s.fpr_bits[1] = std::bit_cast<std::uint64_t>(
                double(std::bit_cast<float>(m.ReadU32(owner + 32))));
            Call(0x82b2b9e0, m, d, s);
            fp();
            auto index =
                116 * m.ReadU32(result + 12) + m.ReadU32(result + 24) + offset;
            m.WriteU32(m.ReadU32(result + 20) + 4 * index,
                       std::bit_cast<unsigned>(
                           float(std::bit_cast<double>(s.fpr_bits[1]))));
          };
          apply(target, hp ? 1 : 3, hp ? 3730 : 3738);
          apply(m.ReadU32(owner + 4), hp ? 0 : 2, hp ? 14 : 22);
          mark();
        }
      }
    }
  } else if (e == 0x82b10e98) {
    if (chance(0x82b08e28)) {
      mark();
      if (!m.ReadU32(owner + 108)) {
        auto source = m.ReadU32(owner + 4);
        m.WriteU32(source + 4880, m.ReadU32(owner + 124));
        for (unsigned id : {258u, 259u, 260u, 261u}) {
          s.r[3] = m.ReadU32(owner + 4);
          s.r[4] = id;
          Call(0x82aca838, m, d, s);
        }
        s.r[3] = m.ReadU32(owner + 4);
        s.r[4] = m.ReadU32(owner + 92);
        s.r[5] = m.ReadU32(owner + 100);
        s.r[6] = m.ReadU32(owner + 120);
        Call(0x82aca830, m, d, s);
      } else {
        s.r[3] = m.ReadU32(0x83264558);
        s.r[4] = 0;
        s.r[5] = 3;
        s.r[6] = 15;
        s.r[7] = m.ReadU32(m.ReadU32(owner + 4) + 64);
        Call(0x82aa0740, m, d, s);
        unsigned bitMask = 1;
        for (std::int32_t count = std::int32_t(Address(s.r[3])); count > 0;
             --count)
          bitMask <<= 1;
        m.WriteU32(m.ReadU32(owner + 8) + 4880, bitMask);
        unsigned selected = 0;
        for (unsigned i = 0; i < 4; ++i)
          if (bitMask & (1u << i))
            selected = 198 + i;
        for (unsigned id : {198u, 199u, 200u, 201u}) {
          s.r[3] = m.ReadU32(owner + 8);
          s.r[4] = id;
          Call(0x82ac9000, m, d, s);
          s.r[3] = id;
          Call(0x82ac84e8, m, d, s);
          m.WriteU32(m.ReadU32(owner + 8) + 4 * (Address(s.r[3]) + 467), 0);
        }
        s.r[3] = m.ReadU32(owner + 8);
        s.r[4] = selected;
        Call(0x82ac8988, m, d, s);
        s.r[3] = selected;
        Call(0x82ac84e8, m, d, s);
        m.WriteU32(m.ReadU32(owner + 8) + 4 * (Address(s.r[3]) + 467), 5);
      }
    }
  } else if (e == 0x82b0f920) {
    bool allowed = eligible(2) || m.ReadU32(owner + 92) != 2;
    if (allowed)
      allowed = eligible(1) || m.ReadU32(owner + 92) != 1;
    if (!allowed)
      m.WriteU8(owner + 208, 0);
    else if (m.ReadU32(owner + 92) != 2 || chance(0x82b08ea8)) {
      mark();
      unsigned total = 0;
      for (unsigned i = 0; i < 4; ++i) {
        m.WriteU32(sp + 96 + 4 * i, 0);
        auto bank = m.ReadU32(owner + 132 + 8 * i);
        if (bank != 255) {
          s.r[3] = m.ReadU32(owner + 8);
          s.r[4] = bank;
          s.r[5] = m.ReadU32(owner + 136 + 8 * i);
          Call(0x82ac85a0, m, d, s);
          m.WriteU32(sp + 96 + 4 * i, Address(s.r[3]));
        }
        total += m.ReadU32(sp + 96 + 4 * i);
      }
      if (!total) {
        s.r[3] = m.ReadU32(0x832cb790);
        Call(0x82b2b298, m, d, s);
      } else {
        unsigned choice;
        do {
          s.r[3] = m.ReadU32(0x83264558);
          s.r[4] = 0;
          s.r[5] = 3;
          s.r[6] = 12;
          s.r[7] = m.ReadU32(m.ReadU32(owner + 4) + 64);
          Call(0x82aa0740, m, d, s);
          choice = Address(s.r[3]);
        } while (!m.ReadU32(sp + 96 + 4 * choice));
        s.r[3] = m.ReadU32(0x83264558);
        s.r[4] = 0;
        s.r[5] = m.ReadU32(sp + 96 + 4 * choice) - 1;
        s.r[6] = 13;
        s.r[7] = m.ReadU32(m.ReadU32(owner + 4) + 64);
        Call(0x82aa0740, m, d, s);
        s.r[5] = s.r[3];
        s.r[3] = m.ReadU32(owner + 8);
        s.r[4] = m.ReadU32(owner + 132 + 8 * choice);
        Call(0x82ac91e0, m, d, s);
      }
    }
  } else if (e == 0x82b0db98 || e == 0x82b12a98) {
    if (!eligible(1))
      m.WriteU8(owner + 208, 0);
    else {
      mark();
      float amount;
      if (e == 0x82b12a98) {
        s.r[3] = m.ReadU32(0x83315fb4);
        s.ctr = m.ReadU32(m.ReadU32(Address(s.r[3])) + 352);
        d.guest.CallIndirect(Address(s.ctr), m, s);
        Call(0x8229dfd8, m, d, s);
        auto play = Address(s.r[3]);
        s.r[3] = 0x832c9c54;
        s.r[4] = m.ReadU32(owner + 4);
        Call(0x82a9bdb0, m, d, s);
        auto actor = Address(s.r[3]), encounter = m.ReadU32(0x832cb778);
        if (encounter == 183 || encounter == 184 || encounter == 185)
          m.WriteU32(actor + 324, 1);
        auto base = play + 8260 * m.ReadU32(actor + 324) + 181104;
        unsigned sum = 0;
        for (unsigned i = 0; i < 1024; ++i)
          sum += m.ReadU32(base + 4 * i);
        fp();
        auto input = std::bit_cast<float>(m.ReadU32(owner + 80));
        auto total = std::int32_t(sum);
        if (!m.ReadU32(owner + 120)) {
          recovery_abi::WriteU64(m, sp + 88,
                                 std::uint64_t(std::int64_t(total)));
          amount = float(input * float(total));
        } else {
          auto divisor = std::int32_t(m.ReadU32(owner + 116));
          auto scaled = std::int32_t(unsigned(total / divisor) * 10u);
          recovery_abi::WriteU64(m, sp + 88,
                                 std::uint64_t(std::int64_t(scaled)));
          auto product = float(float(scaled) * input);
          amount = float(
              -(double(product) * std::bit_cast<float>(m.ReadU32(0x82000d7c)) -
                double(input)));
          auto zero = std::bit_cast<float>(m.ReadU32(0x82000e50));
          if (amount < zero)
            amount = zero;
        }
      } else {
        Call(0x82380a18, m, d, s);
        Call(0x82389b78, m, d, s);
        auto factor = std::int32_t(m.ReadU32(Address(s.r[3]) + 156));
        recovery_abi::WriteU64(m, sp + 88, std::uint64_t(std::int64_t(factor)));
        fp();
        amount =
            float(std::bit_cast<float>(m.ReadU32(owner + 80)) * float(factor));
      }
      m.WriteU32(owner + 28, std::bit_cast<unsigned>(amount));
      s.r[3] = owner;
      Call(0x82b0a0d0, m, d, s);
      fp();
      m.WriteU32(owner + 28, std::bit_cast<unsigned>(
                                 float(std::bit_cast<double>(s.fpr_bits[1]))));
      s.r[3] = owner;
      Call(0x82b0a3b0, m, d, s);
      if (m.ReadU32(owner + 184) == 1) {
        fp();
        m.WriteU32(owner + 32, m.ReadU32(0x82000e50));
      } else {
        m.WriteU32(sp + 88, 0x8204a1d8);
        s.r[3] = m.ReadU32(owner + 8);
        s.r[5] = sp + 80;
        s.r[6] = sp + 84;
        fp();
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(
            double(std::bit_cast<float>(m.ReadU32(owner + 32))));
        Call(0x82ac9618, m, d, s);
        fp();
        m.WriteU32(owner + 32, m.ReadU32(sp + 80));
        if (std::bit_cast<float>(m.ReadU32(sp + 84)) !=
            std::bit_cast<float>(m.ReadU32(0x82000e50))) {
          auto result = m.ReadU32(0x832cb790);
          auto index =
              116 * m.ReadU32(result + 12) + m.ReadU32(result + 24) + 3742;
          m.WriteU32(m.ReadU32(result + 20) + 4 * index, m.ReadU32(sp + 84));
        }
        m.WriteU32(sp + 88, 0x8204a1d8);
      }
      auto result = m.ReadU32(0x832cb790);
      s.r[3] = result;
      s.r[4] = m.ReadU32(owner + 8);
      s.r[6] = 0;
      fp();
      s.fpr_bits[1] = std::bit_cast<std::uint64_t>(
          double(std::bit_cast<float>(m.ReadU32(owner + 32))));
      Call(0x82b2b9e0, m, d, s);
      fp();
      auto index = 116 * m.ReadU32(result + 12) + m.ReadU32(result + 24) + 3726;
      m.WriteU32(
          m.ReadU32(result + 20) + 4 * index,
          std::bit_cast<unsigned>(float(std::bit_cast<double>(s.fpr_bits[1]))));
      m.WriteU32(owner + 172, m.ReadU32(owner + 32));
    }
  } else if (e == 0x82b10368) {
    s.r[3] = 0x832c9c54;
    s.r[4] = m.ReadU32(owner + 4);
    Call(0x82a9bdb0, m, d, s);
    auto actor = Address(s.r[3]);
    if (!eligible(2, 0))
      m.WriteU8(owner + 208, 0);
    else {
      mark();
      auto level = m.ReadU32(actor + 316);
      auto source = m.ReadU32(owner + 4);
      if (!level)
        level = m.ReadU32(source + 140);
      m.WriteU32(source + 4952, m.ReadU32(source + 4952) + 1);
      s.r[3] = m.ReadU32(0x83291dc0);
      s.r[4] = source;
      s.r[5] = level;
      Call(0x82ac3820, m, d, s);
      s.r[3] = m.ReadU32(0x83291dc0);
      s.r[4] = m.ReadU32(owner + 4);
      Call(0x82ac2468, m, d, s);
      s.r[3] = m.ReadU32(0x83291dc0);
      s.r[4] = m.ReadU32(owner + 4);
      s.r[5] = (m.ReadU32(Address(s.r[4]) + 124) >> 28) & 1;
      Call(0x82ac0620, m, d, s);
      source = m.ReadU32(owner + 4);
      fp();
      m.WriteU32(source + 2588, m.ReadU32(source + 2592));
      s.r[3] = m.ReadU32(owner + 8);
      s.r[4] = 0;
      s.r[5] = 1;
      s.r[6] = 1;
      Call(0x82ac9be0, m, d, s);
      s.r[3] = m.ReadU32(owner + 100);
      Call(0x82ac84b8, m, d, s);
      if (!m.ReadU32(owner + 92))
        m.WriteU32(owner + 168, Address(s.r[3]));
    }
  } else if (e == 0x82b0c880) {
    if (!eligible(1))
      m.WriteU8(owner + 208, 0);
    else if (chance(0x82b08e28)) {
      s.r[3] = 0;
      Call(0x8238aab0, m, d, s);
      if (!(Address(s.r[3]) & m.ReadU32(m.ReadU32(owner + 8) + 4876)) &&
          m.ReadU32(owner + 184) != 1) {
        mark();
        auto result = m.ReadU32(0x832cb790), resource = m.ReadU32(owner + 8);
        auto divisor = std::int32_t(m.ReadU32(owner + 108));
        fp();
        auto previous = trunc(std::bit_cast<float>(m.ReadU32(resource + 2588)));
        m.WriteU32(sp + 84, unsigned(previous));
        recovery_abi::WriteU64(m, sp + 88,
                               std::uint64_t(std::int64_t(divisor)));
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(float(divisor)));
        s.r[3] = result;
        s.r[4] = resource;
        s.r[6] = 8;
        Call(0x82b2b9e0, m, d, s);
        fp();
        auto index =
            116 * m.ReadU32(result + 12) + m.ReadU32(result + 24) + 3726;
        m.WriteU32(m.ReadU32(result + 20) + 4 * index,
                   std::bit_cast<unsigned>(
                       float(std::bit_cast<double>(s.fpr_bits[1]))));
        recovery_abi::WriteU64(m, sp + 88,
                               std::uint64_t(std::int64_t(previous)));
        auto difference =
            float(float(previous) -
                  std::bit_cast<float>(m.ReadU32(m.ReadU32(owner + 8) + 2588)));
        m.WriteU32(owner + 32, std::bit_cast<unsigned>(difference));
        m.WriteU32(owner + 172, std::bit_cast<unsigned>(difference));
      }
    }
  } else if (e == 0x82b0af88) {
    if (!eligible(2, 0))
      m.WriteU8(owner + 208, 0);
    else {
      s.r[3] = m.ReadU32(owner + 4);
      s.r[4] = 114;
      Call(0x8238e368, m, d, s);
      if ((Address(s.r[3]) & 255) != 1) {
        mark();
        auto divisor = std::int32_t(m.ReadU32(owner + 108));
        recovery_abi::WriteU64(m, sp + 88,
                               std::uint64_t(std::int64_t(divisor)));
        fp();
        auto amount =
            float(std::bit_cast<float>(m.ReadU32(m.ReadU32(owner + 4) + 2588)) /
                  float(divisor));
        m.WriteU32(owner + 28, std::bit_cast<unsigned>(amount));
        s.r[3] = owner;
        Call(0x82b0a3b0, m, d, s);
        for (auto mode : {3u, 0u}) {
          auto result = m.ReadU32(0x832cb790);
          fp();
          s.fpr_bits[1] = std::bit_cast<std::uint64_t>(
              double(std::bit_cast<float>(m.ReadU32(owner + 32))));
          s.r[3] = result;
          s.r[4] = m.ReadU32(owner + 4);
          s.r[6] = mode;
          Call(0x82b2b9e0, m, d, s);
          fp();
          auto index = 116 * m.ReadU32(result + 12) + m.ReadU32(result + 24) +
                       (mode ? 26 : 14);
          m.WriteU32(m.ReadU32(result + 20) + 4 * index,
                     std::bit_cast<unsigned>(
                         float(std::bit_cast<double>(s.fpr_bits[1]))));
        }
        s.r[3] = m.ReadU32(owner + 4);
        s.r[4] = m.ReadU32(owner + 92);
        s.r[5] = m.ReadU32(owner + 100);
        s.r[6] = 0;
        Call(0x82aca6f0, m, d, s);
      }
    }
  } else if (e == 0x82b110b8) {
    if (eligible(1)) {
      mark();
      if (m.ReadU32(owner + 36) == 0) {
        bool any = false;
        auto find = [&](unsigned id) {
          Call(0x82380a18, m, d, s);
          Call(0x82389b78, m, d, s);
          s.r[4] = id;
          Call(0x8238e308, m, d, s);
          return Address(s.r[3]);
        };
        for (unsigned i = 0;
             std::int32_t(i) <
             std::int32_t(m.ReadU32(m.ReadU32(owner + 40) + 20));
             ++i) {
          auto record = m.ReadU32(owner + 40);
          auto resource = find(m.ReadU32(record + 14884 + 464 * i));
          s.r[3] = m.ReadU32(0x83264558);
          s.r[4] = 0;
          s.r[5] = 1;
          s.r[6] = 16;
          s.r[7] = m.ReadU32(m.ReadU32(owner + 8) + 64);
          Call(0x82aa0740, m, d, s);
          auto flags = (m.ReadU32(resource + 124) & ~0x40000000u) |
                       ((Address(s.r[3]) & 1) << 30);
          m.WriteU32(resource + 124, flags);
          any |= (flags & 0x40000000) != 0;
        }
        if (!any) {
          s.r[3] = m.ReadU32(0x83264558);
          s.r[4] = 0;
          s.r[5] = m.ReadU32(m.ReadU32(owner + 40) + 20) - 1;
          s.r[6] = 17;
          s.r[7] = m.ReadU32(m.ReadU32(owner + 4) + 64);
          Call(0x82aa0740, m, d, s);
          auto resource = find(Address(s.r[3]));
          m.WriteU32(resource + 124, m.ReadU32(resource + 124) | 0x40000000);
        }
      }
    }
  } else if (e == 0x82b0a568 || e == 0x82b0a698 || e == 0x82b0a928 ||
             e == 0x82b0a7a0 || e == 0x82b13220 || e == 0x82b0aa70) {
    bool priced = e == 0x82b13220;
    bool mp = e == 0x82b0a698,
         combined =
             e == 0x82b0a7a0 || (e == 0x82b0aa70 && m.ReadU32(owner + 120));
    if (!mp && !priced)
      m.WriteU8(owner + 203, 0);
    bool allowed = priced ? eligible(1)
                   : mp   ? (m.ReadU32(owner + 116) != 0 || eligible(2))
                          : eligible(2);
    if (e == 0x82b0a568 && !allowed)
      allowed = m.ReadU32(owner + 116) != 0;
    if (!allowed)
      m.WriteU8(owner + 208, 0);
    else if (!priced || chance(0x82b08e28)) {
      mark();
      fp();
      auto input = std::bit_cast<float>(m.ReadU32(owner + 80));
      if (e == 0x82b0a568 &&
          input == std::bit_cast<float>(m.ReadU32(0x822184dc))) {
        auto target = m.ReadU32(owner + 8);
        m.WriteU32(target + 2588, m.ReadU32(target + 2592));
      } else {
        float criticalMultiplier = 0;
        auto calculate = [&](double amount, bool skipStat,
                             bool decideCritical) {
          fp();
          s.fpr_bits[1] = std::bit_cast<std::uint64_t>(amount);
          s.r[3] = owner;
          s.r[5] = skipStat;
          Call(0x82b09b30, m, d, s);
          fp();
          m.WriteU32(owner + 28, std::bit_cast<unsigned>(float(
                                     std::bit_cast<double>(s.fpr_bits[1]))));
          if (decideCritical) {
            s.r[3] = owner;
            Call(0x82b097a0, m, d, s);
            fp();
            criticalMultiplier = std::bit_cast<float>(m.ReadU32(0x82000e1c));
          }
          if (m.ReadU8(owner + 79) == 1) {
            auto value = std::bit_cast<float>(m.ReadU32(owner + 28));
            m.WriteU32(owner + 28, std::bit_cast<unsigned>(
                                       float(value * criticalMultiplier)));
          }
          s.r[3] = owner;
          Call(0x82b0a188, m, d, s);
          fp();
          m.WriteU32(owner + 28, std::bit_cast<unsigned>(float(
                                     std::bit_cast<double>(s.fpr_bits[1]))));
          s.r[3] = owner;
          Call(0x82b0a3b0, m, d, s);
        };
        auto apply = [&](unsigned mode, unsigned offset) {
          auto result = m.ReadU32(0x832cb790);
          fp();
          s.fpr_bits[1] = std::bit_cast<std::uint64_t>(
              double(std::bit_cast<float>(m.ReadU32(owner + 32))));
          s.r[3] = result;
          s.r[4] = m.ReadU32(owner + 8);
          s.r[6] = mode;
          Call(0x82b2b9e0, m, d, s);
          fp();
          auto index =
              116 * m.ReadU32(result + 12) + m.ReadU32(result + 24) + offset;
          m.WriteU32(m.ReadU32(result + 20) + 4 * index,
                     std::bit_cast<unsigned>(
                         float(std::bit_cast<double>(s.fpr_bits[1]))));
        };
        calculate(input, mp, true);
        apply(mp ? 3 : 1, priced ? 18 : mp ? 3738 : 3730);
        if (priced) {
          s.r[3] = m.ReadU32(0x83315fb4);
          s.ctr = m.ReadU32(m.ReadU32(Address(s.r[3])) + 352);
          d.guest.CallIndirect(Address(s.ctr), m, s);
          Call(0x8229dfd8, m, d, s);
          auto play = Address(s.r[3]), cost = m.ReadU32(owner + 120),
               balance = m.ReadU32(play + 76);
          auto remaining = balance - cost;
          if (std::int32_t(remaining) < 0) {
            remaining = 0;
            cost = balance;
          }
          m.WriteU32(play + 76, remaining);
          m.WriteU32(play + 185200, m.ReadU32(play + 185200) + cost);
        }
        if (combined) {
          auto value = std::int32_t(m.ReadU32(owner + 120));
          recovery_abi::WriteU64(m, sp + (e == 0x82b0aa70 ? 88 : 80),
                                 std::uint64_t(std::int64_t(value)));
          calculate(double(float(value)), true, false);
          apply(3, 3738);
        }
        if (e == 0x82b0aa70) {
          property(0x82ac91d8, 92, 100);
          if (m.ReadU32(owner + 104))
            property(0x82ac91d8, 96, 104);
        }
        if (e == 0x82b0a928) {
          s.r[3] = m.ReadU32(owner + 8);
          s.r[4] = m.ReadU32(owner + 92);
          s.r[5] = m.ReadU32(owner + 100);
          s.r[6] = m.ReadU32(owner + 108);
          s.r[7] = m.ReadU32(owner + 112);
          s.r[8] = m.ReadU32(owner + 120);
          Call(0x82ac8ec8, m, d, s);
        }
      }
    }
  } else if (e == 0x82b0ee40 || e == 0x82b0ec18 || e == 0x82b0f7d0) {
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
  if (savedFp30)
    s.fpr_bits[30] = recovery_abi::ReadU64(m, old - savedFp30);
  if (savedFp)
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - savedFp);
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_effect_mutation61
