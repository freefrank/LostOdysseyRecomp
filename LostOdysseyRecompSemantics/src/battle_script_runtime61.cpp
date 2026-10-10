#include "lo_semantics/battle_script_runtime61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::battle_script_runtime61 {
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
    if (e == 0x82af80b0) {
      auto id = Address(s.r[4]);
      Call(0x82380a18);
      s.r[4] = id;
      Call(0x82380d30);
      auto object = Address(s.r[3]);
      s.r[3] = object ? (W(object + 604) >> 4) & 1 : 0;
      return;
    }
    if (e == 0x82afa1a8) {
      auto value = Mode(), actor = Actor();
      m.WriteU32(actor + 64,
                 (W(actor + 64) & ~0x00800000u) | ((value & 1) << 23));
      Next(2);
      return;
    }
    if (e == 0x82afa200) {
      auto value = Get(1), manager = Manager(0x82389b78);
      m.WriteU32(manager + 156, value);
      Next(3);
      return;
    }
    if (e == 0x82aff2a8) {
      unsigned count = 0;
      auto list = Manager(0x8238e2f8);
      for (unsigned i = 0;
           std::int32_t(i) < std::int32_t(W(Manager(0x8238e2f8) + 4)); ++i) {
        auto resource = W(W(list) + 4 * i);
        s.r[3] = 0x832c9c54;
        s.r[4] = resource;
        (void)battle_script_runtime61::Apply(0x82a9bdb0, m, d, s);
        auto actor = Address(s.r[3]);
        if (actor && (W(actor + 64) & 0x00400000))
          ++count;
      }
      Set(1, count);
      Next(3);
      return;
    }
    if (e == 0x82afb5e0 || e == 0x82afb718) {
      auto a = Get(1), b = Get(3), c = Get(5);
      s.r[3] = 0x832ca0e0 + 224;
      s.r[4] = a;
      s.r[5] = b;
      s.r[6] = c;
      Call(e == 0x82afb5e0 ? 0x82b1e4e0 : 0x82b1e540);
      Next(7);
      return;
    }
    if (e == 0x82afb650 || e == 0x82afb6b0) {
      auto value = Get(1);
      s.r[3] = 0x832ca0e0 + 232;
      s.r[4] = value;
      if (e == 0x82afb6b0)
        s.r[5] = 75;
      s.ctr = W(W(0x832ca0e0 + 232) + (e == 0x82afb650 ? 24 : 28));
      d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
      Next(3);
      return;
    }
    if (e == 0x82aff3f0) {
      auto id = Get(1);
      unsigned result = 0;
      if (std::int32_t(id) >= 20)
        result = id;
      else {
        auto list = Manager(0x8238e2f8);
        for (unsigned i = 0;
             std::int32_t(i) < std::int32_t(W(Manager(0x8238e2f8) + 4)); ++i) {
          auto resource = W(W(list) + 4 * i);
          if (W(resource + 68) == id) {
            result = W(resource + 64);
            break;
          }
        }
      }
      Set(3, result);
      Next(5);
      return;
    }
    if (e == 0x82af6f58) {
      auto id = Get(1);
      unsigned resource = 0;
      if (W(Actor() + 4)) {
        Manager(0x82389b78);
        s.r[4] = id;
        Call(0x8238e308);
        resource = Address(s.r[3]);
      }
      if (resource) {
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
    auto mode = Mode();
    if (e == 0x82afcc48) {
      if (mode < 2) {
        s.r[3] = 0x832cc05c;
        Call(mode ? 0x82b035e0 : 0x82b04c50);
      } else if (mode == 2)
        m.WriteU8(0x832ca0e0 + 5764, 1);
      Next(2);
      return;
    }
    if (mode >= 3)
      return;
    if (!mode) {
      s.r[3] = 0x832cc05c;
      Call(0x82b03428);
    } else {
      auto id = Get(2);
      s.r[3] = 0x832ca0e0;
      s.r[4] = id;
      if (mode == 1)
        (void)battle_script_runtime61::Apply(0x82af80b0, m, d, s);
      else
        Call(0x82aad200);
    }
    if (Address(s.r[3]) & 255)
      Next(6);
    else {
      s.r[3] = owner;
      s.r[4] = 4;
      (void)battle_script_extensions61::Apply(0x8238c590, m, d, s);
      m.WriteU32(Actor() + 52, Address(s.r[3]));
    }
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82af69d0 || e == 0x82af6a48) {
    auto old = Address(s.r[1]), owner = Address(s.r[3]),
         resource = Address(s.r[4]);
    auto kind = Address(s.r[5]), result = Address(s.r[6]);
    m.WriteU32(old - 8, Address(s.lr));
    recovery_abi::WriteU64(m, old - 24, s.r[30]);
    recovery_abi::WriteU64(m, old - 16, s.r[31]);
    s.r[1] -= 112;
    m.WriteU32(Address(s.r[1]), old);
    (void)battle_script_runtime61::Apply(0x82a9bdb0, m, d, s);
    auto actor = Address(s.r[3]);
    if (e == 0x82af6a48) {
      if (actor) {
        s.r[3] = owner;
        s.r[4] = resource;
        s.r[5] = 1;
        s.r[6] = 0;
        (void)battle_script_runtime61::Apply(0x82af69d0, m, d, s);
      }
    } else {
      s.r[3] = 0;
      if (actor) {
        auto flags = m.ReadU32(actor + 64);
        if (!(flags & 0x180000)) {
          m.WriteU32(actor + 328, result);
          m.WriteU32(actor + 64, (flags & ~0x138000u) | 0x80000 |
                                     (std::rotl(kind, 15) & 0x38000));
          s.r[3] = 1;
        }
      }
    }
    s.r[1] += 112;
    s.r[30] = recovery_abi::ReadU64(m, old - 24);
    s.r[31] = recovery_abi::ReadU64(m, old - 16);
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82a9bdb0) {
    auto state = m.ReadU32(Address(s.r[3]) + 44);
    unsigned result = 0;
    if (state) {
      auto id = m.ReadU32(Address(s.r[4]) + 64), count = m.ReadU32(state + 12),
           actors = m.ReadU32(state + 4);
      for (unsigned i = 0; std::int32_t(i) < std::int32_t(count); ++i) {
        auto a = actors + 472 * i;
        if (m.ReadU32(a + 8) == id) {
          result = a;
          break;
        }
      }
    }
    s.r[3] = result;
    return true;
  }
  unsigned first = 31, frame = 96;
  switch (e) {
  case 0x82afa200:
  case 0x82af6f58:
    first = 30;
    frame = 112;
    break;
  case 0x82aff2a8:
    first = 26;
    frame = 144;
    break;
  case 0x82aff3f0:
    first = 27;
    frame = 128;
    break;
  case 0x82afb5e0:
  case 0x82afb718:
    first = 29;
    frame = 112;
    break;
  case 0x82afb650:
  case 0x82afb6b0:
  case 0x82afcc48:
  case 0x82afccf8:
  case 0x82af80b0:
    break;
  case 0x82afa1a8:
    Runtime{m, d, s, Address(s.r[3])}.Run(e);
    return true;
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
} // namespace lo::semantic::gpu::battle_script_runtime61
