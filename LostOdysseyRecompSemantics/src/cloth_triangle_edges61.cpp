#include "lo_semantics/cloth_triangle_edges61.h"
#include "lo_semantics/cloth_topology_support61.h"
#include "lo_semantics/recovery_abi.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::cloth_triangle_edges61 {
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
  unsigned Append(unsigned v) {
    auto start = Word(v), end = Word(v + 4);
    if (Word(v + 8) <= end) {
      auto bytes = 2 * ((end - start) / 68 + 1) * 68, p = Allocate(bytes);
      Copy(p, start, end - start);
      if (start)
        Free(start);
      m.WriteU32(v, p);
      m.WriteU32(v + 8, p + bytes);
      end = p + (end - start);
    }
    m.WriteU32(v + 4, end + 68);
    return end;
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
  Vec Normal(Vec n) {
    auto length = Length(n);
    if (length != Float(0x82000e50)) {
      auto scale = Float(0x82007784) / length;
      for (auto &v : n)
        v = float(v * scale);
    }
    return n;
  }
  void Run() {
    auto self = Address(s.r[3]);
    auto old = s.r[1];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(old));
    auto sp = Address(s.r[1]);
    auto triangles = (Word(self + 28) - Word(self + 24)) / 12,
         records = triangles ? Allocate(48 * triangles) : 0;
    unsigned count = 0;
    for (unsigned i = 0; i < triangles; ++i) {
      if (Word(Word(self + 104) + 4 * i) != i)
        continue;
      unsigned tri[3];
      for (unsigned j = 0; j < 3; ++j)
        tri[j] = Word(Word(self + 24) + 12 * i + 4 * j);
      for (unsigned j = 0; j < 3; ++j) {
        auto a = tri[j], b = tri[(j + 1) % 3], c = tri[(j + 2) % 3],
             p = records + 16 * count++;
        m.WriteU32(p, std::min(a, b));
        m.WriteU32(p + 4, std::max(a, b));
        m.WriteU32(p + 8, c);
        m.WriteU32(p + 12, i);
      }
    }
    m.WriteU32(sp + 96, records);
    m.WriteU32(sp + 100, records + 16 * count);
    m.WriteU32(sp + 104, records + 48 * triangles);
    s.r[3] = sp + 96;
    s.r[4] = 0;
    s.r[5] = count - 1;
    (void)cloth_topology_support61::Apply(0x82ba7080, m, s);
    auto out = sp + 128;
    m.WriteU32(out + 16, 0xffffffff);
    m.WriteU32(out + 20, 0xffffffff);
    for (unsigned i = 0; i < count;) {
      auto p = records + 16 * i++, a = Word(p), b = Word(p + 4),
           c = Word(p + 8);
      m.WriteU32(out, a);
      m.WriteU32(out + 4, b);
      m.WriteU32(out + 8, c);
      m.WriteU32(out + 12, 0xffffffff);
      m.WriteU16(out + 24, 0);
      Float(out + 32, Float(0x82000e50));
      Float(out + 36, Float(0x82000e50));
      auto origin = Point(self, a), edge = Sub(Point(self, b), origin);
      Float(out + 28, Length(edge));
      if (i < count && Word(records + 16 * i) == a &&
          Word(records + 16 * i + 4) == b) {
        auto opposite = Word(records + 16 * i + 8);
        m.WriteU32(out + 12, opposite);
        Float(out + 32, Length(Sub(Point(self, opposite), Point(self, c))));
        auto n = Normal(Cross(edge, Sub(Point(self, c), origin))),
             v = Normal(Cross(edge, Sub(Point(self, opposite), origin)));
        float dot = float(float(n[0] * v[0] + n[2] * v[2]) + n[1] * v[1]),
              square = float(dot * dot);
        float poly = float(square * Float(0x820d6098));
        for (unsigned p : {0x820d609cu, 0x820d60a0u, 0x820d60a4u})
          poly = float(poly * square + Float(p));
        poly = float(poly * square + Float(0x82007784));
        Float(out + 36, float(Float(0x820d60a8) - poly * dot));
      }
      while (i < count && Word(records + 16 * i) == a &&
             Word(records + 16 * i + 4) == b)
        ++i;
      Copy(Append(self + 84), out, 68);
    }
    if (records)
      Free(records);
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82bab468)
    return false;
  Edges{m, d, s}.Run();
  return true;
}
} // namespace lo::semantic::gpu::cloth_triangle_edges61
