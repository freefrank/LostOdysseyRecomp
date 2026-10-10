#include "lo_semantics/battle_effect_followups61.h"
#include "lo_semantics/battle_result_application61.h"
#include "lo_semantics/battle_effect_calculation61.h"
#include "lo_semantics/battle_property_mutation61.h"
#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/battle_action_results61.h"
#include "lo_semantics/battle_action_adjustments61.h"
#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/battle_script_runtime61.h"
#include "lo_semantics/battle_script_party61.h"
#include "lo_semantics/battle_random_range61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <limits>
namespace lo::semantic::gpu::battle_effect_followups61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_effect_followups61::Apply(e, m, d, s) &&
      !battle_result_application61::Apply(e, m, d, s) &&
      !battle_effect_calculation61::Apply(e, m, d, s) &&
      !battle_property_mutation61::Apply(e, m, d, s) &&
      !battle_action_readiness61::Apply(e, m, d, s) &&
      !battle_action_results61::Apply(e, m, d, s) &&
      !battle_action_adjustments61::Apply(e, m, d, s) &&
      !battle_script_actions61::Apply(e, m, d, s) &&
      !battle_script_runtime61::Apply(e, m, d, s) &&
      !battle_script_party61::Apply(e, m, d, s) &&
      !battle_random_range61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
std::int32_t Trunc(double x) {
  return x > double(std::numeric_limits<std::int32_t>::max())
             ? std::numeric_limits<std::int32_t>::max()
         : !(x >= -2147483648.) ? std::numeric_limits<std::int32_t>::min()
                                : std::int32_t(x);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82aa12e0) {
    auto report = Address(s.r[3]), item = Address(s.r[4]),
         kind = Address(s.r[5]);
    auto row = m.ReadU32(0x83264978 + 116) + 84 * item;
    s.r[4] = m.ReadU32(row + 52) ? m.ReadU32(row + 48) : 0x821a83d0;
    s.r[3] = m.ReadU32(report + 16) + 12 * kind + 272;
    Call(0x8229f5e0, m, d, s);
    return true;
  }
  if (e == 0x82b22100) {
    auto old = Address(s.r[1]), owner = Address(s.r[3]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 23; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    for (unsigned i = 28; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 88 - 8 * (31 - i), s.fpr_bits[i]);
    s.r[1] -= 208;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    m.WriteU32(sp + 88, 0x8204a1d8);
    m.WriteU32(sp + 80, 0);
    m.WriteU32(sp + 84, 0);
    auto f = [&](unsigned reg, double v) {
      s.fpr_bits[reg] = std::bit_cast<std::uint64_t>(v);
    };
    auto load = [&](unsigned p, unsigned reg) {
      if (s.cached_fp_control & 0x8040) {
        s.cached_fp_control &= ~0x8040u;
        d.fp.SetHostFpControl(s.cached_fp_control);
      }
      auto x = std::bit_cast<float>(m.ReadU32(p));
      f(reg, x);
      return x;
    };
    auto source = [&]() { return m.ReadU32(owner + 4); };
    auto target = [&]() { return m.ReadU32(owner + 8); };
    auto trait = [&](unsigned category, unsigned kind, unsigned high,
                     unsigned middle, unsigned low, unsigned output,
                     bool integer) {
      unsigned level = 0;
      bool matched = false;
      for (auto detail : {13u, 3u, 12u, 2u, 11u, 1u}) {
        s.r[3] = m.ReadU32(0x832cb784);
        s.r[4] = source();
        s.r[5] = category;
        s.r[6] = kind;
        s.r[7] = detail;
        Call(0x82aa0890, m, d, s);
        if ((Address(s.r[3]) & 255) == 1) {
          matched = true;
          break;
        }
        ++level;
      }
      if (!matched)
        return false;
      auto factor = load(level < 2 ? high : level < 4 ? middle : low, 2);
      (void)factor;
      s.r[3] = m.ReadU32(0x832cb784);
      s.r[4] = source();
      s.r[7] = category;
      auto zero = load(0x82000e50, 31);
      f(1, zero);
      f(3, zero);
      Call(0x82aa0e10, m, d, s);
      auto value = float(std::bit_cast<double>(s.fpr_bits[1]));
      if (integer) {
        auto n = Trunc(value);
        s.fpr_bits[0] = std::bit_cast<std::uint64_t>(std::int64_t(n));
        m.WriteU32(output, unsigned(n));
      } else
        m.WriteU32(output, std::bit_cast<unsigned>(value));
      return true;
    };
    auto invokeMask = [&](unsigned mask, unsigned forced) {
      s.r[3] = owner;
      s.r[4] = mask;
      s.r[5] = forced;
      Call(0x82b21970, m, d, s);
    };
    auto work = [&]() {
      auto value = load(owner + 24, 0), zero = load(0x82000e50, 31);
      if (value == zero)
        return;
      s.r[3] = 0x832c9c54;
      s.r[4] = source();
      Call(0x82a9bdb0, m, d, s);
      auto actor = Address(s.r[3]);
      if (actor) {
        unsigned mask = 0;
        auto pending = m.ReadU32(actor + 100);
        if (pending) {
          s.r[3] = pending - 1;
          Call(0x8238aab0, m, d, s);
          mask = Address(s.r[3]);
        }
        m.WriteU32(actor + 100, 0);
        invokeMask(mask, 1);
      }
      if (m.ReadU8(owner + 108) == 1)
        return;
      unsigned mask = 0;
      for (unsigned i = 0; i < 3; ++i) {
        s.r[3] = m.ReadU32(0x832cb784);
        s.r[4] = source();
        s.r[5] = i;
        Call(0x82aa0e98, m, d, s);
        if ((Address(s.r[3]) & 255) == 1)
          mask |= m.ReadU32(source() + 76264 + 4 * i);
      }
      load(0x82000e20, 29);
      load(0x82000e48, 30);
      if (trait(4, 0x400000, 0x82000e20, 0x82000e48, 0x82000e44, owner + 116,
                false))
        mask |= m.ReadU32(m.ReadU32(0x832cb784) + 16);
      if (trait(4, 0x200000, 0x82000dd0, 0x82000e20, 0x82000e48, owner + 112,
                false))
        mask |= m.ReadU32(m.ReadU32(0x832cb784) + 16);
      if (mask)
        invokeMask(mask, 0);
      load(0x8201dd2c, 28);
      load(0x82000da8, 29);
      load(0x82000dec, 30);
      trait(6, 0, 0x8201dd2c, 0x82000dec, 0x82000da8, sp + 84, true);
      trait(6, 1, 0x8201dd2c, 0x82000dec, 0x82000da8, sp + 80, true);
      if (m.ReadU8(target() + 124) & 1)
        return;
      auto row = m.ReadU32(m.ReadU32(0x832ca0d0) + 120) +
                 140 * m.ReadU32(target() + 68);
      if (!m.ReadU32(row + 132) && !m.ReadU32(row + 136))
        return;
      s.r[3] = source();
      s.r[4] = 110;
      Call(0x8238e368, m, d, s);
      bool doubled = (Address(s.r[3]) & 255) == 1;
      auto normal = m.ReadU32(sp + 84), rare = m.ReadU32(sp + 80);
      if (doubled) {
        normal *= 2;
        rare *= 2;
      }
      auto chance = rare ? rare : normal;
      if (!chance)
        return;
      s.r[3] = m.ReadU32(0x83264558);
      s.r[4] = chance;
      s.r[5] = 103;
      s.r[6] = m.ReadU32(source() + 64);
      Call(0x82aa0838, m, d, s);
      if ((Address(s.r[3]) & 255) != 1)
        return;
      auto item = rare ? m.ReadU32(row + 136) : 0;
      if (!item)
        item = m.ReadU32(row + 132);
      if (!item)
        return;
      s.r[3] = m.ReadU32(0x83315fb4);
      s.ctr = m.ReadU32(m.ReadU32(Address(s.r[3])) + 352);
      d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
      Call(0x8229dfd8, m, d, s);
      s.r[3] = 0x832c9c54;
      s.r[4] = item;
      s.r[5] = 1;
      Call(0x82a9e5e0, m, d, s);
      s.r[3] = m.ReadU32(0x832cb790);
      Call(0x82b2b248, m, d, s);
      m.WriteU32(target() + 124, m.ReadU32(target() + 124) | 0x01000000);
      s.r[3] = m.ReadU32(0x832cb798);
      s.r[4] = item;
      s.r[5] = 2;
      Call(0x82aa12e0, m, d, s);
      auto report = m.ReadU32(0x832cb798);
      m.WriteU32(report + 20, 2);
      m.WriteU32(m.ReadU32(report + 16) + 264, 2);
      m.WriteU32(report + 24, 2);
      m.WriteU32(m.ReadU32(report + 16) + 268, 2);
    };
    work();
    m.WriteU32(sp + 88, 0x8204a1d8);
    s.r[1] += 208;
    for (unsigned i = 28; i < 32; ++i)
      s.fpr_bits[i] = recovery_abi::ReadU64(m, old - 88 - 8 * (31 - i));
    for (unsigned i = 23; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e != 0x82b21970)
    return false;
  auto old = Address(s.r[1]), owner = Address(s.r[3]), mask = Address(s.r[4]),
       forced = Address(s.r[5]) & 255;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = 16; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  recovery_abi::WriteU64(m, old - 152, s.fpr_bits[30]);
  recovery_abi::WriteU64(m, old - 144, s.fpr_bits[31]);
  s.r[1] -= 576;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  m.WriteU32(sp + 88, 0x8204a1d8);
  m.WriteU32(owner + 104, 0);
  auto f = [&](unsigned reg, double v) {
    s.fpr_bits[reg] = std::bit_cast<std::uint64_t>(v);
  };
  auto load = [&](unsigned p, unsigned reg) {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto v = std::bit_cast<float>(m.ReadU32(p));
    f(reg, v);
    return v;
  };
  auto source = [&]() { return m.ReadU32(owner + 4); };
  auto target = [&]() { return m.ReadU32(owner + 8); };
  auto property = [&](unsigned resource, unsigned id) {
    s.r[3] = resource;
    s.r[4] = id;
    Call(0x8238e368, m, d, s);
    return Address(s.r[3]) & 255;
  };
  auto manager = [&](unsigned method) {
    Call(0x82380a18, m, d, s);
    Call(method, m, d, s);
    return Address(s.r[3]);
  };
  auto resolve = [&](unsigned id) {
    manager(0x82389b78);
    s.r[4] = id;
    Call(0x8238e308, m, d, s);
    return Address(s.r[3]);
  };
  auto putInt = [&](unsigned offset, std::int32_t value) {
    recovery_abi::WriteU64(m, sp + offset, std::uint64_t(std::int64_t(value)));
    f(0, float(value));
    f(1, float(value));
  };
  auto heal = [&](unsigned who, std::int32_t value, bool hp, unsigned scratch) {
    auto report = m.ReadU32(0x832cb790);
    putInt(scratch, value);
    s.r[3] = report;
    s.r[4] = who;
    s.r[6] = hp ? 1 : 3;
    Call(0x82b2b9e0, m, d, s);
    auto address =
        m.ReadU32(report + 20) + 4 * (116 * m.ReadU32(report + 12) +
                                      m.ReadU32(report + 24) + (hp ? 18 : 26));
    m.WriteU32(address, std::bit_cast<unsigned>(
                            float(std::bit_cast<double>(s.fpr_bits[1]))));
  };
  if (mask) {
    auto list = manager(0x8238e2f8);
    unsigned all = 0, back = 0;
    for (unsigned i = 0;
         std::int32_t(i) < std::int32_t(m.ReadU32(manager(0x8238e2f8) + 4));
         ++i) {
      auto resource = m.ReadU32(m.ReadU32(list) + 4 * i);
      s.r[3] = resource;
      s.ctr = m.ReadU32(m.ReadU32(resource) + 292);
      d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
      if (Address(s.r[3]))
        continue;
      auto flags = m.ReadU32(resource + 124);
      if ((flags ^ m.ReadU32(source() + 124)) & 0x10000000u)
        continue;
      auto id = m.ReadU32(resource + 64);
      m.WriteU32(sp + 160 + 4 * all++, id);
      if (!(flags & 0x40000000u))
        m.WriteU32(sp + 288 + 4 * back++, id);
    }
    auto scale = load(0x82000d7c, 30), zero = load(0x82000e50, 31);
    for (unsigned bit = 0; bit < 32; ++bit) {
      auto flag = 1u << bit;
      if (!(mask & flag) || property(target(), bit))
        continue;
      if (forced) {
        s.r[3] = m.ReadU32(0x83264558);
        s.r[4] = 50;
        s.r[5] = 34;
        s.r[6] = m.ReadU32(source() + 64);
        Call(0x82aa0838, m, d, s);
        if (!(Address(s.r[3]) & 255))
          continue;
      }
      if (bit != 21 && bit != 22) {
        s.r[3] = target();
        s.r[4] = bit;
        s.r[5] = 1;
        s.r[6] = 0;
        Call(0x82ac9be0, m, d, s);
        m.WriteU32(owner + 104, m.ReadU32(owner + 104) | flag);
        continue;
      }
      auto response = m.ReadU32(owner + 52);
      if ((response >= 1 && response <= 4) || property(target(), 142) == 1 ||
          load(owner + 24, 0) == zero)
        continue;
      bool hp = bit == 21;
      std::int32_t percent;
      if (forced) {
        if (hp)
          percent = 20;
        else {
          s.r[3] = 230;
          Call(0x8238aab0, m, d, s);
          Call(0x82ac84b8, m, d, s);
          percent =
              std::int32_t(m.ReadU32(source() + 4 * (Address(s.r[3]) + 535)));
        }
        m.WriteU32(sp + 80, unsigned(percent));
        if (property(source(), 239) == 1) {
          percent = std::int32_t(unsigned(percent) * 2);
          m.WriteU32(sp + 80, unsigned(percent));
        }
      } else {
        percent = Trunc(load(owner + (hp ? 112 : 116), 0));
        s.fpr_bits[0] = std::bit_cast<std::uint64_t>(std::int64_t(percent));
        m.WriteU32(sp + 80, unsigned(percent));
      }
      std::int32_t value = 0;
      m.WriteU32(sp + 84, 0);
      if (percent) {
        recovery_abi::WriteU64(m, sp + (hp ? 128 : 152),
                               std::uint64_t(std::int64_t(percent)));
        f(13, float(percent));
        auto raw = float(float(float(percent) * load(owner + 24, 0)) * scale);
        value = Trunc(raw);
        s.fpr_bits[0] = std::bit_cast<std::uint64_t>(std::int64_t(value));
        m.WriteU32(sp + 84, unsigned(value));
        if (hp ? value <= 0 : value < 0)
          value = 1;
      }
      if (property(source(), 103)) {
        if (all) {
          if (value) {
            value /= std::int32_t(all);
            if (value <= 0)
              value = 1;
          }
          for (unsigned i = 0; i < all; ++i) {
            auto who = resolve(m.ReadU32(sp + 160 + 4 * i));
            if (m.ReadU32(source() + 64) == m.ReadU32(who + 64) ||
                ((m.ReadU32(source() + 124) ^ m.ReadU32(who + 124)) &
                 0x10000000u))
              continue;
            heal(who, value, hp, hp ? 112 : 104);
          }
        }
      } else if (property(source(), 254) &&
                 (m.ReadU32(source() + 124) & 0x40000000u) && back) {
        s.r[3] = m.ReadU32(0x83264558);
        s.r[4] = 0;
        s.r[5] = back - 1;
        s.r[6] = 104;
        s.r[7] = m.ReadU32(source() + 64);
        Call(0x82aa0740, m, d, s);
        auto who = resolve(m.ReadU32(sp + 288 + 4 * Address(s.r[3])));
        heal(who, value, hp, hp ? 144 : 120);
        value = 0;
      }
      heal(source(), value, hp, hp ? 96 : 136);
      s.r[3] = m.ReadU32(0x832cb790);
      Call(0x82b2b270, m, d, s);
      if (hp)
        m.WriteU8(owner + 120, 1);
      else {
        s.r[3] = target();
        s.r[4] = 46;
        Call(0x82ac9000, m, d, s);
      }
    }
  }
  m.WriteU32(owner + 88, mask);
  m.WriteU32(sp + 88, 0x8204a1d8);
  s.r[1] += 576;
  s.fpr_bits[30] = recovery_abi::ReadU64(m, old - 152);
  s.fpr_bits[31] = recovery_abi::ReadU64(m, old - 144);
  for (unsigned i = 16; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_effect_followups61
