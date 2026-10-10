#include "lo_semantics/battle_action_adjustments61.h"
#include "lo_semantics/battle_action_parameters61.h"
#include "lo_semantics/battle_action_snapshot61.h"
#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/battle_random_range61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <limits>
namespace lo::semantic::gpu::battle_action_parameters61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_action_adjustments61::Apply(e, m, d, s) &&
      !battle_action_snapshot61::Apply(e, m, d, s) &&
      !battle_action_readiness61::Apply(e, m, d, s) &&
      !battle_random_range61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
bool Property(unsigned resource, unsigned id, GuestMemory &m, Dependencies d,
              Registers &s) {
  s.r[3] = resource;
  s.r[4] = id;
  Call(0x8238e368, m, d, s);
  return (Address(s.r[3]) & 255) == 1;
}
struct Parameters {
  unsigned group, value, extra;
};
Parameters Complex(unsigned resource, unsigned kind, unsigned detail,
                   std::uint64_t variant, GuestMemory &m, Dependencies d,
                   Registers &s) {
  auto sp = Address(s.r[1]);
  m.WriteU32(sp + 80, 0);
  m.WriteU32(sp + 84, 0);
  m.WriteU32(sp + 88, 0);
  s.r[3] = m.ReadU32(0x83264558);
  s.r[4] = 150;
  s.r[5] = 300;
  s.r[6] = 110;
  s.r[7] = m.ReadU32(resource + 64);
  Call(0x82aa0740, m, d, s);
  auto random = Address(s.r[3]);
  unsigned increment = 0, value = 0;
  bool add = true, skill = false, item = false, special = false, check99 = true;
  if (kind == 2 || (kind >= 6 && kind <= 9)) {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto f = std::bit_cast<float>(m.ReadU32(resource + 2640)),
         bias = std::bit_cast<float>(m.ReadU32(0x82000a98));
    s.fpr_bits[13] = std::bit_cast<std::uint64_t>(double(f));
    auto sum = float(f + bias);
    auto n = sum >= 2147483648.f ? std::numeric_limits<std::int32_t>::max()
             : !(sum >= -2147483648.f)
                 ? std::numeric_limits<std::int32_t>::min()
                 : std::int32_t(sum);
    s.fpr_bits[0] = std::bit_cast<std::uint64_t>(std::int64_t(n));
    m.WriteU32(sp + 88, unsigned(n));
    auto record = m.ReadU32(0x83264984) + 96 * detail;
    m.WriteU32(sp + 84, m.ReadU32(record + 20));
    value = m.ReadU32(record + 24);
    m.WriteU32(sp + 80, value);
    skill = true;
    if (m.ReadU32(resource + 68) == 10) {
      value = 1;
      m.WriteU32(sp + 80, value);
      skill = false;
    }
    increment = m.ReadU32(sp + 88);
    if (m.ReadU32(resource + 4944)) {
      value = 1;
      m.WriteU32(sp + 80, value);
      skill = false;
      check99 = false;
    }
  } else if (kind == 3) {
    auto record = m.ReadU32(0x832649c0) + 104 * detail;
    increment = 9;
    if (detail == 1) {
      s.r[3] = m.ReadU32(0x83264558);
      s.r[4] = 1;
      s.r[5] = 9;
      s.r[6] = 2;
      s.r[7] = m.ReadU32(resource + 64);
      Call(0x82aa0740, m, d, s);
      increment = Address(s.r[3]);
    }
    m.WriteU32(sp + 84, m.ReadU32(record + 28));
    value = m.ReadU32(record + 32);
    m.WriteU32(sp + 80, value);
    item = true;
  } else if (kind == 10) {
    auto record = m.ReadU32(m.ReadU32(0x832ca0d0) + 132) + 68 * detail;
    increment = 9;
    m.WriteU32(sp + 84, m.ReadU32(record + 8));
    value = m.ReadU32(record + 12);
    m.WriteU32(sp + 80, value);
    if (value == 99) {
      add = false;
      value = 1;
      m.WriteU32(sp + 80, value);
      check99 = false;
    }
  } else if (kind == 11) {
    increment = 1;
    random = 75;
    special = true;
    check99 = false;
  }
  if (check99 && value == 99) {
    increment = 0;
    random = 0;
    m.WriteU32(sp + 84, 0);
    value = 0;
    m.WriteU32(sp + 80, 0);
  }
  if ((Address(variant) & 255) == 1) {
    m.WriteU32(sp + 84, 0);
    increment = 0;
    value = m.ReadU32(m.ReadU32(0x8324570c) + 16);
    random = 0xffffffff;
    m.WriteU32(sp + 80, value);
  }
  if (add) {
    value += increment;
    m.WriteU32(sp + 80, value);
  }
  if (std::int32_t(value) >= 25) {
    auto groups = std::int32_t(value) / 25;
    m.WriteU32(sp + 84, unsigned(groups));
    value -= unsigned(groups) * 25;
    m.WriteU32(sp + 80, value);
  }
  for (unsigned e : {skill ? 0x82acd770u : 0u, item ? 0x82acdb50u : 0u,
                     special ? 0x82acdda0u : 0u})
    if (e) {
      s.r[3] = m.ReadU32(0x8324570c);
      s.r[4] = resource;
      s.r[5] = sp + 84;
      s.r[6] = sp + 80;
      s.r[7] = 0;
      if (e != 0x82acdda0)
        s.r[8] = variant;
      Call(e, m, d, s);
    }
  return {m.ReadU32(sp + 84), m.ReadU32(sp + 80), random};
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned first, frame;
  if (e == 0x82b21340) {
    first = 27;
    frame = 144;
  } else if (e == 0x82b1f798) {
    first = 30;
    frame = 112;
  } else if (e == 0x82b11df0) {
    first = 20;
    frame = 208;
  } else
    return false;
  auto variantFull = s.r[7];
  auto detail = Address(s.r[6]);
  auto resource = Address(s.r[4]), kind = Address(s.r[5]),
       variant = Address(s.r[7]) & 255, old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  unsigned value = 1, extra = 0, group = 0;
  if (e == 0x82b11df0) {
    auto p = Complex(resource, kind, detail, variantFull, m, d, s);
    group = p.group;
    value = p.value;
    extra = p.extra;
  } else if (e == 0x82b21340) {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto f = std::bit_cast<float>(m.ReadU32(resource + 2612));
    auto base = f >= 2147483648.f ? std::numeric_limits<std::int32_t>::max()
                : !(f >= -2147483648.f)
                    ? std::numeric_limits<std::int32_t>::min()
                    : std::int32_t(f);
    s.fpr_bits[0] = std::bit_cast<std::uint64_t>(std::int64_t(base));
    m.WriteU32(sp + 80, unsigned(base));
    s.r[3] = m.ReadU32(0x83264558);
    s.r[4] = 150;
    s.r[5] = 300;
    s.r[6] = 109;
    s.r[7] = m.ReadU32(resource + 64);
    Call(0x82aa0740, m, d, s);
    extra = Address(s.r[3]);
    m.WriteU32(sp + 84, 0x8204a1d8);
    if (variant == 1) {
      value = m.ReadU32(m.ReadU32(0x8324570c) + 16);
      extra = 0xffffffff;
    } else if (Property(resource, 2, m, d, s)) {
      value = m.ReadU32(sp + 80) * 2;
      if (std::int32_t(value) > 24)
        value = 24;
    } else if (Property(resource, 161, m, d, s)) {
      auto half = std::int32_t(m.ReadU32(sp + 80)) / 2;
      value = half < 1 ? 1 : unsigned(half);
    } else if (Property(resource, 193, m, d, s)) {
      value = m.ReadU32(sp + 80) * 2;
      if (std::int32_t(value) > 24)
        value = 24;
    } else
      value = m.ReadU32(sp + 80);
  } else if (variant == 1) {
    value = m.ReadU32(m.ReadU32(0x8324570c) + 16);
    extra = 0xffffffff;
  } else if (kind == 5) {
    s.r[3] = m.ReadU32(0x83264558);
    s.r[4] = 1;
    s.r[5] = 9;
    s.r[6] = 2;
    s.r[7] = m.ReadU32(resource + 64);
    Call(0x82aa0740, m, d, s);
    value = Address(s.r[3]);
  }
  s.r[3] = m.ReadU32(0x8324570c);
  s.r[4] = group;
  s.r[5] = value;
  s.r[6] = extra;
  Call(0x82acd3b0, m, d, s);
  if (e == 0x82b21340)
    m.WriteU32(sp + 84, 0x8204a1d8);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_action_parameters61
