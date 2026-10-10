#include "lo_semantics/battle_script_global_modes61.h"
#include "lo_semantics/battle_group_gauge61.h"
#include "lo_semantics/battle_formation61.h"
#include "lo_semantics/battle_action_records61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/battle_script_runtime61.h"
#include "lo_semantics/battle_script_binding61.h"
namespace lo::semantic::gpu::battle_script_global_modes61 {
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
    if (e == 0x82afd2f0) {
      (void)battle_action_records61::Apply(e, m, d, s);
      return;
    }
    if (e == 0x82aab870) {
      (void)battle_script_global_modes61::Apply(e, m, d, s);
      return;
    }
    if (!battle_group_gauge61::Apply(e, m, d, s) &&
        !battle_formation61::Apply(e, m, d, s))
      d.guest.CallDirect(e, m, s);
  }
  unsigned Manager(unsigned method) {
    Call(0x82380a18);
    Call(method);
    return Address(s.r[3]);
  }
  unsigned Buffer() {
    s.r[3] = W(0x83315fb4);
    s.ctr = W(W(Address(s.r[3])) + 352);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    Call(0x8229dfd8);
    return Address(s.r[3]);
  }
  void ActorFlags(unsigned set, unsigned clear = 0) {
    auto actor = Actor();
    m.WriteU32(actor + 64, (W(actor + 64) | set) & ~clear);
  }
  void Insert(unsigned base, unsigned countOffset, unsigned valuesOffset,
              unsigned value) {
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(W(base + countOffset));
         ++i)
      if (!W(base + valuesOffset + 4 * i)) {
        m.WriteU32(base + valuesOffset + 4 * i, value);
        break;
      }
  }
  void FirstGroup(unsigned flagOffset, unsigned value) {
    auto list = Manager(0x8238e2f8);
    for (unsigned i = 0;
         std::int32_t(i) < std::int32_t(W(Manager(0x8238e2f8) + 4)); ++i) {
      auto resource = W(W(list) + 4 * i);
      if (!resource || (W(resource + 68) != 8 && W(resource + 68) != 9))
        continue;
      if (!(W(resource + flagOffset) & 0x80000000u)) {
        m.WriteU32(resource + flagOffset,
                   W(resource + flagOffset) | 0x80000000u);
        Insert(resource, 5156, 5160, value);
      }
      break;
    }
  }
  void Run(unsigned) {
    auto mode = Mode();
    switch (mode) {
    case 0:
      s.r[3] = 0x832ca0e0 + 5232;
      Call(0x82abdfd0);
      break;
    case 1:
      ActorFlags(0x1000, 0x80);
      break;
    case 25:
      ActorFlags(0, 0x80);
      break;
    case 2:
    case 3: {
      s.r[3] = owner;
      s.r[4] = W(Actor() + 4);
      (void)battle_script_runtime61::Apply(0x82a9bdb0, m, d, s);
      auto actor = Address(s.r[3]);
      m.WriteU32(actor + 64, W(actor + 64) | (mode == 2 ? 0x800 : 0x400));
      break;
    }
    case 4: {
      auto state = W(owner + 44);
      m.WriteU32(state + 28, W(state + 28) | 0x100000);
      break;
    }
    case 5: {
      auto buffer = Buffer(), list = Manager(0x8238e2f8);
      for (unsigned i = 0;
           std::int32_t(i) < std::int32_t(W(Manager(0x8238e2f8) + 4)); ++i) {
        auto resource = W(W(list) + 4 * i);
        if (!resource || (W(resource + 68) != 8 && W(resource + 68) != 9))
          continue;
        m.WriteU32(resource + 8672, W(resource + 8672) & 0x7fffffffu);
        for (unsigned j = 0; std::int32_t(j) < std::int32_t(W(resource + 5156));
             ++j)
          if (W(resource + 5160 + 4 * j) == 212) {
            m.WriteU32(resource + 5160 + 4 * j, 0);
            break;
          }
      }
      m.WriteU32(buffer + 117808, W(buffer + 117808) | 0x80000000u);
      Insert(buffer, 117236, 117240, 28);
      m.WriteU32(buffer + 132116, W(buffer + 132116) | 0x80000000u);
      Insert(buffer, 131544, 131548, 28);
      FirstGroup(5728, 28);
      break;
    }
    case 6: {
      auto manager = W(0x832ca0d0), count = W(manager + 76),
           entries = W(manager + 72);
      unsigned one = 0xffffffff, twelve = 0xffffffff;
      for (unsigned i = 0; std::int32_t(i) < std::int32_t(count); ++i) {
        auto id = W(entries + 72 * i);
        if (id == 1)
          one = i;
        if (id == 12)
          twelve = i;
      }
      if (one != 0xffffffff && twelve != 0xffffffff) {
        auto first = entries + 72 * one, second = entries + 72 * twelve;
        s.r[3] = first + 44;
        s.r[4] = second + 44;
        Call(0x822b3f50);
        s.r[3] = first + 20;
        s.r[4] = second + 20;
        Call(0x822b3f50);
      }
      FirstGroup(8672, 212);
      break;
    }
    case 7:
      FirstGroup(8672, 212);
      break;
    case 8:
    case 9:
      s.r[3] = 0x832ca0e0;
      s.r[4] = mode == 8;
      Call(0x82aab870);
      break;
    case 10:
    case 11:
      m.WriteU8(0x832ca0e0 + 5738, mode == 10);
      break;
    case 12: {
      auto player = Manager(0x82389aa0);
      auto value = m.ReadU8(player + 56);
      Manager(0x82389b78);
      s.r[4] = 1;
      s.r[5] = value;
      Call(0x82af5ba8);
      break;
    }
    case 13:
    case 14:
      m.WriteU32(W(0x832cb784) + 4, mode == 13);
      break;
    case 15: {
      auto manager = Manager(0x82389b78);
      m.WriteU32(manager + 148, W(manager + 148) | 8);
      break;
    }
    case 16:
      ActorFlags(0, 0x1000);
      break;
    case 17:
      s.r[3] = owner;
      Call(0x82afd2f0);
      break;
    case 18:
      ActorFlags(0x200);
      break;
    case 19:
      s.r[3] = W(0x832652f0);
      Call(0x8285ff08);
      break;
    case 20:
    case 21:
    case 22:
    case 23: {
      auto resource = W(Actor() + 4);
      constexpr unsigned bits[]{0x8000, 0x4000, 0x2000, 0xe000};
      if (resource)
        m.WriteU32(resource + 124, W(resource + 124) | bits[mode - 20]);
      break;
    }
    case 24:
      ActorFlags(0x80, 0x1000);
      break;
    case 26:
      s.r[3] = W(0x832652f0);
      s.r[4] = 1;
      Call(0x8285f9b8);
      break;
    case 27:
      s.r[3] = W(0x832652f0);
      Call(0x8285fea8);
      break;
    case 28:
    case 38:
      s.r[3] = W(0x832aeb00);
      s.r[4] = mode == 38;
      s.r[5] = 0;
      (void)battle_script_binding61::Apply(0x82a9da60, m, d, s);
      break;
    case 29:
      s.r[3] = W(0x832aeb00);
      s.r[4] = 0;
      Call(0x82afd218);
      break;
    case 30: {
      auto state = W(0x832c9c54 + 44), buffer = Buffer();
      m.WriteU32(state + 28, (W(state + 28) & ~0x18000u) | 0x8000);
      m.WriteU32(buffer + 170860, 10);
      break;
    }
    case 31: {
      auto state = W(0x832c9c54 + 44);
      m.WriteU32(state + 28, (W(state + 28) & ~0x18000u) | 0x10000);
      break;
    }
    case 32:
    case 33:
      m.WriteU8(0x832ca0e0 + 5792 + (mode - 32), 1);
      break;
    case 34: {
      auto state = W(0x832c9c54 + 44);
      m.WriteU32(state + 28, W(state + 28) | 0x4000);
      break;
    }
    case 35:
    case 36:
      m.WriteU8(0x832ca0e0 + 5512, mode - 34);
      break;
    case 37:
      s.r[3] = W(0x832aeb00);
      Call(0x82ac8448);
      break;
    }
    Next(2);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82aab870) {
    auto peer = m.ReadU32(0x832c34e0);
    if (peer) {
      auto value = Address(s.r[4]) & 255;
      m.WriteU8(Address(s.r[3]) + 5737, value);
      auto flags = m.ReadU8(peer + 465);
      m.WriteU8(peer + 465, value ? flags & ~1u : flags | 1u);
    }
    return true;
  }
  if (e != 0x82b00e98)
    return false;
  unsigned first = 28, frame = 128;
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
} // namespace lo::semantic::gpu::battle_script_global_modes61
