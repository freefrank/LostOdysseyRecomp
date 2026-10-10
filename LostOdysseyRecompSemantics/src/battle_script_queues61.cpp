#include "lo_semantics/battle_script_queues61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/battle_script_events61.h"
namespace lo::semantic::gpu::battle_script_queues61 {
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
    if (e == 0x82afd670) {
      auto list = Manager(0x8238e2f8);
      for (unsigned i = 0;
           std::int32_t(i) < std::int32_t(W(Manager(0x8238e2f8) + 4)); ++i) {
        auto resource = W(W(list) + 4 * i);
        if ((W(resource + 60) & 255) != 2)
          continue;
        for (unsigned j = 0;
             std::int32_t(j) < std::int32_t(W(resource + 14660)); ++j) {
          auto record = W(resource + 14656) + 124208 * j;
          m.WriteU32(resource + 76368, 0);
          m.WriteU32(record, 13);
          m.WriteU32(record + 4, 0);
          m.WriteU32(resource + 88, 0);
          m.WriteU32(resource + 100,
                     (W(resource + 100) & 0x3fffffffu) | 0x80000000u);
          m.WriteU32(resource + 96, 0xffffffff);
          m.WriteU32(resource + 60, 2);
          m.WriteU32(resource + 92, W(W(0x8324570c) + 16));
        }
      }
      return;
    }
    if (e == 0x82aff4a8) {
      s.r[3] = owner;
      (void)battle_script_queues61::Apply(0x82afd670, m, d, s);
      Next(1);
      return;
    }
    if (e == 0x82af7760 || e == 0x82af7890 || e == 0x82afa440) {
      auto id = Get(e == 0x82afa440 ? 2 : 1);
      unsigned value = 0;
      if (e != 0x82afa440)
        value = Get(3);
      Manager(0x82389b78);
      s.r[4] = id;
      Call(0x8238e308);
      auto resource = Address(s.r[3]);
      if (resource) {
        if (e == 0x82afa440)
          m.WriteU32(resource + 76344, W(Mode() ? 0x82000e50 : 0x82001270));
        else
          m.WriteU32(resource + (e == 0x82af7760 ? 4880 : 76356), value);
      }
      Next(e == 0x82afa440 ? 4 : 5);
      return;
    }
    if (e == 0x82af77c0) {
      auto value = Get(1), state = W(owner + 44);
      m.WriteU32(state + 28,
                 (W(state + 28) & ~0x01800000u) | ((value & 3) << 23));
      Next(3);
      return;
    }
    if (e == 0x82afa2e8) {
      auto a = Get(1), b = Get(3), c = Get(5), dv = Get(7),
           sp = Address(s.r[1]);
      recovery_abi::WriteU64(m, sp + 80,
                             std::uint64_t(std::int64_t(std::int32_t(c))));
      recovery_abi::WriteU64(m, sp + 88,
                             std::uint64_t(std::int64_t(std::int32_t(dv))));
      s.r[3] = W(0x832652f0);
      s.r[4] = a;
      s.r[5] = b;
      s.fpr_bits[1] =
          std::bit_cast<std::uint64_t>(double(float(std::int32_t(c))));
      s.fpr_bits[2] =
          std::bit_cast<std::uint64_t>(double(float(std::int32_t(dv))));
      Call(0x8285fe68);
      Next(9);
      return;
    }
    if (e == 0x82af7810) {
      auto resource = Other(1);
      Set(5, resource ? W(resource + 64) : 0xffffffff);
      Next(7);
      return;
    }
    if (e == 0x82afa388) {
      auto mode = Mode(), state = W(owner + 44);
      if (mode < 4) {
        auto bit = 0x00200000u << (mode & 1);
        m.WriteU32(state + 28,
                   mode < 2 ? W(state + 28) | bit : W(state + 28) & ~bit);
      }
      Next(2);
      return;
    }
    if (e == 0x82af9870) {
      auto mode = Mode(), manager = Manager(0x82389b78);
      m.WriteU16(manager + 148, mode ? m.ReadU16(manager + 148) | mode : 0);
      if (mode & 32)
        m.WriteU32(Actor() + 64, W(Actor() + 64) | 0x4000);
      Next(3);
      return;
    }
    if (e == 0x82af7ad0) {
      auto state = W(owner + 44);
      for (unsigned i = 0; std::int32_t(i) < std::int32_t(W(state + 16744));
           ++i)
        if (W(W(state + 16728) + 4 * i) != W(W(state + 16740) + 4 * i)) {
          Jump(1);
          return;
        }
      Next(3);
      return;
    }
    if (e == 0x82afa500) {
      auto value = Get(1), state = W(owner + 44);
      if (!W(state + 16744)) {
        for (unsigned offset : {16740u, 16728u, 16736u}) {
          unsigned size = offset == 16740 ? 1024 : 4096;
          s.r[3] = size;
          Call(0x82486c88);
          m.WriteU32(W(owner + 44) + offset, Address(s.r[3]));
          s.r[3] = W(W(owner + 44) + offset);
          s.r[4] = 0;
          s.r[5] = size;
          Call(0x82b7bc40);
        }
      }
      state = W(owner + 44);
      m.WriteU32(W(state + 16740) + 4 * W(state + 16744), value);
      m.WriteU32(state + 16744, W(state + 16744) + 1);
      Next(3);
      return;
    }
    if (e == 0x82af78f8) {
      auto kind = Get(1), detail = Get(3), value = Get(5);
      unsigned count = kind == 31                  ? 1
                       : kind == 3 && detail == 26 ? 2
                       : kind == 3 && detail == 27 ? 3
                                                   : 0;
      auto state = W(owner + 44), index = W(state + 16732);
      for (unsigned i = 0; i < count; ++i) {
        m.WriteU32(W(state + 16728) + 4 * (index + i), value);
        m.WriteU32(W(state + 16736) + 4 * (index + i), W(Actor() + 8));
      }
      if (count)
        m.WriteU32(state + 16732, index + count);
      Next(7);
      return;
    }
    if (e == 0x82af7f18) {
      Set(1, m.ReadU8(Actor() + 312) != 0);
      Next(3);
      return;
    }
    if (e == 0x82af7f70) {
      Set(1, W(W(owner + 44) + 32));
      Next(3);
      return;
    }
    auto resource = W(Actor() + 4);
    if (Mode() == 0 && resource)
      m.WriteU32(resource + 76348, W(resource + 76348) | 0x80000000u);
    Next(2);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned first = 31, frame = 96;
  switch (e) {
  case 0x82afd670:
    first = 22;
    frame = 176;
    break;
  case 0x82afa2e8:
    first = 28;
    frame = 144;
    break;
  case 0x82af7760:
  case 0x82af7890:
  case 0x82af78f8:
    first = 29;
    frame = 112;
    break;
  case 0x82af7810:
  case 0x82af9870:
  case 0x82afa440:
  case 0x82afa500:
    first = 30;
    frame = 112;
    break;
  case 0x82af77c0:
  case 0x82aff4a8:
  case 0x82af7ad0:
  case 0x82af7f18:
  case 0x82af7f70:
    break;
  case 0x82afa388:
  case 0x82afd150:
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
} // namespace lo::semantic::gpu::battle_script_queues61
