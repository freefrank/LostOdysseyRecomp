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
