#include "lo_semantics/battle_resource_growth61.h"
#include "lo_semantics/battle_resource_stats61.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <limits>
#include <utility>
namespace lo::semantic::gpu::battle_resource_growth61 {
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  using recovery_abi::Address;
  if (e == 0x82abfe38) {
    auto resource = Address(s.r[4]);
    m.WriteU32(resource + 4940, 3);
    for (unsigned off :
         {4944u, 4948u, 4952u, 5088u, 5092u, 180u, 5100u, 5096u, 4956u})
      m.WriteU32(resource + off, 0);
    for (unsigned i = 0; i < 32; ++i)
      m.WriteU32(resource + 4960 + 4 * i, 99);
    return true;
  }
  if (e == 0x82abfe90) {
    auto resource = Address(s.r[4]),
         flags = m.ReadU32(resource + 124) & ~0x07200000u,
         group = m.ReadU32(resource + 68);
    m.WriteU32(resource + 132, 1);
    for (unsigned off : {196u, 204u, 212u, 216u, 96u, 88u, 92u})
      m.WriteU32(resource + off, 0);
    m.WriteU32(resource + 200, m.ReadU32(resource + 200) & 0x7fffffff);
    m.WriteU32(resource + 208, m.ReadU32(resource + 208) & 0x3fffffff);
    m.WriteU32(resource + 100, m.ReadU32(resource + 100) & 0x7fffffff);
    m.WriteU32(resource + 124,
               (group == 1 || group == 2 || group == 3 || group == 5)
                   ? (flags | 0x100000)
                   : (flags & ~0x100000u));
    return true;
  }
  if (e != 0x82ac0588 && e != 0x82ac25e8 && e != 0x82ac3820)
    return false;
  auto owner = Address(s.r[3]), resource = Address(s.r[4]),
       argument = Address(s.r[5]);
  constexpr unsigned destination[]{2412, 2440, 2416, 2420, 2428, 2424, 2432,
                                   2444, 2448, 2456, 2452, 2460, 2464};
  constexpr unsigned templateOffset[]{124, 152, 108, 112, 120, 116, 128,
                                      132, 140, 148, 144, 156, 136};
  constexpr unsigned creatureOffset[]{4,  8,  12, 16, 24, 20, 28,
                                      32, 36, 44, 40, 48, 52};
  if (e == 0x82ac0588) {
    auto row = m.ReadU32(0x83264978 + 36) + 204 * m.ReadU32(resource + 68);
    for (unsigned i = 0; i < 13; ++i)
      m.WriteU32(resource + destination[i], m.ReadU32(row + templateOffset[i]));
    m.WriteU32(resource + 152, m.ReadU32(row + 160));
    return true;
  }
  auto old = Address(s.r[1]);
  unsigned frame = e == 0x82ac3820 ? 144 : 112,
           first = e == 0x82ac3820 ? 27 : 30;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  if (s.cached_fp_control & 0x8040) {
    s.cached_fp_control &= ~0x8040u;
    d.fp.SetHostFpControl(s.cached_fp_control);
  }
  auto f = [&](unsigned p) { return std::bit_cast<float>(m.ReadU32(p)); };
  auto q = [&](unsigned p) {
    return std::bit_cast<double>(recovery_abi::ReadU64(m, p));
  };
  auto put = [&](unsigned p, float v) {
    m.WriteU32(p, std::bit_cast<unsigned>(v));
  };
  auto trunc = [](float v) {
    return v > double(std::numeric_limits<std::int32_t>::max())
               ? std::numeric_limits<std::int32_t>::max()
           : !(v >= -2147483648.) ? std::numeric_limits<std::int32_t>::min()
                                  : std::int32_t(v);
  };
  if (e == 0x82ac3820) {
    for (auto range : {std::pair{2408u, 60u}, {2528u, 60u}, {2588u, 2352u}})
      (void)FillGuestMemory(m, resource + range.first, 0, range.second);
    auto table = m.ReadU32(m.ReadU32(0x832ca0d0) + 120);
    auto row = table + 140 * m.ReadU32(resource + 68);
    m.WriteU32(resource + 140, argument ? argument : m.ReadU32(row));
    auto flags = m.ReadU32(row + 60);
    auto layer = [&](unsigned source, bool add) {
      for (unsigned i = 0; i < 13; ++i) {
        float value;
        if (i < 2) {
          auto integer = std::int32_t(m.ReadU32(source + creatureOffset[i]));
          recovery_abi::WriteU64(m, sp + 80,
                                 std::uint64_t(std::int64_t(integer)));
          value = float(integer);
        } else
          value = f(source + creatureOffset[i]);
        if (add)
          value = float(value + f(resource + destination[i]));
        put(resource + destination[i], value);
      }
    };
    if (flags & 0x8000)
      layer(table + 71540, false);
    else if (flags & 0x4000)
      layer(table + 71400, false);
    unsigned archetype = table + 69300;
    for (unsigned i = 0; i < 14; ++i)
      if (flags & (1u << i)) {
        archetype = table + 140 * (496 + i);
        break;
      }
    layer(archetype, true);
    layer(row, true);
    s.r[3] = owner;
    s.r[4] = resource;
    (void)battle_resource_growth61::Apply(0x82ac25e8, m, d, s);
    m.WriteU32(resource + 2588, m.ReadU32(resource + 2472));
    m.WriteU32(resource + 2616, m.ReadU32(resource + 2500));
    for (auto pair : {std::pair{56u, 4880u},
                      {60u, 4888u},
                      {64u, 4884u},
                      {68u, 4876u},
                      {100u, 4892u},
                      {96u, 4896u},
                      {104u, 4900u},
                      {108u, 4904u},
                      {112u, 4908u},
                      {116u, 4912u}})
      m.WriteU32(resource + pair.second, m.ReadU32(row + pair.first));
    m.WriteU32(owner + 4, m.ReadU32(row + 72));
    for (auto pair :
         {std::pair{80u, 5116u}, {84u, 5120u}, {88u, 5124u}, {76u, 5128u}})
      m.WriteU32(resource + pair.second, m.ReadU32(row + pair.first));
    for (unsigned i = 0; i < 5; ++i)
      m.WriteU32(owner + 12 + 4 * i, m.ReadU32(resource + 5116 + 4 * i));
    m.WriteU32(resource + 5156, 3);
    for (unsigned i = 0; i < 3; ++i)
      m.WriteU32(resource + 5160 + 4 * i, m.ReadU32(row + 120 + 4 * i));
  } else {
    auto level =
        std::int32_t(m.ReadU32(resource + 4952) + m.ReadU32(resource + 140));
    if (level >= 99)
      level = 99;
    auto row = m.ReadU32(0x83264978 + 36) + 204 * (unsigned(level) + 16);
    double center = q(0x820c00d0), unit = q(0x82000e90), slope = q(0x822183d8),
           scale = q(0x822181b8), mpSlope = q(0x82021080);
    float half = f(0x8201f9f0), zero = f(0x82000e50);
    auto curve = [&](unsigned attribute, unsigned tableOffset,
                     double coefficient, double multiplier) {
      double value =
          -((center - double(f(resource + attribute))) * coefficient - unit);
      value *= scale;
      value *= double(f(row + tableOffset));
      value *= multiplier;
      return float(value);
    };
    auto nonnegative = [&](float value) { return value > zero ? value : zero; };
    auto nearestTen = [](std::int32_t value) {
      return std::int32_t(unsigned(value) + 5) / 10 * 10;
    };
    auto hp = trunc(float(curve(2412, 124, slope, q(0x820c00c8)) + half));
    float hpValue = float(nearestTen(hp));
    if (!(hpValue > zero))
      hpValue = f(0x82007784);
    put(resource + 2472, hpValue);
    auto mp = trunc(curve(2440, 152, mpSlope, q(0x82001010)));
    if (mp % 5)
      mp = nearestTen(mp);
    put(resource + 2500, nonnegative(float(mp)));
    for (auto tuple : {std::pair{2416u, 108u}, {2420u, 112u}}) {
      auto v =
          trunc(float(curve(tuple.first, tuple.second, slope, slope) + half));
      recovery_abi::WriteU64(m, sp + 80, std::uint64_t(std::int64_t(v)));
      put(resource + (tuple.first == 2416 ? 2476 : 2480),
          nonnegative(float(v)));
    }
    double fraction = q(0x82218280);
    put(resource + 2484,
        nonnegative(curve(2424, 116, fraction, q(0x820c00c0))));
    put(resource + 2488, nonnegative(curve(2428, 120, mpSlope, fraction)));
    m.WriteU32(resource + 2492, m.ReadU32(resource + 2432));
    float centerSingle = f(0x820009fc), hundred = f(0x8201dd2c),
          percent = f(0x82000d7c);
    auto singleCurve = [&](unsigned attribute, unsigned tableOffset,
                           float coefficient, float multiplier) {
      float difference = float(centerSingle - f(resource + attribute));
      float value =
          float(-(double(difference) * double(coefficient) - double(hundred)));
      value = float(value * percent);
      value = float(value * f(row + tableOffset));
      return float(value * multiplier);
    };
    put(resource + 2504,
        nonnegative(singleCurve(2444, 132, centerSingle, f(0x82059ad0))));
    put(resource + 2508, singleCurve(2448, 140, centerSingle, f(0x82000e44)));
    put(resource + 2512,
        nonnegative(singleCurve(2452, 144, f(0x821baa74), f(0x82000dd4))));
    put(resource + 2516,
        nonnegative(singleCurve(2456, 148, f(0x8200bca0), f(0x82000e48))));
    m.WriteU32(resource + 2524, m.ReadU32(resource + 2464));
    m.WriteU32(resource + 2520, m.ReadU32(resource + 2460));
    (void)FillGuestMemory(m, resource + 2528, 0, 60);
    s.r[3] = owner;
    s.r[4] = resource;
    (void)battle_resource_stats61::Apply(0x82ac1860, m, d, s);
  }
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_resource_growth61
