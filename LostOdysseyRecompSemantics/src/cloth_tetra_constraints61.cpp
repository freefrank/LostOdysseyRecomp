#include "lo_semantics/cloth_tetra_constraints61.h"
#include "lo_semantics/cloth_topology_support61.h"
#include "lo_semantics/recovery_abi.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::cloth_tetra_constraints61 {
namespace {
using recovery_abi::Address;
using Vec = std::array<float, 3>;
struct Edges {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  float Float(unsigned p) { return std::bit_cast<float>(Word(p)); }
  void Float(unsigned p, float v) { m.WriteU32(p, std::bit_cast<unsigned>(v)); }
  void Invoke(unsigned slot) {
    s.r[3] = Word(0x832df548);
    s.ctr = Word(Word(Address(s.r[3])) + slot);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  unsigned Allocate(unsigned bytes) {
    s.r[4] = bytes;
    s.r[5] = 282;
    Invoke(8);
    return Address(s.r[3]);
  }
  void Free(unsigned p) {
    s.r[4] = p;
    Invoke(20);
  }
  void Copy(unsigned out, unsigned in, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i)
      m.WriteU8(out + i, m.ReadU8(in + i));
  }
  void Resize(unsigned v, unsigned count) {
    auto start = Word(v), end = Word(v + 4), used = (end - start) / 68;
    if (count > used) {
      if ((Word(v + 8) - start) / 68 < count) {
        auto p = Allocate(2 * count * 68);
        Copy(p, start, end - start);
        if (start)
          Free(start);
        start = p;
        m.WriteU32(v, p);
        m.WriteU32(v + 8, p + 2 * count * 68);
      }
      for (unsigned i = 68 * used; i < 68 * count; ++i)
        m.WriteU8(start + i, 0);
    }
    end = start + 68 * count;
    m.WriteU32(v + 4, end);
    if (end == start) {
      if (start)
        Free(start);
      m.WriteU32(v, 0);
      m.WriteU32(v + 4, 0);
      m.WriteU32(v + 8, 0);
    } else if (Word(v + 8) > end) {
      s.r[4] = start;
      s.r[5] = count * 68;
      Invoke(16);
      start = Address(s.r[3]);
      m.WriteU32(v, start);
      m.WriteU32(v + 4, start + count * 68);
      m.WriteU32(v + 8, start + count * 68);
    }
  }
  Vec Point(unsigned self, unsigned index) {
    auto p = Word(self + 4) + 12 * index;
    return {Float(p), Float(p + 4), Float(p + 8)};
  }
  static Vec Sub(Vec a, Vec b) {
    return {float(a[0] - b[0]), float(a[1] - b[1]), float(a[2] - b[2])};
  }
  static Vec Cross(Vec a, Vec b) {
    return {float(a[1] * b[2] - a[2] * b[1]), float(a[2] * b[0] - a[0] * b[2]),
            float(a[0] * b[1] - a[1] * b[0])};
  }
  static float Length(Vec v) {
    return std::sqrt(float(float(v[0] * v[0] + v[2] * v[2]) + v[1] * v[1]));
  }
  void Run() {
    auto self = Address(s.r[3]), old = Address(s.r[1]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 19; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 400;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    constexpr unsigned pairs[6][2] = {{0, 1}, {0, 2}, {0, 3},
                                      {1, 2}, {1, 3}, {2, 3}};
    auto count = (Word(self + 28) - Word(self + 24)) / 16;
    auto edges = count ? Allocate(count * 72) : 0;
    for (unsigned i = 0; i < count; ++i) {
      auto face = Word(self + 24) + 16 * i;
      for (unsigned j = 0; j < 6; ++j) {
        auto a = Word(face + 4 * pairs[j][0]), b = Word(face + 4 * pairs[j][1]);
        auto p = edges + 12 * (6 * i + j);
        m.WriteU32(p, std::min(a, b));
        m.WriteU32(p + 4, std::max(a, b));
        m.WriteU32(p + 8, i);
      }
    }
    m.WriteU32(sp + 80, edges);
    m.WriteU32(sp + 84, edges + count * 72);
    m.WriteU32(sp + 88, edges + count * 72);
    s.r[3] = sp + 80;
    s.r[4] = 0;
    s.r[5] = 6 * count - 1;
    (void)cloth_topology_support61::Apply(0x82ba7250, m, s);
    Resize(self + 84, count);
    for (unsigned i = 0; i < count; ++i) {
      auto face = Word(self + 24) + 16 * i, out = Word(self + 84) + 68 * i;
      // The source initializes the whole temporary record once, then fills it.
      for (unsigned j = 0; j < 68; ++j)
        m.WriteU8(out + j, 0);
      for (unsigned j = 0; j < 4; ++j)
        m.WriteU32(out + 4 * j, Word(face + 4 * j));
      auto origin = Point(self, Word(face));
      auto a = Sub(Point(self, Word(face + 4)), origin),
           b = Sub(Point(self, Word(face + 8)), origin),
           c = Sub(Point(self, Word(face + 12)), origin);
      auto normal = Cross(a, b);
      Float(out + 40, float(float(normal[2] * c[2] + normal[0] * c[0]) +
                            normal[1] * c[1]));
      for (unsigned j = 0; j < 6; ++j) {
        auto a = Word(face + 4 * pairs[j][0]), b = Word(face + 4 * pairs[j][1]);
        m.WriteU32(sp + 104, std::min(a, b));
        m.WriteU32(sp + 108, std::max(a, b));
        m.WriteU32(sp + 112, i);
        s.r[3] = sp + 80;
        s.r[4] = sp + 104;
        (void)cloth_topology_support61::Apply(0x82ba7db8, m, s);
        auto length = Length(Sub(Point(self, b), Point(self, a)));
        Float(out + 44 + 4 * j, Address(s.r[3]) ? length : -length);
      }
    }
    if (edges)
      Free(edges);
    s.r[1] += 400;
    for (unsigned i = 19; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = Word(old - 8);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82babe50)
    return false;
  Edges{m, d, s}.Run();
  return true;
}
} // namespace lo::semantic::gpu::cloth_tetra_constraints61
