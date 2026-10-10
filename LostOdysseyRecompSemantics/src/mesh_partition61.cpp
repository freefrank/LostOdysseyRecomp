#include "lo_semantics/mesh_partition61.h"
#include "lo_semantics/mesh_cook_storage61.h"
#include "lo_semantics/mesh_geometry_math61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/crt_reader_follow61.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
#include <bit>
#include <cmath>
#include <algorithm>
namespace lo::semantic::gpu::mesh_partition61 {
namespace {
using recovery_abi::Address;
using Vec = std::array<float, 3>;
struct Partition {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  float Float(unsigned p) { return std::bit_cast<float>(Word(p)); }
  void Invoke(unsigned slot) {
    s.ctr = Word(Word(Address(s.r[3])) + slot);
    d.lifetime.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  unsigned Allocate(unsigned bytes, unsigned tag, bool sdk = false) {
    if (sdk)
      s.r[3] = Word(0x832df548);
    else
      (void)crt_close_recursive_buffer_context::Apply(0x82bd0798, m,
                                                      d.lifetime.guest, s);
    s.r[4] = bytes;
    s.r[5] = tag;
    Invoke(sdk ? 8 : 0);
    return Address(s.r[3]);
  }
  void Free(unsigned p) {
    (void)crt_close_recursive_buffer_context::Apply(0x82bd0798, m,
                                                    d.lifetime.guest, s);
    s.r[4] = p;
    Invoke(12);
  }
  void Support(unsigned e, unsigned self) {
    s.r[3] = self;
    (void)mesh_partition_support61::Apply(e, m, d, s);
  }
  void Array(unsigned e, unsigned self) {
    s.r[3] = self;
    (void)object_sort_support61::Apply(e, m, {d.lifetime.guest, d.lifetime.fp},
                                       s);
  }
  unsigned Sort(unsigned sorter, unsigned values, unsigned count) {
    Array(0x82bd2c50, sorter);
    s.r[3] = sorter;
    s.r[4] = values;
    s.r[5] = count;
    s.r[6] = 1;
    (void)crt_reader_bucket_sort61::Apply(0x82bd2df0, m, d.edge.engine.sort, s);
    return Word(Address(s.r[3]) + 4);
  }
  void DropSort(unsigned sorter) {
    s.r[3] = sorter;
    (void)crt_reader_follow61::Apply(0x82bd2c78, m, d.lifetime.guest, s);
  }
  Vec Point(unsigned points, unsigned index) {
    auto p = points + 12 * index;
    return {Float(p), Float(p + 4), Float(p + 8)};
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
    auto p = Point(points, Word(indices + 12 * face));
    auto n = Cross(Sub(p, Point(points, Word(indices + 12 * face + 4))),
                   Sub(p, Point(points, Word(indices + 12 * face + 8))));
    auto v = Dot(n, n);
    if (v != Float(0x82000e50)) {
      auto scale = Float(0x82007784) / std::sqrt(v);
      for (auto &x : n)
        x = float(x * scale);
    }
    return n;
  }
  float Angle(unsigned indices, unsigned points, unsigned a, unsigned b) {
    auto u = Normal(indices, points, a), v = Normal(indices, points, b),
         cross = Cross(u, v);
    s.fpr_bits[1] =
        std::bit_cast<std::uint64_t>(double(std::sqrt(Dot(cross, cross))));
    s.fpr_bits[2] = std::bit_cast<std::uint64_t>(double(Dot(u, v)));
    (void)mesh_geometry_math61::Apply(0x822da388, m, d.lifetime.fp, s);
    return std::abs(float(std::bit_cast<double>(s.fpr_bits[1])));
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
  bool Build(unsigned self, unsigned count, unsigned indices, unsigned points) {
    auto sp = Address(s.r[1]), cache = sp + 96, desc = sp + 128;
    m.WriteU32(desc, count);
    m.WriteU32(desc + 4, indices);
    m.WriteU32(desc + 8, 0);
    m.WriteU8(desc + 12, 1);
    m.WriteU8(desc + 13, 1);
    m.WriteU32(desc + 16, points);
    m.WriteU32(desc + 20, Word(0x82000dac));
    s.r[3] = cache;
    (void)mesh_edge_build61::Apply(0x82bbd4c0, m, d.edge, s);
    s.r[3] = cache;
    s.r[4] = desc;
    (void)mesh_cache_build61::Apply(0x82bbddf0, m, d, s);
    auto cleanup = [&]() {
      s.r[3] = cache;
      (void)mesh_cook_storage61::Apply(0x82bbd4e0, m, d.lifetime, s);
    };
    if (!(s.r[3] & 255) || !Word(cache) || !Word(cache + 4) ||
        !Word(cache + 16) || !Word(cache + 20)) {
      cleanup();
      return false;
    }
    auto edges = Word(cache), convex = Allocate(edges, 1);
    if (!convex) {
      cleanup();
      return false;
    }
    auto sharp = Allocate(edges, 1);
    if (!sharp) {
      cleanup();
      return false;
    }
    for (unsigned i = 0; i < edges; ++i) {
      auto record = Word(cache + 16) + 8 * i;
      bool a = false, b = false;
      if (m.ReadU16(record + 2) != 2) {
        a = b = true;
      } else {
        auto incidence = Word(cache + 20) + 4 * Word(record + 4),
             first = Word(incidence), second = Word(incidence + 4),
             edge = Word(cache + 4) + 8 * i;
        auto opposite =
            Opposite(indices + 12 * first, Word(edge), Word(edge + 4));
        auto plane = sp + 176;
        s.r[3] = plane;
        for (unsigned j = 0; j < 3; ++j)
          s.r[4 + j] = points + 12 * Word(indices + 12 * second + 4 * j);
        (void)mesh_geometry_math61::Apply(0x82bd92c0, m, d.lifetime.fp, s);
        Vec n{Float(plane), Float(plane + 4), Float(plane + 8)};
        float distance =
            float(Dot(Point(points, opposite), n) + Float(plane + 12));
        if (!(distance <= Float(0x82000e50)))
          a = !(Angle(indices, points, first, second) <= Float(0x82000d7c));
        b = !(Angle(indices, points, first, second) <= Float(0x82000d7c));
      }
      m.WriteU8(convex + i, a);
      m.WriteU8(sharp + i, b);
    }
    auto categories = Allocate(4 * count, 41);
    m.WriteU32(self + 48, categories);
    for (unsigned i = 0; i < count; ++i)
      m.WriteU32(categories + 4 * i, 0xffffffff);
    m.WriteU32(self, indices);
    m.WriteU32(self + 4, points);
    m.WriteU32(self + 8, cache);
    m.WriteU32(self + 12, 0);
    m.WriteU32(self + 16, 0);
    m.WriteU32(self + 20, sharp);
    m.WriteU32(self + 24, categories);
    m.WriteU32(self + 44, 0);
    unsigned visited = 0;
    while (visited < count) {
      unsigned face = 0;
      while (Word(categories + 4 * face) != 0xffffffff)
        ++face;
      m.WriteU32(self + 28, Word(self + 44));
      s.r[4] = face;
      s.r[5] = count;
      Support(0x82bc1c10, self);
      visited += Word(self + 32);
      m.WriteU32(self + 44, Word(self + 44) + 1);
    }
    auto sums = Allocate(4 * Word(self + 44), 1),
         weights = Allocate(4 * count, 1);
    for (unsigned i = 0; i < Word(self + 44); ++i)
      m.WriteU32(sums + 4 * i, 0);
    for (unsigned i = 0; i < count; ++i) {
      unsigned weight = 0;
      for (unsigned j = 0; j < 3; ++j)
        weight += m.ReadU8(convex + (Word(Word(cache + 12) + 12 * i + 4 * j) &
                                     0x0fffffff)) != 0;
      m.WriteU32(weights + 4 * i, weight);
      auto p = sums + 4 * Word(categories + 4 * i);
      m.WriteU32(p, Word(p) + weight);
    }
    for (unsigned i = 0; i < count; ++i)
      m.WriteU32(weights + 4 * i, Word(sums + 4 * Word(categories + 4 * i)));
    if (sums)
      Free(sums);
    auto sorter = sp + 240, order = Sort(sorter, weights, count);
    if (weights)
      Free(weights);
    auto groups = Allocate(4 * count, 42);
    m.WriteU32(self + 40, groups);
    for (unsigned i = 0; i < count; ++i)
      m.WriteU32(groups + 4 * i, 0xffffffff);
    auto faces = sp + 160, vertices = sp + 192;
    Array(0x82bd2a08, faces);
    Array(0x82bd2a08, vertices);
    m.WriteU32(self + 36, 0);
    m.WriteU32(self + 12, faces);
    m.WriteU32(self + 16, vertices);
    m.WriteU32(self + 20, convex);
    m.WriteU32(self + 24, groups);
    visited = 0;
    while (visited < count) {
      unsigned rank = count - 1;
      while (Word(groups + 4 * Word(order + 4 * rank)) != 0xffffffff)
        --rank;
      auto face = Word(order + 4 * rank);
      m.WriteU32(faces + 4, 0);
      m.WriteU32(vertices + 4, 0);
      m.WriteU32(self + 28, Word(self + 36));
      if (face < count) {
        m.WriteU32(self + 32, 0);
        m.WriteU32(0x832dc4c8, 0);
        s.r[4] = face;
        s.r[5] = 0xffffffff;
        Support(0x82bc1b10, self);
      }
      visited += Word(self + 32);
      m.WriteU32(self + 36, Word(self + 36) + 1);
    }
    Array(0x82bd2c08, vertices);
    Array(0x82bd2c08, faces);
    DropSort(sorter);
    Free(sharp);
    Free(convex);
    sorter = sp + 208;
    order = Sort(sorter, groups, count);
    auto scratch = Word(sorter + 8);
    unsigned current = Word(groups + 4 * Word(order)), n = 0;
    for (unsigned i = 0; i <= count; ++i) {
      auto face = i == count ? 0xffffffff : Word(order + 4 * i),
           group = i == count ? 0xffffffff : Word(groups + 4 * face);
      if (group != current) {
        s.r[4] = n;
        s.r[5] = scratch;
        s.r[6] = indices;
        s.r[7] = points;
        Support(0x82bc1c68, self);
        s.r[4] = n;
        s.r[5] = scratch;
        Support(0x82bc1190, self);
        current = group;
        n = 0;
      }
      m.WriteU32(scratch + 4 * n++, face);
    }
    DropSort(sorter);
    if (count) {
      unsigned maximum = 0;
      for (unsigned i = 0; i < count; ++i)
        maximum = std::max(maximum, Word(categories + 4 * i));
      m.WriteU32(self + 44, maximum + 1);
    }
    cleanup();
    return true;
  }
  bool Export(unsigned adapter) {
    auto mesh = Word(adapter), workspace = Address(s.r[1]) + 352;
    Support(0x82bc10f0, workspace);
    (void)Build(workspace, Word(mesh + 4), Word(mesh + 12), Word(mesh + 8));
    m.WriteU32(mesh + 24, Word(workspace + 36));
    m.WriteU32(mesh + 28, Word(workspace + 44));
    for (unsigned off : {24, 28})
      if (Word(mesh + off) > 65536) {
        s.r[3] = 4;
        s.r[4] = 0xffffffff820d62ccull;
        s.r[5] = off == 24 ? 411 : 417;
        s.r[6] = 0;
        s.r[7] = off == 24 ? 0xffffffff820d6388ull : 0xffffffff820d6318ull;
        (void)diagnostic_format_routes61::Apply(0x82b9c298, m,
                                                d.edge.diagnostics, s);
        Support(0x82bc1108, workspace);
        return false;
      }
    auto count = Word(mesh + 4), out = Allocate(2 * count, 268, true);
    m.WriteU32(mesh + 32, out);
    for (unsigned i = 0; i < count; ++i)
      m.WriteU16(out + 2 * i, Word(Word(workspace + 40) + 4 * i));
    bool bytes = Word(mesh + 28) < 256;
    out = Allocate(count * (bytes ? 1 : 2), bytes ? 267 : 268, true);
    m.WriteU32(mesh + 36, out);
    for (unsigned i = 0; i < count; ++i) {
      auto v = Word(Word(workspace + 48) + 4 * i);
      if (bytes)
        m.WriteU8(out + i, v);
      else
        m.WriteU16(out + 2 * i, v);
    }
    Support(0x82bc1108, workspace);
    return true;
  }
  void Run(unsigned entry) {
    auto a = s.r;
    auto sp = s.r[1];
    m.WriteU32(Address(sp - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(sp - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 768;
    m.WriteU32(Address(s.r[1]), Address(sp));
    s.r[3] = entry == 0x82bb4cf0 ? Export(Address(a[3]))
                                 : Build(Address(a[3]), Address(a[4]),
                                         Address(a[5]), Address(a[7]));
    s.r[1] += 768;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(sp - 16 - 8 * (31 - i)));
    s.lr = Word(Address(sp - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82bc1f00 && e != 0x82bb4cf0)
    return false;
  Partition{m, d, s}.Run(e);
  return true;
}
} // namespace lo::semantic::gpu::mesh_partition61
