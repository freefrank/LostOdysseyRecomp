#include "lo_semantics/mesh_partition_support61.h"
#include "lo_semantics/mesh_geometry_math61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include <array>
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_partition_support61 {
namespace {
using recovery_abi::Address;
using Vec = std::array<float, 3>;
struct Support {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  float Float(unsigned p) { return std::bit_cast<float>(Word(p)); }
  void Invoke(unsigned slot) {
    s.ctr = Word(Word(Address(s.r[3])) + slot);
    d.lifetime.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  void Allocator() {
    (void)crt_close_recursive_buffer_context::Apply(0x82bd0798, m,
                                                    d.lifetime.guest, s);
  }
  void Free(unsigned p) {
    Allocator();
    s.r[4] = p;
    Invoke(12);
  }
  void Compact(unsigned self, unsigned count, unsigned faces) {
    auto groups = Word(self + 44);
    Allocator();
    s.r[4] = 4 * groups;
    s.r[5] = 1;
    Invoke(0);
    auto map = Address(s.r[3]);
    for (unsigned i = 0; i < groups; ++i)
      m.WriteU32(map + 4 * i, 0xffffffff);
    unsigned next = 0;
    for (unsigned i = 0; i < count; ++i) {
      auto face = Word(faces + 4 * i), out = Word(self + 48) + 4 * face,
           group = Word(out), v = Word(map + 4 * group);
      if (v == 0xffffffff) {
        v = next++;
        m.WriteU32(map + 4 * group, v);
      }
      m.WriteU32(out, v);
    }
    if (map)
      Free(map);
  }
  static Vec Sub(Vec a, Vec b) {
    return {float(a[0] - b[0]), float(a[1] - b[1]), float(a[2] - b[2])};
  }
  static Vec Cross(Vec a, Vec b) {
    return {float(a[1] * b[2] - a[2] * b[1]), float(a[2] * b[0] - a[0] * b[2]),
            float(a[0] * b[1] - a[1] * b[0])};
  }
  static float Dot(Vec a, Vec b) {
    return float(float(a[1] * b[1] + a[2] * b[2]) + a[0] * b[0]);
  }
  Vec Normal(unsigned indices, unsigned points, unsigned face) {
    auto point = [&](unsigned j) {
      auto p = points + 12 * Word(indices + 12 * face + 4 * j);
      return Vec{Float(p), Float(p + 4), Float(p + 8)};
    };
    auto a = point(0), n = Cross(Sub(a, point(1)), Sub(a, point(2)));
    auto squared = Dot(n, n);
    if (squared != Float(0x82000e50)) {
      auto scale = Float(0x82007784) / std::sqrt(squared);
      for (auto &v : n)
        v = float(v * scale);
    }
    return n;
  }
  void Merge(unsigned self, unsigned count, unsigned faces, unsigned indices,
             unsigned points) {
    for (unsigned i = 0; i < count; ++i) {
      auto face = Word(faces + 4 * i), group = Word(Word(self + 48) + 4 * face);
      for (unsigned j = i; j < count; ++j) {
        auto other = Word(faces + 4 * j), out = Word(self + 48) + 4 * other;
        if (Word(out) == group)
          continue;
        auto a = Normal(indices, points, face),
             b = Normal(indices, points, other), cross = Cross(a, b);
        s.fpr_bits[1] =
            std::bit_cast<std::uint64_t>(double(std::sqrt(Dot(cross, cross))));
        s.fpr_bits[2] = std::bit_cast<std::uint64_t>(double(Dot(a, b)));
        s.lr = 0x82bc1eb0;
        (void)mesh_geometry_math61::Apply(0x822da388, m, d.lifetime.fp, s);
        if (!(std::abs(float(std::bit_cast<double>(s.fpr_bits[1]))) >=
              Float(0x82000d7c)))
          m.WriteU32(out, group);
      }
    }
  }
  void Push(unsigned queue, unsigned value) {
    if (Word(queue + 4) == Word(queue)) {
      s.r[3] = queue;
      s.r[4] = 1;
      (void)reader_buffer_growth61::Apply(0x82bd2870, m,
                                          {d.lifetime.guest, d.lifetime.fp}, s);
    }
    auto n = Word(queue + 4);
    m.WriteU32(Word(queue + 8) + 4 * n, value);
    m.WriteU32(queue + 4, n + 1);
  }
  void Plane(unsigned self, unsigned face, unsigned out) {
    auto indices = Word(self), points = Word(self + 4);
    s.r[3] = out;
    for (unsigned i = 0; i < 3; ++i)
      s.r[4 + i] = points + 12 * Word(indices + 12 * face + 4 * i);
    (void)mesh_geometry_math61::Apply(0x82bd92c0, m, d.lifetime.fp, s);
  }
  float Distance(unsigned plane, unsigned point) {
    return float(float(float(Float(point) * Float(plane) +
                             float(Float(point + 4) * Float(plane + 4) +
                                   Float(point + 8) * Float(plane + 8)))) +
                 Float(plane + 12));
  }
  bool Accept(unsigned self, unsigned face, unsigned vertex) {
    auto accepted = Word(self + 12), vertices = Word(self + 16);
    if (!accepted || !vertices)
      return true;
    auto plane = Address(s.r[1]) + 80;
    if (vertex != 0xffffffff) {
      for (unsigned i = 0; i < Word(accepted + 4); ++i) {
        auto f = Word(Word(accepted + 8) + 4 * i), tri = Word(self) + 12 * f;
        bool contains = false;
        for (unsigned j = 0; j < 3; ++j)
          contains |= Word(tri + 4 * j) == vertex;
        if (contains)
          continue;
        Plane(self, f, plane);
        if (Distance(plane, Word(self + 4) + 12 * vertex) > Float(0x820d6670))
          return false;
      }
      Plane(self, face, plane);
      for (unsigned i = 0; i < Word(vertices + 4); ++i)
        if (Distance(plane,
                     Word(self + 4) + 12 * Word(Word(vertices + 8) + 4 * i)) >
            Float(0x820d6670))
          return false;
    }
    Push(accepted, face);
    if (vertex != 0xffffffff)
      Push(vertices, vertex);
    else
      for (unsigned j = 0; j < 3; ++j)
        Push(vertices, Word(Word(self) + 12 * face + 4 * j));
    return true;
  }
  unsigned Opposite(unsigned tri, unsigned a, unsigned b) {
    auto x = Word(tri), y = Word(tri + 4), z = Word(tri + 8);
    if ((x == a && y == b) || (x == b && y == a))
      return z;
    if ((x == a && z == b) || (x == b && z == a))
      return y;
    if ((y == a && z == b) || (y == b && z == a))
      return x;
    return 0xffffffff;
  }
  void Visit(unsigned self, unsigned queue, unsigned face, unsigned vertex) {
    m.WriteU32(0x832dc4c8, Word(0x832dc4c8) + 1);
    auto labels = Word(self + 24);
    if (Word(labels + 4 * face) != 0xffffffff || !Accept(self, face, vertex))
      return;
    m.WriteU32(labels + 4 * face, Word(self + 28));
    m.WriteU32(self + 32, Word(self + 32) + 1);
    auto cache = Word(self + 8), tri = Word(self) + 12 * face;
    constexpr unsigned pairs[3][2] = {{0, 1}, {1, 2}, {0, 2}};
    for (unsigned j = 0; j < 3; ++j) {
      auto edge = Word(Word(cache + 12) + 12 * face + 4 * j) & 0x0fffffff;
      if (m.ReadU8(Word(self + 20) + edge))
        continue;
      auto record = Word(cache + 16) + 8 * edge;
      for (unsigned k = 0; k < m.ReadU16(record + 2); ++k) {
        auto other = Word(Word(cache + 20) + 4 * (Word(record + 4) + k));
        if (other == face)
          continue;
        auto opposite =
            Opposite(Word(self) + 12 * other, Word(tri + 4 * pairs[j][0]),
                     Word(tri + 4 * pairs[j][1]));
        if (Word(Word(self + 24) + 4 * other) == 0xffffffff) {
          Push(queue, other);
          Push(queue, opposite);
        }
      }
    }
  }
  void Flood(unsigned self, unsigned face) {
    auto queue = Address(s.r[1]) + 160;
    s.r[3] = queue;
    (void)object_sort_support61::Apply(0x82bd2a08, m,
                                       {d.lifetime.guest, d.lifetime.fp}, s);
    m.WriteU32(queue + 16, 0);
    Push(queue, face);
    Push(queue, 0xffffffff);
    auto pop = [&]() {
      auto read = Word(queue + 16), v = Word(Word(queue + 8) + 4 * read);
      m.WriteU32(queue + 16, read + 1);
      if (read + 1 == Word(queue + 4)) {
        m.WriteU32(queue + 4, 0);
        m.WriteU32(queue + 16, 0);
      }
      return v;
    };
    while (Word(queue + 4)) {
      auto f = pop(), v = pop();
      Visit(self, queue, f, v);
    }
    s.r[3] = queue;
    (void)object_sort_support61::Apply(0x82bd2c08, m,
                                       {d.lifetime.guest, d.lifetime.fp}, s);
  }
  void Run(unsigned entry) {
    auto a = s.r;
    auto self = Address(a[3]);
    if (entry == 0x82bc10f0) {
      for (unsigned off : {36, 40, 44, 48})
        m.WriteU32(self + off, 0);
      return;
    }
    if (entry == 0x82bc4258) {
      auto count = Word(self + 4);
      if (!count) {
        s.r[3] = 0;
        return;
      }
      auto read = Word(self + 16);
      m.WriteU32(self + 16, read + 1);
      m.WriteU32(Address(a[4]), Word(Word(self + 8) + 4 * read));
      if (read + 1 == count) {
        m.WriteU32(self + 4, 0);
        m.WriteU32(self + 16, 0);
      }
      s.r[3] = 1;
      return;
    }
    auto old = s.r[1];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(old));
    if (entry == 0x82bc1108) {
      for (unsigned off : {48, 40})
        if (auto p = Word(self + off)) {
          Free(p);
          m.WriteU32(self + off, 0);
        }
    } else if (entry == 0x82bc1190)
      Compact(self, Address(a[4]), Address(a[5]));
    else if (entry == 0x82bc1c68)
      Merge(self, Address(a[4]), Address(a[5]), Address(a[6]), Address(a[7]));
    else if (entry == 0x82bc1260)
      s.r[3] = Accept(self, Address(a[4]), Address(a[5]));
    else if (entry == 0x82bc15d8)
      Visit(self, Address(a[4]), Address(a[5]), Address(a[6]));
    else if (entry == 0x82bc1b10)
      Flood(self, Address(a[4]));
    else if (Address(a[4]) >= Address(a[5]))
      s.r[3] = 0;
    else {
      m.WriteU32(self + 32, 0);
      m.WriteU32(0x832dc4c8, 0);
      Flood(self, Address(a[4]));
      s.r[3] = 1;
    }
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82bc10f0 && e != 0x82bc1108 && e != 0x82bc1190 &&
      e != 0x82bc1c68 && e != 0x82bc4258 && e != 0x82bc1260 &&
      e != 0x82bc15d8 && e != 0x82bc1b10 && e != 0x82bc1c10)
    return false;
  Support{m, d, s}.Run(e);
  return true;
}
} // namespace lo::semantic::gpu::mesh_partition_support61
