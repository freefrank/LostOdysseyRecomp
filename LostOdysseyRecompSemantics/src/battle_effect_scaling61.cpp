#include "lo_semantics/battle_effect_scaling61.h"
#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/battle_random_range61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <limits>
namespace lo::semantic::gpu::battle_effect_scaling61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_action_readiness61::Apply(e, m, d, s) &&
      !battle_random_range61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned frame, first, fpOffset = 0, literal = 0;
  switch (e) {
  case 0x82b09b30:
    frame = 128;
    first = 29;
    fpOffset = 40;
    literal = 80;
    break;
  case 0x82b097a0:
    frame = 128;
    first = 28;
    literal = 80;
    break;
  case 0x82b0a188:
    frame = 112;
    first = 31;
    break;
  case 0x82b0a3b0:
    frame = 160;
    first = 27;
    fpOffset = 56;
    literal = 88;
    break;
  default:
    return false;
  }
  auto old = Address(s.r[1]), owner = Address(s.r[3]),
       option = Address(s.r[5]) & 255;
  auto input = std::bit_cast<double>(s.fpr_bits[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  if (fpOffset)
    recovery_abi::WriteU64(m, old - fpOffset, s.fpr_bits[31]);
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
  auto get = [&](unsigned p) { return std::bit_cast<float>(m.ReadU32(p)); };
  auto put = [&](unsigned p, float value) {
    m.WriteU32(p, std::bit_cast<unsigned>(value));
  };
  auto trunc = [](float value) {
    return value > double(std::numeric_limits<std::int32_t>::max())
               ? std::numeric_limits<std::int32_t>::max()
           : !(value >= -2147483648.) ? std::numeric_limits<std::int32_t>::min()
                                      : std::int32_t(value);
  };
  auto property = [&](unsigned resource, unsigned id) {
    s.r[3] = resource;
    s.r[4] = id;
    Call(0x8238e368, m, d, s);
    return Address(s.r[3]) & 255;
  };
  auto chance = [&](unsigned threshold, unsigned tag) {
    s.r[3] = m.ReadU32(0x83264558);
    s.r[4] = threshold;
    s.r[5] = tag;
    s.r[6] = m.ReadU32(m.ReadU32(owner + 4) + 64);
    Call(0x82aa0838, m, d, s);
    return Address(s.r[3]) & 255;
  };
  if (e == 0x82b09b30) {
    fp();
    double value = input;
    if (!option && !m.ReadU8(owner + 77) && !m.ReadU8(owner + 78)) {
      auto source = m.ReadU32(owner + 4), target = m.ReadU32(owner + 8);
      auto base = float(get(source + 2524) - get(0x82007784));
      auto factor = float(double(base) * get(0x82000e48) + get(0x8201dd2c));
      auto scaled = float(double(factor) * value);
      value = float(scaled * get(0x82000d7c));
      if (property(target, 134) == 1 ||
          property(m.ReadU32(owner + 8), 165) == 1) {
        fp();
        value = float(value * get(0x82218674));
      }
    }
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(value);
  } else if (e == 0x82b097a0) {
    m.WriteU8(owner + 79, 0);
    if (m.ReadU8(owner + 77) == 1) {
      auto source = m.ReadU32(owner + 4);
      if (property(source, 102) == 1)
        m.WriteU8(owner + 79, 1);
      else if (property(m.ReadU32(owner + 4), 104) == 1) {
        auto category = m.ReadU32(owner + 176);
        if (category == 1 || category == 23 || category == 24)
          m.WriteU8(owner + 79, 1);
      }
    } else if (chance(m.ReadU32(m.ReadU32(owner + 4) + 5100), 112) == 1)
      m.WriteU8(owner + 79, 1);
  } else if (e == 0x82b0a188) {
    fp();
    auto amount = unsigned(trunc(get(owner + 28)));
    m.WriteU32(sp + 80, amount);
    bool explicitZero = false;
    if (!m.ReadU8(owner + 77)) {
      if (!amount && m.ReadU8(owner + 68) == 1)
        explicitZero = true;
      else {
        auto upper = std::int32_t(amount) / 8;
        s.r[3] = m.ReadU32(0x83264558);
        s.r[4] = 0;
        s.r[5] = unsigned(upper ? upper : 1);
        s.r[6] = upper ? 9 : 2568;
        s.r[7] = m.ReadU32(m.ReadU32(owner + 4) + 64);
        Call(0x82aa0740, m, d, s);
        amount += Address(s.r[3]);
      }
    }
    fp();
    if (explicitZero)
      s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(get(0x82000e50)));
    else {
      recovery_abi::WriteU64(m, sp + 80,
                             std::uint64_t(std::int64_t(std::int32_t(amount))));
      s.fpr_bits[1] =
          std::bit_cast<std::uint64_t>(double(float(std::int32_t(amount))));
    }
  } else {
    fp();
    put(owner + 32, get(owner + 28));
    auto zero = get(0x82000e50);
    if (!m.ReadU8(owner + 77) && !m.ReadU8(owner + 200) &&
        property(m.ReadU32(owner + 4), 7) == 1) {
      fp();
      if (get(owner + 32) != zero)
        put(owner + 32, get(0x82007784));
    }
    if (m.ReadU8(owner + 203) && property(m.ReadU32(owner + 8), 129) &&
        chance(5, 103) == 1) {
      fp();
      put(owner + 32, zero);
    }
    fp();
    auto half = get(0x8201f9f0);
    if (m.ReadU32(m.ReadU32(owner + 4) + 68) == 20) {
      auto find = [&](unsigned id) {
        Call(0x82380a18, m, d, s);
        Call(0x82389b78, m, d, s);
        s.r[4] = id;
        Call(0x8238e308, m, d, s);
        return Address(s.r[3]);
      };
      auto firstPeer = find(21), secondPeer = find(22);
      auto unavailable = [&](unsigned object) {
        s.r[3] = object;
        s.ctr = m.ReadU32(m.ReadU32(object) + 292);
        d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        return Address(s.r[3]) != 0;
      };
      if (unavailable(firstPeer) || unavailable(secondPeer)) {
        fp();
        put(owner + 32, float(get(owner + 32) * half));
      }
    }
    fp();
    auto rounded = trunc(float(get(owner + 32) + half));
    m.WriteU32(sp + 80, unsigned(rounded));
    recovery_abi::WriteU64(m, sp + 80, std::uint64_t(std::int64_t(rounded)));
    put(owner + 32, float(rounded));
  }
  if (literal)
    m.WriteU32(sp + literal, 0x8204a1d8);
  s.r[1] += frame;
  if (fpOffset)
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - fpOffset);
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_effect_scaling61
