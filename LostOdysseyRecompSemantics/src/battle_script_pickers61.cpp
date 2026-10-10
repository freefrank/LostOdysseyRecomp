#include "lo_semantics/battle_script_pickers61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/battle_script_skill_cost61.h"
#include "lo_semantics/battle_script_preparation61.h"
namespace lo::semantic::gpu::battle_script_pickers61 {
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
    if (!battle_script_skill_cost61::Apply(e, m, d, s))
      d.guest.CallDirect(e, m, s);
  }
  unsigned Manager(unsigned method) {
    Call(0x82380a18);
    Call(method);
    return Address(s.r[3]);
  }
  unsigned Find(unsigned id) {
    Manager(0x82389b78);
    s.r[4] = id;
    Call(0x8238e308);
    return Address(s.r[3]);
  }
  unsigned Virtual(unsigned object, unsigned slot) {
    s.r[3] = object;
    s.ctr = W(W(object) + slot);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    return Address(s.r[3]);
  }
  unsigned Candidates(unsigned resource, unsigned target, unsigned table,
                      unsigned count) {
    unsigned found = 0;
    for (unsigned i = 0; i < count; ++i) {
      auto id = W(table + 4 * i);
      s.r[3] = owner;
      s.r[4] = resource;
      s.r[5] = target;
      s.r[6] = id;
      (void)battle_script_pickers61::Apply(0x82afa778, m, d, s);
      if (Address(s.r[3]) & 255)
        m.WriteU32(Address(s.r[1]) + 80 + 4 * found++, id);
    }
    return found;
  }
  unsigned Bits(unsigned value) {
    unsigned n = 0;
    auto v = std::int32_t(value);
    while (v) {
      n += unsigned(v) & 1;
      v >>= 1;
    }
    return n;
  }
  void Run(unsigned e) {
    if (e == 0x82afa778) {
      auto resource = Address(s.r[4]), target = Address(s.r[5]),
           id = Address(s.r[6]), sp = Address(s.r[1]);
      unsigned result = 0;
      m.WriteU32(sp + 80, 0x8204a1d8);
      for (unsigned i = 0; i < 512; ++i) {
        auto slot = resource + 15212 + 44 * i;
        if (W(slot) != id)
          continue;
        s.r[3] = resource;
        s.r[4] = 2;
        s.r[5] = W(slot);
        Call(0x82ac9aa8);
        if ((Address(s.r[3]) & 255) != 1)
          continue;
        auto value = W(slot);
        Manager(0x82389b78);
        s.r[4] = resource;
        s.r[5] = target;
        s.r[6] = 2;
        s.r[7] = value;
        Call(0x82ad0c10);
        if (Address(s.r[3]) & 255) {
          result = 1;
          break;
        }
      }
      s.r[3] = result;
      m.WriteU32(sp + 80, 0x8204a1d8);
      return;
    }
    auto kindOut = Address(s.r[5]), idOut = Address(s.r[6]),
         resource = W(Actor() + 4);
    if (!resource) {
      s.r[3] = 0;
      return;
    }
    auto target = Find(e == 0x82aff9e8 ? W(resource + 64) : 20);
    unsigned count = 0, tag = 0;
    if (e == 0x82aff4e8) {
      auto flags = W(target + 232);
      if (!flags) {
        s.r[3] = 0;
        return;
      }
      constexpr unsigned table = 0x832139e8;
      if (Bits(flags) > 1) {
        count = Candidates(resource, target, table + 80, 5);
        if (count)
          tag = 90;
      }
      if (!count && (Virtual(target, 284) || Virtual(target, 352))) {
        count = Candidates(resource, target, table, 5);
        if (count)
          tag = 91;
      }
      if (!count && Virtual(target, 288)) {
        count = Candidates(resource, target, table + 20, 5);
        if (count)
          tag = 92;
      }
      if (!count && Virtual(target, 344)) {
        count = Candidates(resource, target, table + 40, 5);
        if (count)
          tag = 93;
      }
      if (!count && (Virtual(target, 380) || Virtual(target, 420))) {
        count = Candidates(resource, target, table + 60, 5);
        if (count)
          tag = 94;
      }
      if (!count && Bits(flags) > 1) {
        count = Candidates(resource, target, table + 80, 5);
        if (count)
          tag = 95;
      }
    } else {
      count = Candidates(resource, target,
                         e == 0x82aff8e0 ? 0x83213a4c : 0x83213a68,
                         e == 0x82aff8e0 ? 6 : 53);
      tag = e == 0x82aff8e0 ? 100 : 101;
    }
    if (!count) {
      s.r[3] = 0;
      return;
    }
    s.r[3] = W(0x83264558);
    s.r[4] = 0;
    s.r[5] = count - 1;
    s.r[6] = tag;
    s.r[7] = W(resource + 64);
    Call(0x82aa0740);
    auto id = W(Address(s.r[1]) + 80 + 4 * Address(s.r[3]));
    m.WriteU32(idOut, id);
    unsigned mode = 4;
    if (e == 0x82aff9e8)
      mode = (W(W(0x83264984) + 96 * id + 8) & 8) ? 2 : 17;
    s.r[3] = W(0x832ca0d8);
    s.r[4] = id;
    Call(0x82b08b80);
    m.WriteU32(kindOut, Address(s.r[3]));
    s.r[3] = owner;
    s.r[4] = mode;
    s.r[5] = 0;
    (void)battle_script_preparation61::Apply(0x82afde70, m, d, s);
    s.r[3] = 1;
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned first = 22, frame = 1200;
  switch (e) {
  case 0x82aff4e8:
    first = 21;
    break;
  case 0x82aff8e0:
  case 0x82aff9e8:
    break;
  case 0x82afa778:
    first = 23;
    frame = 176;
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
} // namespace lo::semantic::gpu::battle_script_pickers61
