#include "lo_semantics/battle_script_execution61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/battle_action_records61.h"
#include "lo_semantics/battle_script_preparation61.h"
#include "lo_semantics/battle_script_runtime61.h"
namespace lo::semantic::gpu::battle_script_execution61 {
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
    if (!battle_action_records61::Apply(e, m, d, s) &&
        !battle_script_preparation61::Apply(e, m, d, s))
      d.guest.CallDirect(e, m, s);
  }
  unsigned Manager(unsigned method) {
    Call(0x82380a18);
    Call(method);
    return Address(s.r[3]);
  }
  void Emit(unsigned e, unsigned resource, unsigned kind, unsigned detail,
            unsigned target, unsigned index) {
    s.r[3] = resource;
    s.r[4] = kind;
    s.r[5] = detail;
    s.r[6] = target;
    s.r[7] = index;
    Call(e);
  }
  void Link(unsigned e, unsigned resource, unsigned target, unsigned index) {
    s.r[3] = resource;
    s.r[4] = target;
    s.r[5] = index;
    s.r[6] = 1;
    Call(e);
  }
  void Run(unsigned e) {
    if (e == 0x82af6d60) {
      auto item = Address(s.r[4]), resource = Address(s.r[5]);
      s.r[3] = owner;
      s.r[4] = resource;
      (void)battle_script_runtime61::Apply(0x82a9bdb0, m, d, s);
      auto actor = Address(s.r[3]);
      auto p =
          W(owner + 44) + 4 * (2065 * ((W(actor + 64) >> 13) & 1) + item + 50);
      auto value = W(p) - 1;
      m.WriteU32(p, std::int32_t(value) > 0 ? value : 0);
      return;
    }
    auto resource = W(Actor() + 4), index = Address(s.r[4]);
    if (e == 0x82afd970) {
      if (W(resource + 212)) {
        if (!W(resource + 216)) {
          unsigned order = 1;
          auto list = Manager(0x8238e2f8);
          for (unsigned i = 0;
               std::int32_t(i) < std::int32_t(W(Manager(0x8238e2f8) + 4));
               ++i) {
            auto peer = W(W(list) + 4 * i);
            if (W(peer + 212) == W(resource + 212) && W(peer + 216) == order) {
              Link(0x82ab0b28, resource, W(peer + 64), index);
              ++order;
            }
          }
        }
      } else if (W(resource + 204)) {
        auto list = Manager(0x8238e2f8);
        for (unsigned i = 0;
             std::int32_t(i) < std::int32_t(W(Manager(0x8238e2f8) + 4)); ++i) {
          auto peer = W(W(list) + 4 * i);
          if (W(peer + 64) != W(resource + 64) &&
              W(peer + 204) == W(resource + 204))
            Link(0x82ab0b28, resource, W(peer + 64), index);
        }
      }
      for (unsigned i = 0; std::int32_t(i) < std::int32_t(W(Actor() + 84));
           ++i) {
        auto id = m.ReadU8(W(Actor() + 80) + i);
        Manager(0x82389b78);
        s.r[4] = id;
        Call(0x8238e308);
        auto target = Address(s.r[3]);
        if (W(target + 204)) {
          auto list = Manager(0x8238e2f8);
          for (unsigned j = 0;
               std::int32_t(j) < std::int32_t(W(Manager(0x8238e2f8) + 4));
               ++j) {
            auto peer = W(W(list) + 4 * j);
            if (W(peer + 64) != W(target + 64) &&
                W(peer + 204) == W(target + 204))
              Link(0x82ab0b98, resource, W(peer + 64), index);
          }
        }
      }
      return;
    }
    auto kind = Address(s.r[4]), detail = Address(s.r[5]),
         busy = W(Actor() + 96), sp = Address(s.r[1]);
    m.WriteU32(sp + 80, 0x8204a1d8);
    bool normal = e == 0x82b00698, doTargets = true;
    if (!normal) {
      s.r[3] = W(0x8324570c);
      s.r[4] = resource;
      s.r[5] = e == 0x82afdcf0;
      Call(0x82acee70);
      m.WriteU32(resource + 92, 0);
    }
    s.r[3] = resource;
    s.r[4] = normal ? 1 : 0;
    Call(0x82ac9a28);
    if (Address(s.r[3]) & 255) {
      if (!normal)
        return;
      if (!(W(Actor() + 64) & 0x0c000000)) {
        Emit(0x82ab36c8, resource, 0, 0, W(resource + 64), busy);
        doTargets = false;
      }
    }
    if (doTargets) {
      auto actor = Actor();
      if (normal && W(actor + 300)) {
        kind = W(actor + 300);
        m.WriteU32(actor + 88, kind);
        detail = W(Actor() + 304);
        m.WriteU32(Actor() + 92, detail);
        s.r[3] = owner;
        s.r[4] = W(Actor() + 308);
        s.r[5] = 0;
        Call(0x82afde70);
      } else {
        m.WriteU32(actor + 88, kind);
        m.WriteU32(Actor() + 92, detail);
        if (kind == 1) {
          if (detail)
            m.WriteU32(Actor() + 100, detail);
          m.WriteU32(Actor() + 100, W(Actor() + 100) + 1);
          detail = 0;
          m.WriteU32(Actor() + 92, 0);
        }
      }
      for (unsigned i = 0; std::int32_t(i) < std::int32_t(W(Actor() + 84));
           ++i) {
        if (i)
          kind = detail = 0xffffffff;
        auto target = m.ReadU8(W(Actor() + 80) + i);
        Emit(normal ? 0x82ab36c8 : 0x82ab38f0, resource, kind, detail, target,
             busy);
      }
    }
    s.r[3] = owner;
    s.r[4] = busy;
    (void)battle_script_execution61::Apply(0x82afd970, m, d, s);
    if (e == 0x82afdb90 || !(m.ReadU8(Actor() + 64) & 1))
      Emit(normal ? 0x82ab36c8 : 0x82ab38f0, resource, 0xffffffff, 0xffffffff,
           0xffffffff, busy);
    if (e != 0x82afdb90)
      m.WriteU32(Actor() + 96, W(Actor() + 96) + 1);
    m.WriteU32(sp + 80, 0x8204a1d8);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82afde70)
    return battle_script_preparation61::Apply(e, m, d, s);
  unsigned first = 24, frame = 160;
  switch (e) {
  case 0x82b00698:
  case 0x82afdb90:
  case 0x82afdcf0:
    break;
  case 0x82afd970:
    first = 25;
    frame = 144;
    break;
  case 0x82af6d60:
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
} // namespace lo::semantic::gpu::battle_script_execution61
