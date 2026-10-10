#include "lo_semantics/battle_script_binding61.h"
#include "lo_semantics/battle_group_gauge61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_script_binding61 {
namespace {
using recovery_abi::Address;
struct Binding {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Actor() { return W(owner + 24); }
  unsigned State() { return W(owner + 44); }
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
                    ((W(State() + 28) & 0x04000000) ? 2 : 1));
  }
  unsigned Runtime(unsigned method) {
    d.guest.CallDirect(0x82380a18, m, s);
    d.guest.CallDirect(method, m, s);
    return Address(s.r[3]);
  }
  void Attach(unsigned resource, unsigned id) {
    auto a = Actor();
    m.WriteU32(a + 4, resource);
    m.WriteU32(a + 8, id);
    m.WriteU32(resource + 148, W(a));
  }
  void Stop() {
    auto st = State();
    m.WriteU32(st + 28, W(st + 28) | 0x80000000);
  }
  void Run(unsigned e) {
    if (e == 0x82a9da60) {
      auto mode = Address(s.r[4]);
      m.WriteU8(owner + 24 * (mode + 1), 0);
      (void)battle_group_gauge61::Apply(0x82ac7b08, m, d, s);
      for (auto target : {0x82ac7fc8u, 0x82ac6e60u, 0x82ac6f08u}) {
        s.r[3] = owner;
        s.r[4] = mode;
        (void)battle_group_gauge61::Apply(target, m, d, s);
      }
      return;
    }
    if (e == 0x82afd7f8 || e == 0x82afeaf8) {
      auto id = Get(1);
      bool force = e == 0x82afd7f8;
      if (force) {
        auto state = State();
        for (unsigned i = 0;
             i < W(state + 12) && std::int32_t(i) < std::int32_t(W(state + 12));
             ++i) {
          auto a = W(state + 4) + 472 * i;
          unsigned candidate;
          if (std::int32_t(id) >= 20)
            candidate = W(a + 8);
          else {
            auto resource = W(a + 4);
            if (!resource)
              continue;
            candidate = W(resource + 68);
          }
          if (candidate == id) {
            m.WriteU32(a + 4, 0);
            m.WriteU32(a + 8, 0xffffffff);
            m.WriteU32(a + 64, W(a + 64) | 0x80000000);
            break;
          }
        }
      }
      if (force || !W(Actor() + 4)) {
        auto list = Runtime(0x8238e2f8);
        for (unsigned i = 0;
             std::int32_t(i) < std::int32_t(W(Runtime(0x8238e2f8) + 4)); ++i) {
          auto resource = W(W(list) + 4 * i);
          bool match;
          if (force)
            match = W(resource + (std::int32_t(id) >= 20 ? 64 : 68)) == id;
          else
            match = W(resource + 68) == id && W(resource + 148) == 0xffffffff;
          if (match) {
            Attach(resource,
                   force && std::int32_t(id) >= 20 ? id : W(resource + 64));
            break;
          }
        }
      }
      if (force && !W(Actor() + 4)) {
        auto a = Actor();
        m.WriteU32(a + 64, W(a + 64) | 0x80000000);
        Stop();
      }
      Next(3);
      return;
    }
    if (e == 0x82a9e988) {
      auto resource = W(Actor() + 4);
      if (resource) {
        auto value = Mode();
        auto a = Actor();
        m.WriteU32(a + 64, (W(a + 64) & ~0x40000000u) | ((value & 1) << 30));
        value = Mode();
        auto flags = (W(resource + 124) & ~0x00400000u) | ((value & 1) << 22);
        m.WriteU32(resource + 124, flags);
        if (W(0x832ca0e0 + 5784) != 233 || W(resource + 64) != 24) {
          s.r[3] = W(0x832aeb00);
          s.r[4] = (flags >> 28) & 1;
          s.r[5] = 0;
          (void)battle_script_binding61::Apply(0x82a9da60, m, d, s);
        }
      }
      Next(2);
      return;
    }
    if (e == 0x82af9ae0) {
      auto mode = Mode();
      bool transition = true;
      if (mode == 0)
        transition = W(Runtime(0x82389b78) + 56) != 9;
      if (transition) {
        auto manager = Runtime(0x82389b78);
        s.r[3] = manager;
        s.r[4] = 13;
        s.r[5] = 1;
        d.guest.CallDirect(0x82aaa7c8, m, s);
        Next(2);
      }
      Stop();
      return;
    }
    if (e == 0x82af9b88) {
      auto value = Mode(), manager = Runtime(0x82389b78);
      m.WriteU32(manager + 148,
                 (W(manager + 148) & ~0x800u) | ((value & 1) << 11));
      Next(2);
      return;
    }
    auto manager = Runtime(0x82389b78);
    Set(1, W(manager + 52));
    Next(3);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned frame = 96, first = 31;
  switch (e) {
  case 0x82afd7f8:
    frame = 128;
    first = 27;
    break;
  case 0x82afeaf8:
    frame = 128;
    first = 28;
    break;
  case 0x82a9da60:
  case 0x82af9b88:
    frame = 112;
    first = 30;
    break;
  case 0x82a9e988:
  case 0x82af9ae0:
  case 0x82af9a88:
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
  Binding{m, d, s, owner}.Run(e);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_binding61
