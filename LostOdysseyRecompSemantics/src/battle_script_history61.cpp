#include "lo_semantics/battle_script_history61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
#include <bit>
#include <cmath>
#include <limits>
namespace lo::semantic::gpu::battle_script_history61 {
namespace {
using recovery_abi::Address;
struct History {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner, sp;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Actor() { return W(owner + 24); }
  unsigned Resource() { return W(Actor() + 4); }
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
  unsigned Int(double f) {
    std::int32_t v;
    if (f > double(std::numeric_limits<std::int32_t>::max()))
      v = std::numeric_limits<std::int32_t>::max();
    else if (std::isnan(f) ||
             f < double(std::numeric_limits<std::int32_t>::min()))
      v = std::numeric_limits<std::int32_t>::min();
    else
      v = std::int32_t(f);
    s.fpr_bits[0] = std::uint64_t(std::int64_t(v));
    m.WriteU32(sp + 80, unsigned(v));
    return unsigned(v);
  }
  unsigned Runtime(unsigned e) {
    d.guest.CallDirect(0x82380a18, m, s);
    d.guest.CallDirect(e, m, s);
    return Address(s.r[3]);
  }
  void Run(unsigned e) {
    if (e == 0x82af7198) {
      Set(1, 0);
      Set(3, 0);
      Set(5, 0);
      auto resource = Resource();
      if (resource) {
        auto scale = F(0x82000d48);
        std::array<float, 3> v{float(F(resource + 76324) * scale),
                               float(F(resource + 76328) * scale),
                               float(F(resource + 76332) * scale)};
        for (unsigned i = 0; i < 3; ++i)
          Set(1 + 2 * i, Int(v[i]));
      }
      Next(7);
      return;
    }
    if (e == 0x82af72a8) {
      auto resource = Resource();
      if (resource) {
        auto x = Get(1), y = Get(3), z = Get(5);
        recovery_abi::WriteU64(m, sp + 80,
                               std::uint64_t(std::int64_t(std::int32_t(z))));
        auto scale = F(0x82000d6c);
        m.WriteU32(resource + 76328, std::bit_cast<unsigned>(float(
                                         float(std::int32_t(y)) * scale)));
        m.WriteU32(resource + 76324, std::bit_cast<unsigned>(float(
                                         float(std::int32_t(x)) * scale)));
        m.WriteU32(resource + 76332, std::bit_cast<unsigned>(float(
                                         float(std::int32_t(z)) * scale)));
      }
      Next(7);
      return;
    }
    if (e == 0x82af9800) {
      auto value = Get(1), manager = Runtime(0x82389b78);
      m.WriteU16(manager + 148, value ? m.ReadU16(manager + 148) | value : 0);
      Next(3);
      return;
    }
    if (e == 0x82af73a0) {
      auto resource = Resource();
      if (resource) {
        auto value = Get(1), enabled = Get(3);
        m.WriteU32(resource + 196, value);
        m.WriteU32(resource + 200,
                   (W(resource + 200) & 0x7fffffff) |
                       (std::int32_t(enabled) > 0 ? 0x80000000 : 0));
      }
      Next(5);
      return;
    }
    if (e == 0x82af9cb0) {
      auto actor = Actor();
      m.WriteU32(actor + 296, 0);
      auto value = F(0x82000e50);
      for (unsigned i = 0; i < 8; ++i) {
        auto p = actor + 104 + 24 * i;
        for (unsigned j = 0; j < 6; ++j)
          m.WriteU32(p + 4 * j, j == 3 ? std::bit_cast<unsigned>(value) : 0);
      }
      Next(1);
      return;
    }
    if (e == 0x82af7410) {
      std::array<unsigned, 6> values{};
      auto actor = Actor(), count = W(actor + 296);
      float number = F(0x82000e50);
      if (std::int32_t(count) > 0) {
        auto p = actor + 80 + 24 * count;
        values = {W(p), W(p + 4), W(p + 8), 0, W(p + 16), W(p + 20)};
        number = F(p + 12);
      }
      Set(1, values[0]);
      Set(3, values[1]);
      Set(5, values[2]);
      Set(7, Int(number));
      Set(9, values[4]);
      Set(11, values[5]);
      Next(13);
      return;
    }
    if (e == 0x82af9d08) {
      unsigned result = 0xffffffff;
      if (std::int32_t(W(Actor() + 296)) > 0) {
        auto mode = Mode(), x = Get(4), y = Get(6), actor = Actor(),
             count = W(actor + 296);
        unsigned found = 0xffffffff;
        // Original descending search keeps replacing a match and excludes slot
        // 0.
        for (unsigned i = count - 1; std::int32_t(i) > 0; --i) {
          auto p = actor + 104 + 24 * i;
          bool match = false;
          switch (mode) {
          case 0:
            match = W(p) == x;
            break;
          case 1:
            match = W(p + 4) == x;
            break;
          case 2:
            match = W(p + 8) == x;
            break;
          case 3:
            match = W(p + 4) == x && W(p + 8) == y;
            break;
          case 4:
            match = W(p + 16) == x;
            break;
          case 5:
            match = W(p + 20) == x;
            break;
          }
          if (match)
            found = i;
        }
        if (found != 0xffffffff)
          result = count - found - 1;
      }
      Set(2, result);
      Next(8);
      return;
    }
    if (e == 0x82af9e68) {
      auto mode = Mode();
      if (mode < 2) {
        auto manager = Runtime(0x82389b78);
        m.WriteU32(manager + 148, W(manager + 148) | (mode ? 0x4000 : 0x8000));
      }
      Next(2);
      return;
    }
    if (e == 0x82afebc0) {
      auto resource = Resource();
      if (resource) {
        auto group = Get(2);
        if (group) {
          auto mode = Mode();
          m.WriteU32(resource + 208,
                     W(resource + 208) | (mode ? 0x40000000 : 0x80000000));
          m.WriteU32(resource + 204, group);
        } else {
          auto groupId = W(resource + 204), list = Runtime(0x8238e2f8);
          for (unsigned i = 0;
               std::int32_t(i) < std::int32_t(W(Runtime(0x8238e2f8) + 4));
               ++i) {
            auto p = W(W(list) + 4 * i);
            if (W(p + 204) == groupId) {
              m.WriteU32(p + 204, 0);
              m.WriteU32(p + 208, W(p + 208) & 0x3fffffff);
            }
          }
        }
      }
      Next(4);
      return;
    }
    if (e == 0x82af85b8) {
      auto resource = Resource();
      if (resource) {
        auto mode = Mode(), actor = Actor();
        if (!mode) {
          m.WriteU32(actor + 64, W(actor + 64) | 0x01000000);
          m.WriteU32(Actor() + 96, 0);
        } else if (m.ReadU8(actor + 64) & 1) {
          auto event = W(actor + 60);
          s.r[3] = resource;
          s.r[4] = s.r[5] = s.r[6] = ~std::uint64_t(0);
          s.r[7] = W(actor + 96);
          d.guest.CallDirect(event == 1 ? 0x82ab36c8 : 0x82ab38f0, m, s);
          actor = Actor();
          m.WriteU32(actor + 64, W(actor + 64) & ~0x01000000u);
        }
      }
      Next(2);
      return;
    }
    if (e == 0x82af7518) {
      auto actor = Actor();
      m.WriteU32(actor + 64, (W(actor + 64) & ~0x0c000000u) | 0x04000000);
      Next(1);
      return;
    }
    if (Resource())
      Next(3);
    else {
      s.r[3] = owner;
      s.r[4] = 1;
      (void)battle_script_extensions61::Apply(0x8238c590, m, d, s);
      m.WriteU32(Actor() + 52, Address(s.r[3]));
    }
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned first = 31, frame = 96, fpFirst = 32, fpOffset = 0;
  switch (e) {
  case 0x82af7198:
    frame = 128;
    fpFirst = 30;
    fpOffset = 24;
    break;
  case 0x82af72a8:
    frame = 128;
    first = 30;
    fpFirst = 30;
    fpOffset = 32;
    break;
  case 0x82af9800:
    frame = 112;
    first = 30;
    break;
  case 0x82af73a0:
    frame = 112;
    first = 29;
    break;
  case 0x82af7410:
    frame = 144;
    first = 27;
    fpFirst = 31;
    fpOffset = 56;
    break;
  case 0x82af9d08:
    frame = 128;
    first = 28;
    break;
  case 0x82afebc0:
    frame = 128;
    first = 27;
    break;
  case 0x82af9e68:
  case 0x82af85b8:
  case 0x82af7540:
    break;
  case 0x82af9cb0:
  case 0x82af7518:
    History{m, d, s, Address(s.r[3]), Address(s.r[1])}.Run(e);
    return true;
  default:
    return false;
  }
  auto owner = Address(s.r[3]), old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  for (unsigned i = fpFirst; i < 32; ++i)
    recovery_abi::WriteU64(m, old - fpOffset - 8 * (31 - i), s.fpr_bits[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  History{m, d, s, owner, sp}.Run(e);
  s.r[1] += frame;
  for (unsigned i = fpFirst; i < 32; ++i)
    s.fpr_bits[i] = recovery_abi::ReadU64(m, old - fpOffset - 8 * (31 - i));
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_history61
