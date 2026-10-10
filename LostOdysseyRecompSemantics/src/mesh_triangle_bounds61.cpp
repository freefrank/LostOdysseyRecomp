#include "lo_semantics/mesh_triangle_bounds61.h"
#include "lo_semantics/geometry_primitives61.h"
#include "lo_semantics/mesh_bounds_select61.h"
#include "lo_semantics/power_math61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::mesh_triangle_bounds61 {
namespace {
using recovery_abi::Address;
struct Bounds {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  float Float(unsigned p) { return std::bit_cast<float>(Word(p)); }
  void Float(unsigned p, float v) { m.WriteU32(p, std::bit_cast<unsigned>(v)); }
  void Classify(unsigned self) {
    auto axis = Word(self + 176);
    if (axis == 255)
      return;
    auto plane = Float(self + 180);
    if (!(Float(self + 128 + 4 * axis) < plane)) {
      m.WriteU32(self + 172, axis);
      return;
    }
    if (!(Float(self + 140 + 4 * axis) > plane)) {
      m.WriteU32(self + 172, axis | 8);
      return;
    }
    s.r[3] = 206;
    s.r[4] = 0xffffffff820d5a68ull;
    s.r[5] = 102;
    s.r[6] = 0;
    s.r[7] = 0xffffffff820d5a90ull;
    s.lr = 0x82b9d498;
    (void)diagnostic_format_routes61::Apply(0x82b9c298, m, d.edge.diagnostics,
                                            s);
    Float(self + 180, Float(0x82000e50));
    m.WriteU32(self + 176, 255);
    m.WriteU32(self + 172, 255);
  }
  void Build(unsigned self) {
    auto sp = Address(s.r[1]);
    s.r[3] = sp + 96;
    s.r[4] = sp + 80;
    s.r[5] = Word(self + 4);
    s.r[6] = Word(self + 12);
    (void)geometry_primitives61::Apply(0x82bca410, m, d.lifetime.fp, s);
    s.fpr_bits[2] = recovery_abi::ReadU64(m, 0x820d5e30);
    s.fpr_bits[1] = recovery_abi::ReadU64(m, 0x82001010);
    (void)power_math61::Apply(0x82b7e860, m, d.lifetime.fp, s);
    float multiplier = float(std::bit_cast<double>(s.fpr_bits[1]));
    float x = Float(sp + 80), y = Float(sp + 84), z = Float(sp + 88);
    float maximum = x > y ? x : y;
    maximum = maximum > z ? maximum : z;
    Float(self + 168, float(maximum * multiplier));
    auto axis = Word(self + 176);
    if (axis < 3) {
      auto plane = Float(self + 180);
      if (plane < Float(0x82000e50))
        Float(sp + 96 + 4 * axis, plane);
      else {
        Float(sp + 80 + 4 * axis, plane);
        if (axis == 0)
          x = plane;
        else if (axis == 1)
          y = plane;
        else
          z = plane;
      }
    }
    for (unsigned i = 0; i < 3; ++i)
      m.WriteU32(self + 128 + 4 * i, Word(sp + 96 + 4 * i));
    Float(self + 140, x);
    Float(self + 144, y);
    Float(self + 148, z);
    Classify(self);
    s.r[3] = self + 152;
    s.r[4] = Word(self + 4);
    s.r[5] = Word(self + 12);
    (void)mesh_bounds_select61::Apply(0x82bc9c68, m,
                                      {d.lifetime.guest, d.lifetime.fp}, s);
  }
  void Run(unsigned entry) {
    auto self = Address(s.r[3]);
    auto old = s.r[1];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(old));
    if (entry == 0x82b9d410)
      Classify(self);
    else
      Build(self);
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82b9d410 && e != 0x82ba6458)
    return false;
  Bounds{m, d, s}.Run(e);
  return true;
}
} // namespace lo::semantic::gpu::mesh_triangle_bounds61
