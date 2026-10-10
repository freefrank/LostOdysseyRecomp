#include "lo_semantics/battle_script_marshaling61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/battle_script_execution61.h"
namespace lo::semantic::gpu::battle_script_marshaling61 {
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
    if (!battle_script_execution61::Apply(e, m, d, s))
      d.guest.CallDirect(e, m, s);
  }
  unsigned Manager(unsigned method) {
    Call(0x82380a18);
    Call(method);
    return Address(s.r[3]);
  }
  void Run(unsigned e) {
    auto sp = Address(s.r[1]);
    if (e == 0x82b00d08) {
      auto mode = Get(1);
      m.WriteU32(sp + 80, 0);
      m.WriteU32(sp + 84, 0);
      auto manager = Manager(0x82389b78);
      Call(0x82acf108);
      auto state = W(manager + 24), resource = W(Actor() + 4);
      unsigned result = 0, a = 0, b = 0;
      if (resource) {
        bool execute = true;
        if ((mode == 3 || mode == 4) && state == 1)
          mode = 1;
        if (mode == 0 || mode == 4) {
          s.r[3] = owner;
          s.r[4] = mode == 4 ? 17 : 0;
          s.r[5] = 0;
          Call(0x82afde70);
          if (mode == 4)
            a = 30;
        } else if (mode <= 3) {
          s.r[3] = owner;
          s.r[4] = mode;
          s.r[5] = sp + 80;
          s.r[6] = sp + 84;
          Call(mode == 1 ? 0x82aff4e8 : mode == 2 ? 0x82aff8e0 : 0x82aff9e8);
          execute = (Address(s.r[3]) & 255) != 0;
          if (execute) {
            a = W(sp + 80);
            b = W(sp + 84);
          } else
            result = 1;
        }
        if (execute) {
          m.WriteU32(resource + 14680, 0);
          s.r[3] = owner;
          s.r[4] = a;
          s.r[5] = b;
          Call(0x82b00698);
        }
      }
      Set(3, result);
      Next(5);
      return;
    }
    auto a = Get(1), b = Get(3), c = Get(5);
    if (e == 0x82afcec8) {
      auto dval = Get(7);
      s.r[3] = 0x832ca0e0 + 5232;
      s.r[4] = a;
      s.r[5] = b;
      s.r[6] = c != 0;
      s.r[7] = dval != 0;
      Call(0x82b2a138);
      Next(9);
      return;
    }
    auto buffer = sp + (e == 0x82afcb08 ? 160 : 80);
    for (unsigned i = 0; i < 64; ++i)
      m.WriteU16(buffer + 2 * i, 0);
    for (unsigned i = 0; i < 32; ++i) {
      auto actor = Actor(), p = W(actor + 36) + W(actor + 52) + 7 + 2 * i;
      auto value = (unsigned(m.ReadU8(p)) << 8) | m.ReadU8(p + 1);
      // The second source loop intentionally overwrites its first unit.
      m.WriteU16(buffer + (e == 0x82afcb08 ? 2 * i : 0), value);
      if (e == 0x82afcb08)
        m.WriteU16(actor + 340 + 2 * i, value);
    }
    s.r[3] = 0x832ca0e0;
    s.r[4] = a;
    if (e == 0x82afcf60) {
      s.r[3] += 5232;
      s.r[5] = buffer;
      s.r[6] = b != 0;
      s.r[7] = c != 0;
      Call(0x82b2a330);
    } else {
      auto zero = W(0x82000e50), one = W(0x82007784);
      m.WriteU32(sp + 116, 0);
      m.WriteU32(sp + 136, zero);
      m.WriteU32(sp + 128, 0);
      m.WriteU32(sp + 132, 0);
      m.WriteU32(sp + 144, one);
      m.WriteU32(sp + 148, one);
      m.WriteU32(sp + 152, one);
      m.WriteU32(sp + 88, zero);
      recovery_abi::WriteU64(m, sp + 96, 0);
      m.WriteU32(sp + 104, 0);
      s.r[5] = b;
      s.r[6] = c;
      s.r[7] = buffer;
      s.r[8] = (std::uint64_t(one) << 32) | one;
      s.r[9] = std::uint64_t(one) << 32;
      s.r[10] = (std::uint64_t(zero) << 32) | zero;
      Call(0x82aadcb8);
    }
    Next(71);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned first, frame;
  switch (e) {
  case 0x82b00d08:
    first = 26;
    frame = 144;
    break;
  case 0x82afcb08:
    first = 28;
    frame = 720;
    break;
  case 0x82afcec8:
    first = 28;
    frame = 128;
    break;
  case 0x82afcf60:
    first = 29;
    frame = 624;
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
} // namespace lo::semantic::gpu::battle_script_marshaling61
