#include "lo_semantics/battle_script_owned_labels61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::battle_script_owned_labels61 {
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
  void Call(unsigned e) { d.guest.CallDirect(e, m, s); }
  unsigned Manager(unsigned method) {
    Call(0x82380a18);
    Call(method);
    return Address(s.r[3]);
  }
  void Run(unsigned e) {
    auto sp = Address(s.r[1]);
    if (e == 0x82afb0c8 || e == 0x82afb198 || e == 0x82afb120) {
      auto a = Get(1);
      unsigned b = 0;
      if (e == 0x82afb120)
        b = Get(3);
      s.r[3] = 0x832ca0e0 + 5232;
      s.r[4] = a;
      if (e == 0x82afb120) {
        s.r[5] = b;
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(
            double(std::bit_cast<float>(W(0x82000e50))));
      }
      Call(e == 0x82afb0c8   ? 0x82ab6850
           : e == 0x82afb198 ? 0x82ab4758
                             : 0x82ab6b00);
      if (e == 0x82afb198 && !(Address(s.r[3]) & 255)) {
        s.r[3] = owner;
        s.r[4] = 3;
        (void)battle_script_extensions61::Apply(0x8238c590, m, d, s);
        m.WriteU32(Actor() + 52, Address(s.r[3]));
      } else
        Next(e == 0x82afb0c8 ? 3 : 5);
      return;
    }
    bool flagged = e == 0x82b018c0 || e == 0x82b01ae8;
    auto a = Get(flagged ? 2 : 1);
    unsigned b = 0;
    if (e == 0x82b016e8)
      b = Get(3);
    unsigned offset = e == 0x82b016e8 ? 5 : e == 0x82b01ae8 ? 4 : 3;
    for (unsigned i = 0; i < 64; ++i)
      m.WriteU16(sp + 96 + 2 * i, 0);
    for (unsigned i = 0; i < 32; ++i) {
      auto actor = Actor(), p = W(actor + 36) + W(actor + 52) + offset + 2 * i;
      m.WriteU16(sp + 96 + 2 * i,
                 (unsigned(m.ReadU8(p)) << 8) | m.ReadU8(p + 1));
    }
    unsigned flag = flagged && Mode() == 1;
    m.WriteU32(sp + 92, sp + 80);
    unsigned count = 0;
    if (m.ReadU16(sp + 96)) {
      s.r[3] = sp + 96;
      Call(0x82296830);
      count = Address(s.r[3]) + 1;
    }
    if (flagged) {
      s.r[3] = sp + 80;
      s.r[4] = count;
      Call(0x822954d8);
    } else {
      m.WriteU32(sp + 80, 0);
      m.WriteU32(sp + 84, count);
      m.WriteU32(sp + 88, count);
      if (count) {
        auto allocator = W(0x8330b608);
        if (!allocator) {
          s.r[3] = 0;
          Call(0x827c5f38);
          allocator = W(0x8330b608);
        }
        s.r[3] = allocator;
        s.r[4] = 0;
        s.r[5] = 2 * count;
        s.r[6] = 8;
        s.ctr = W(W(allocator) + 8);
        d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        m.WriteU32(sp + 80, Address(s.r[3]));
      }
    }
    if (W(sp + 84)) {
      s.r[3] = W(sp + 80);
      s.r[4] = sp + 96;
      s.r[5] = 2 * W(sp + 84);
      Call(0x82b7a0b0);
    }
    s.r[3] = 0x832ca0e0 + 5232;
    s.r[4] = a;
    if (e == 0x82b016e8) {
      s.r[5] = b;
      s.r[6] = sp + 80;
      Call(0x82ab8e08);
      Next(69);
    } else {
      s.r[5] = sp + 80;
      if (flagged)
        s.r[6] = flag;
      Call(e == 0x82b01d10   ? 0x82abde20
           : e == 0x82b018c0 ? 0x82ab68c0
                             : 0x82abdc68);
      Next(flagged ? 68 : 67);
    }
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned first = 31, frame = 96;
  switch (e) {
  case 0x82b016e8:
    first = 26;
    frame = 672;
    break;
  case 0x82b018c0:
  case 0x82b01ae8:
    first = 29;
    frame = 640;
    break;
  case 0x82b01d10:
    first = 27;
    frame = 656;
    break;
  case 0x82afb120:
    first = 30;
    frame = 112;
    break;
  case 0x82afb0c8:
  case 0x82afb198:
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
} // namespace lo::semantic::gpu::battle_script_owned_labels61
