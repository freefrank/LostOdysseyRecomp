#include "lo_semantics/cpx_lifecycle61.h"
#include "lo_semantics/cpx_context61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::cpx_lifecycle61 {
namespace {
using recovery_abi::Address;
bool Unlink(GuestMemory &m, Dependencies d, Registers &s, unsigned registry,
            unsigned node) {
  if (!node)
    return false;
  auto lock = registry + 4388;
  s.r[3] = lock;
  d.guest.CallDirect(0x830d9c6c, m, s);
  auto link = registry + 4348, current = m.ReadU32(link);
  bool removed = false;
  while (current) {
    if (current == node) {
      m.WriteU32(link, m.ReadU32(node));
      m.WriteU32(node, 0);
      removed = true;
      break;
    }
    link = current;
    current = m.ReadU32(current);
  }
  s.r[3] = lock;
  d.guest.CallDirect(0x830d9c7c, m, s);
  return removed;
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x8284d6c8 && e != 0x82850ac0 && e != 0x82857820)
    return false;
  auto self = Address(s.r[3]), arg = Address(s.r[4]), old = Address(s.r[1]);
  auto frame = e == 0x8284d6c8 ? 128u : 96u,
       first = e == 0x8284d6c8 ? 28u : 31u;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  if (e == 0x8284d6c8)
    s.r[3] = Unlink(m, d, s, self, arg);
  else if (e == 0x82850ac0) {
    // Registry singleton construction remains an explicit guest boundary.
    d.guest.CallDirect(0x82850970, m, s);
    s.r[4] = self;
    (void)cpx_lifecycle61::Apply(0x8284d6c8, m, d, s);
  } else {
    (void)cpx_lifecycle61::Apply(0x82850ac0, m, d, s);
    m.WriteU8(self + 21, 0);
    s.r[3] = self;
    (void)cpx_context61::Apply(0x82857290, m, d, s);
    s.r[3] = self;
    (void)cpx_context61::Apply(0x82857200, m, d, s);
  }
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::cpx_lifecycle61
