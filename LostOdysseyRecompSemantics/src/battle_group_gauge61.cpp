#include "lo_semantics/battle_group_gauge61.h"
#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/battle_evaluation_theft61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <limits>
namespace lo::semantic::gpu::battle_group_gauge61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_group_gauge61::Apply(e, m, d, s) &&
      !battle_action_readiness61::Apply(e, m, d, s) &&
      !battle_evaluation_theft61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned frame = 0, first = 32;
  switch (e) {
  case 0x82ac7178:
    break;
  case 0x82ac7000:
    frame = 144;
    first = 27;
    break;
  case 0x82ac71e8:
    frame = 96;
    break;
  case 0x82ac80b8:
    frame = 144;
    first = 26;
    break;
  case 0x82ac6e60:
    break;
  case 0x82ac7550:
  case 0x82ac6f08:
    frame = 112;
    first = 30;
    break;
  case 0x82ac7b08:
    frame = 240;
    first = 18;
    break;
  case 0x82ac7fc8:
    frame = 160;
    first = 25;
    break;
  case 0x82ac8448:
  case 0x82afd218:
    frame = 96;
    first = 31;
    break;
  default:
    return false;
  }
  auto old = Address(s.r[1]), owner = Address(s.r[3]), mode = Address(s.r[4]);
  auto argument = Address(s.r[5]);
  bool initializeCurrent = (Address(s.r[5]) & 255) == 1;
  if (frame) {
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= frame;
    m.WriteU32(Address(s.r[1]), old);
  }
  auto get = [&](unsigned p) { return std::bit_cast<float>(m.ReadU32(p)); };
  auto put = [&](unsigned p, float value) {
    m.WriteU32(p, std::bit_cast<unsigned>(value));
  };
  auto fp = [&]() {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
  };
  auto trunc = [](float value) {
    return value > double(std::numeric_limits<std::int32_t>::max())
               ? std::numeric_limits<std::int32_t>::max()
           : !(value >= -2147483648.) ? std::numeric_limits<std::int32_t>::min()
                                      : std::int32_t(value);
  };
  auto row = owner + 24 * mode;
  if (e == 0x82ac7178) {
    auto resource = mode, flags = m.ReadU32(resource + 124),
         side = (flags >> 28) & 1;
    if (m.ReadU8(owner + 24 * (side + 1)) && (flags & 0x40000000)) {
      auto value = m.ReadU32(resource + 188) + argument;
      m.WriteU32(resource + 188, value);
      recovery_abi::WriteU64(m, old - 16,
                             std::uint64_t(std::int64_t(std::int32_t(value))));
      fp();
      auto hp = get(resource + 2592);
      if (float(std::int32_t(value)) > hp)
        m.WriteU32(resource + 188, unsigned(trunc(hp)));
    }
  } else if (e == 0x82ac7000) {
    m.WriteU32(Address(s.r[1]) + 80, 0x8204a1d8);
    auto resource = mode;
    if (m.ReadU8(owner + 24 * (((m.ReadU32(resource + 124) >> 28) & 1) + 1))) {
      auto property = [&](unsigned id) {
        s.r[3] = resource;
        s.r[4] = id;
        Call(0x8238e368, m, d, s);
        return (Address(s.r[3]) & 255) != 0;
      };
      unsigned amount = argument;
      if (property(227))
        amount = unsigned(std::int32_t(amount * 150u) / 100);
      if (property(248) && amount) {
        auto half = std::int32_t(amount) / 2;
        amount = half > 0 ? unsigned(half) : 1u;
      }
      auto flags = m.ReadU32(resource + 124);
      if (flags & 0x40000000) {
        auto current = m.ReadU32(resource + 188), remaining = current - amount;
        if (std::int32_t(remaining) < 0) {
          remaining = 0;
          amount = current;
        }
        m.WriteU32(resource + 188, remaining);
        recovery_abi::WriteU64(
            m, Address(s.r[1]) + 88,
            std::uint64_t(std::int64_t(std::int32_t(amount))));
        fp();
        auto group = owner + 24 * ((flags >> 28) & 1);
        auto next = float(get(group + 4) - float(std::int32_t(amount)));
        put(group + 4, next);
        auto zero = get(0x82000e50);
        if (next < zero) {
          group = owner + 24 * ((m.ReadU32(resource + 124) >> 28) & 1);
          put(group + 4, zero);
        }
      }
    }
    m.WriteU32(Address(s.r[1]) + 80, 0x8204a1d8);
  } else if (e == 0x82ac71e8) {
    recovery_abi::WriteU64(m, Address(s.r[1]) + 80,
                           std::uint64_t(std::int64_t(std::int32_t(argument))));
    fp();
    auto amount = unsigned(
        trunc(float(get(mode + 2592) / float(std::int32_t(argument)))));
    m.WriteU32(Address(s.r[1]) + 80, amount);
    s.r[5] = amount;
    Call(0x82ac7000, m, d, s);
  } else if (e == 0x82ac80b8) {
    auto resource = mode;
    fp();
    auto maximum =
        trunc(get(owner + 24 * ((m.ReadU32(resource + 124) >> 28) & 1) + 16));
    m.WriteU32(Address(s.r[1]) + 80, unsigned(maximum));
    auto total = unsigned(maximum / 4) * argument;
    unsigned count = 0;
    auto visit = [&](auto action) {
      Call(0x82380a18, m, d, s);
      Call(0x8238e2f8, m, d, s);
      auto list = Address(s.r[3]);
      for (unsigned i = 0;; ++i) {
        Call(0x82380a18, m, d, s);
        Call(0x8238e2f8, m, d, s);
        if (std::int32_t(i) >= std::int32_t(m.ReadU32(Address(s.r[3]) + 4)))
          break;
        auto candidate = m.ReadU32(m.ReadU32(list) + 4 * i);
        if ((m.ReadU32(candidate + 124) ^ m.ReadU32(resource + 124)) &
            0x10000000)
          continue;
        s.r[3] = owner;
        s.r[4] = candidate;
        Call(0x82ac7550, m, d, s);
        if ((Address(s.r[3]) & 255) == 1)
          action();
      }
    };
    visit([&]() { ++count; });
    if (count) {
      auto amount = unsigned(std::int64_t(std::int32_t(total)) /
                             std::int64_t(std::int32_t(count)));
      visit([&]() { // The source reapplies each share to the original resource,
                    // not the iterated peer.
        s.r[3] = owner;
        s.r[4] = resource;
        s.r[5] = amount;
        Call(0x82ac7000, m, d, s);
      });
    }
  } else if (e == 0x82ac7b08) {
    unsigned maximum = 0, current = 0;
    bool hasEligible = false, hasOrdinary = false;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp + 80, 0);
    m.WriteU32(sp + 84, 0);
    m.WriteU32(row + 20, 0);
    Call(0x82380a18, m, d, s);
    Call(0x8238e2f8, m, d, s);
    auto list = Address(s.r[3]);
    for (unsigned i = 0;; ++i) {
      Call(0x82380a18, m, d, s);
      Call(0x8238e2f8, m, d, s);
      if (std::int32_t(i) >= std::int32_t(m.ReadU32(Address(s.r[3]) + 4)))
        break;
      auto resource = m.ReadU32(m.ReadU32(list) + 4 * i);
      fp();
      m.WriteU32(resource + 192, unsigned(trunc(get(resource + 2592))));
      if (initializeCurrent)
        m.WriteU32(resource + 188, unsigned(trunc(get(resource + 2588))));
      auto flags = m.ReadU32(resource + 124);
      if (mode == ((flags & 0x10000000) ? 0u : 1u))
        continue;
      if (!(flags & 0x40000000)) {
        hasOrdinary = true;
        continue;
      }
      s.r[3] = owner;
      s.r[4] = resource;
      Call(0x82ac7550, m, d, s);
      if ((Address(s.r[3]) & 255) != 1)
        continue;
      fp();
      float hp = get(resource + 2592);
      maximum = unsigned(trunc(float(float(std::int32_t(maximum)) + hp)));
      if (initializeCurrent)
        current = unsigned(trunc(float(float(std::int32_t(current)) + hp)));
      else
        current += m.ReadU32(resource + 188);
      m.WriteU32(sp + 80, current);
      m.WriteU32(sp + 84, maximum);
      m.WriteU32(row + 20, m.ReadU32(row + 20) + 1);
      hasEligible = true;
    }
    fp();
    recovery_abi::WriteU64(m, sp + 104,
                           std::uint64_t(std::int64_t(std::int32_t(maximum))));
    recovery_abi::WriteU64(m, sp + 96,
                           std::uint64_t(std::int64_t(std::int32_t(current))));
    put(row + 16, float(std::int32_t(maximum)));
    put(row + 4, float(std::int32_t(current)));
    m.WriteU8(row + 24, 1);
    if (!(hasEligible && hasOrdinary) && mode == 0)
      m.WriteU8(owner + 24, 0);
    Call(0x82380a18, m, d, s);
    s.r[4] = mode;
    Call(0x82a9b288, m, d, s);
    auto destination = Address(s.r[3]);
    fp();
    m.WriteU32(destination + 12632, unsigned(trunc(get(row + 4))));
    m.WriteU32(destination + 12652, m.ReadU32(row + 8));
    m.WriteU32(destination + 12636, m.ReadU32(row + 8));
    m.WriteU32(destination + 12640,
               unsigned(trunc(float(get(row + 12) * get(0x8201dd2c)))));
    m.WriteU32(destination + 12644,
               (m.ReadU32(destination + 12644) & 0x7fffffff) |
                   (unsigned(m.ReadU8(row + 24)) << 31));
  } else if (e == 0x82ac7550) {

    auto resource = mode;
    bool valid = !(m.ReadU32(0x832cb778) == 242 && !m.ReadU32(resource + 64)) &&
                 (m.ReadU32(resource + 124) & 0x40000000);
    if (valid) {
      for (auto off : {292u, 380u, 384u}) {
        s.r[3] = resource;
        s.ctr = m.ReadU32(m.ReadU32(resource) + off);
        d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        if (Address(s.r[3])) {
          valid = false;
          break;
        }
      }
      if (valid)
        valid = !(m.ReadU32(resource + 124) & 0x650000);
    }
    s.r[3] = valid;
  } else if (e == 0x82ac6e60) {
    if (!m.ReadU8(row + 24)) {
      m.WriteU32(row + 12, m.ReadU32(0x82007784));
      m.WriteU32(row + 8, 3);
    } else {
      fp();
      float ratio = get(row + 4) / get(row + 16);
      unsigned rank = !(ratio < get(0x8200104c))   ? 3
                      : !(ratio < get(0x8201f9f0)) ? 2
                      : !(ratio < get(0x82000da4)) ? 1
                                                   : 0;
      put(row + 12, ratio);
      m.WriteU32(row + 8, rank);
    }
  } else if (e == 0x82ac7fc8) {
    if (m.ReadU8(row + 24)) {
      Call(0x82380a18, m, d, s);
      Call(0x8238e2f8, m, d, s);
      auto list = Address(s.r[3]);
      unsigned total = 0;
      for (unsigned i = 0;; ++i) {
        Call(0x82380a18, m, d, s);
        Call(0x8238e2f8, m, d, s);
        if (std::int32_t(i) >= std::int32_t(m.ReadU32(Address(s.r[3]) + 4)))
          break;
        auto resource = m.ReadU32(m.ReadU32(list) + 4 * i);
        bool party = (m.ReadU32(resource + 124) & 0x10000000) != 0;
        if (mode == (party ? 0u : 1u))
          continue;
        s.r[3] = owner;
        s.r[4] = resource;
        Call(0x82ac7550, m, d, s);
        if ((Address(s.r[3]) & 255) == 1)
          total += m.ReadU32(resource + 188);
      }
      fp();
      recovery_abi::WriteU64(m, Address(s.r[1]) + 80,
                             std::uint64_t(std::int64_t(std::int32_t(total))));
      float value = float(std::int32_t(total)), limit = get(row + 16);
      put(row + 4, value);
      if (value > limit)
        put(row + 4, limit);
    }
  } else if (e == 0x82ac6f08) {
    Call(0x82380a18, m, d, s);
    s.r[4] = mode;
    Call(0x82a9b288, m, d, s);
    auto destination = Address(s.r[3]);
    fp();
    m.WriteU32(destination + 12632, unsigned(trunc(get(row + 4))));
    m.WriteU32(destination + 12636, m.ReadU32(row + 8));
    m.WriteU32(destination + 12640,
               unsigned(trunc(float(get(row + 12) * get(0x8201dd2c)))));
    m.WriteU32(destination + 12648, unsigned(trunc(get(row + 16))));
  } else if (e == 0x82afd218) {
    Call(0x82380a18, m, d, s);
    s.r[4] = mode;
    Call(0x82a9b288, m, d, s);
    m.WriteU8(m.ReadU32(0x832aeb00) + 24 * (mode + 1), 0);
    auto destination = Address(s.r[3]);
    m.WriteU32(destination + 12644,
               m.ReadU32(destination + 12644) & 0x7fffffff);
  } else {
    for (auto target : {0x82ac7fc8u, 0x82ac6e60u, 0x82ac6f08u})
      for (unsigned i = 0; i < 2; ++i) {
        s.r[3] = m.ReadU32(0x832aeb00);
        s.r[4] = i;
        Call(target, m, d, s);
      }
  }
  if (frame) {
    s.r[1] += frame;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
  }
  return true;
}
} // namespace lo::semantic::gpu::battle_group_gauge61
