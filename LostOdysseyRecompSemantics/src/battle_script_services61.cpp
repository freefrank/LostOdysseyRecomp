#include "lo_semantics/battle_script_services61.h"
#include "lo_semantics/battle_script_parameters61.h"
#include "lo_semantics/battle_script_core61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
#include <limits>
namespace lo::semantic::gpu::battle_script_services61 {
namespace {
using recovery_abi::Address;
struct Services {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner, sp;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Get(unsigned off) {
    s.r[3] = owner;
    s.r[4] = off;
    s.r[5] = 0;
    (void)battle_script_parameters61::Apply(0x8238be38, m, d, s);
    return Address(s.r[3]);
  }
  void Set(unsigned off, unsigned value) {
    s.r[3] = owner;
    s.r[4] = off;
    s.r[5] = value;
    s.r[6] = 0;
    (void)battle_script_core61::Apply(0x8238c210, m, d, s);
  }
  void Next(unsigned n) {
    auto a = W(owner + 24);
    m.WriteU32(a + 52, W(a + 52) + n);
  }
  void Fp() {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
  }
  float F(unsigned p) {
    Fp();
    return std::bit_cast<float>(W(p));
  }
  unsigned Integer(double value) {
    std::int32_t result;
    if (value > double(std::numeric_limits<std::int32_t>::max()))
      result = std::numeric_limits<std::int32_t>::max();
    else if (std::isnan(value) ||
             value < double(std::numeric_limits<std::int32_t>::min()))
      result = std::numeric_limits<std::int32_t>::min();
    else
      result = std::int32_t(value);
    s.fpr_bits[0] = std::uint64_t(std::int64_t(result));
    m.WriteU32(sp + 80, unsigned(result));
    return unsigned(result);
  }
  void Invoke(unsigned object, unsigned table, unsigned slot) {
    s.r[3] = object;
    s.ctr = W(table + slot);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  void Run(unsigned e) {
    if (e == 0x82a9d570) {
      auto value = Get(5), row = Get(1), column = Get(3);
      m.WriteU32(W(owner + 28) + 4 * (3577 * row + column + 689), value);
      Next(7);
      return;
    }
    if (e == 0x82a9ecc0 || e == 0x82a9edd0) {
      if (e == 0x82a9ecc0)
        m.WriteU32(sp + 80, 0);
      unsigned value = e == 0x82a9edd0 ? Get(4) : 0;
      auto id = Get(2);
      d.guest.CallDirect(0x82380a18, m, s);
      d.guest.CallDirect(0x82389b78, m, s);
      s.r[4] = id;
      d.guest.CallDirect(0x8238e308, m, s);
      auto resource = Address(s.r[3]);
      if (resource) {
        auto actor = W(owner + 24);
        auto code = W(actor + 36) + W(actor + 52);
        auto mode =
            m.ReadU8(code + ((W(W(owner + 44) + 28) & 0x04000000) ? 2 : 1));
        if (mode < 4) {
          if (e == 0x82a9ecc0) {
            constexpr unsigned offsets[]{2588, 2592, 2616, 2620};
            value = Integer(F(resource + offsets[mode]));
          } else {
            Fp();
            recovery_abi::WriteU64(
                m, sp + 80, std::uint64_t(std::int64_t(std::int32_t(value))));
            auto p = resource + (mode < 2 ? 2588 : 2616);
            float f = float(std::int32_t(value));
            if (mode & 1)
              f = float(f + F(p));
            m.WriteU32(p, std::bit_cast<unsigned>(f));
          }
        }
      }
      if (e == 0x82a9ecc0)
        Set(4, m.ReadU32(sp + 80));
      Next(6);
      return;
    }
    if (e == 0x82a9ef08) {
      auto object = W(0x83315fb4);
      Invoke(object, W(object), 352);
      object = Address(s.r[3]);
      Invoke(object, W(object), 300);
      Set(1, Address(s.r[3]));
      Next(3);
      return;
    }
    auto play = W(owner + 28), table = W(play);
    s.r[6] = play;
    if (e == 0x82a9d638) {
      auto last = Get(7), b = Get(5);
      recovery_abi::WriteU64(m, sp + 80,
                             std::uint64_t(std::int64_t(std::int32_t(b))));
      auto scale = F(0x82000d6c);
      s.fpr_bits[2] = std::bit_cast<std::uint64_t>(
          double(float(float(std::int32_t(b)) * scale)));
      auto a = Get(3);
      recovery_abi::WriteU64(m, sp + 80,
                             std::uint64_t(std::int64_t(std::int32_t(a))));
      s.fpr_bits[1] = std::bit_cast<std::uint64_t>(
          double(float(float(std::int32_t(a)) * scale)));
      auto key = Get(1);
      s.r[4] = key;
      s.r[7] = last;
      Invoke(play, table, 552);
      Next(9);
      return;
    }
    if (e == 0x82a9d920) {
      auto value = Get(3);
      recovery_abi::WriteU64(m, sp + 80,
                             std::uint64_t(std::int64_t(std::int32_t(value))));
      s.fpr_bits[1] = std::bit_cast<std::uint64_t>(
          double(float(float(std::int32_t(value)) * F(0x82000d6c))));
      auto key = Get(1);
      s.r[4] = key;
      Invoke(play, table, 568);
      Next(5);
      return;
    }
    if (e == 0x82a9d7c0 || e == 0x82a9d9b8) {
      auto value = Get(3), key = Get(1);
      s.r[4] = key;
      s.r[5] = value;
      Invoke(play, table, e == 0x82a9d7c0 ? 560 : 572);
      Next(5);
      return;
    }
    auto key = Get(1);
    s.r[4] = key;
    unsigned slot = e == 0x82a9d6f0   ? 556
                    : e == 0x82a9d758 ? 564
                    : e == 0x82a9d828 ? 580
                                      : 576;
    Invoke(play, table, slot);
    if (e == 0x82a9d828) {
      Set(3, Integer(std::bit_cast<double>(s.fpr_bits[1])));
      Next(5);
    } else if (e == 0x82a9d8a8) {
      Set(3, Address(s.r[3]));
      Next(5);
    } else
      Next(3);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned first = 30, frame = 112;
  switch (e) {
  case 0x82a9d570:
  case 0x82a9ecc0:
  case 0x82a9d6f0:
  case 0x82a9d758:
  case 0x82a9d920:
    break;
  case 0x82a9edd0:
  case 0x82a9d638:
    first = 29;
    frame = 128;
    break;
  case 0x82a9d7c0:
  case 0x82a9d9b8:
    first = 29;
    break;
  case 0x82a9d828:
    first = 31;
    break;
  case 0x82a9d8a8:
  case 0x82a9ef08:
    first = 31;
    frame = 96;
    break;
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
  Services{m, d, s, owner, sp}.Run(e);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_services61
