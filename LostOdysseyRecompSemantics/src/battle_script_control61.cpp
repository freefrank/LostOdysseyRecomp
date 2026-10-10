#include "lo_semantics/battle_script_control61.h"
#include "lo_semantics/battle_script_core61.h"
#include "lo_semantics/battle_script_parameters61.h"
#include "lo_semantics/battle_script_events61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_script_control61 {
namespace {
using recovery_abi::Address;
struct Control {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner, sp;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Actor() { return W(owner + 24); }
  unsigned Code() { return W(Actor() + 36) + W(Actor() + 52); }
  unsigned Get(unsigned offset) {
    s.r[3] = owner;
    s.r[4] = offset;
    s.r[5] = 0;
    (void)battle_script_parameters61::Apply(0x8238be38, m, d, s);
    return Address(s.r[3]);
  }
  void Set(unsigned value) {
    s.r[3] = owner;
    s.r[4] = 1;
    s.r[5] = value;
    s.r[6] = 0;
    (void)battle_script_core61::Apply(0x8238c210, m, d, s);
  }
  void Next(unsigned n) { m.WriteU32(Actor() + 52, W(Actor() + 52) + n); }
  void Jump(unsigned offset) {
    auto p = Code() + offset;
    auto v = unsigned(m.ReadU8(p)) | (unsigned(m.ReadU8(p + 1)) << 8);
    m.WriteU32(Actor() + 52, unsigned(std::int32_t(std::int16_t(v))));
  }
  void ActorFlags() {
    auto code = Code();
    unsigned id = 0;
    for (unsigned i = 0; i < 4; ++i)
      id |= unsigned(m.ReadU8(code + 2 + i)) << (8 * i);
    s.r[3] = owner;
    s.r[4] = id;
    (void)battle_script_events61::Apply(0x8238c118, m, d, s);
    auto target = Address(s.r[3]);
    if (target) {
      auto value = [&]() {
        return m.ReadU8(Code() +
                        ((W(W(owner + 44) + 28) & 0x04000000) ? 2 : 1));
      };
      m.WriteU32(target + 64,
                 (W(target + 64) & 0x7fffffff) | (unsigned(value() & 1) << 31));
      unsigned resource;
      if (std::int32_t(W(target)) <= 5) {
        d.guest.CallDirect(0x82380a18, m, s);
        d.guest.CallDirect(0x82389b78, m, s);
        s.r[4] = W(target);
        d.guest.CallDirect(0x8238e308, m, s);
        resource = Address(s.r[3]);
      } else
        resource = W(target + 4);
      if (resource) {
        if (W(resource + 124) & 0x10000000) {
          m.WriteU32(sp + 80, 0x8204a1d8);
          s.r[3] = resource;
          s.r[4] = 20;
          if (W(target + 64) & 0x80000000) {
            s.r[5] = 1;
            s.r[6] = 0;
            d.guest.CallDirect(0x82ac9be0, m, s);
          } else
            d.guest.CallDirect(0x82ac9000, m, s);
          m.WriteU32(sp + 80, 0x8204a1d8);
        }
        m.WriteU32(resource + 124,
                   (W(resource + 124) & ~0x10000u) | (value() ? 0x10000u : 0));
        m.WriteU32(resource + 132, (W(target + 64) & 0x80000000) ? 0 : 1);
      }
    }
    Next(6);
  }
  void Run(unsigned entry) {
    if (entry == 0x82a9e7d0) {
      ActorFlags();
      return;
    }
    if (entry == 0x82a9d2b0) {
      auto value = std::int32_t(Get(1)), low = std::int32_t(Get(3));
      bool pass = value >= low && value <= std::int32_t(Get(5));
      if (pass)
        Next(10);
      else
        Jump(8);
      return;
    }
    if (entry == 0x82a9d228) {
      auto right = Get(3), left = Get(1);
      if (left & right)
        Next(7);
      else
        Jump(5);
      return;
    }
    if (entry == 0x82a9d0d0) {
      auto value = Get(1);
      Set(value + W(W(owner + 44) + 16));
      Next(3);
      return;
    }
    if (entry == 0x82a9d130) {
      auto left = Get(3), right = Get(5);
      Set(left && right ? unsigned(std::int32_t(left) % std::int32_t(right))
                        : 0);
      Next(7);
      return;
    }
    auto state = W(owner + 28), value = Get(1), sum = W(state + 76) + value;
    m.WriteU32(state + 76, std::int32_t(sum) > 9999999 ? 9999999u : sum);
    Next(3);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82a9e7d0 && e != 0x82a9d2b0 && e != 0x82a9d228 &&
      e != 0x82a9d0d0 && e != 0x82a9d130 && e != 0x82a9d360)
    return false;
  auto owner = Address(s.r[3]), old = Address(s.r[1]);
  unsigned frame = e == 0x82a9e7d0   ? 144
                   : e == 0x82a9d2b0 ? 112
                                     : 96,
           first = e == 0x82a9e7d0   ? 27
                   : e == 0x82a9d2b0 ? 30
                                     : 31;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  Control{m, d, s, owner, sp}.Run(e);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_control61
