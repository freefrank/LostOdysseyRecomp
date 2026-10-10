#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/battle_script_events61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/battle_script_execution61.h"
namespace lo::semantic::gpu::battle_script_actions61 {
namespace {
using recovery_abi::Address;
struct Actions {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner, sp;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Actor() { return W(owner + 24); }
  unsigned Resource() { return W(Actor() + 4); }
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
  unsigned Mode() {
    auto a = Actor();
    return m.ReadU8(W(a + 36) + W(a + 52) +
                    ((W(W(owner + 44) + 28) & 0x04000000) ? 2 : 1));
  }
  void Branch(bool pass) {
    if (pass) {
      Next(3);
      return;
    }
    s.r[3] = owner;
    s.r[4] = 1;
    (void)battle_script_extensions61::Apply(0x8238c590, m, d, s);
    m.WriteU32(Actor() + 52, Address(s.r[3]));
  }
  void Call(unsigned e) {
    if (!battle_script_execution61::Apply(e, m, d, s))
      d.guest.CallDirect(e, m, s);
  }
  unsigned Manager() {
    Call(0x82380a18);
    Call(0x82389b78);
    return Address(s.r[3]);
  }
  unsigned Find(unsigned id) {
    Manager();
    s.r[4] = id;
    Call(0x8238e308);
    return Address(s.r[3]);
  }
  unsigned Other() {
    s.r[3] = owner;
    s.r[4] = 1;
    (void)battle_script_actions61::Apply(0x8238c0d8, m, d, s);
    auto id = Address(s.r[3]);
    s.r[3] = owner;
    s.r[4] = id;
    (void)battle_script_events61::Apply(0x8238c118, m, d, s);
    return Address(s.r[3]);
  }
  void RefreshFlag() {
    auto resource = Resource();
    if (resource && !(W(resource + 100) & 0x80000000)) {
      auto a = Actor();
      m.WriteU32(a + 64, W(a + 64) & ~0x20000000u);
    }
  }
  unsigned CanProceed() {
    auto kind = W(Actor() + 88);
    if (kind - 1 > 10)
      return 1;
    if (kind == 1 || kind == 4 || kind == 5)
      return 0;
    auto target = Find(m.ReadU8(W(Actor() + 80)));
    auto a = Actor(), resource = W(a + 4), detail = W(a + 92);
    kind = W(a + 88);
    auto manager = Manager();
    s.r[3] = manager;
    s.r[4] = resource;
    s.r[5] = target;
    s.r[6] = kind;
    s.r[7] = detail;
    Call(0x82ad0c10);
    return (Address(s.r[3]) & 255) == 1 ? 0 : 1;
  }
  void Run(unsigned e) {
    if (e == 0x8238c510) {
      RefreshFlag();
      return;
    }
    if (e == 0x82af65f8) {
      s.r[3] = CanProceed();
      return;
    }
    if (e == 0x8238d0e8) {
      auto manager = Manager();
      Set(1, W(manager + 56));
      Next(3);
      return;
    }
    if (e == 0x82af99d8) {
      auto value = Mode(), a = Actor();
      m.WriteU32(a + 64, (W(a + 64) & ~0x10000000u) | ((value & 1) << 28));
      Next(2);
      return;
    }
    if (e == 0x82af6e10) {
      Branch(!(W(Actor() + 64) & 0x20000000));
      return;
    }
    if (e == 0x82af7110) {
      bool pass = Resource() != 0;
      if (pass && (W(Actor() + 64) & 0x20000000)) {
        s.r[3] = owner;
        (void)battle_script_actions61::Apply(0x82af65f8, m, d, s);
        pass = Address(s.r[3]) == 1;
      }
      Branch(pass);
      return;
    }
    if (e == 0x8238c3e8) {
      auto resource = Resource();
      bool branch = !resource;
      if (resource) {
        Call(0x82380a18);
        Call(0x82389b48);
        auto mode = Address(s.r[3]) & 255;
        auto category = W(resource + 60) & 255;
        if (mode <= 3 || mode >= 9 || category == 5) {
          RefreshFlag();
          branch = true;
        } else
          branch = category == 4 || category == 6 || W(resource + 156) == 6 ||
                   (W(resource + 124) & 0x200000);
      }
      if (branch) {
        Branch(false);
        return;
      }
      auto a = Actor();
      m.WriteU32(a + 96, 0);
      m.WriteU32(a + 64, W(a + 64) & ~0x01000000u);
      if (W(resource + 100) & 0x80000000) {
        s.r[3] = W(0x8324570c);
        s.r[4] = resource;
        Call(0x82acd530);
      }
      m.WriteU32(resource + 124, W(resource + 124) | 512);
      Next(3);
      return;
    }
    if (e == 0x82b00888) {
      auto a = Actor(), resource = W(a + 4), busy = W(a + 96);
      bool run = resource != 0;
      if (run && std::int32_t(W(a + 84)) <= 0) {
        if (Get(1) != 0)
          run = false;
        else {
          m.WriteU8(W(Actor() + 80), W(resource + 64));
          m.WriteU32(Actor() + 84, 1);
        }
      }
      if (run && std::int32_t(busy) > 0 && !(m.ReadU8(Actor() + 64) & 1))
        run = false;
      if (run) {
        auto event = W(Actor() + 60), second = Get(3), first = Get(1);
        s.r[3] = owner;
        s.r[4] = first;
        s.r[5] = second;
        Call(event == 2 ? 0x82afdcf0 : event == 3 ? 0x82afdb90 : 0x82b00698);
      }
      Next(5);
      return;
    }
    if (e == 0x82afe628) {
      auto value = Get(1);
      unsigned result = 0;
      if (Resource()) {
        s.r[3] = owner;
        s.r[4] = value;
        s.r[5] = 0;
        Call(0x82afde70);
        result = (Address(s.r[3]) & 255) == 1;
      }
      Set(3, result);
      Next(5);
      return;
    }
    if (e == 0x82af6e70) {
      auto id = Get(1);
      bool ready = false;
      if (Resource()) {
        auto resource = Find(id);
        if (resource) {
          s.ctr = W(W(resource) + 292);
          d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
          ready = Address(s.r[3]) == 0;
        }
      }
      if (ready) {
        m.WriteU8(W(Actor() + 80), id);
        m.WriteU32(Actor() + 84, 1);
        Set(3, 0);
        Next(5);
        m.WriteU32(Actor() + 336, 8);
      } else {
        Set(3, 1);
        Next(5);
      }
      return;
    }
    if (e == 0x82af9c10) {
      auto a = Other(), resource = W(a + 4);
      unsigned x = 0xffffffff, y = 0, z = 0;
      if (resource && W(resource + 14660)) {
        auto record = W(resource + 14656);
        x = W(record);
        y = W(record + 4);
        z = W(record + 14884);
      }
      Set(5, x);
      Set(7, y);
      Set(9, z);
      Next(11);
      return;
    }
    if (e == 0x82af9740) {
      auto resource = Resource();
      if (resource) {
        auto slot = Get(1), value = Get(3);
        if (slot == 0xffffffff) {
          s.r[3] = W(0x83291dc0);
          Call(0x82ac3118);
        } else {
          auto p = resource + 16 * slot;
          m.WriteU32(p + 75932, value);
          if (value == 23 || value == 24 || !value) {
            m.WriteU32(resource + 16 * (slot + 4746), 0xfffffffb);
            m.WriteU32(p + 75940, 0);
            m.WriteU32(p + 75944, 0);
          }
        }
      }
      Next(5);
      return;
    }
    if (e == 0x82af9568) {
      auto resource = Resource();
      if (resource) {
        auto mode = Mode();
        if (mode < 2)
          m.WriteU32(resource + 132, mode);
        else if (mode == 2 && (m.ReadU16(Manager() + 148) & 1)) {
          auto manager = Manager();
          m.WriteU32(manager + 164, W(manager + 164) + 1);
          m.WriteU32(resource + 132, 1);
        }
      }
      Next(2);
      return;
    }
    // Target-actor resource property/effect handlers share their original
    // marker.
    m.WriteU32(sp + 80, 0x8204a1d8);
    auto a = Other(), resource = a ? W(a + 4) : 0;
    unsigned result = 0;
    if (resource) {
      auto value = Get(5);
      s.r[3] = resource;
      s.r[4] = value;
      if (e == 0x8238c018) {
        (void)battle_script_actions61::Apply(0x8238c1a0, m, d, s);
        result = Address(s.r[3]);
      } else if (e == 0x82af8458) {
        s.r[5] = 1;
        s.r[6] = 0;
        Call(0x82aca1b8);
      } else
        Call(0x82ac9000);
    }
    if (e == 0x8238c018) {
      Set(7, result);
      Next(9);
    } else
      Next(7);
    m.WriteU32(sp + 80, 0x8204a1d8);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x8238c0d8) {
    auto a = m.ReadU32(Address(s.r[3]) + 24),
         p = m.ReadU32(a + 36) + m.ReadU32(a + 52) + Address(s.r[4]);
    unsigned value = 0;
    for (unsigned i = 0; i < 4; ++i)
      value |= unsigned(m.ReadU8(p + i)) << (8 * i);
    s.r[3] = value;
    return true;
  }
  if (e == 0x8238c570) {
    s.r[3] = (m.ReadU32(Address(s.r[4]) + 100) >> 31) & 1;
    return true;
  }
  if (e == 0x8238aa80 || e == 0x8238aab0) {
    auto index = std::int32_t(Address(s.r[3])), q = index / 32;
    auto p = 0x83213438u + 8 * unsigned(index - q * 32);
    s.r[3] = e == 0x8238aa80 ? m.ReadU32(p) + unsigned(q) : m.ReadU32(p + 4);
    return true;
  }
  unsigned frame = 96, first = 31;
  switch (e) {
  case 0x8238c1a0:
  case 0x82af6e70:
  case 0x82af9568:
    frame = 112;
    first = 30;
    break;
  case 0x8238c3e8:
  case 0x82b00888:
  case 0x82af9740:
    frame = 112;
    first = 29;
    break;
  case 0x82af65f8:
  case 0x82af9c10:
  case 0x82af8458:
  case 0x82af8510:
    frame = 128;
    first = 28;
    break;
  case 0x8238c018:
    frame = 144;
    first = 27;
    break;
  case 0x8238d0e8:
  case 0x82af6e10:
  case 0x82afe628:
  case 0x82af7110:
  case 0x8238c510:
    break;
  case 0x82af99d8: {
    Actions{m, d, s, Address(s.r[3]), Address(s.r[1])}.Run(e);
    return true;
  }
  default:
    return false;
  }
  auto owner = Address(s.r[3]), old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  if (e == 0x8238c1a0) {
    auto index = Address(s.r[4]);
    if (std::int32_t(index) > 262)
      s.r[3] = 0;
    else {
      s.r[3] = index;
      (void)battle_script_actions61::Apply(0x8238aa80, m, d, s);
      auto base = owner + 272 * Address(s.r[3]);
      s.r[3] = index;
      (void)battle_script_actions61::Apply(0x8238aab0, m, d, s);
      s.r[3] = Address(s.r[3]) & m.ReadU32(base + 232);
    }
  } else
    Actions{m, d, s, owner, sp}.Run(e);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_actions61
