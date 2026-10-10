#include "lo_semantics/battle_script_presentation61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <limits>
namespace lo::semantic::gpu::battle_script_presentation61 {
namespace {
using recovery_abi::Address;
struct Presentation {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner, sp;
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
  void Call(unsigned e) {
    s.r[3] = 0x832cc130;
    d.guest.CallDirect(e, m, s);
  }
  void Next(unsigned n) {
    auto a = Actor();
    m.WriteU32(a + 52, W(a + 52) + n);
  }
  float Float(unsigned off, unsigned scratch = 80) {
    auto value = Get(off);
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    recovery_abi::WriteU64(m, sp + scratch,
                           std::uint64_t(std::int64_t(std::int32_t(value))));
    return float(std::int32_t(value));
  }
  void F(unsigned i, float value) {
    s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(value));
  }
  unsigned Integer(float value) {
    auto n = value >= 2147483648.f ? std::numeric_limits<std::int32_t>::max()
                                   : std::int32_t(value);
    m.WriteU32(sp + 80, unsigned(n));
    return unsigned(n);
  }
  void Numeric(unsigned mode, unsigned id) {
    if (mode == 0 || mode == 144) {
      auto a = Float(4, mode == 144 ? 112 : 80);
      F(1, a);
      s.r[5] = id;
      Call(mode ? 0x82b1be20 : 0x82b1bba8);
    } else if (mode == 16 || mode == 32) {
      auto a = Float(4), b = Float(6);
      F(1, a);
      F(2, b);
      s.r[6] = id;
      Call(mode == 16 ? 0x82b1ba30 : 0x82b1baa8);
    } else if (mode == 48) {
      auto a = Float(4), b = Float(6);
      auto y = Integer(b), x = Integer(a);
      s.r[4] = x;
      s.r[5] = y;
      s.r[6] = id;
      Call(0x82b1b9b8);
    } else if (mode == 64) {
      auto a = Float(4), b = Float(6), c = Float(8), d0 = Float(10);
      F(1, a);
      F(2, b);
      F(3, c);
      F(4, d0);
      s.r[8] = id;
      Call(0x82b1bb20);
    } else if (mode == 80) {
      auto key = Get(4);
      auto a = Float(6), b = Float(8);
      s.r[4] = key;
      s.r[7] = id;
      F(1, a);
      F(2, b);
      Call(0x82b1bc10);
    } else if (mode == 96) {
      auto a = Float(4), b = Float(6), c = Float(8);
      m.WriteU32(sp + 112, std::bit_cast<unsigned>(a));
      m.WriteU32(sp + 116, std::bit_cast<unsigned>(b));
      m.WriteU32(sp + 120, std::bit_cast<unsigned>(c));
      auto d0 = Float(10);
      m.WriteU32(sp + 124, std::bit_cast<unsigned>(d0));
      s.r[4] = recovery_abi::ReadU64(m, sp + 112);
      s.r[5] = recovery_abi::ReadU64(m, sp + 120);
      s.r[6] = id;
      Call(0x82b1bc98);
    } else if (mode == 112) {
      auto a = Float(4), b = Float(6), c = Float(8);
      auto alpha = Get(10);
      auto ia = std::int64_t(a), ib = std::int64_t(b), ic = std::int64_t(c);
      recovery_abi::WriteU64(m, sp + 88, std::uint64_t(ia));
      recovery_abi::WriteU64(m, sp + 96, std::uint64_t(ib));
      recovery_abi::WriteU64(m, sp + 112, std::uint64_t(ic));
      m.WriteU8(sp + 80, alpha);
      m.WriteU8(sp + 81, unsigned(ia));
      m.WriteU8(sp + 82, unsigned(ib));
      m.WriteU8(sp + 83, unsigned(ic));
      s.r[4] = W(sp + 80);
      s.r[5] = id;
      Call(0x82b1bd30);
    } else if (mode == 128) {
      auto a = Float(4, 112), b = Float(6, 112);
      m.WriteU32(sp + 96, std::bit_cast<unsigned>(a));
      m.WriteU32(sp + 100, std::bit_cast<unsigned>(b));
      auto c = Float(8, 112);
      m.WriteU32(sp + 104, std::bit_cast<unsigned>(c));
      s.r[4] = recovery_abi::ReadU64(m, sp + 96);
      s.r[5] = std::uint64_t(W(sp + 104)) << 32;
      s.r[6] = id;
      Call(0x82b1bd98);
    }
  }
  void Command(unsigned mode, unsigned id) {
    if (mode == 0 || mode == 48) {
      s.r[4] = id;
      Call(mode ? 0x82b1c7e0 : 0x82b1c788);
    } else if (mode < 32 || (mode > 32 && mode < 80)) {
      auto value = Get(4);
      unsigned base = mode < 32 ? 16 : 64;
      auto sub = mode - base;
      s.r[4] = sub <= 4 ? sub : 0;
      s.r[5] = value;
      s.r[6] = id;
      Call(mode < 32 ? 0x82b1b7a0 : 0x82b1b850);
    } else if (mode == 32 || mode == 80 || mode == 96 || mode == 128) {
      auto value = Get(4);
      s.r[4] = value;
      s.r[5] = id;
      Call(mode == 32   ? 0x82b1b7f8
           : mode == 80 ? 0x82b1b8a8
           : mode == 96 ? 0x82b1b900
                        : 0x82b1bfd8);
    } else if (mode == 112 || mode == 160) {
      auto a = Get(4), b = Get(6), c = Get(8);
      s.r[4] = a;
      s.r[5] = b;
      s.r[6] = c;
      s.r[7] = id;
      Call(mode == 112 ? 0x82b1bef0 : 0x82b1c0c8);
    } else if (mode < 160) {
      auto value = Get(4);
      constexpr unsigned codes[]{0, 1, 2, 32, 33, 34, 75, 65, 66};
      auto i = mode - 144;
      s.r[4] = value;
      s.r[5] = i < 9 ? codes[i] : 32;
      s.r[6] = id;
      Call(0x82b1c050);
    } else if (mode < 192) {
      auto flag = Get(4) != 0;
      auto value = Float(6);
      s.r[4] = flag;
      s.r[6] = mode == 177;
      s.r[7] = id;
      F(1, value);
      Call(0x82b1c1c0);
    } else if (mode == 192) {
      auto key = Get(4), flag = unsigned(Get(6) != 0);
      s.r[4] = key;
      s.r[5] = flag;
      s.r[6] = id;
      Call(0x82b1c238);
    } else if (mode == 208) {
      auto value = Float(4);
      auto flag = Get(6) != 0;
      s.r[5] = flag;
      s.r[6] = id;
      F(1, value);
      Call(0x82b1c2a0);
    } else if (mode < 240) {
      s.r[4] = mode != 224;
      s.r[5] = id;
      Call(0x82b1be88);
    }
  }
  void Run(unsigned e) {
    auto mode = Mode();
    if (e == 0x82afb788) {
      if (mode == 0) {
        Call(0x82b1c5f8);
        Set(2, Address(s.r[3]));
      } else if (mode == 1) {
        auto value = Get(2);
        s.r[4] = value;
        Call(0x82b1c6c0);
      } else if (mode == 2) {
        auto value = Get(2);
        m.WriteU32(0x832cc130, value);
      } else if (mode == 3)
        Set(2, W(0x832cc130));
      Next(4);
      return;
    }
    auto id = Get(2);
    if (e == 0x82afbea0) {
      Numeric(mode, id);
      Next(12);
    } else {
      Command(mode, id);
      Next(10);
    }
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82afbea0 && e != 0x82afb880 && e != 0x82afb788)
    return false;
  unsigned first = e == 0x82afbea0   ? 29
                   : e == 0x82afb880 ? 28
                                     : 31,
           frame = e == 0x82afbea0   ? 192
                   : e == 0x82afb880 ? 144
                                     : 96;
  auto owner = Address(s.r[3]), old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  if (e == 0x82afbea0)
    for (unsigned i = 29; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 40 - 8 * (31 - i), s.fpr_bits[i]);
  else if (e == 0x82afb880)
    recovery_abi::WriteU64(m, old - 48, s.fpr_bits[31]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  Presentation{m, d, s, owner, sp}.Run(e);
  s.r[1] += frame;
  if (e == 0x82afbea0)
    for (unsigned i = 29; i < 32; ++i)
      s.fpr_bits[i] = recovery_abi::ReadU64(m, old - 40 - 8 * (31 - i));
  else if (e == 0x82afb880)
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - 48);
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_presentation61
