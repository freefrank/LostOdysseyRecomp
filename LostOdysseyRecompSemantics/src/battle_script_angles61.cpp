#include "lo_semantics/battle_script_angles61.h"
#include "lo_semantics/battle_script_parameters61.h"
#include "lo_semantics/battle_script_core61.h"
#include "lo_semantics/mesh_hull_incremental61.h"
#include "lo_semantics/mesh_geometry_math61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
#include <limits>
namespace lo::semantic::gpu::battle_script_angles61 {
namespace {
using recovery_abi::Address;
}
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x822b94c8) {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto value = std::bit_cast<double>(s.fpr_bits[1]);
    auto integral = value >= 9223372036854775808.
                        ? std::numeric_limits<std::int64_t>::max()
                    : !(value >= -9223372036854775808.)
                        ? std::numeric_limits<std::int64_t>::min()
                        : std::int64_t(value);
    double rounded = double(integral), magnitude = std::abs(value);
    auto number = [&](unsigned p) {
      return std::bit_cast<double>(recovery_abi::ReadU64(m, p));
    };
    double limit = number(0x820029c0) - magnitude,
           below = rounded - number(0x82000f28);
    rounded = (value - rounded >= 0) ? rounded : below;
    rounded = limit >= 0 ? rounded : value;
    s.fpr_bits[1] =
        std::bit_cast<std::uint64_t>(-magnitude >= 0 ? value : rounded);
    s.fpr_bits[0] = std::bit_cast<std::uint64_t>(rounded);
    return true;
  }
  if (e == 0x82323488) {
    auto old = Address(s.r[1]), output = Address(s.r[3]),
         input = Address(s.r[4]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 30; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    for (unsigned i = 29; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 32 - 8 * (31 - i), s.fpr_bits[i]);
    s.r[1] -= 128;
    m.WriteU32(Address(s.r[1]), old);
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto load = [&](unsigned p) { return std::bit_cast<float>(m.ReadU32(p)); };
    auto set = [&](unsigned i, double v) {
      s.fpr_bits[i] = std::bit_cast<std::uint64_t>(v);
    };
    auto angle = [&](double y, double x) {
      set(1, y);
      set(2, x);
      (void)mesh_geometry_math61::Apply(0x822da388, m, d.fp, s);
      float radians = float(std::bit_cast<double>(s.fpr_bits[1]));
      float scaled = float(radians * load(0x82000e38));
      scaled =
          float(double(scaled) *
                std::bit_cast<double>(recovery_abi::ReadU64(m, 0x82000fc8)));
      set(1, float(scaled + load(0x82189798)));
      (void)battle_script_angles61::Apply(0x822b94c8, m, d, s);
      float value = float(std::bit_cast<double>(s.fpr_bits[1]));
      return value > double(std::numeric_limits<std::int32_t>::max())
                 ? std::numeric_limits<std::int32_t>::max()
             : !(value >= -2147483648.)
                 ? std::numeric_limits<std::int32_t>::min()
                 : std::int32_t(value);
    };
    m.WriteU32(output + 4, unsigned(angle(load(input + 4), load(input))));
    float x = load(input), y = load(input + 4), z = load(input + 8);
    float xx = float(x * x),
          lengthSquared = float(double(y) * double(y) + double(xx));
    float length = float(std::sqrt(double(lengthSquared)));
    m.WriteU32(output, unsigned(angle(z, length)));
    m.WriteU32(output + 8, 0);
    s.r[3] = output;
    s.r[1] += 128;
    for (unsigned i = 29; i < 32; ++i)
      s.fpr_bits[i] = recovery_abi::ReadU64(m, old - 32 - 8 * (31 - i));
    for (unsigned i = 30; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e != 0x82a9c6d8 && e != 0x82a9c790 && e != 0x82a9c848)
    return false;
  auto owner = Address(s.r[3]), old = Address(s.r[1]);
  unsigned first = e == 0x82a9c848 ? 31 : 30;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= 112;
  m.WriteU32(Address(s.r[1]), old);
  auto get = [&](unsigned off) {
    s.r[3] = owner;
    s.r[4] = off;
    s.r[5] = 0;
    (void)battle_script_parameters61::Apply(0x8238be38, m, d, s);
    return Address(s.r[3]);
  };
  auto number = [&](unsigned p) {
    return std::bit_cast<double>(recovery_abi::ReadU64(m, p));
  };
  auto setf = [&](unsigned i, double v) {
    s.fpr_bits[i] = std::bit_cast<std::uint64_t>(v);
  };
  if (s.cached_fp_control & 0x8040) {
    s.cached_fp_control &= ~0x8040u;
    d.fp.SetHostFpControl(s.cached_fp_control);
  }
  double value;
  if (e == 0x82a9c848) {
    auto x = get(5), y = get(3);
    double a = double(std::int32_t(0u - y)), b = double(std::int32_t(x));
    auto epsilon = number(0x820bc4f8);
    setf(1, a);
    setf(2, b);
    if (std::abs(a) < epsilon && std::abs(b) < epsilon)
      setf(1, number(0x82000fe8));
    else
      (void)mesh_geometry_math61::Apply(0x822da388, m, d.fp, s);
    value = (std::bit_cast<double>(s.fpr_bits[1]) * number(0x820bc508)) *
            number(0x82000fc8);
  } else {
    auto angle = get(3), amplitude = get(5);
    double radians = double(std::int32_t(angle)) * number(0x820bc500);
    auto trig =
        mesh_hull_incremental61::EvaluateGuestTrig(m, radians, e == 0x82a9c790);
    setf(1, trig);
    value = trig * double(std::int32_t(amplitude));
  }
  std::int32_t result;
  if (value > double(std::numeric_limits<std::int32_t>::max()))
    result = std::numeric_limits<std::int32_t>::max();
  else if (std::isnan(value) ||
           value < double(std::numeric_limits<std::int32_t>::min()))
    result = std::numeric_limits<std::int32_t>::min();
  else
    result = std::int32_t(value);
  s.fpr_bits[0] = std::uint64_t(std::int64_t(result));
  m.WriteU32(Address(s.r[1]) + 80, unsigned(result));
  s.r[3] = owner;
  s.r[4] = 1;
  s.r[5] = unsigned(result);
  s.r[6] = 0;
  (void)battle_script_core61::Apply(0x8238c210, m, d, s);
  auto actor = m.ReadU32(owner + 24);
  m.WriteU32(actor + 52, m.ReadU32(actor + 52) + 7);
  s.r[1] += 112;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_angles61
