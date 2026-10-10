#include "lo_semantics/mesh_triangle_normals61.h"
#include "lo_semantics/mesh_geometry_math61.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_triangle_normals61 {
namespace {
using recovery_abi::Address;
using Vec = std::array<float, 3>;
struct Normals {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  float Float(unsigned p) { return std::bit_cast<float>(Word(p)); }
  Vec Read(unsigned p) { return {Float(p), Float(p + 4), Float(p + 8)}; }
  void Write(unsigned p, const Vec &v) {
    for (unsigned i = 0; i < 3; ++i)
      m.WriteU32(p + 4 * i, std::bit_cast<unsigned>(v[i]));
  }
  static Vec Sub(const Vec &a, const Vec &b) {
    return {float(a[0] - b[0]), float(a[1] - b[1]), float(a[2] - b[2])};
  }
  static Vec Cross(const Vec &a, const Vec &b) {
    return {float(a[1] * b[2] - a[2] * b[1]), float(a[2] * b[0] - a[0] * b[2]),
            float(a[0] * b[1] - a[1] * b[0])};
  }
  static float Length(const Vec &v) {
    return std::sqrt(float(float(v[0] * v[0] + v[2] * v[2]) + v[1] * v[1]));
  }
  static bool Zero(const Vec &v) {
    return (std::bit_cast<unsigned>(v[0]) & 0x7fffffff) == 0 &&
           (std::bit_cast<unsigned>(v[1]) & 0x7fffffff) == 0 &&
           (std::bit_cast<unsigned>(v[2]) & 0x7fffffff) == 0;
  }
  Vec Normalize(Vec v) {
    auto length = Length(v);
    if (length != Float(0x82000e50)) {
      float scale = Float(0x82007784) / length;
      for (auto &x : v)
        x = float(x * scale);
    }
    return v;
  }
  float Angle(unsigned vertices, const std::array<unsigned, 3> &tri,
              unsigned vertex) {
    unsigned a = 0, b = 0;
    if (vertex == tri[0]) {
      a = 2;
      b = 1;
    } else if (vertex == tri[1])
      a = 2;
    else if (vertex == tri[2])
      b = 1;
    auto p = Read(vertices + 12 * vertex),
         u = Sub(Read(vertices + 12 * tri[a]), p),
         v = Sub(Read(vertices + 12 * tri[b]), p);
    auto cross = Cross(u, v);
    float dot = float(float(u[1] * v[1] + u[2] * v[2]) + u[0] * v[0]);
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(Length(cross)));
    s.fpr_bits[2] = std::bit_cast<std::uint64_t>(double(dot));
    s.lr = 0x82bca3fc;
    (void)mesh_geometry_math61::Apply(0x822da388, m, d.lifetime.fp, s);
    float angle = float(std::bit_cast<double>(s.fpr_bits[1]));
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(angle));
    return angle;
  }
  unsigned Allocate(unsigned bytes, unsigned lr) {
    s.r[3] = Word(0x832df548);
    s.r[4] = bytes;
    s.r[5] = 257;
    s.ctr = Word(Word(Address(s.r[3])) + 8);
    s.lr = lr;
    d.lifetime.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    return Address(s.r[3]);
  }
  void Free(unsigned p, unsigned lr) {
    s.r[3] = Word(0x832df548);
    s.r[4] = p;
    s.ctr = Word(Word(Address(s.r[3])) + 20);
    s.lr = lr;
    d.lifetime.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  bool Build(unsigned triangles, unsigned count, unsigned vertices,
             unsigned words, unsigned halves, unsigned out, bool flip) {
    if (!vertices || !out || !triangles || !count)
      return false;
    auto faces = Allocate(12 * triangles, 0x82bcc4e8);
    if (!faces)
      return false;
    auto indices = [&](unsigned i) {
      std::array<unsigned, 3> v;
      for (unsigned j = 0; j < 3; ++j)
        v[j] = words    ? Word(words + 12 * i + 4 * j)
               : halves ? m.ReadU16(halves + 6 * i + 2 * j)
                        : j;
      return v;
    };
    for (unsigned i = 0; i < triangles; ++i) {
      auto ix = indices(i);
      auto p = Read(vertices + 12 * ix[0]);
      auto a = Sub(Read(vertices + 12 * ix[1 + unsigned(flip)]), p),
           b = Sub(Read(vertices + 12 * ix[2 - unsigned(flip)]), p);
      Write(faces + 12 * i, Normalize(Cross(b, a)));
    }
    for (unsigned i = 0; i < count; ++i)
      Write(out + 12 * i, {0, 0, 0});
    auto fallback = Allocate(12 * count, 0x82bcc6d0);
    for (unsigned i = 0; i < count; ++i)
      Write(fallback + 12 * i, {0, 0, 0});
    for (unsigned i = 0; i < triangles; ++i) {
      auto ix = indices(i);
      auto n = Read(faces + 12 * i);
      for (auto v : ix)
        if (Zero(Read(fallback + 12 * v)))
          Write(fallback + 12 * v, n);
    }
    for (unsigned i = 0; i < triangles; ++i) {
      auto ix = indices(i);
      auto n = Read(faces + 12 * i);
      for (auto v : ix) {
        auto weight = Angle(vertices, ix, v);
        auto sum = Read(out + 12 * v);
        for (unsigned j = 0; j < 3; ++j)
          sum[j] = float(sum[j] + float(weight * n[j]));
        Write(out + 12 * v, sum);
      }
    }
    for (unsigned i = 0; i < count; ++i) {
      auto v = Read(out + 12 * i);
      if (Zero(v))
        v = Read(fallback + 12 * i);
      if (Zero(v))
        v[1] = Float(0x82007784);
      Write(out + 12 * i, Normalize(v));
    }
    if (fallback)
      Free(fallback, 0x82bccb08);
    Free(faces, 0x82bccb20);
    return true;
  }
  void Run(unsigned entry) {
    auto a = s.r;
    auto old = s.r[1];
    auto f30 = s.fpr_bits[30], f31 = s.fpr_bits[31];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(old));
    if (entry == 0x82bca2e8) {
      std::array<unsigned, 3> ix{Word(Address(a[4])), Word(Address(a[4]) + 4),
                                 Word(Address(a[4]) + 8)};
      (void)Angle(Address(a[3]), ix, Address(a[5]));
    } else
      s.r[3] = Build(Address(a[3]), Address(a[4]), Address(a[5]), Address(a[6]),
                     Address(a[7]), Address(a[8]), (a[9] & 255) != 0)
                   ? 1
                   : 0;
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.fpr_bits[30] = f30;
    s.fpr_bits[31] = f31;
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82bca2e8 && e != 0x82bcc470)
    return false;
  Normals{m, d, s}.Run(e);
  return true;
}
} // namespace lo::semantic::gpu::mesh_triangle_normals61
