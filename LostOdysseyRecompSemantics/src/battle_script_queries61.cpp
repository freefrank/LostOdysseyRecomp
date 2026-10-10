#include "lo_semantics/battle_script_queries61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/battle_script_events61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_script_queries61 {
namespace {
using recovery_abi::Address;
struct Queries {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner, sp;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Actor() { return W(owner + 24); }
  unsigned Resource() { return W(Actor() + 4); }
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
  unsigned Runtime(unsigned e) {
    Call(0x82380a18);
    Call(e);
    return Address(s.r[3]);
  }
  unsigned Find(unsigned id) {
    Runtime(0x82389b78);
    s.r[4] = id;
    Call(0x8238e308);
    return Address(s.r[3]);
  }
  unsigned ScriptActor(unsigned id) {
    s.r[3] = owner;
    s.r[4] = id;
    (void)battle_script_events61::Apply(0x8238c118, m, d, s);
    return Address(s.r[3]);
  }
  void Virtual(unsigned object, unsigned slot) {
    s.r[3] = object;
    s.ctr = W(W(object) + slot);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  void ReverseAction() {
    auto type = Get(6), detail = Get(8), mode = Mode();
    s.r[3] = owner;
    s.r[4] = 2;
    (void)battle_script_actions61::Apply(0x8238c0d8, m, d, s);
    auto targetActor = ScriptActor(Address(s.r[3]));
    unsigned result = 0xffffffff, count = 0;
    if (targetActor) {
      auto target = W(targetActor + 4);
      if (target) {
        auto targetId = W(target + 64), list = Runtime(0x8238e2f8);
        for (unsigned i = 0;
             std::int32_t(i) < std::int32_t(W(Runtime(0x8238e2f8) + 4)); ++i) {
          auto resource = W(W(list) + 4 * i);
          bool upper = W(resource + 124) & 0x10000000;
          if (mode && mode != (upper ? 1u : 2u))
            continue;
          auto actor = ScriptActor(W(resource + 148));
          if (!actor || !(W(actor + 64) & 0x20000000))
            continue;
          auto records = W(resource + 14656), n = W(resource + 14660);
          for (unsigned j = 0; std::int32_t(j) < std::int32_t(n); ++j) {
            auto record = records + 124208 * j;
            if (type != 0xffffffff && W(record) != type)
              continue;
            // A detail filter without an explicit type never matches in the
            // source.
            if (detail != 0xffffffff &&
                (type == 0xffffffff || W(record + 4) != detail))
              continue;
            bool targets = false;
            for (unsigned k = 0; std::int32_t(k) < std::int32_t(W(record + 20));
                 ++k)
              if (W(record + 14884 + 464 * k) == targetId) {
                targets = true;
                break;
              }
            if (!targets)
              continue;
            auto id = W(resource + 64);
            bool duplicate = false;
            for (unsigned k = 0; k < count; ++k)
              if (W(sp + 80 + 4 * k) == id) {
                duplicate = true;
                break;
              }
            if (!duplicate)
              m.WriteU32(sp + 80 + 4 * count++, id);
          }
        }
        if (count) {
          s.r[3] = W(0x83264558);
          s.r[4] = 0;
          s.r[5] = count - 1;
          s.r[6] = 87;
          s.r[7] = W(target + 64);
          Call(0x82aa0740);
          result = W(sp + 80 + 4 * Address(s.r[3]));
        }
      }
    }
    Set(10, result);
    Next(12);
  }
  void Run(unsigned e) {
    if (e == 0x82afef20) {
      ReverseAction();
      return;
    }
    if (e == 0x82afecc8) {
      auto mode = Mode();
      unsigned count = 0;
      if (mode < 4) {
        auto list = Runtime(0x8238e2f8);
        for (unsigned i = 0;
             std::int32_t(i) < std::int32_t(W(Runtime(0x8238e2f8) + 4)); ++i) {
          auto resource = W(W(list) + 4 * i);
          bool upper = W(resource + 124) & 0x10000000;
          if (upper != bool(mode & 1))
            continue;
          if (mode >= 2) {
            Virtual(resource, mode == 2 ? 308 : 292);
            if (Address(s.r[3]))
              continue;
          }
          ++count;
        }
      }
      Set(2, count);
      Next(4);
      return;
    }
    if (e == 0x82af9f08) {
      auto resource = Resource();
      if (resource && (W(resource + 60) & 255) != 6)
        Next(3);
      else {
        s.r[3] = owner;
        s.r[4] = 1;
        (void)battle_script_extensions61::Apply(0x8238c590, m, d, s);
        m.WriteU32(Actor() + 52, Address(s.r[3]));
      }
      return;
    }
    if (e == 0x82af9980) {
      auto manager = Runtime(0x82389b78);
      Set(1, m.ReadU16(manager + 148));
      Next(3);
      return;
    }
    if (e == 0x82af75a0) {
      Set(1, m.ReadU8(W(Actor() + 72)));
      Next(3);
      return;
    }
    if (e == 0x82af75f0) {
      auto resource = Resource();
      if (resource) {
        auto value = Get(1);
        m.WriteU32(resource + 76352, value);
      }
      Next(3);
      return;
    }
    if (e == 0x82af7658) {
      auto a = Get(1), b = Get(3);
      m.WriteU32(Actor() + 320, a);
      m.WriteU32(Actor() + 324, b);
      Next(5);
      return;
    }
    if (e == 0x82aff1b8) {
      auto item = Get(1), bank = Get(3);
      Virtual(W(0x83315fb4), 352);
      Call(0x8229dfd8);
      auto buffer = Address(s.r[3]);
      unsigned value = 0;
      if (item == 1024)
        value = W(buffer + 185200);
      else if (item)
        value = W(buffer + 4 * (2065 * bank + item + 45276));
      else
        for (unsigned i = 0; i < 1024; ++i)
          if (W(buffer + 8260 * bank + 181104 + 4 * i)) {
            value = W(buffer + 4 * (2065 * bank + i + 45276));
            break;
          }
      Set(5, value);
      Next(7);
      return;
    }
    m.WriteU32(sp + 80, 0x8204a1d8);
    auto id = Get(1), resource = Find(id);
    unsigned value = 0;
    if (resource) {
      auto property = Get(3);
      s.r[3] = resource;
      s.r[4] = property;
      if (e == 0x82af9f80) {
        (void)battle_script_actions61::Apply(0x8238c1a0, m, d, s);
        value = Address(s.r[3]);
      } else if (e == 0x82afa048) {
        s.r[5] = 1;
        s.r[6] = 0;
        Call(0x82ac9be0);
      } else
        Call(0x82ac9000);
    }
    if (e == 0x82af9f80) {
      Set(5, value);
      Next(7);
    } else
      Next(5);
    m.WriteU32(sp + 80, 0x8204a1d8);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned first = 31, frame = 96;
  switch (e) {
  case 0x82afecc8:
    first = 27;
    frame = 128;
    break;
  case 0x82afef20:
    first = 20;
    frame = 320;
    break;
  case 0x82af9f08:
  case 0x82af9980:
  case 0x82af75a0:
    break;
  case 0x82af9f80:
    first = 27;
    frame = 144;
    break;
  case 0x82afa048:
  case 0x82afa100:
  case 0x82aff1b8:
    first = 28;
    frame = 128;
    break;
  case 0x82af75f0:
  case 0x82af7658:
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
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  Queries{m, d, s, owner, sp}.Run(e);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_queries61
