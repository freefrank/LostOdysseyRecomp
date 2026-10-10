#include "lo_semantics/battle_script_scene61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/battle_completion61.h"
#include <bit>
namespace lo::semantic::gpu::battle_script_scene61 {
namespace {
using recovery_abi::Address;
struct Scene {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner, sp;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Actor() { return W(owner + 24); }
  unsigned Code() {
    auto a = Actor();
    return W(a + 36) + W(a + 52);
  }
  unsigned Mode() {
    return m.ReadU8(Code() + ((W(W(owner + 44) + 28) & 0x04000000) ? 2 : 1));
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
  void Branch(bool pass, unsigned n, unsigned offset) {
    if (pass) {
      Next(n);
      return;
    }
    s.r[3] = owner;
    s.r[4] = offset;
    (void)battle_script_extensions61::Apply(0x8238c590, m, d, s);
    m.WriteU32(Actor() + 52, Address(s.r[3]));
  }
  void Call(unsigned e) {
    if (!battle_completion61::Apply(e, m, d, s))
      d.guest.CallDirect(e, m, s);
  }
  float Number(unsigned value) {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    recovery_abi::WriteU64(m, sp + 80,
                           std::uint64_t(std::int64_t(std::int32_t(value))));
    return float(std::int32_t(value));
  }
  void Run(unsigned e) {
    if (e == 0x82af7ea0 || e == 0x82af7c20) {
      auto id = Get(1);
      s.r[4] = id;
      s.r[3] = e == 0x82af7ea0 ? 0x832cc05c : 0x832cc130;
      Call(e == 0x82af7ea0 ? 0x82b02880 : 0x82b1b958);
      Branch((Address(s.r[3]) & 255) != 0, 5, 3);
      return;
    }
    auto mode = Mode();
    if (e == 0x82afc9c8) {
      if (mode == 3) {
        auto a = Get(2), b = Get(4);
        s.r[3] = 0x832ca0e0;
        s.r[4] = a;
        s.r[5] = b;
        s.r[6] = 0;
        s.r[7] = 0;
        Call(0x82aae3c0);
      } else if (mode < 2) {
        auto a = Get(2), b = Get(4), c = Get(6);
        s.r[3] = 0x832cc05c;
        s.r[4] = std::uint64_t(std::int64_t(std::int8_t(a)));
        s.r[5] = b;
        s.r[6] = c;
        s.r[7] = 0;
        if (!mode)
          s.r[8] = 11;
        Call(mode ? 0x82b02ac0 : 0x82b04720);
      }
      Next(8);
      return;
    }
    if (e == 0x82afc8a0) {
      (void)Get(10);
      auto last = Get(12);
      auto x = Get(4);
      auto scale = std::bit_cast<float>(W(0x82000d6c));
      s.fpr_bits[31] = std::bit_cast<std::uint64_t>(double(scale));
      m.WriteU32(sp + 88, std::bit_cast<unsigned>(float(Number(x) * scale)));
      auto y = Get(6);
      m.WriteU32(sp + 92, std::bit_cast<unsigned>(float(Number(y) * scale)));
      auto z = Get(8);
      m.WriteU32(sp + 96, std::bit_cast<unsigned>(float(Number(z) * scale)));
      s.r[3] = 0x832cc0fc;
      s.r[4] = 0x61000000;
      s.r[5] = recovery_abi::ReadU64(m, sp + 88);
      s.r[6] = std::uint64_t(W(sp + 96)) << 32;
      s.r[7] = mode != 0;
      s.r[8] = last;
      s.r[9] = mode == 0;
      Call(0x82b1afe8);
      Next(14);
      return;
    }
    if (e == 0x82afc790) {
      if (mode == 0) {
        auto value = Get(4);
        s.r[3] = 0x832cc0fc;
        s.r[4] = value;
        Call(0x82b1aa50);
        Set(2, Address(s.r[3]));
      } else if (mode == 1) {
        auto value = Get(2);
        s.r[3] = 0x832cc0fc;
        s.r[4] = value;
        Call(0x82b1a518);
      } else if (mode == 16) {
        auto a = Get(4), b = Get(6);
        s.r[3] = 0x832cc0fc;
        s.r[4] = a;
        s.r[5] = b;
        s.r[6] = 0;
        Call(0x82b1aca8);
        Set(2, Address(s.r[3]));
      }
      Next(8);
      return;
    }
    if (e == 0x82afc4f0) {
      bool pass = false;
      switch (mode) {
      case 0:
        Call(0x82b19cc0);
        pass = (Address(s.r[3]) & 255) != 0;
        break;
      case 1: {
        auto id = Get(2);
        s.r[3] = 0x832cc0fc;
        s.r[4] = id;
        Call(0x82b19fd0);
        pass = (Address(s.r[3]) & 255) != 0;
        break;
      }
      case 2:
        pass = m.ReadU8(0x832ca0e0 + 5739) != 0;
        break;
      case 3: {
        auto id = Get(2);
        Call(0x82380a18);
        Call(0x82389b78);
        s.r[4] = id;
        Call(0x8238e308);
        auto resource = Address(s.r[3]);
        pass = resource && !(W(resource + 124) & 64);
        break;
      }
      case 4:
        s.r[3] = W(0x832652f0);
        Call(0x8285fe58);
        pass = Address(s.r[3]) == 1;
        break;
      case 5:
        s.r[3] = 0x832ca0e0 + 5232;
        s.r[4] = Actor() + 340;
        Call(0x82ab5768);
        pass = (Address(s.r[3]) & 255) == 1;
        break;
      case 6:
        s.r[3] = W(0x832652f0);
        Call(0x8285ff18);
        pass = Address(s.r[3]) == 1;
        break;
      default:
        return;
      }
      Branch(pass, 6, 4);
      return;
    }
    if (e == 0x82afc418) {
      auto a = Get(2);
      if (!mode) {
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(Number(a)));
        Call(0x82b1a048);
      } else {
        auto b = Get(4);
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(Number(b)));
        s.r[3] = a;
        Call(0x82b19c00);
      }
      Next(6);
      return;
    }
    // Inline 32-unit text payload followed by three script operands.
    auto id = Get(65), duration = Get(67);
    s.fpr_bits[31] = std::bit_cast<std::uint64_t>(double(Number(duration)));
    auto flags = Get(69) & 255;
    for (unsigned i = 0; i < 64; ++i)
      m.WriteU16(sp + 96 + 2 * i, 0);
    for (unsigned i = 0; i < 32; ++i) {
      auto p = Code() + 1 + 2 * i;
      m.WriteU16(sp + 96 + 2 * i,
                 (unsigned(m.ReadU8(p)) << 8) | m.ReadU8(p + 1));
    }
    constexpr unsigned masks[]{1, 256, 4352, 8448, 12544, 512, 66048, 131584};
    unsigned expanded = 0;
    for (unsigned i = 0; i < 8; ++i)
      if (flags & (1u << i))
        expanded |= masks[i];
    s.r[3] = sp + 96;
    s.r[4] = id;
    s.r[5] = flags;
    s.r[6] = expanded;
    s.fpr_bits[1] = s.fpr_bits[31];
    Call(0x82b1b2e8);
    Next(71);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned first = 30, frame = 112, fpSave = 0;
  switch (e) {
  case 0x82af7ea0:
  case 0x82af7c20:
    first = 31;
    frame = 96;
    break;
  case 0x82afc9c8:
    first = 29;
    break;
  case 0x82afc8a0:
    first = 29;
    frame = 144;
    fpSave = 40;
    break;
  case 0x82afc790:
  case 0x82afc4f0:
  case 0x82afc418:
    break;
  case 0x82af7c98:
    frame = 640;
    fpSave = 32;
    break;
  default:
    return false;
  }
  auto owner = recovery_abi::Address(s.r[3]),
       old = recovery_abi::Address(s.r[1]);
  m.WriteU32(old - 8, recovery_abi::Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  if (fpSave)
    recovery_abi::WriteU64(m, old - fpSave, s.fpr_bits[31]);
  s.r[1] -= frame;
  auto sp = recovery_abi::Address(s.r[1]);
  m.WriteU32(sp, old);
  Scene{m, d, s, owner, sp}.Run(e);
  s.r[1] += frame;
  if (fpSave)
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - fpSave);
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_scene61
