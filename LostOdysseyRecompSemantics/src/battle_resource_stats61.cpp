#include "lo_semantics/battle_resource_stats61.h"
#include "lo_semantics/battle_property_mutation61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::battle_resource_stats61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_resource_stats61::Apply(e, m, d, s) &&
      !battle_property_mutation61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82ac1860 && e != 0x82ac0620 && e != 0x82ac3058)
    return false;
  auto owner = Address(s.r[3]), resource = Address(s.r[4]),
       mode = Address(s.r[5]), old = Address(s.r[1]);
  unsigned frame = e == 0x82ac1860 ? 144 : 112,
           first = e == 0x82ac1860 ? 26 : 30;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  if (e == 0x82ac1860) {
    m.WriteU32(sp + 80, 0x8204a1d8);
    for (unsigned offset = 0; offset < 26624; offset += 104) {
      auto row = m.ReadU32(0x83264978 + 72) + offset;
      if (m.ReadU32(row + 44) != 1)
        continue;
      s.r[3] = resource;
      s.r[4] = m.ReadU32(row + 48);
      s.r[5] = m.ReadU32(row + 52);
      Call(0x82ac91d8, m, d, s);
      if (m.ReadU32(row + 60)) {
        s.r[3] = resource;
        s.r[4] = m.ReadU32(row + 56);
        s.r[5] = m.ReadU32(row + 60);
        Call(0x82ac91d8, m, d, s);
      }
    }
    m.WriteU32(sp + 80, 0x8204a1d8);
  } else if (e == 0x82ac3058) {
    Call(0x82380a18, m, d, s);
    Call(0x82ab0110, m, d, s);
    for (unsigned i = 0; i < 60; i += 4)
      m.WriteU32(resource + 2528 + i, 0);
    s.r[3] = owner;
    s.r[4] = resource;
    Call(0x82ac1860, m, d, s);
    m.WriteU32(m.ReadU32(0x83291dc0) + 4, m.ReadU32(resource + 5108));
    for (unsigned i = 0; i < 5; ++i)
      m.WriteU32(m.ReadU32(0x83291dc0) + 12 + 4 * i,
                 m.ReadU32(resource + 5116 + 4 * i));
    for (auto method : {0x82ac0888u, 0x82ac2468u, 0x82ac0620u}) {
      s.r[3] = owner;
      s.r[4] = resource;
      if (method == 0x82ac0620)
        s.r[5] = (m.ReadU32(resource + 124) >> 28) & 1;
      Call(method, m, d, s);
    }
  } else {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto f = [&](unsigned reg, double value) {
      s.fpr_bits[reg] = std::bit_cast<std::uint64_t>(value);
    };
    auto load = [&](unsigned p, unsigned reg) {
      auto value = std::bit_cast<float>(m.ReadU32(p));
      f(reg, value);
      return value;
    };
    auto put = [&](unsigned off, float value) {
      m.WriteU32(resource + off, std::bit_cast<unsigned>(value));
    };
    s.r[3] = resource;
    auto one = load(0x82007784, 11);
    auto hp =
        float(float(load(resource + 4920, 0) + load(resource + 2532, 13)) +
              load(resource + 2472, 13));
    f(0, hp);
    put(2592, hp);
    if (hp < one)
      put(2592, one);
    if (mode) {
      auto limit = load(0x822184dc, 0);
      if (load(resource + 2592, 13) > limit)
        put(2592, limit);
    }
    auto maxHP = load(resource + 2592, 12);
    if (load(resource + 2588, 0) > maxHP)
      put(2588, maxHP);
    auto mp =
             float(float(load(resource + 4924, 0) + load(resource + 2560, 13)) +
                   load(resource + 2500, 13)),
         zero = load(0x82000e50, 13);
    f(0, mp);
    put(2620, mp);
    if (mp < zero)
      put(2620, zero);
    if (mode) {
      auto limit = load(0x822181e4, 0);
      if (load(resource + 2620, 13) > limit)
        put(2620, limit);
    }
    auto maxMP = load(resource + 2620, 0);
    if (load(resource + 2616, 13) > maxMP)
      put(2616, maxMP);
    if (!mode) {
      put(2588, maxHP);
      put(2616, maxMP);
    }
    for (unsigned i = 0; i < 4; ++i) {
      auto value = float(load(resource + 2536 + 4 * i, 13 - i) +
                         load(resource + 2476 + 4 * i, 12 - i));
      put(2596 + 4 * i, value);
    }
    auto speed = float(load(resource + 2552, 0) + load(resource + 2492, 13)),
         cap = load(0x822182a0, 13);
    f(0, speed);
    put(2612, speed);
    if (speed > cap)
      put(2612, cap);
    if (load(resource + 2612, 0) < one)
      put(2612, one);
    for (unsigned i = 0; i < 4; ++i) {
      auto value = float(load(resource + 2564 + 4 * i, 12 - i) +
                         load(resource + 2504 + 4 * i, 10 - i));
      put(2624 + 4 * i, value);
    }
    put(2644, float(load(resource + 2584, 6) + load(resource + 2524, 7)));
    auto other = float(load(resource + 2580, 0) + load(resource + 2520, 12));
    f(0, other);
    put(2640, other);
    if (other > cap)
      put(2640, cap);
    if (load(resource + 2640, 0) < one)
      put(2640, one);
    m.WriteU32(sp + 80, 0x8204a1d8);
    auto threshold = float(load(resource + 2592, 13) * load(0x82000b3c, 0));
    f(0, threshold);
    s.r[3] = resource;
    s.r[4] = 1;
    if (threshold < load(resource + 2588, 13))
      Call(0x82ac9000, m, d, s);
    else {
      s.r[5] = 1;
      s.r[6] = 0;
      Call(0x82ac9be0, m, d, s);
    }
    m.WriteU32(sp + 80, 0x8204a1d8);
  }
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_resource_stats61
