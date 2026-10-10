#include "lo_semantics/battle_script_commands61.h"
#include "lo_semantics/battle_resource_growth61.h"
#include "lo_semantics/battle_resource_stats61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/battle_script_parameters61.h"
#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/battle_script_events61.h"
namespace lo::semantic::gpu::battle_script_commands61 {
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
    if (!battle_resource_growth61::Apply(e, m, d, s) &&
        !battle_resource_stats61::Apply(e, m, d, s))
      d.guest.CallDirect(e, m, s);
  }
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
  unsigned Raw(unsigned offset) {
    s.r[3] = owner;
    s.r[4] = offset;
    s.r[5] = 0;
    (void)battle_script_parameters61::Apply(0x8238be38, m, d, s);
    return Address(s.r[3]);
  }
  void Run(unsigned e) {
    if (e == 0x82afd038) {
      auto value = Get(1), actor = Actor(), resource = W(actor + 4);
      if (resource) {
        m.WriteU32(actor + 316, value);
        if (value - 1 <= 98) {
          s.r[3] = W(0x83291dc0);
          s.r[4] = resource;
          s.r[5] = value;
          if (std::int32_t(W(resource + 64)) < 20) {
            m.WriteU32(resource + 140, value);
            Call(0x82ac0588);
            s.r[3] = W(0x83291dc0);
            s.r[4] = resource;
            Call(0x82ac25e8);
            Call(0x82380a18);
            Call(0x82ab0110);
            m.WriteU32(W(0x83291dc0) + 4, W(resource + 5108));
            auto output = W(0x83291dc0);
            for (unsigned i = 0; i < 5; ++i)
              m.WriteU32(output + 12 + 4 * i, W(resource + 5116 + 4 * i));
          } else
            Call(0x82ac3820);
          s.r[3] = W(0x83291dc0);
          s.r[4] = resource;
          Call(0x82ac2468);
          s.r[3] = W(0x83291dc0);
          s.r[4] = resource;
          s.r[5] = (W(resource + 124) >> 28) & 1;
          Call(0x82ac0620);
          m.WriteU32(resource + 2588, W(resource + 2592));
          m.WriteU32(resource + 2616, W(resource + 2620));
          auto state = W(0x832c9c54 + 44);
          m.WriteU32(state + 28, W(state + 28) | 0x10000000u);
        }
      }
      Next(3);
      return;
    }
    if (e == 0x82a9f790) {
      auto a = Raw(2), b = Raw(4), c = Raw(6), mode = Mode();
      if (mode < 4) {
        constexpr unsigned calls[]{0x82a9f548, 0x82a9f648, 0x82a9f5c8,
                                   0x82a9f6b0};
        s.r[3] = owner;
        s.r[4] = a;
        s.r[5] = b;
        s.r[6] = c;
        Call(calls[mode]);
      }
      Next(8);
      return;
    }
    if (e == 0x82a9f8d8) {
      auto sp = Address(s.r[1]);
      for (unsigned i = 0; i < 17; ++i)
        m.WriteU16(sp + 80 + 2 * i, 0);
      auto a = Raw(2), b = Raw(4);
      for (unsigned i = 0; i < 8; ++i) {
        auto actor = Actor(), p = W(actor + 36) + W(actor + 52) + 6 + 2 * i;
        unsigned value = (unsigned(m.ReadU8(p)) << 8) | m.ReadU8(p + 1);
        if (value == 9675 || value == 9679)
          value = 0xe000;
        else if (value == 9633 || value == 9632)
          value = 0xe001;
        m.WriteU16(sp + 80 + 2 * i, value);
        if (!value)
          break;
      }
      auto mode = Mode();
      if (mode < 2) {
        if (!mode && !a && !b)
          a = b = 0xffffffff;
        Call(0x823a5058);
        s.r[4] = a;
        s.r[5] = b;
        s.r[6] = sp + 80;
        s.r[7] = mode;
        Call(0x82b079e8);
      }
      Next(22);
      return;
    }
    auto a = Get(1);
    unsigned b = 0, c = 0;
    if (e != 0x82afb488)
      b = Get(3);
    if (e == 0x82afb210)
      c = Get(5);
    else if (e == 0x82afb278 || e == 0x82afb3b0)
      c = Get(4);
    if (e == 0x82afb2e0) {
      c = Address(s.r[1]) + 80;
      for (unsigned i = 0; i < 64; ++i)
        m.WriteU16(c + 2 * i, 0);
      for (unsigned i = 0; i < 32; ++i) {
        auto actor = Actor(), p = W(actor + 36) + W(actor + 52) + 5 + 2 * i;
        m.WriteU16(c + 2 * i, (unsigned(m.ReadU8(p)) << 8) | m.ReadU8(p + 1));
      }
    }
    unsigned service = 0;
    switch (e) {
    case 0x82afb210:
      service = 0x82aa0ce0;
      break;
    case 0x82afb278:
      service = 0x82aa0cf0;
      break;
    case 0x82afb2e0:
      service = 0x82aa0d00;
      break;
    case 0x82afb3b0:
      service = 0x82aa0d10;
      break;
    case 0x82afb418:
      service = 0x82aa0d20;
      break;
    case 0x82afb488:
      service = 0x82aa0d30;
      break;
    case 0x82afb500:
      service = 0x82aa0d40;
      break;
    }
    s.r[3] = W(0x832cb798);
    s.r[4] = a;
    if (e != 0x82afb488)
      s.r[5] = b;
    if (e == 0x82afb210 || e == 0x82afb278 || e == 0x82afb2e0 ||
        e == 0x82afb3b0)
      s.r[6] = c;
    Call(service);
    if (e == 0x82afb488 && !(Address(s.r[3]) & 255))
      Jump(3);
    else
      Next(e == 0x82afb2e0                                         ? 69
           : e == 0x82afb210 || e == 0x82afb278 || e == 0x82afb3b0 ? 7
                                                                   : 5);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned first = 29, frame = 112;
  switch (e) {
  case 0x82a9f8d8:
    first = 28;
    frame = 160;
    break;
  case 0x82afb2e0:
    first = 30;
    frame = 624;
    break;
  case 0x82afb418:
  case 0x82afb500:
    first = 30;
    break;
  case 0x82afb488:
    first = 31;
    frame = 96;
    break;
  case 0x82afb210:
  case 0x82afb278:
  case 0x82afb3b0:
  case 0x82afd038:
  case 0x82a9f790:
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
} // namespace lo::semantic::gpu::battle_script_commands61
