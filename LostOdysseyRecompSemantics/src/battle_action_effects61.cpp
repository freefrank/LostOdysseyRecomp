#include "lo_semantics/battle_action_parameters61.h"
#include "lo_semantics/battle_action_snapshot61.h"
#include "lo_semantics/battle_action_finalize61.h"
#include "lo_semantics/battle_action_storage61.h"
#include "lo_semantics/battle_action_effects61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/battle_script_events61.h"
namespace lo::semantic::gpu::battle_action_effects61 {
namespace {
using recovery_abi::Address;
struct Runtime {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Actor() { return W(owner + 24); }
  unsigned Mode() {
    auto a = Actor();
    return m.ReadU8(W(a + 36) + W(a + 52) +
                    ((W(W(owner + 44) + 28) & 0x04000000) ? 2 : 1));
  }
  unsigned Get(unsigned off) {
    s.r[3] = owner;
    s.r[4] = off;
    (void)battle_script_extensions61::Apply(0x8238c198, m, d, s);
    return Address(s.r[3]);
  }
  void Set(unsigned off, unsigned value) {
    s.r[3] = owner;
    s.r[4] = off;
    s.r[5] = value;
    (void)battle_script_extensions61::Apply(0x8238c208, m, d, s);
  }
  void Next(unsigned n) {
    auto a = Actor();
    m.WriteU32(a + 52, W(a + 52) + n);
  }
  void Call(unsigned e) {
    if (!battle_action_parameters61::Apply(e, m, d, s) &&
        !battle_action_snapshot61::Apply(e, m, d, s) &&
        !battle_action_finalize61::Apply(e, m, d, s) &&
        !battle_action_storage61::Apply(e, m, d, s))
      d.guest.CallDirect(e, m, s);
  }
  unsigned Manager(unsigned method) {
    Call(0x82380a18);
    Call(method);
    return Address(s.r[3]);
  }
  void Run(unsigned e) {
    if (e == 0x82af68d8) {
      s.r[4] = W(Address(s.r[4]) + 148);
      (void)battle_script_events61::Apply(0x8238c118, m, d, s);
      auto actor = Address(s.r[3]);
      if (actor)
        m.WriteU32(actor + 64, W(actor + 64) | 0x20000000u);
      return;
    }
    if (e == 0x82ab31e0) {
      m.WriteU32(owner + 14680, 0);
      s.r[3] = owner + 14656;
      s.r[4] = 0;
      Call(0x82a9b698);
      s.r[3] = owner;
      s.r[4] = 0;
      Call(0x82ab2d88);
      m.WriteU32(owner + 124, W(owner + 124) & ~0x20000u);
      m.WriteU32(owner + 76316, 0);
      return;
    }
    auto kind = Address(s.r[4]), detail = Address(s.r[5]),
         variant = Address(s.r[6]);
    unsigned reset = 0, special = 0;
    if (kind == 0 || kind == 13 || kind == 14 || kind == 15 || kind == 18 ||
        kind == 19) {
      s.r[3] = W(0x8324570c);
      s.r[4] = 0;
      s.r[5] = kind == 14 ? 1 : 24;
      s.r[6] = 0;
      Call(0x82acd3b0);
      reset = 1;
    } else if (kind == 1 || kind == 12 || kind == 16 || kind == 22 ||
               kind == 30 || kind == 31) {
      s.r[3] = W(0x832ca0cc);
      s.r[4] = owner;
      s.r[5] = kind;
      s.r[6] = detail;
      s.r[7] = variant;
      Call(0x82b21340);
    } else if (kind == 2 || kind == 3 || (kind >= 6 && kind <= 11)) {
      special = kind == 2 || (kind >= 6 && kind <= 9);
      s.r[3] = W(0x832ca0d8);
      s.r[4] = owner;
      s.r[5] = kind;
      s.r[6] = detail;
      s.r[7] = variant;
      Call(0x82b11df0);
    } else if (kind == 4 || kind == 5) {
      s.r[3] = W(0x832cb78c);
      s.r[4] = owner;
      s.r[5] = kind;
      s.r[6] = detail;
      s.r[7] = variant;
      Call(0x82b1f798);
    } else if (kind >= 25 && kind <= 29) {
      s.r[3] = W(0x8324570c);
      s.r[4] = 0;
      s.r[5] = W(Address(s.r[3]) + 16);
      s.r[6] = 0xffffffff;
      Call(0x82acd3b0);
    }
    s.r[3] = W(0x8324570c);
    s.r[4] = owner;
    s.r[5] = reset;
    s.r[6] = variant;
    s.r[7] = special;
    Call(0x82acde40);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82b1f1d0) {
    auto resource = Address(s.r[3]), sp = Address(s.r[1]);
    for (unsigned i = 0; i < 8; ++i)
      m.WriteU8(sp - 16 + i, i);
    m.WriteU16(sp - 8, 0);
    auto value = m.ReadU32(resource + 60);
    unsigned next = value < 6 ? value + 1 : 0;
    m.WriteU32(resource + 60, next);
    s.r[3] = next;
    return true;
  }
  unsigned first, frame;
  switch (e) {
  case 0x82ab0d50:
    first = 27;
    frame = 128;
    break;
  case 0x82af68d8:
    first = 32;
    frame = 96;
    break;
  case 0x82ab31e0:
    first = 30;
    frame = 112;
    break;
  default:
    return false;
  }
  auto owner = Address(s.r[3]), old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  Runtime{m, d, s, owner}.Run(e);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_action_effects61
