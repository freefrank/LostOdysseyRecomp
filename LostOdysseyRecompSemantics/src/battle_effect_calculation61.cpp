#include "lo_semantics/battle_effect_calculation61.h"
#include "lo_semantics/battle_evaluation_chance61.h"
#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/battle_action_results61.h"
#include "lo_semantics/battle_random_range61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <limits>
namespace lo::semantic::gpu::battle_effect_calculation61 {
using recovery_abi::Address;
namespace {
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_effect_calculation61::Apply(e, m, d, s) &&
      !battle_evaluation_chance61::Apply(e, m, d, s) &&
      !battle_action_readiness61::Apply(e, m, d, s) &&
      !battle_action_results61::Apply(e, m, d, s) &&
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
  unsigned frame = 112, first = 31, literal = 0, saveFloat = 0;
  switch (e) {
  case 0x82b20ef0:
    frame = 128;
    first = 29;
    literal = 80;
    saveFloat = 40;
    break;
  case 0x82b21148:
    frame = 144;
    first = 29;
    literal = 88;
    saveFloat = 40;
    break;
  case 0x82b1f830:
    frame = 144;
    first = 29;
    literal = 88;
    saveFloat = 40;
    break;
  case 0x82b1f918:
    frame = 160;
    first = 28;
    literal = 80;
    saveFloat = 48;
    break;
  case 0x82b1fb00:
    frame = 144;
    first = 29;
    literal = 80;
    saveFloat = 40;
    break;
  case 0x82b1fc18:
    frame = 128;
    first = 30;
    literal = 80;
    saveFloat = 32;
    break;
  case 0x82b1fcc8:
    frame = 144;
    first = 27;
    literal = 88;
    break;
  case 0x82b204c8:
    frame = 128;
    first = 28;
    literal = 80;
    break;
  case 0x82b21068:
    break;
  case 0x82b20650:
    first = 30;
    break;
  default:
    return false;
  }
  auto owner = Address(s.r[3]), old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  if (saveFloat)
    recovery_abi::WriteU64(m, old - saveFloat, s.fpr_bits[31]);
  if (e == 0x82b1f918) {
    recovery_abi::WriteU64(m, old - 64, s.fpr_bits[29]);
    recovery_abi::WriteU64(m, old - 56, s.fpr_bits[30]);
  }
  if (e == 0x82b21148)
    recovery_abi::WriteU64(m, old - 48, s.fpr_bits[30]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  if (literal)
    m.WriteU32(sp + literal, 0x8204a1d8);
  auto f = [&](unsigned reg, double x) {
    s.fpr_bits[reg] = std::bit_cast<std::uint64_t>(x);
  };
  auto floatMode = [&]() {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
  };
  auto readFloat = [&](unsigned address, unsigned reg) {
    floatMode();
    auto x = std::bit_cast<float>(m.ReadU32(address));
    f(reg, double(x));
    return x;
  };
  auto property = [&](unsigned resource, unsigned id) {
    s.r[3] = resource;
    s.r[4] = id;
    Call(0x8238e368, m, d, s);
    return (Address(s.r[3]) & 255) == 1;
  };
  auto integer = [&](double x, unsigned offset) {
    auto n = Trunc(x);
    s.fpr_bits[0] = std::bit_cast<std::uint64_t>(std::int64_t(n));
    m.WriteU32(sp + offset, unsigned(n));
    return n;
  };
  if (e == 0x82b20ef0) {
    auto amount = readFloat(owner + 20, 31);
    auto source = m.ReadU32(owner + 4), target = m.ReadU32(owner + 8);
    bool apply = !(m.ReadU32(target + 124) & 0x40000000u) &&
                 m.ReadU32(source + 2648) != 4 &&
                 m.ReadU32(m.ReadU32(owner + 32)) != 30;
    if (apply && property(source, 112)) {
      s.r[3] = m.ReadU32(0x83264558);
      s.r[4] = 30;
      s.r[5] = 99;
      s.r[6] = m.ReadU32(m.ReadU32(owner + 4) + 64);
      Call(0x82aa0838, m, d, s);
      apply = (Address(s.r[3]) & 255) == 1;
    }
    if (apply) {
      target = m.ReadU32(owner + 8);
      auto side = (m.ReadU32(target + 124) >> 28) & 1,
           record = m.ReadU32(0x832aeb00) + 24 * side;
      bool enabled = m.ReadU8(record + 24) != 0;
      auto adjustment = readFloat(enabled ? record + 12 : 0x82000e50, 13);
      auto base = readFloat(0x82000fb0, 0);
      auto factor = float(base - adjustment);
      f(0, double(factor));
      auto cap = readFloat(0x82007784, 13);
      if (!(factor <= cap))
        factor = cap;
      f(0, double(factor));
      amount = float(factor * amount);
      f(31, double(amount));
      if (enabled) {
        auto report = m.ReadU32(0x832cb790);
        m.WriteU32(m.ReadU32(report + 20) + 4 * (116 * m.ReadU32(report + 12) +
                                                 m.ReadU32(report + 24) + 3776),
                   1);
      }
    }
    f(1, double(amount));
  } else if (e == 0x82b21148) {
    auto n = integer(readFloat(owner + 20, 0), 80);
    auto a = readFloat(owner + 80, 13), b = readFloat(owner + 76, 12),
         c = readFloat(owner + 72, 11);
    recovery_abi::WriteU64(m, sp + 80, std::uint64_t(std::int64_t(n)));
    auto amount = float(n);
    amount = float(amount + a);
    amount = float(amount + b);
    amount = float(amount + c);
    f(0, double(amount));
    m.WriteU32(owner + 24, std::bit_cast<unsigned>(amount));
    bool force = property(m.ReadU32(owner + 4), 7);
    auto zero = readFloat(0x82000e50, 30);
    if (force) {
      if (m.ReadU8(owner + 36) == 1) {
        auto value = readFloat(owner + 24, 13),
             scale = readFloat(0x82000e1c, 0);
        amount = float(value * scale);
        f(0, double(amount));
        m.WriteU32(owner + 24, std::bit_cast<unsigned>(amount));
      } else {
        amount = readFloat(owner + 24, 0);
        if (amount != zero) {
          amount = readFloat(0x82007784, 0);
          m.WriteU32(owner + 24, std::bit_cast<unsigned>(amount));
        }
      }
    }
    bool reduced = property(m.ReadU32(owner + 4), 96);
    auto multiplier = readFloat(0x821baa74, 31);
    if (reduced) {
      amount = readFloat(owner + 24, 0);
      amount = float(amount * multiplier);
      f(0, double(amount));
      m.WriteU32(owner + 24, std::bit_cast<unsigned>(amount));
    }
    auto count = std::int32_t(m.ReadU32(owner + 56));
    amount = readFloat(owner + 24, 0);
    recovery_abi::WriteU64(m, sp + 80, std::uint64_t(std::int64_t(count)));
    f(13, double(float(count)));
    amount = float(float(count) * amount);
    f(0, double(amount));
    m.WriteU32(owner + 24, std::bit_cast<unsigned>(amount));
    if (m.ReadU8(owner + 108) == 1 && property(m.ReadU32(owner + 4), 238)) {
      s.r[3] = m.ReadU32(0x83264558);
      s.r[4] = 10;
      s.r[5] = 108;
      s.r[6] = m.ReadU32(m.ReadU32(owner + 4) + 64);
      Call(0x82aa0838, m, d, s);
      if (Address(s.r[3]) & 255) {
        amount = readFloat(owner + 24, 0);
        amount = float(amount * multiplier);
        f(0, double(amount));
        m.WriteU32(owner + 24, std::bit_cast<unsigned>(amount));
      }
    }
    amount = readFloat(owner + 24, 13);
    auto bias = readFloat(0x8201f9f0, 0);
    auto rounded = integer(float(amount + bias), 80);
    recovery_abi::WriteU64(m, sp + 80, std::uint64_t(std::int64_t(rounded)));
    f(0, double(float(rounded)));
    m.WriteU32(owner + 24, std::bit_cast<unsigned>(float(rounded)));
    if (m.ReadU8(owner + 64))
      m.WriteU32(owner + 24, std::bit_cast<unsigned>(zero));
  } else if (e == 0x82b1f830 || e == 0x82b1f918) {
    bool attack = e == 0x82b1f830;
    auto resource = m.ReadU32(owner + (attack ? 4 : 8));
    auto numeric = [&](unsigned id) {
      s.r[3] = resource;
      s.r[4] = id;
      Call(0x82ac9858, m, d, s);
      return std::int32_t(Address(s.r[3]));
    };
    std::int32_t stat = 0;
    if (attack)
      stat = std::int32_t(unsigned(numeric(47)) * 5u);
    float bonus = 0;
    if (!attack)
      bonus = readFloat(0x82000e50, 29);
    float base = readFloat(resource + (attack ? 2596 : 2600), 0),
          extra = readFloat(resource + (attack ? 2536 : 2540), 13);
    float sum = float(base + extra);
    f(31, double(sum));
    if (attack) {
      auto other = std::int32_t(unsigned(numeric(37)) * 30u);
      if (stat < other)
        stat = other;
    } else
      stat = numeric(32);
    auto signedScale = attack ? stat : std::int32_t(unsigned(stat) * 10u);
    if (!attack && stat >= 0 && property(resource, 19)) {
      auto factor =
          readFloat(property(resource, 108) ? 0x82218420 : 0x822185dc, 0);
      bonus = float(sum * factor);
      f(29, double(bonus));
    }
    if (!attack && stat < 0) {
      bool present = property(resource, 19);
      auto scale = readFloat(0x82000d7c, 30);
      if (present) {
        unsigned bias = property(resource, 108) ? 140 : 120;
        auto adjusted = std::int32_t(unsigned(signedScale) + bias);
        recovery_abi::WriteU64(m, sp + 88,
                               std::uint64_t(std::int64_t(adjusted)));
        f(0, double(float(adjusted)));
        auto product = float(float(adjusted) * sum);
        f(0, double(product));
        bonus = float(product * scale);
        f(29, double(bonus));
      }
    }
    unsigned scratch = attack ? 80 : 88;
    recovery_abi::WriteU64(m, sp + scratch,
                           std::uint64_t(std::int64_t(signedScale)));
    f(0, double(signedScale));
    auto factor = float(signedScale);
    f(13, double(factor));
    auto bias = readFloat(0x8201dd2c, 0);
    factor = float(factor + bias);
    f(0, double(factor));
    auto product = float(factor * sum);
    f(attack || stat >= 0 ? 13 : 0, double(product));
    auto scale = std::bit_cast<float>(m.ReadU32(0x82000d7c));
    if (attack || stat >= 0)
      readFloat(0x82000d7c, 0);
    auto value = float(product * scale);
    f(0, double(value));
    auto selected = !attack && !(bonus <= value) ? bonus : value;
    auto result = integer(selected, scratch);
    s.r[3] = unsigned(result);
  } else if (e == 0x82b1fb00 || e == 0x82b1fc18) {
    auto source = m.ReadU32(owner + 4), target = m.ReadU32(owner + 8);
    auto base = readFloat(e == 0x82b1fb00 ? source + 2604 : target + 2608, 31);
    if (e == 0x82b1fb00) {
      bool special = m.ReadU8(owner + 65) == 1;
      if (!special)
        for (auto id : {3u, 15u, 16u})
          if (property(target, id)) {
            special = true;
            break;
          }
      if (!special)
        special = m.ReadU32(source + 68) == 7 && m.ReadU32(target + 68) == 306;
      if (special)
        base = readFloat(0x8204fc20, 31);
    }
    s.r[3] = m.ReadU32(owner + 8);
    s.r[4] = e == 0x82b1fb00 ? 34 : 35;
    Call(0x82ac9858, m, d, s);
    auto n = std::int32_t(Address(s.r[3]) * 10u);
    recovery_abi::WriteU64(m, sp + 88, std::uint64_t(std::int64_t(n)));
    floatMode();
    f(0, double(float(n)));
    f(1, double(float(float(n) + base)));
  } else if (e == 0x82b1fcc8) {
    s.r[3] = owner;
    Call(0x82b1fb00, m, d, s);
    integer(std::bit_cast<double>(s.fpr_bits[1]), 80);
    s.r[3] = owner;
    Call(0x82b1fc18, m, d, s);
    integer(std::bit_cast<double>(s.fpr_bits[1]), 84);
    bool half = property(m.ReadU32(owner + 4), 10);
    auto value = std::int32_t(m.ReadU32(sp + 80));
    if (half)
      value /= 2;
    auto difference = std::int32_t(unsigned(value) - m.ReadU32(sp + 84));
    if (difference < 0)
      difference = 0;
    auto source = m.ReadU32(owner + 4);
    s.r[3] = m.ReadU32(0x83264558);
    s.r[4] = 0;
    s.r[5] = 79;
    s.r[6] = 20;
    s.r[7] = m.ReadU32(source + 64);
    Call(0x82aa0740, m, d, s);
    bool result = difference > std::int32_t(Address(s.r[3]));
    source = m.ReadU32(owner + 4);
    auto target = m.ReadU32(owner + 8);
    if ((m.ReadU32(source + 124) & 0x10000000u) &&
        !(m.ReadU32(target + 124) & 0x10000000u) &&
        (m.ReadU32(target + 68) == 262 || m.ReadU32(target + 68) == 270))
      result = true;
    s.r[3] = result;
  } else if (e == 0x82b204c8) {
    m.WriteU32(owner + 60, 0);
    if (!m.ReadU8(owner + 45))
      for (unsigned i = 0; i < 4; ++i) {
        auto target = m.ReadU32(owner + 8);
        if (!property(target, 198 + i))
          continue;
        s.r[3] = m.ReadU32(0x83264558);
        s.r[4] = 100;
        s.r[5] = 29 + i;
        s.r[6] = m.ReadU32(m.ReadU32(owner + 8) + 64);
        Call(0x82aa0838, m, d, s);
        if ((Address(s.r[3]) & 255) == 1) {
          m.WriteU32(owner + 60, 1u << i);
          break;
        }
      }
    s.r[3] = m.ReadU32(0x832cb790);
    s.r[4] = m.ReadU32(owner + 60);
    Call(0x82b2b3d8, m, d, s);
  } else if (e == 0x82b21068) {
    auto value = integer(readFloat(owner + 20, 0), 80);
    if (value == 0 && m.ReadU8(owner + 64) == 1) {
      auto zero = readFloat(0x82000e50, 1);
      for (auto offset : {68u, 72u, 76u})
        m.WriteU32(owner + offset, std::bit_cast<unsigned>(zero));
    } else {
      auto source = m.ReadU32(owner + 4);
      unsigned low =
          value / 8 <= 0 && (m.ReadU32(source + 124) & 0x10000000u) ? 1 : 0;
      s.r[3] = m.ReadU32(0x83264558);
      s.r[4] = low;
      s.r[5] = low + 1;
      s.r[6] = 33;
      s.r[7] = m.ReadU32(source + 64);
      Call(0x82aa0740, m, d, s);
      auto n = std::int32_t(unsigned(value) + Address(s.r[3]));
      recovery_abi::WriteU64(m, sp + 80, std::uint64_t(std::int64_t(n)));
      floatMode();
      f(0, double(n));
      f(1, double(float(n)));
    }
  } else {
    s.r[3] = owner;
    Call(0x82b1f918, m, d, s);
    auto defense = Address(s.r[3]);
    s.r[3] = owner;
    Call(0x82b1f830, m, d, s);
    auto n = std::int32_t(Address(s.r[3]) - defense);
    recovery_abi::WriteU64(m, sp + 80, std::uint64_t(std::int64_t(n)));
    floatMode();
    f(0, double(float(n)));
    auto zero = readFloat(0x82000e50, 1);
    if (!(float(n) < zero))
      f(1, double(float(n)));
  }
  if (literal)
    m.WriteU32(sp + literal, 0x8204a1d8);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  if (e == 0x82b1f918) {
    s.fpr_bits[29] = recovery_abi::ReadU64(m, old - 64);
    s.fpr_bits[30] = recovery_abi::ReadU64(m, old - 56);
  }
  if (e == 0x82b21148)
    s.fpr_bits[30] = recovery_abi::ReadU64(m, old - 48);
  if (saveFloat)
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - saveFloat);
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_effect_calculation61
