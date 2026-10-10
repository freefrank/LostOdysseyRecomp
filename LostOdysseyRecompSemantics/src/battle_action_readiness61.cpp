#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/recovery_abi.h"
#include <initializer_list>
namespace lo::semantic::gpu::battle_action_readiness61 {
namespace {
using recovery_abi::Address;
bool Property(GuestMemory &m, unsigned resource, unsigned id) {
  auto signedId = std::int32_t(id), quotient = signedId / 32;
  auto p = 0x83213438u + 8 * unsigned(signedId - quotient * 32);
  auto bank = m.ReadU32(p) + unsigned(quotient);
  return (m.ReadU32(resource + 272 * bank + 232) & m.ReadU32(p + 4)) != 0;
}
bool Flags(GuestMemory &m, unsigned resource) {
  return (m.ReadU32(resource + 124) & 0x210000u) != 0;
}
bool Unavailable(GuestMemory &m, unsigned resource, bool extended) {
  for (unsigned id : {0u, 3u, 15u, 16u, 28u, 20u})
    if (Property(m, resource, id))
      return true;
  if (extended)
    for (unsigned id : {242u, 243u, 111u})
      if (Property(m, resource, id))
        return true;
  return Flags(m, resource);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  auto resource = Address(s.r[3]);
  if (e == 0x8238e368) {
    auto id = Address(s.r[4]);
    s.r[3] = std::int32_t(id) > 262 ? 0 : unsigned(Property(m, resource, id));
    return true;
  }
  if (e == 0x82ac9768) {
    s.r[3] = Unavailable(m, resource, false);
    return true;
  }
  unsigned frame, first;
  if (e == 0x8238a8a0) {
    frame = 112;
    first = 30;
  } else if (e == 0x82ac9a28) {
    frame = 96;
    first = 31;
  } else
    return false;
  auto old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  if (e == 0x8238a8a0)
    s.r[3] = Unavailable(m, resource, true);
  else {
    auto mode = Address(s.r[4]) & 255;
    (void)battle_action_readiness61::Apply(mode ? 0x82ac9768 : 0x8238a8a0, m, d,
                                           s);
    s.r[3] = (Address(s.r[3]) & 255) != 0;
  }
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_action_readiness61
