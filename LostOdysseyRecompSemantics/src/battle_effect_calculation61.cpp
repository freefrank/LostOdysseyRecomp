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
  if (e == 0x82b1fb00 || e == 0x82b1fc18) {
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
  if (saveFloat)
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - saveFloat);
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_effect_calculation61
