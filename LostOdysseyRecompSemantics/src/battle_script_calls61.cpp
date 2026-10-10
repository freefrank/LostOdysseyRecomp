#include "lo_semantics/battle_script_calls61.h"
#include "lo_semantics/battle_script_core61.h"
#include "lo_semantics/battle_script_parameters61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_script_calls61 {
namespace {
using recovery_abi::Address;
}
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82a9c5e8 && e != 0x82a9c650 && e != 0x82a9c928 &&
      e != 0x82a9c9a8 && e != 0x82a9ca28)
    return false;
  auto owner = Address(s.r[3]);
  auto actor = [&]() { return m.ReadU32(owner + 24); };
  if (e == 0x82a9c9a8) {
    auto a = actor(), depth = m.ReadU32(a + 44);
    if (depth == 32) {
      auto state = m.ReadU32(owner + 44);
      m.WriteU32(state + 28, m.ReadU32(state + 28) | 0x80000000);
      return true;
    }
    auto pc = m.ReadU32(a + 52), code = m.ReadU32(a + 36) + pc;
    m.WriteU32(m.ReadU32(a + 48) + 4 * depth, pc + 3);
    unsigned target =
        unsigned(m.ReadU8(code + 1)) | (unsigned(m.ReadU8(code + 2)) << 8);
    m.WriteU32(a + 52, unsigned(std::int32_t(std::int16_t(target))));
    m.WriteU32(a + 44, depth + 1);
    return true;
  }
  if (e == 0x82a9ca28) {
    auto a = actor(), depth = m.ReadU32(a + 44);
    if (!depth)
      return battle_script_core61::Apply(0x8238c648, m, d, s);
    m.WriteU32(a + 44, depth - 1);
    m.WriteU32(a + 52, m.ReadU32(m.ReadU32(a + 48) + 4 * (depth - 1)));
    return true;
  }
  auto old = Address(s.r[1]);
  unsigned frame = e == 0x82a9c5e8 ? 96 : 112,
           first = e == 0x82a9c5e8 ? 31 : 30;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  auto get = [&](unsigned offset) {
    s.r[3] = owner;
    s.r[4] = offset;
    s.r[5] = 0;
    (void)battle_script_parameters61::Apply(0x8238be38, m, d, s);
    return Address(s.r[3]);
  };
  auto set = [&](unsigned offset, unsigned value) {
    s.r[3] = owner;
    s.r[4] = offset;
    s.r[5] = value;
    s.r[6] = 0;
    (void)battle_script_core61::Apply(0x8238c210, m, d, s);
  };
  if (e == 0x82a9c5e8) {
    auto right = get(3), left = get(1);
    set(1, left * right);
  } else {
    auto left = get(1), right = get(3);
    if (e == 0x82a9c928) {
      set(1, right);
      set(3, left);
    } else
      set(1, left && right ? unsigned(std::int32_t(left) / std::int32_t(right))
                           : 0);
  }
  m.WriteU32(actor() + 52, m.ReadU32(actor() + 52) + 5);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_calls61
