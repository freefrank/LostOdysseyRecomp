#include "lo_semantics/battle_script_status61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/battle_script_events61.h"
namespace lo::semantic::gpu::battle_script_status61 {
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
  unsigned Other(unsigned off) {
    s.r[3] = owner;
    s.r[4] = off;
    (void)battle_script_actions61::Apply(0x8238c0d8, m, d, s);
    auto id = Address(s.r[3]);
    s.r[3] = owner;
    s.r[4] = id;
    (void)battle_script_events61::Apply(0x8238c118, m, d, s);
    auto actor = Address(s.r[3]);
    return actor ? W(actor + 4) : 0;
  }
  void Jump(unsigned off) {
    s.r[3] = owner;
    s.r[4] = off;
    (void)battle_script_extensions61::Apply(0x8238c590, m, d, s);
    m.WriteU32(Actor() + 52, Address(s.r[3]));
  }
  void Run(unsigned e) {
    if (e == 0x82afd770) {
      auto group = W(Address(s.r[4]) + 212);
      if (!group)
        return;
      auto list = Manager(0x8238e2f8);
      for (unsigned i = 0;
           std::int32_t(i) < std::int32_t(W(Manager(0x8238e2f8) + 4)); ++i) {
        auto resource = W(W(list) + 4 * i);
        if (W(resource + 212) == group) {
          m.WriteU32(resource + 212, 0);
          m.WriteU32(resource + 216, 0);
        }
      }
      return;
    }
    if (e == 0x82af7e18) {
      auto resource = Other(1);
      if (resource && !(W(resource + 124) & 64))
        Next(7);
      else
        Jump(5);
      return;
    }
    if (e == 0x82af7b68) {
      auto state = W(owner + 44), phase = (W(state + 28) >> 18) & 3;
      if (!phase) {
        if (Get(1)) {
          m.WriteU32(state + 28, (W(state + 28) & ~0xc0000u) | 0x40000);
          m.WriteU32(state + 16724, 60);
          Set(1, 1);
        }
      } else if (phase == 1)
        Set(1, 1);
      else if (phase == 2) {
        m.WriteU32(state + 28, W(state + 28) & ~0xc0000u);
        Set(1, 0);
      }
      Next(3);
      return;
    }
    if (e == 0x82afac40) {
      unsigned value = Mode() == 0, resource = W(Actor() + 4);
      if (resource) {
        auto id = Get(2);
        for (unsigned i = 0; i < 12; ++i)
          if (W(resource + 75932 + 16 * i) == id) {
            m.WriteU32(resource + 75944 + 16 * i, value);
            break;
          }
      }
      Next(4);
      return;
    }
    if (e == 0x82af76c0) {
      unsigned first = 0, second = 0;
      auto actor = Actor(), flags = W(actor + 64);
      if (W(actor + 4) && (flags & 0x180000) == 0x100000) {
        first = (flags >> 15) & 7;
        second = W(actor + 328);
        m.WriteU32(actor + 64, flags & ~0x180000u);
      }
      Set(1, first);
      Set(3, second);
      Next(5);
      return;
    }
    if (e == 0x82afa258) {
      auto value = W(W(0x832aeb00) + 24 * Mode() + 8);
      Set(2, value);
      Next(4);
      return;
    }
    if (e == 0x82aff360) {
      auto id = Get(1), group = Get(3), value = Get(5);
      Manager(0x82389b78);
      s.r[4] = id;
      Call(0x8238e308);
      auto resource = Address(s.r[3]);
      if (resource) {
        if (group) {
          m.WriteU32(resource + 212, group);
          m.WriteU32(resource + 216, value);
        } else {
          s.r[3] = owner;
          s.r[4] = resource;
          (void)battle_script_status61::Apply(0x82afd770, m, d, s);
        }
      }
      Next(7);
      return;
    }
    if (e == 0x82af7028) {
      auto resource = Other(1);
      bool selected = false;
      if (resource) {
        auto id = W(resource + 64);
        Manager(0x82389b78);
        s.r[4] = id;
        Call(0x8238e308);
        auto found = Address(s.r[3]);
        if (found) {
          s.ctr = W(W(found) + 292);
          d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
          selected = Address(s.r[3]) == 0;
        }
      }
      if (selected) {
        m.WriteU8(W(Actor() + 80), W(resource + 64));
        m.WriteU32(Actor() + 84, 1);
      }
      Set(5, selected ? 0 : 1);
      Next(7);
      if (selected)
        m.WriteU32(Actor() + 336, 8);
      return;
    }
    if (e == 0x82af9648) {
      auto resource = Other(2);
      if (resource) {
        auto mode = Mode();
        if (mode < 2)
          m.WriteU32(resource + 132, mode);
        else if (mode == 2) {
          auto manager = Manager(0x82389b78);
          if (m.ReadU16(manager + 148) & 1) {
            manager = Manager(0x82389b78);
            m.WriteU32(manager + 164, W(manager + 164) + 1);
            m.WriteU32(resource + 132, 1);
          }
        }
      }
      Next(6);
      return;
    }
    unsigned service = 0;
    bool branch = false;
    switch (e) {
    case 0x82afacf8:
      service = 0x82ad6c90;
      break;
    case 0x82afae10:
      service = 0x82ad6088;
      break;
    case 0x82afaf28:
      service = 0x82ad6318;
      break;
    case 0x82afb040:
      service = 0x82ad6710;
      break;
    case 0x82afad80:
      service = 0x82ad5f28;
      branch = true;
      break;
    case 0x82afae98:
      service = 0x82ad6198;
      branch = true;
      break;
    case 0x82afafb0:
      service = 0x82ad6448;
      branch = true;
      break;
    }
    auto id = Get(branch ? 1 : 3);
    unsigned result = 1;
    if (!(W(W(0x832c9c54 + 44) + 28) & 0x20000)) {
      s.r[3] = 0x832cbfb0;
      s.r[4] = id;
      Call(service);
      result = Address(s.r[3]) & 255;
    }
    if (branch) {
      if (result == 1)
        Next(5);
      else
        Jump(3);
    } else {
      Set(1, result);
      Next(5);
    }
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned first = 31, frame = 96;
  switch (e) {
  case 0x82afd770:
  case 0x82aff360:
    first = 28;
    frame = 128;
    break;
  case 0x82afac40:
  case 0x82af7028:
    first = 29;
    frame = 112;
    break;
  case 0x82af76c0:
  case 0x82af9648:
    first = 30;
    frame = 112;
    break;
  case 0x82af7e18:
  case 0x82af7b68:
  case 0x82afa258:
  case 0x82afacf8:
  case 0x82afae10:
  case 0x82afaf28:
  case 0x82afb040:
  case 0x82afad80:
  case 0x82afae98:
  case 0x82afafb0:
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
} // namespace lo::semantic::gpu::battle_script_status61
