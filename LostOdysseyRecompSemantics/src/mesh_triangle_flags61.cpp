#include "lo_semantics/mesh_triangle_flags61.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_triangle_flags61 {
namespace {
using recovery_abi::Address;
using Vec = std::array<float, 3>;
struct Flags {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  float Float(unsigned p) { return std::bit_cast<float>(Word(p)); }
  Vec Point(unsigned mesh, unsigned triangle, unsigned corner) {
    auto ix = Word(Word(mesh + 12) + 12 * triangle + 4 * corner),
         p = Word(mesh + 8) + 12 * ix;
    return {Float(p), Float(p + 4), Float(p + 8)};
  }
  static Vec Sub(Vec a, Vec b) {
    return {float(a[0] - b[0]), float(a[1] - b[1]), float(a[2] - b[2])};
  }
  Vec Normal(unsigned mesh, unsigned face) {
    auto p = Point(mesh, face, 0), a = Sub(Point(mesh, face, 1), p),
         b = Sub(Point(mesh, face, 2), p);
    Vec n{float(a[1] * b[2] - a[2] * b[1]), float(a[2] * b[0] - a[0] * b[2]),
          float(a[0] * b[1] - a[1] * b[0])};
    auto length =
        std::sqrt(float(float(n[1] * n[1] + n[2] * n[2]) + n[0] * n[0]));
    if (length != Float(0x82000e50)) {
      float scale = Float(0x82007784) / length;
      for (auto &v : n)
        v = float(v * scale);
    }
    return n;
  }
  static float Dot(Vec a, Vec b) {
    return float(float(a[1] * b[1] + a[2] * b[2]) + a[0] * b[0]);
  }
  void Mark(unsigned mesh, unsigned encoded) {
    auto p = Word(mesh + 40) + (encoded & 0x3fffffff);
    m.WriteU8(p, m.ReadU8(p) | (1u << (encoded >> 30)));
  }
  void Edge(unsigned record, unsigned adapter) {
    auto other = Word(record + 12);
    if (other == 0xffffffff)
      return;
    auto mesh = Word(adapter), first = Word(record + 8),
         face = first & 0x3fffffff, neighbor = other & 0x3fffffff,
         slot = other >> 30;
    Mark(mesh, other);
    auto n = Normal(mesh, face), v = Normal(mesh, neighbor);
    if (Dot(n, v) > float(Float(0x82007784) - Float(mesh + 96))) {
      Mark(mesh, first);
      return;
    }
    unsigned a = 0, b = 2;
    if (slot == 1)
      b = 1;
    else if (slot == 2) {
      a = 1;
      b = 0;
    }
    auto direction = Sub(Point(mesh, neighbor, b), Point(mesh, neighbor, a));
    if (Dot(direction, n) > Float(0x82000e50))
      Mark(mesh, first);
  }
  std::array<unsigned, 4> Record(unsigned p) {
    return {Word(p), Word(p + 4), Word(p + 8), Word(p + 12)};
  }
  void Record(unsigned p, const std::array<unsigned, 4> &v) {
    for (unsigned i = 0; i < 4; ++i)
      m.WriteU32(p + 4 * i, v[i]);
  }
  static bool Less(const std::array<unsigned, 4> &a,
                   const std::array<unsigned, 4> &b) {
    return a[0] < b[0] || (a[0] == b[0] && a[1] < b[1]);
  }
  void Sort(unsigned base, std::int32_t left, std::int32_t right) {
    while (true) {
      auto i = left, j = right;
      auto pivot =
          Record(base + 16 * unsigned((std::int64_t(left) + right) / 2));
      while (i <= j) {
        while (Less(Record(base + 16 * unsigned(i)), pivot))
          ++i;
        while (Less(pivot, Record(base + 16 * unsigned(j))))
          --j;
        if (i > j)
          break;
        auto a = Record(base + 16 * unsigned(i)),
             b = Record(base + 16 * unsigned(j));
        Record(base + 16 * unsigned(i), b);
        Record(base + 16 * unsigned(j), a);
        ++i;
        --j;
      }
      if (left < j)
        Sort(base, left, j);
      if (i >= right)
        break;
      left = i;
    }
  }
  void Invoke(unsigned target, unsigned lr) {
    s.ctr = target;
    s.lr = lr;
    d.lifetime.guest.CallIndirect(target & ~3u, m, s);
  }
  unsigned Allocate(unsigned bytes, unsigned tag, unsigned lr) {
    s.r[3] = Word(0x832df548);
    s.r[4] = bytes;
    s.r[5] = tag;
    Invoke(Word(Word(Address(s.r[3])) + 8), lr);
    return Address(s.r[3]);
  }
  void Free(unsigned p, unsigned lr) {
    s.r[3] = Word(0x832df548);
    s.r[4] = p;
    Invoke(Word(Word(Address(s.r[3])) + 20), lr);
  }
  void Build(unsigned adapter) {
    auto mesh = Word(adapter);
    if (auto p = Word(mesh + 40)) {
      Free(p, 0x82bb526c);
      m.WriteU32(mesh + 40, 0);
    }
    auto count = Word(mesh + 4);
    if (count >= 0x40000000) {
      s.r[3] = 1;
      s.r[4] = 0xffffffff820d62ccull;
      s.r[5] = 752;
      s.r[6] = 0;
      s.r[7] = 0xffffffff820d63e0ull;
      (void)diagnostic_format_routes61::Apply(0x82b9c298, m, d.edge.diagnostics,
                                              s);
      return;
    }
    auto flags = Allocate(count, 20, 0x82bb52d0);
    m.WriteU32(mesh + 40, flags);
    for (unsigned i = 0; i < count; ++i)
      m.WriteU8(flags + i, 0);
    unsigned edges = count * 3;
    auto records =
        Allocate(edges <= 0x0fffffff ? edges * 16 : 0xffffffff, 18, 0x82bb5338);
    for (unsigned i = 0; i < count; ++i) {
      unsigned v[]{Word(Word(mesh + 12) + 12 * i),
                   Word(Word(mesh + 12) + 12 * i + 4),
                   Word(Word(mesh + 12) + 12 * i + 8)};
      constexpr unsigned a[]{0, 0, 1}, b[]{1, 2, 2};
      for (unsigned side = 0; side < 3; ++side) {
        auto x = v[a[side]], y = v[b[side]];
        if (x > y)
          std::swap(x, y);
        Record(records + 16 * (3 * i + side),
               {x, y, i | (side << 30), 0xffffffff});
      }
    }
    Sort(records, 0, std::int32_t(edges - 1));
    unsigned compact = 0;
    for (unsigned i = 1; i < edges; ++i) {
      auto previous = Record(records + 16 * compact),
           next = Record(records + 16 * i);
      if (previous[0] == next[0] && previous[1] == next[1]) {
        if (previous[3] == 0xffffffff)
          m.WriteU32(records + 16 * compact + 12, next[2]);
        else
          Mark(mesh, next[2]);
      } else
        Record(records + 16 * (++compact), next);
    }
    for (unsigned i = 0; i <= compact; ++i)
      Edge(records + 16 * i, adapter);
    Free(records, 0x82bb5510);
  }
  void Run(unsigned entry) {
    auto a = s.r;
    auto old = s.r[1];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(old));
    if (entry == 0x82bb42e8)
      Sort(Address(a[3]), std::int32_t(a[4]), std::int32_t(a[5]));
    else if (entry == 0x82bb4f40)
      Edge(Address(a[3]), Address(a[4]));
    else
      Build(Address(a[3]));
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82bb42e8 && e != 0x82bb4f40 && e != 0x82bb5230)
    return false;
  Flags{m, d, s}.Run(e);
  return true;
}
} // namespace lo::semantic::gpu::mesh_triangle_flags61
