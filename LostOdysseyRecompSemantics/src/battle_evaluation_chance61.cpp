#include "lo_semantics/battle_evaluation_chance61.h"
#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/battle_random_range61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <limits>
namespace lo::semantic::gpu::battle_evaluation_chance61 {
using recovery_abi::Address;
namespace {
unsigned Index(unsigned mask) {
  unsigned i = 0;
  for (; i < 31; ++i)
    if (mask & (1u << i))
      break;
  return i;
}
void FloatMode(Dependencies d, Registers &s) {
  if (s.cached_fp_control & 0x8040) {
    s.cached_fp_control &= ~0x8040u;
    d.fp.SetHostFpControl(s.cached_fp_control);
  }
}
std::int32_t Truncate(double value) {
  return value > double(std::numeric_limits<std::int32_t>::max())
             ? std::numeric_limits<std::int32_t>::max()
         : !(value >= -2147483648.) ? std::numeric_limits<std::int32_t>::min()
                                    : std::int32_t(value);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  auto owner = Address(s.r[3]);
  if (e == 0x82ac9858) {
    auto id = std::int32_t(Address(s.r[4])), q = id / 32;
    auto table = 0x83213438u + 8 * unsigned(id - q * 32);
    auto mask = m.ReadU32(table + 4), index = Index(mask);
    unsigned result;
    if (unsigned(id) - 32 <= 31) {
      result = m.ReadU32(owner + 4 * (index + 127));
      if (id == 32 || id == 33) {
        auto bonusTable = 0x83213438u + (id == 32 ? 24 : 32);
        auto bank = m.ReadU32(bonusTable),
             bonusMask = m.ReadU32(bonusTable + 4);
        if (m.ReadU32(owner + 272 * bank + 1320) & bonusMask) {
          auto bonus =
              m.ReadU32(owner + 4 * (68 * bank + Index(bonusMask) + 331));
          if (std::int32_t(bonus) > std::int32_t(result))
            result = bonus;
        }
      }
      result -= m.ReadU32(owner + 4 * (index + 195));
    } else
      result = m.ReadU32(
          owner + 4 * (68 * (m.ReadU32(table) + unsigned(q)) + index + 91));
    s.r[3] = result;
    return true;
  }
  if (e != 0x82b08d98 && e != 0x82b08e28 && e != 0x82b08ea8)
    return false;
  unsigned frame = e == 0x82b08d98   ? 128
                   : e == 0x82b08e28 ? 112
                                     : 144,
           first = e == 0x82b08d98   ? 30
                   : e == 0x82b08e28 ? 31
                                     : 27;
  auto old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  if (e == 0x82b08ea8)
    recovery_abi::WriteU64(m, old - 56, s.fpr_bits[31]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  if (e == 0x82b08d98) {
    m.WriteU32(sp + 80, 0x8204a1d8);
    s.r[3] = m.ReadU32(owner + 8);
    s.r[4] = 36;
    (void)battle_evaluation_chance61::Apply(0x82ac9858, m, d, s);
    auto value = std::int32_t(Address(s.r[3]) * 5u);
    recovery_abi::WriteU64(m, sp + 88, std::uint64_t(std::int64_t(value)));
    FloatMode(d, s);
    s.fpr_bits[0] = std::bit_cast<std::uint64_t>(double(value));
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(float(value)));
    m.WriteU32(sp + 80, 0x8204a1d8);
  } else {
    bool allowed = true, result = e == 0x82b08e28 && m.ReadU8(owner + 77) != 0;
    if (e == 0x82b08ea8) {
      m.WriteU32(sp + 84, 0x8204a1d8);
      allowed = m.ReadU32(owner + 184) != 1;
      if (allowed) {
        s.r[3] = m.ReadU32(owner + 8);
        s.r[4] = 163;
        (void)battle_action_readiness61::Apply(0x8238e368, m, d, s);
        allowed = (Address(s.r[3]) & 255) != 1;
      }
    }
    if (allowed && !result) {
      FloatMode(d, s);
      auto chance = double(std::bit_cast<float>(m.ReadU32(owner + 88)));
      auto sourceId = m.ReadU32(m.ReadU32(owner + 4) + 64),
           random = m.ReadU32(0x83264558);
      if (e == 0x82b08ea8) {
        s.fpr_bits[31] = std::bit_cast<std::uint64_t>(chance);
        s.r[3] = owner;
        (void)battle_evaluation_chance61::Apply(0x82b08d98, m, d, s);
        chance = double(float(chance - std::bit_cast<double>(s.fpr_bits[1])));
      }
      auto threshold = Truncate(chance);
      s.fpr_bits[0] = std::bit_cast<std::uint64_t>(std::int64_t(threshold));
      m.WriteU32(sp + 80, unsigned(threshold));
      s.r[3] = random;
      s.r[4] = unsigned(threshold);
      s.r[5] = e == 0x82b08e28 ? 2 : 3;
      s.r[6] = sourceId;
      (void)battle_random_range61::Apply(0x82aa0838, m, d, s);
      result = (Address(s.r[3]) & 255) != 0;
    }
    s.r[3] = result;
    if (e == 0x82b08ea8)
      m.WriteU32(sp + 84, 0x8204a1d8);
  }
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  if (e == 0x82b08ea8)
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - 56);
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_evaluation_chance61
