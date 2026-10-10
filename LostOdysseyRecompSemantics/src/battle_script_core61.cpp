#include "lo_semantics/battle_script_core61.h"
#include "lo_semantics/battle_script_parameters61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::battle_script_core61 {
namespace {
using recovery_abi::Address;
struct Core {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Actor() { return W(owner + 24); }
  unsigned Code() { return W(Actor() + 36) + W(Actor() + 52); }
  unsigned SignedHalf(unsigned p) {
    return unsigned(std::int32_t(std::int16_t(
        unsigned(m.ReadU8(p)) | (unsigned(m.ReadU8(p + 1)) << 8))));
  }
  unsigned Get(unsigned offset, unsigned extra = 0) {
    s.r[3] = owner;
    s.r[4] = offset;
    s.r[5] = extra;
    (void)battle_script_parameters61::Apply(0x8238be38, m, d, s);
    return Address(s.r[3]);
  }
  void Set(unsigned offset, unsigned value, unsigned extra = 0) {
    auto index = SignedHalf(Code() + offset) + extra;
    if (index & 0x8000)
      return;
    if (std::int32_t(index) < 2048) {
      if (std::int32_t(index) < 256)
        m.WriteU32(W(Actor() + 12) + 4 * index, value);
      return;
    }
    if (std::int32_t(index) < 4096) {
      m.WriteU32(W(owner + 28) + 4 * (index + 40668), value);
      return;
    }
    if (std::int32_t(index) < 6144) {
      index -= 4096;
      if (std::int32_t(index) < 512)
        m.WriteU32(W(W(owner + 44)) + 4 * index, value);
      return;
    }
    if (std::int32_t(index) < 22528) {
      auto bit = index - 6144, p = W(owner + 28) + 4 * (44764 + bit / 32),
           mask = 1u << (bit % 32);
      m.WriteU32(p, value ? W(p) | mask : W(p) & ~mask);
      return;
    }
    auto resource = W(Actor() + 4);
    if (index >= 32640 || !resource || index - 32514 > 3)
      return;
    constexpr unsigned offsets[]{2592, 2620, 2588, 2616};
    recovery_abi::WriteU64(m, Address(s.r[1]) - 16,
                           std::uint64_t(std::int64_t(std::int32_t(value))));
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    float f = float(std::int32_t(value));
    s.fpr_bits[0] = std::bit_cast<std::uint64_t>(double(f));
    m.WriteU32(resource + offsets[index - 32514], std::bit_cast<unsigned>(f));
  }
  void Next(unsigned n) { m.WriteU32(Actor() + 52, W(Actor() + 52) + n); }
  void End() {
    auto actor = Actor(), p = W(actor + 40) + 24 * W(actor + 56);
    m.WriteU32(p, 255);
    m.WriteU32(p + 4, 0);
    m.WriteU32(p + 8, 0xffffffff);
    m.WriteU32(p + 12, 0);
    m.WriteU32(p + 16, 0);
    auto state = W(owner + 44);
    m.WriteU32(state + 28, W(state + 28) | 0x80000000);
  }
  void Condition() {
    auto mode = m.ReadU8(Code() + 5) & 15;
    bool pass = false;
    if (mode <= 10) {
      unsigned left, right;
      if (mode == 10) {
        left = Get(1);
        right = Get(3);
      } else {
        right = Get(3);
        left = Get(1);
      }
      switch (mode) {
      case 0:
        pass = left == right;
        break;
      case 1:
        pass = left != right;
        break;
      case 2:
        pass = std::int32_t(left) > std::int32_t(right);
        break;
      case 3:
        pass = std::int32_t(left) < std::int32_t(right);
        break;
      case 4:
        pass = std::int32_t(left) >= std::int32_t(right);
        break;
      case 5:
        pass = std::int32_t(left) <= std::int32_t(right);
        break;
      case 6:
      case 9:
        pass = (left & right) != 0;
        break;
      case 7:
        pass = (left ^ right) != 0;
        break;
      case 8:
        pass = (left | right) != 0;
        break;
      case 10:
        pass = (right & ~left) != 0;
        break;
      }
    }
    if (pass)
      Next(8);
    else
      m.WriteU32(Actor() + 52, SignedHalf(Code() + 6));
  }
  void Operation(unsigned entry) {
    unsigned value = 0, length = 5, extra = 0;
    if (entry == 0x8238c5c0)
      value = Get(3);
    else if (entry == 0x82a9d5f8) {
      value = 1;
      length = 3;
    } else if (entry == 0x82a9c048) {
      value = 0;
      length = 3;
    } else if (entry == 0x82a9c258 || entry == 0x82a9c2b0) {
      value = Get(1) + (entry == 0x82a9c258 ? 1u : 0xffffffffu);
      length = 3;
    } else if (entry == 0x82a9c520 || entry == 0x82a9c570) {
      s.r[3] = owner;
      d.guest.CallDirect(0x822c9fc0, m, s);
      auto random = Address(s.r[3]);
      if (entry == 0x82a9c520) {
        value = random;
        length = 3;
      } else {
        auto bound = Get(3) + 1;
        value = unsigned(std::int32_t(random) % std::int32_t(bound));
      }
    } else {
      auto right = Get(3);
      if (entry == 0x82a9c158 || entry == 0x82a9c1d8) {
        extra = unsigned(std::int32_t(right) >> 5);
        auto left = Get(1, extra), mask = 1u << (right & 31);
        value = entry == 0x82a9c158 ? left | mask : left & ~mask;
      } else {
        auto left = Get(1);
        switch (entry) {
        case 0x82a9c088:
          value = left + right;
          break;
        case 0x82a9c0f0:
          value = left - right;
          break;
        case 0x82a9c308:
          value = left & right;
          break;
        case 0x82a9c370:
          value = left | right;
          break;
        case 0x82a9c3d8:
          value = left ^ right;
          break;
        case 0x82a9c440: {
          auto n = right & 63;
          value = n < 32 ? left << n : 0;
          break;
        }
        case 0x82a9c4b0: {
          auto n = right & 63;
          bool negative = std::int32_t(left) < 0;
          s.xer_ca = negative && (n >= 32 || (left & ((1u << n) - 1)) != 0);
          value = n >= 32 ? (negative ? 0xffffffffu : 0)
                          : unsigned(std::int32_t(left) >> n);
          break;
        }
        }
      }
    }
    s.r[3] = owner;
    s.r[4] = 1;
    s.r[5] = value;
    s.r[6] = extra;
    Set(1, value, extra);
    Next(length);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  bool operation = false;
  switch (e) {
  case 0x8238c210:
  case 0x8238c648:
  case 0x8238c618:
  case 0x8238bb90:
    break;
  case 0x8238c5c0:
  case 0x82a9d5f8:
  case 0x82a9c048:
  case 0x82a9c088:
  case 0x82a9c0f0:
  case 0x82a9c158:
  case 0x82a9c1d8:
  case 0x82a9c258:
  case 0x82a9c2b0:
  case 0x82a9c308:
  case 0x82a9c370:
  case 0x82a9c3d8:
  case 0x82a9c440:
  case 0x82a9c4b0:
  case 0x82a9c520:
  case 0x82a9c570:
    operation = true;
    break;
  default:
    return false;
  }
  auto owner = Address(s.r[3]);
  Core c{m, d, s, owner};
  if (e == 0x8238c210) {
    c.Set(Address(s.r[4]), Address(s.r[5]), Address(s.r[6]));
    return true;
  }
  if (e == 0x8238c648) {
    c.End();
    return true;
  }
  if (e == 0x8238c618) {
    m.WriteU32(c.Actor() + 52, c.SignedHalf(c.Code() + 1));
    return true;
  }
  auto old = Address(s.r[1]);
  bool two = e == 0x82a9c158 || e == 0x82a9c1d8 || e == 0x82a9c440 ||
             e == 0x82a9c4b0 || e == 0x82a9c570;
  unsigned frame = two ? 112 : 96,
           first = two                                    ? 30
                   : (e == 0x82a9d5f8 || e == 0x82a9c048) ? 32
                                                          : 31;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  if (operation)
    c.Operation(e);
  else
    c.Condition();
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_core61
