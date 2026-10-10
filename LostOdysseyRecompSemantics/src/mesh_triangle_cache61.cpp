#include "lo_semantics/mesh_triangle_cache61.h"
#include "lo_semantics/mesh_mass_cache61.h"
#include "lo_semantics/mesh_cook_storage61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::mesh_triangle_cache61 {
namespace {
using recovery_abi::Address;
struct Cache {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  float Float(unsigned p) { return std::bit_cast<float>(Word(p)); }
  void Float(unsigned p, float v) { m.WriteU32(p, std::bit_cast<unsigned>(v)); }
  void Invoke(unsigned p, unsigned lr) {
    s.ctr = p;
    s.lr = lr;
    d.lifetime.guest.CallIndirect(p & ~3u, m, s);
  }
  void Edge(unsigned self) {
    auto desc = Address(s.r[1]) + 80;
    m.WriteU32(desc, Word(self + 4));
    m.WriteU32(desc + 4, Word(self + 12));
    m.WriteU32(desc + 8, 0);
    m.WriteU8(desc + 12, 1);
    m.WriteU8(desc + 13, 1);
    m.WriteU32(desc + 16, Word(self + 8));
    m.WriteU32(desc + 20, Word(0x82000dac));
    (void)crt_close_recursive_buffer_context::Apply(0x82bd0798, m,
                                                    d.lifetime.guest, s);
    s.r[4] = 24;
    s.r[5] = 17;
    Invoke(Word(Word(Address(s.r[3]))), 0x82bc5f3c);
    if (s.r[3])
      (void)mesh_edge_build61::Apply(0x82bbd4c0, m, d.edge, s);
    auto cache = Address(s.r[3]);
    m.WriteU32(self + 88, cache);
    s.r[4] = desc;
    (void)mesh_cache_build61::Apply(0x82bbddf0, m, d, s);
    if (!(s.r[3] & 255) && cache) {
      s.r[3] = cache;
      (void)mesh_cook_storage61::Apply(0x82bbd4e0, m, d.lifetime, s);
      (void)crt_close_recursive_buffer_context::Apply(0x82bd0798, m,
                                                      d.lifetime.guest, s);
      s.r[4] = cache;
      Invoke(Word(Word(Address(s.r[3])) + 12), 0x82bc5f94);
      m.WriteU32(self + 88, 0);
    }
  }
  bool Accept(float v) {
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(v));
    mesh_mass_cache61::Classify(m, d.lifetime.fp, s);
    return !(s.r[3] & 519);
  }
  void Mass(unsigned self) {
    if (!(Float(self + 208) < Float(0x82000e50))) {
      s.r[3] = self + 208;
      return;
    }
    auto sp = Address(s.r[1]);
    m.WriteU32(sp + 80, Word(self + 4));
    m.WriteU32(sp + 84, Word(self + 8));
    m.WriteU32(sp + 88, 12);
    m.WriteU32(sp + 92, 12);
    m.WriteU32(sp + 96, Word(self + 12));
    m.WriteU32(sp + 100, Word(self + 16));
    m.WriteU32(sp + 104, 0);
    s.r[3] = sp + 80;
    s.r[5] = sp + 112;
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(Float(0x82007784)));
    (void)mesh_mass_math61::Apply(0x82bcd8a8, m, d.lifetime.fp, s);
    if (!(s.r[3] & 255)) {
      s.r[3] = 0;
      return;
    }
    for (unsigned j = 0; j < 3; ++j)
      for (unsigned i = 0; i < 3; ++i)
        Float(self + 212 + 12 * j + 4 * i,
              float(std::bit_cast<double>(
                  recovery_abi::ReadU64(m, sp + 136 + 24 * j + 8 * i))));
    for (unsigned i = 0; i < 3; ++i)
      m.WriteU32(self + 248 + 4 * i, Word(sp + 112 + 4 * i));
    for (unsigned i = 0; i < 12; ++i)
      if (!Accept(Float(self + 212 + 4 * i))) {
        s.r[3] = 0;
        return;
      }
    double mass = std::bit_cast<double>(recovery_abi::ReadU64(m, sp + 128));
    if (!Accept(float(mass))) {
      s.r[3] = 0;
      return;
    }
    if (mass < std::bit_cast<double>(recovery_abi::ReadU64(m, 0x82000fe8))) {
      mass = -mass;
      for (unsigned i = 0; i < 9; ++i)
        Float(self + 212 + 4 * i, -Float(self + 212 + 4 * i));
    }
    Float(self + 208, float(mass));
    s.r[3] = self + 208;
  }
  void Run(unsigned entry) {
    auto self = Address(s.r[3]);
    auto old = s.r[1];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(old));
    if (entry == 0x82ba6868)
      Mass(self);
    else
      Edge(self);
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82ba6868 && e != 0x82bc5ed8)
    return false;
  Cache{m, d, s}.Run(e);
  return true;
}
} // namespace lo::semantic::gpu::mesh_triangle_cache61
