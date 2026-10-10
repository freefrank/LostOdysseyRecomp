#include "lo_semantics/battle_script_preparation61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/battle_script_targets61.h"
#include "lo_semantics/battle_script_actions61.h"
namespace lo::semantic::gpu::battle_script_preparation61 {
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
  void Pools(unsigned sp, unsigned mode) {
    s.r[3] = owner;
    s.r[4] = sp + 384;
    s.r[5] = sp + 128;
    s.r[6] = sp + 640;
    s.r[7] = sp + 96;
    s.r[8] = mode;
    (void)battle_script_targets61::Apply(0x8238de58, m, d, s);
  }
  unsigned Find(unsigned id) {
    Manager(0x82389b78);
    s.r[4] = id;
    Call(0x8238e308);
    return Address(s.r[3]);
  }
  void Single(unsigned id) {
    m.WriteU8(W(Actor() + 80), id);
    m.WriteU32(Actor() + 84, W(Actor() + 84) + 1);
    m.WriteU32(Actor() + 336, 0);
  }
  void Copy(unsigned pool, unsigned count, unsigned mode) {
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(count); ++i) {
      m.WriteU8(W(Actor() + 80) + i, W(pool + 4 * i));
      m.WriteU32(Actor() + 84, W(Actor() + 84) + 1);
    }
    m.WriteU32(Actor() + 336, mode);
  }
  unsigned Random(unsigned count, unsigned tag, unsigned self) {
    s.r[3] = W(0x83264558);
    s.r[4] = 0;
    s.r[5] = count - 1;
    s.r[6] = tag;
    s.r[7] = self;
    Call(0x82aa0740);
    return Address(s.r[3]);
  }
  void Run(unsigned) {
    auto mode = Address(s.r[4]), mask = Address(s.r[5]), sp = Address(s.r[1]),
         resource = W(Actor() + 4);
    unsigned self = resource ? W(resource + 64) : 31;
    m.WriteU32(sp + 80, 0x8204a1d8);
    Pools(sp, 0);
    m.WriteU32(Actor() + 84, 0);
    unsigned result = 0;
    auto fail = [&]() { result = 1; };
    auto other = [&]() {
      auto count = W(sp + 104);
      if (std::int32_t(count) <= 1) {
        fail();
        return;
      }
      for (;;) {
        auto id = W(sp + 768 + 4 * Random(count, 83, self));
        if (id != W(Actor() + 8)) {
          Single(id);
          return;
        }
      }
    };
    switch (mode) {
    case 0:
      Single(W(W(Actor() + 4) + 64));
      break;
    case 1:
      if (!W(sp + 104)) {
        fail();
        break;
      }
      s.r[3] = resource;
      s.r[4] = 226;
      Call(0x8238e368);
      if (Address(s.r[3]) & 255) {
        s.r[3] = 226;
        (void)battle_script_actions61::Apply(0x8238aab0, m, d, s);
        Call(0x82ac84b8);
        auto id = W(resource + 4 * (Address(s.r[3]) + 567));
        Find(id);
        Call(0x82ab0958);
        if (!(Address(s.r[3]) & 255)) {
          Single(id);
          break;
        }
      }
      s.r[3] = W(0x832aeb00);
      Call(0x82ac8228);
      Single(Address(s.r[3]));
      break;
    case 2:
      if (!W(sp + 104))
        fail();
      else
        Copy(sp + 768, W(sp + 104), 1);
      break;
    case 3:
      Single(W(sp + 640 + 4 * Random(W(sp + 116), 80, self)));
      break;
    case 4:
      Copy(sp + 640, W(sp + 116), 4);
      break;
    case 5:
    case 7:
    case 9:
    case 11: {
      unsigned countOffset = mode == 5   ? 96
                             : mode == 7 ? 100
                             : mode == 9 ? 108
                                         : 112,
               pool = mode == 5   ? 512
                      : mode == 7 ? 256
                      : mode == 9 ? 384
                                  : 128,
               tag = mode == 5   ? 78
                     : mode == 7 ? 79
                     : mode == 9 ? 81
                                 : 82;
      auto count = W(sp + countOffset);
      if (!count)
        fail();
      else
        Single(W(sp + pool + 4 * Random(count, tag, self)));
      break;
    }
    case 6:
    case 8:
    case 10:
    case 12: {
      unsigned countOffset = mode == 6    ? 96
                             : mode == 8  ? 100
                             : mode == 10 ? 108
                                          : 112,
               pool = mode == 6    ? 512
                      : mode == 8  ? 256
                      : mode == 10 ? 384
                                   : 128,
               outMode = mode == 6    ? 2
                         : mode == 8  ? 3
                         : mode == 10 ? 5
                                      : 6;
      auto count = W(sp + countOffset);
      if (!count)
        fail();
      else
        Copy(sp + pool, count, outMode);
      break;
    }
    case 13: {
      auto count = W(Actor() + 76);
      if (!count) {
        fail();
        break;
      }
      for (unsigned i = 0; std::int32_t(i) < std::int32_t(W(Actor() + 76));
           ++i) {
        m.WriteU8(W(Actor() + 80) + i, m.ReadU8(W(Actor() + 72) + i));
        m.WriteU32(Actor() + 84, W(Actor() + 84) + 1);
      }
      m.WriteU32(Actor() + 336, 0);
      break;
    }
    case 14: {
      if (!W(Actor() + 76)) {
        fail();
        break;
      }
      auto target = Find(m.ReadU8(W(Actor() + 72)));
      if (!target) {
        fail();
        break;
      }
      auto flags = W(target + 124);
      bool team = (flags & 0x40000000) != 0;
      Copy(sp + (team ? 512 : 256), W(sp + (team ? 96 : 100)),
           (flags & 0x10000000) ? (team ? 2 : 3) : (team ? 5 : 6));
      break;
    }
    case 15: {
      Pools(sp, 2);
      auto count = W(sp + 116);
      unsigned out = 0;
      for (unsigned i = 0; std::int32_t(i) < std::int32_t(count); ++i) {
        auto id = W(sp + 640 + 4 * i), target = Find(id);
        if (target && (W(target + 124) & 0x00400000)) {
          m.WriteU8(W(Actor() + 80) + out++, id);
          m.WriteU32(Actor() + 84, W(Actor() + 84) + 1);
        }
      }
      m.WriteU32(Actor() + 336, 0);
      break;
    }
    case 16:
      if (!mask) {
        m.WriteU8(W(Actor() + 80), W(Actor() + 8));
        m.WriteU32(Actor() + 16, 1);
      } else {
        m.WriteU32(Actor() + 16, 0);
        unsigned out = 0;
        for (unsigned i = 0; i < 31; ++i)
          if (mask & (1u << i))
            m.WriteU8(W(Actor() + 80) + out++, i);
      }
      other();
      break;
    case 17:
      other();
      break;
    }
    s.r[3] = result;
    m.WriteU32(sp + 80, 0x8204a1d8);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82afde70)
    return false;
  unsigned first = 24, frame = 976;
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
} // namespace lo::semantic::gpu::battle_script_preparation61
