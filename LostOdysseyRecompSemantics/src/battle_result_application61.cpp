#include "lo_semantics/battle_result_application61.h"
#include "lo_semantics/battle_progression61.h"
#include "lo_semantics/battle_property_mutation61.h"
#include "lo_semantics/battle_random_range61.h"
#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <limits>
namespace lo::semantic::gpu::battle_result_application61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_result_application61::Apply(e, m, d, s) &&
      !battle_progression61::Apply(e, m, d, s) &&
      !battle_property_mutation61::Apply(e, m, d, s) &&
      !battle_random_range61::Apply(e, m, d, s) &&
      !battle_action_readiness61::Apply(e, m, d, s) &&
      !battle_script_actions61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
std::int32_t Trunc(double x) {
  return x > double(std::numeric_limits<std::int32_t>::max())
             ? std::numeric_limits<std::int32_t>::max()
         : !(x >= -2147483648.) ? std::numeric_limits<std::int32_t>::min()
                                : std::int32_t(x);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82acad40) {
    auto old = Address(s.r[1]), resource = Address(s.r[4]),
         element = Address(s.r[5]), excluded = Address(s.r[6]);
    auto before = std::bit_cast<double>(s.fpr_bits[1]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 24; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 160;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    m.WriteU32(sp + 80, 0x8204a1d8);
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto hp = std::bit_cast<float>(m.ReadU32(resource + 2588));
    s.fpr_bits[0] = std::bit_cast<std::uint64_t>(double(hp));
    auto removable = [&](unsigned id) {
      auto p = 0x83213438 + 8 * id, mask = m.ReadU32(p + 4);
      return !(excluded & mask) &&
             (m.ReadU32(resource + 272 * m.ReadU32(p) + 232) & mask);
    };
    auto remove = [&](unsigned id) {
      s.r[3] = resource;
      s.r[4] = id;
      s.r[5] = 1;
      Call(0x82ac8ee8, m, d, s);
    };
    bool wake = false;
    if (before > hp) {
      if (removable(11)) {
        s.r[3] = m.ReadU32(0x83264558);
        s.r[4] = 50;
        s.r[5] = 50;
        s.r[6] = m.ReadU32(resource + 64);
        Call(0x82aa0838, m, d, s);
        if (Address(s.r[3]) & 255)
          remove(11);
      }
      if (removable(3)) {
        remove(3);
        wake = true;
      }
    }
    if (element == 1) {
      remove(2);
      remove(16);
    }
    if (wake) {
      m.WriteU32(resource + 92, 0);
      m.WriteU32(resource + 88, 0);
      m.WriteU32(resource + 60, 6);
      m.WriteU32(resource + 100, m.ReadU32(resource + 100) & 0x7fffffffu);
    }
    m.WriteU32(sp + 80, 0x8204a1d8);
    s.r[1] += 160;
    for (unsigned i = 24; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82b2b9e0 || e == 0x82b2b720 || e == 0x82b2b7f8 ||
      e == 0x82b2b910) {
    auto old = Address(s.r[1]), owner = Address(s.r[3]),
         resource = Address(s.r[4]), mode = Address(s.r[6]);
    bool dispatcher = e == 0x82b2b9e0, divide = e == 0x82b2b720,
         quarter = e == 0x82b2b7f8;
    unsigned frame = dispatcher ? 96 : 128, first = dispatcher ? 32
                                                    : divide   ? 29
                                                               : 30;
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    if (!dispatcher && !divide)
      recovery_abi::WriteU64(m, old - 32, s.fpr_bits[31]);
    s.r[1] -= frame;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto f = [&](unsigned reg, double v) {
      s.fpr_bits[reg] = std::bit_cast<std::uint64_t>(v);
    };
    auto load = [&](unsigned p, unsigned reg) {
      auto v = std::bit_cast<float>(m.ReadU32(p));
      f(reg, v);
      return v;
    };
    auto put = [&](unsigned p, float v) {
      m.WriteU32(p, std::bit_cast<unsigned>(v));
    };
    auto trunc = [&](float value, unsigned reg) {
      auto v = Trunc(value);
      s.fpr_bits[reg] = std::bit_cast<std::uint64_t>(std::int64_t(v));
      m.WriteU32(sp + 80, unsigned(v));
      return v;
    };
    auto integer = [&](std::int32_t value, unsigned off, unsigned reg) {
      recovery_abi::WriteU64(m, sp + off, std::uint64_t(std::int64_t(value)));
      f(reg, float(value));
      return float(value);
    };
    auto normalize = [&]() {
      s.r[3] = owner;
      s.r[4] = resource;
      s.r[5] = 0;
      Call(0x82b2b468, m, d, s);
    };
    auto count = [&](std::int32_t n) {
      s.r[3] = m.ReadU32(0x83291dc0);
      s.r[4] = 2;
      s.r[5] = unsigned(n < 0 ? 0 : n);
      Call(0x82ac34f8, m, d, s);
    };
    float input = float(std::bit_cast<double>(s.fpr_bits[1]));
    if (dispatcher) {
      auto adjusted = float(input + load(0x8201f9f0, 0));
      auto zero = load(0x82000e50, 1);
      auto value = integer(trunc(adjusted, 0), 80, 0);
      if (mode == 0 || mode == 1 || mode == 6 || mode == 7 || mode == 8) {
        f(1, value);
        if (mode == 0)
          s.r[6] = 1;
        Call(mode == 0   ? 0x82b2b640
             : mode == 1 ? 0x82b2b5e0
             : mode == 6 ? 0x82b2b7f8
             : mode == 7 ? 0x82b2b910
                         : 0x82b2b720,
             m, d, s);
      } else if (mode == 2 || mode == 3) {
        auto current = load(resource + 2616, 13);
        auto updated =
            mode == 2 ? float(current - value) : float(current + value);
        f(13, updated);
        put(resource + 2616, updated);
        if (mode == 2) {
          if (!(updated > zero))
            put(resource + 2616, zero);
        } else {
          auto max = load(resource + 2620, 12);
          if (updated > max)
            put(resource + 2616, max);
        }
        f(1, value);
      } else if (mode == 4 || mode == 5) {
        unsigned off = mode == 4 ? 2588 : 2616;
        put(owner + 28, load(resource + off, 13));
        auto floor = float(load(resource + off + 4, 13) / value);
        f(1, floor);
        if (load(resource + off, 0) < floor)
          put(resource + off, floor);
      }
    } else {
      put(owner + 28, load(resource + 2588, 0));
      if (divide) {
        auto next = trunc(float(load(resource + 2588, 0) / input), 0);
        auto hp = next > 0 ? integer(next, 80, 0) : load(0x82007784, 0);
        put(resource + 2588, hp);
        auto change =
            trunc(float(load(owner + 28, 0) - load(resource + 2588, 13)), 0);
        if (!(m.ReadU32(resource + 124) & 0x10000000u)) {
          if (change < 0)
            change = 0;
          count(change);
        }
        normalize();
        f(1, integer(change, 80, 0));
      } else {
        float cap;
        if (quarter) {
          cap = integer(
              trunc(float(load(resource + 2592, 13) * load(0x82000b3c, 0)), 0),
              88, 31);
        } else {
          cap = input;
          f(31, cap);
        }
        if (load(resource + 2588, 0) > cap)
          put(resource + 2588, cap);
        normalize();
        auto previous = load(owner + 28, quarter ? 0 : 13);
        auto difference = float(previous - cap);
        if (quarter) {
          auto change = trunc(difference, 13);
          if (change < 0)
            change = 0;
          if (!(m.ReadU32(resource + 124) & 0x10000000u))
            count(trunc(float(previous - load(resource + 2588, 13)), 0));
          f(1, integer(change, 88, 0));
        } else {
          auto zero = load(0x82000e50, 0);
          if (difference < zero)
            difference = zero;
          f(31, difference);
          if (!(m.ReadU32(resource + 124) & 0x10000000u))
            count(trunc(float(previous - load(resource + 2588, 0)), 0));
          f(1, std::bit_cast<double>(s.fpr_bits[31]));
        }
      }
    }
    s.r[1] += frame;
    if (!dispatcher && !divide)
      s.fpr_bits[31] = recovery_abi::ReadU64(m, old - 32);
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  unsigned frame, first;
  switch (e) {
  case 0x82ac9618:
  case 0x82b2b5e0:
    frame = 96;
    first = 32;
    break;
  case 0x82b2b468:
    frame = 144;
    first = 28;
    break;
  case 0x82b2b640:
    frame = 144;
    first = 27;
    break;
  case 0x82b2bd50:
    frame = 176;
    first = 27;
    break;
  default:
    return false;
  }
  auto owner = Address(s.r[3]), resource = Address(s.r[4]),
       mode = Address(s.r[5]), old = Address(s.r[1]);
  auto damageMode = Address(s.r[6]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  unsigned fpOffset = e == 0x82b2b5e0 ? 16 : e == 0x82b2b468 ? 48 : 56;
  if (e != 0x82ac9618)
    recovery_abi::WriteU64(m, old - fpOffset, s.fpr_bits[31]);
  if (e == 0x82b2bd50) {
    recovery_abi::WriteU64(m, old - 72, s.fpr_bits[29]);
    recovery_abi::WriteU64(m, old - 64, s.fpr_bits[30]);
  }
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  unsigned literal = e == 0x82b2bd50 ? 88 : e == 0x82b2b640 ? 84 : 80;
  if (first != 32)
    m.WriteU32(sp + literal, 0x8204a1d8);
  if (s.cached_fp_control & 0x8040) {
    s.cached_fp_control &= ~0x8040u;
    d.fp.SetHostFpControl(s.cached_fp_control);
  }
  auto f = [&](unsigned reg, double v) {
    s.fpr_bits[reg] = std::bit_cast<std::uint64_t>(v);
  };
  auto load = [&](unsigned p, unsigned reg) {
    auto x = std::bit_cast<float>(m.ReadU32(p));
    f(reg, x);
    return x;
  };
  auto put = [&](unsigned p, float x) {
    m.WriteU32(p, std::bit_cast<unsigned>(x));
  };
  auto amount = float(std::bit_cast<double>(s.fpr_bits[1]));
  auto property = [&](unsigned id) {
    s.r[3] = resource;
    s.r[4] = id;
    Call(0x8238e368, m, d, s);
    return Address(s.r[3]) & 255;
  };
  auto remove = [&](unsigned id) {
    s.r[3] = resource;
    s.r[4] = id;
    Call(0x82ac9000, m, d, s);
  };
  if (e == 0x82ac9618) {
    auto output = mode, absorbed = damageMode;
    put(output, amount);
    auto zero = load(0x82000e50, 13);
    put(absorbed, zero);
    auto mask = m.ReadU32(0x8321343c), bank = m.ReadU32(0x83213438);
    if (m.ReadU32(owner + 272 * bank + 2136) & mask) {
      auto index = [](unsigned bits) {
        unsigned i = 0;
        while (i < 31 && !(bits & (1u << i)))
          ++i;
        return i;
      };
      auto raw = std::int32_t(m.ReadU32(owner + 4 * (535 + index(mask))));
      recovery_abi::WriteU64(m, sp + 80, std::uint64_t(std::int64_t(raw)));
      float capacity = float(raw);
      f(0, capacity);
      float remaining = float(capacity - amount);
      f(12, remaining);
      auto slot = owner + 4 * (535 + index(m.ReadU32(0x8321343c)));
      if (!(remaining > zero)) {
        f(13, float(amount - capacity));
        put(output, float(amount - capacity));
        put(absorbed, capacity);
        m.WriteU32(slot, 0);
        s.r[3] = owner;
        s.r[4] = 224;
        s.r[5] = 1;
        Call(0x82ac8ee8, m, d, s);
      } else {
        put(output, zero);
        put(absorbed, amount);
        auto v = Trunc(remaining);
        s.fpr_bits[0] = std::bit_cast<std::uint64_t>(std::int64_t(v));
        m.WriteU32(slot, unsigned(v));
      }
    }
  } else if (e == 0x82b2b468) {
    auto hp = load(resource + 2588, 0), one = load(0x82007784, 31);
    if (hp < one) {
      if (m.ReadU32(resource + 68) == 279)
        put(resource + 2588, one);
      else if (property(135) == 1) {
        put(resource + 2588, one);
        remove(135);
      } else {
        put(resource + 2588, load(0x82000e50, 0));
        if (!property(0)) {
          s.r[3] = resource;
          s.r[4] = 0;
          s.r[5] = 1;
          s.r[6] = mode;
          Call(0x82ac9ee8, m, d, s);
        }
      }
    } else {
      if (property(0) == 1)
        remove(0);
      auto threshold = float(load(resource + 2592, 13) * load(0x82000b3c, 0));
      f(0, threshold);
      if (!(threshold < load(resource + 2588, 13))) {
        if (!property(1)) {
          s.r[3] = resource;
          s.r[4] = 1;
          s.r[5] = 1;
          s.r[6] = 0;
          Call(0x82ac9be0, m, d, s);
        }
      } else if (property(1) == 1)
        remove(1);
    }
  } else if (e == 0x82b2b5e0 || e == 0x82b2b640) {
    f(31, amount);
    put(owner + 28, load(resource + 2588, 0));
    if (e == 0x82b2b5e0) {
      auto value = float(amount + load(resource + 2588, 0)),
           limit = load(resource + 2592, 13);
      f(0, value);
      put(resource + 2588, value);
      if (value > limit)
        put(resource + 2588, limit);
      s.r[3] = owner;
      s.r[4] = resource;
      s.r[5] = 0;
      Call(0x82b2b468, m, d, s);
    } else if (!(m.ReadU32(resource + 76348) & 0x80000000u)) {
      auto value = float(load(resource + 2588, 0) - amount);
      f(0, value);
      put(resource + 2588, value);
      if (!(m.ReadU32(resource + 124) & 0x10000000u)) {
        auto value = Trunc(amount);
        s.fpr_bits[0] = std::bit_cast<std::uint64_t>(std::int64_t(value));
        m.WriteU32(sp + 80, unsigned(value));
        s.r[3] = m.ReadU32(0x83291dc0);
        s.r[4] = 2;
        s.r[5] = unsigned(value);
        Call(0x82ac34f8, m, d, s);
      }
      s.r[3] = owner;
      s.r[4] = resource;
      s.r[5] = damageMode;
      Call(0x82b2b468, m, d, s);
    }
    f(1, std::bit_cast<double>(s.fpr_bits[31]));
  } else {
    auto zero = load(0x82000e50, 31);
    f(29, zero);
    auto row = [&]() {
      return m.ReadU32(owner + 20) + 464 * m.ReadU32(owner + 12);
    };
    auto find = [&](unsigned id) {
      Call(0x82380a18, m, d, s);
      Call(0x82389b78, m, d, s);
      s.r[4] = id;
      Call(0x8238e308, m, d, s);
      return Address(s.r[3]);
    };
    auto source = find(m.ReadU32(row() + 36));
    auto target = find(m.ReadU32(row() + 14884));
    if (!m.ReadU8(owner + 16)) {
      auto sentinel = load(0x82000e40, 30);
      auto at = [&](unsigned off) {
        return m.ReadU32(owner + 20) +
               4 * (116 * m.ReadU32(owner + 12) + m.ReadU32(owner + 24) + off);
      };
      auto damage = [&](unsigned who, unsigned off, unsigned absorbedOff,
                        bool accumulate, bool first, bool result) {
        auto value = load(at(off), 0);
        if (value == sentinel)
          return;
        s.r[3] = who;
        f(1, value);
        s.r[5] = sp + (first ? 84 : 80);
        s.r[6] = sp + (first ? 80 : 84);
        Call(0x82ac9618, m, d, s);
        auto absorbed = load(sp + (first ? 80 : 84), 0);
        if (absorbed != zero) {
          if (accumulate) {
            auto previous = load(at(absorbedOff), 13);
            if (previous != sentinel)
              absorbed = float(previous + absorbed);
          }
          put(at(absorbedOff), absorbed);
        }
        put(at(off), load(sp + (first ? 84 : 80), 0));
        auto actual = load(at(off), result ? 29 : 1);
        f(1, actual);
        s.r[3] = owner;
        s.r[4] = who;
        s.r[6] = 1;
        Call(0x82b2b640, m, d, s);
      };
      auto heal = [&](unsigned who, unsigned off) {
        auto value = load(at(off), 1);
        if (value != sentinel) {
          s.r[3] = owner;
          s.r[4] = who;
          Call(0x82b2b5e0, m, d, s);
        }
      };
      auto mp = [&](unsigned who, unsigned off, bool add) {
        auto value = load(at(off), 0);
        if (value == sentinel)
          return;
        auto before = load(who + 2616, add ? 12 : 13);
        value = add ? float(before + value) : float(before - value);
        f(0, value);
        put(who + 2616, value);
        if (add) {
          auto max = load(who + 2620, 13);
          if (value > max)
            put(who + 2616, max);
        } else if (!(value > zero))
          put(who + 2616, zero);
      };
      damage(source, 14, 30, false, true, false);
      mp(source, 22, false);
      heal(source, 18);
      mp(source, 26, true);
      damage(source, 34, 30, true, false, false);
      heal(source, 38);
      damage(target, 3726, 3742, false, false, true);
      mp(target, 3734, false);
      heal(target, 3730);
      mp(target, 3738, true);
    }
    f(1, std::bit_cast<double>(s.fpr_bits[29]));
  }
  if (first != 32)
    m.WriteU32(sp + literal, 0x8204a1d8);
  s.r[1] += frame;
  if (e != 0x82ac9618)
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - fpOffset);
  if (e == 0x82b2bd50) {
    s.fpr_bits[29] = recovery_abi::ReadU64(m, old - 72);
    s.fpr_bits[30] = recovery_abi::ReadU64(m, old - 64);
  }
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_result_application61
