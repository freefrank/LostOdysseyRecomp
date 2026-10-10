#include "lo_semantics/battle_script_scene_state61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::battle_script_scene_state61 {
namespace {
using recovery_abi::Address;
struct SceneState {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner, sp;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Actor() { return W(owner + 24); }
  unsigned State() { return W(owner + 44); }
  unsigned Code() {
    auto a = Actor();
    return W(a + 36) + W(a + 52);
  }
  unsigned Mode() {
    return m.ReadU8(Code() + ((W(State() + 28) & 0x04000000) ? 2 : 1));
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
  void Virtual(unsigned object, unsigned slot) {
    s.r[3] = object;
    s.ctr = W(W(object) + slot);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  unsigned Object() { return W(0x832cc05c + 112); }
  bool Suppressed() { return W(W(0x832c9c54 + 44) + 28) & 0x00020000; }
  void Text(unsigned dest, unsigned offset, bool clear) {
    if (clear)
      for (unsigned i = 0; i < 64; ++i)
        m.WriteU16(dest + 2 * i, 0);
    for (unsigned i = 0; i < 32; ++i) {
      auto p = Code() + offset + 2 * i;
      m.WriteU16(dest + 2 * i, (unsigned(m.ReadU8(p)) << 8) | m.ReadU8(p + 1));
    }
  }
  void Run(unsigned e) {
    if (e == 0x82b00508) {
      auto id = Get(2), value = Get(4), mode = Mode();
      s.r[5] = value;
      if (mode == 2 || mode == 4) {
        auto state = State();
        Call(0x82380a18);
        s.r[4] = id;
        Call(0x82380d30);
        if (Address(s.r[3])) {
          if (mode == 2)
            Call(0x82ad9af0);
          else if (state + 16752 && state + 17264) {
            s.r[4] = state + 17264;
            s.r[5] = state + 16752;
            Call(0x82adc1c8);
          }
        }
      } else if (mode < 8) {
        constexpr unsigned targets[]{0x82afd278, 0x82aad100, 0,
                                     0x82af8110, 0,          0x82aad1a0,
                                     0x82aad278, 0x82ab57f8};
        s.r[3] = 0x832ca0e0 + (mode == 7 ? 5232 : 0);
        s.r[4] = id;
        Call(targets[mode]);
      }
      Next(6);
      return;
    }
    if (e == 0x82afce18) {
      auto target = State() + 16752 + 512 * Mode();
      Text(target, 3, true);
      Next(67);
      return;
    }
    if (e == 0x82afb570) {
      auto a = Get(1), b = Get(3);
      s.r[3] = W(0x832cb798);
      s.r[4] = a;
      s.r[5] = b;
      Call(0x82ae1498);
      Next(5);
      return;
    }
    if (e == 0x82afc6d8) {
      Text(sp + 80, 1, true);
      for (unsigned i = 0; i < 32; ++i)
        m.WriteU16(Actor() + 340 + 2 * i, m.ReadU16(sp + 80 + 2 * i));
      s.r[3] = 0x832ca0e0 + 5232;
      s.r[4] = sp + 80;
      Call(0x82ab8870);
      Next(65);
      return;
    }
    if (e == 0x82afa5f0) {
      auto resource = W(Actor() + 4);
      unsigned resultType = 0xffffffff, resultId = 0;
      if (resource) {
        for (unsigned i = 0; i < 12; ++i)
          m.WriteU32(sp + 96 + 4 * i, 0);
        auto type = Get(1);
        (void)Get(3);
        if (type == 2 || (type >= 6 && type <= 9)) {
          unsigned count = 0;
          for (unsigned i = 0; i < 12; ++i)
            if (W(resource + 10744 + 4 * i) & 0x80000000) {
              auto record = W(0x83264978 + 12) + 96 * (50 + i);
              s.r[3] = W(0x83291dc0);
              s.r[4] = resource;
              s.r[5] = W(record + 16);
              Call(0x82ac1af0);
              auto cost = std::int32_t(Address(s.r[3]));
              recovery_abi::WriteU64(m, sp + 80,
                                     std::uint64_t(std::int64_t(cost)));
              if (s.cached_fp_control & 0x8040) {
                s.cached_fp_control &= ~0x8040u;
                d.fp.SetHostFpControl(s.cached_fp_control);
              }
              auto available = std::bit_cast<float>(W(resource + 2616));
              if (!(float(cost) > available))
                m.WriteU32(sp + 96 + 4 * count++, 50 + i);
            }
          if (count) {
            s.r[3] = W(0x83264558);
            s.r[4] = 0;
            s.r[5] = count - 1;
            s.r[6] = 89;
            s.r[7] = W(resource + 64);
            Call(0x82aa0740);
            resultType = 7;
            resultId = W(sp + 96 + 4 * Address(s.r[3]));
          }
        }
      }
      Set(5, resultType);
      Set(7, resultId);
      Next(9);
      return;
    }
    if (e == 0x82af9920) {
      auto value = Get(1);
      Call(0x82380a18);
      Call(0x82389b78);
      auto manager = Address(s.r[3]);
      m.WriteU32(manager + 148,
                 (W(manager + 148) & ~0x780u) | ((value & 15) << 7));
      Next(3);
      return;
    }
    if (e == 0x82af9a30) {
      auto value = Mode(), actor = Actor();
      m.WriteU32(actor + 64,
                 (W(actor + 64) & ~0x00200000u) | ((value & 1) << 21));
      Next(2);
      return;
    }
    if (e == 0x82afa850) {
      auto count = W(State() + 196), value = Get(2);
      m.WriteU32(State() + 68 + 8 * count, value);
      auto mode = Mode();
      if (mode < 4) {
        m.WriteU8(State() + 73 + 8 * count, mode == 0 || mode == 2);
        m.WriteU8(State() + 72 + 8 * count, mode == 0 || mode == 1);
      }
      auto state = State();
      m.WriteU32(state + 196, W(state + 196) + 1);
      Next(4);
      return;
    }
    if (e == 0x82b00398) {
      Virtual(W(0x83315fb4), 352);
      Call(0x8229dfd8);
      auto buffer = Address(s.r[3]);
      auto value = Get(1);
      if (value == 2)
        m.WriteU32(buffer + 128, 0);
      s.r[3] = 0x832cc05c;
      s.r[4] = value;
      Call(0x82af7ff0);
      Next(3);
      return;
    }
    if (e == 0x82afa980) {
      auto object = Object();
      if (object && W(object + 4) == 3)
        Next(3);
      else {
        s.r[3] = owner;
        s.r[4] = 1;
        (void)battle_script_extensions61::Apply(0x8238c590, m, d, s);
        m.WriteU32(Actor() + 52, Address(s.r[3]));
      }
      return;
    }
    if (e == 0x82afaa08) {
      auto object = Object();
      if (object)
        Virtual(object, 16);
      Next(1);
      return;
    }
    if (e == 0x82afaa68) {
      auto mode = Mode(), object = Object();
      if (object) {
        s.r[3] = object;
        s.r[4] = mode ? 0 : 11;
        s.r[5] = mode ? 11 : 0;
        Call(0x82b2c590);
      }
      Next(2);
      return;
    }
    if (e == 0x82b00428) {
      if (!Suppressed()) {
        Text(sp + 80, 1, true);
        s.r[3] = 0x832cbfb0;
        s.r[4] = sp + 80;
        Call(0x82ad6ab0);
        auto object = Object();
        if (object && std::int32_t(Address(s.r[3])) >= 0)
          m.WriteU32(object + 220, Address(s.r[3]));
      }
      Next(65);
      return;
    }
    if (e == 0x82afab10) {
      (void)Get(1);
      if (!Suppressed())
        Text(sp + 80, 3, false);
      Next(67);
      return;
    }
    auto object = Object();
    unsigned result = 0;
    if (object) {
      Virtual(object, 12);
      result = (Address(s.r[3]) & 255) == 1;
    }
    Set(1, result);
    Next(3);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned first = 31, frame = 96;
  switch (e) {
  case 0x82b00508:
    first = 28;
    frame = 128;
    break;
  case 0x82afb570:
  case 0x82af9920:
  case 0x82b00398:
  case 0x82afabb0:
    first = 30;
    frame = 112;
    break;
  case 0x82afc6d8:
  case 0x82b00428:
  case 0x82afab10:
    frame = 608;
    break;
  case 0x82afa5f0:
    first = 21;
    frame = 240;
    break;
  case 0x82afa850:
    first = 29;
    frame = 112;
    break;
  case 0x82afa980:
  case 0x82afaa08:
  case 0x82afaa68:
    break;
  case 0x82afce18:
  case 0x82af9a30:
    SceneState{m, d, s, Address(s.r[3]), Address(s.r[1])}.Run(e);
    return true;
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
  SceneState{m, d, s, owner, sp}.Run(e);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_scene_state61
