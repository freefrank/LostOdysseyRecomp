#include "lo_semantics/battle_random_range61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_random_range61 {
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82aa0740)
    return false;
  using recovery_abi::Address;
  auto owner = Address(s.r[3]), low = Address(s.r[4]), high = Address(s.r[5]),
       tag = Address(s.r[6]), id = Address(s.r[7]), old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = 27; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= 128;
  m.WriteU32(Address(s.r[1]), old);
  unsigned group;
  if (std::int32_t(id) < 20) {
    d.guest.CallDirect(0x82380a18, m, s);
    d.guest.CallDirect(0x82389b78, m, s);
    s.r[4] = id;
    d.guest.CallDirect(0x8238e308, m, s);
    group = m.ReadU32(Address(s.r[3]) + 68);
  } else
    group = id == 255 ? 31 : id - 9;
  if (std::int32_t(group) > 31)
    group = 31;
  if (std::int32_t(low) < 0 || std::int32_t(high) < 0)
    s.r[3] = 0;
  else if (low == high)
    s.r[3] = high;
  else {
    auto slot = owner + 4 * (128 * group + tag + 3), index = m.ReadU32(slot);
    auto value = std::int32_t(m.ReadU32(0x831f3300 + 4 * index));
    auto span = std::int32_t(high - low + 1);
    // Preserve signed division/remainder and the caller-supplied range
    // contract.
    auto result = unsigned(std::int64_t(value) % span) + low;
    m.WriteU32(owner + 4, result);
    auto next = m.ReadU32(slot) + 1;
    m.WriteU32(slot, next);
    if (std::int32_t(next) >= 32768)
      m.WriteU32(slot, 0);
    s.r[3] = m.ReadU32(owner + 4);
  }
  s.r[1] += 128;
  for (unsigned i = 27; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_random_range61
