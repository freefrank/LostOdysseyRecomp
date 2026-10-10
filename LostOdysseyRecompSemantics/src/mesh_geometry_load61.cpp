#include "lo_semantics/mesh_geometry_load61.h"
#include "lo_semantics/mesh_hull_incremental61.h"
#include "lo_semantics/mesh_stream_codec61.h"
#include "lo_semantics/mesh_valence_stream61.h"
#include "lo_semantics/mesh_polygon_collect61.h"
#include "lo_semantics/recovery_abi.h"
#include <algorithm>
#include <array>
#include <bit>
namespace lo::semantic::gpu::mesh_geometry_load61 {
namespace {
using recovery_abi::Address;
struct Reader {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  float Float(unsigned p) { return std::bit_cast<float>(Word(p)); }
  void Float(unsigned p, float v) { m.WriteU32(p, std::bit_cast<unsigned>(v)); }
  float Trig(float angle, bool cosine) {
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(angle));
    (void)mesh_hull_incremental61::Apply(cosine ? 0x822a2f08u : 0x822a2fe0u, m,
                                         d, s);
    return float(std::bit_cast<double>(s.fpr_bits[1]));
  }
  void Normals(unsigned count, unsigned out, unsigned swap, unsigned stream) {
    auto frame = s.r[1];
    s.r[12] = std::uint32_t(0 - 2 * count) & 0xfffffff0u;
    mesh_polygon_collect61::ProbeStack(m, s);
    auto back = Word(Address(s.r[1]));
    s.r[1] += s.r[12];
    m.WriteU32(Address(s.r[1]), back);
    auto packed = Address(s.r[1] + 80);
    s.r[3] = packed;
    s.r[4] = count;
    s.r[5] = swap;
    s.r[6] = stream;
    (void)mesh_stream_codec61::Apply(0x82bd7ed8u, m,
                                     {d.lifetime.guest, d.lifetime.fp}, s);
    constexpr unsigned registered = 0x832df53cu, initialized = 0x832df538u,
                       table = 0x832dc538u;
    if (!(Word(registered) & 1)) {
      m.WriteU32(registered, Word(registered) | 1);
      s.r[3] = 0xffffffff830d9a60ull;
      s.lr = 0x82bc6a50u;
      d.lifetime.guest.CallDirect(0x82b7be48u, m, s);
    }
    if (!m.ReadU8(initialized)) {
      m.WriteU8(initialized, 1);
      auto step = Float(0x820d6954u);
      for (unsigned i = 0; i < 32; ++i) {
        float angle = float(float(i) * step), cosine = Trig(angle, true),
              sine = Trig(angle, false);
        for (unsigned j = 0; j < 32; ++j) {
          float phi = float(float(j) * step), c = Trig(phi, true),
                a = float(c * cosine), b = Trig(phi, false),
                z = float(c * sine);
          if (a > b)
            std::swap(a, b);
          if (b > z)
            std::swap(b, z);
          if (a > b)
            std::swap(a, b);
          if (b > z)
            std::swap(b, z);
          auto p = table + 12 * (32 * i + j);
          Float(p, a);
          Float(p + 4, b);
          Float(p + 8, z);
        }
      }
    }
    constexpr unsigned permutations[]{1176, 8544, 6660};
    for (unsigned i = 0; i < count; ++i) {
      auto code = m.ReadU16(packed + 2 * i);
      unsigned point = table + 12 * (code >> 6), shift = 2 * (code & 7),
               sign = (code >> 3) & 7;
      for (unsigned axis = 0; axis < 3; ++axis) {
        auto component = (permutations[axis] >> shift) & 3;
        auto bits = Word(point + 4 * component) | (((sign >> axis) & 1u) << 31);
        m.WriteU32(out + 12 * i + 4 * axis, bits);
      }
    }
    s.r[1] = frame;
  }
  void Codec(unsigned entry) {
    (void)mesh_stream_codec61::Apply(entry, m,
                                     {d.lifetime.guest, d.lifetime.fp}, s);
  }
  void Block(unsigned stream, unsigned out, unsigned bytes, unsigned ret) {
    s.r[3] = stream;
    s.r[4] = out;
    s.r[5] = bytes;
    s.ctr = Word(Word(stream) + 24);
    s.lr = ret;
    d.lifetime.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  unsigned Scalar(unsigned stream, unsigned swap) {
    s.r[3] = swap;
    s.r[4] = stream;
    Codec(0x82bad8b8u);
    return Address(s.r[3]);
  }
  void Words(unsigned stream, unsigned out, unsigned count, unsigned swap) {
    s.r[3] = out;
    s.r[4] = count;
    s.r[5] = swap;
    s.r[6] = stream;
    Codec(0x82bd7f58u);
  }
  void Adaptive(unsigned stream, unsigned out, unsigned count, unsigned maximum,
                unsigned swap, bool half) {
    s.r[3] = maximum;
    s.r[4] = count;
    s.r[5] = out;
    s.r[6] = stream;
    s.r[7] = swap;
    Codec(half ? 0x82bd8468u : 0x82bd8748u);
  }
  unsigned Allocate(unsigned bytes, unsigned tag, unsigned ret) {
    (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u, m,
                                                    d.lifetime.guest, s);
    s.r[4] = bytes;
    s.r[5] = tag;
    s.ctr = Word(Word(s.r[3]));
    s.lr = ret;
    d.lifetime.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    return Address(s.r[3]);
  }
  void Free(unsigned pointer, unsigned ret) {
    if (!pointer)
      return;
    (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u, m,
                                                    d.lifetime.guest, s);
    s.r[4] = pointer;
    s.ctr = Word(Word(s.r[3]) + 12);
    s.lr = ret;
    d.lifetime.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  static unsigned Align(unsigned p, unsigned multiple) {
    auto remainder = p % multiple;
    return remainder ? p - remainder + multiple : p;
  }
  bool Header(unsigned stream, char a, char b, char c, char e, unsigned version,
              unsigned endian) {
    s.r[3] = a;
    s.r[4] = b;
    s.r[5] = c;
    s.r[6] = e;
    s.r[7] = version;
    s.r[8] = endian;
    s.r[9] = stream;
    Codec(0x82bd81b0u);
    return (s.r[3] & 255u) != 0;
  }
  bool Geometry(unsigned self, unsigned stream) {
    auto frame = Address(s.r[1]);
    if (!Header(stream, 'C', 'V', 'H', 'L', frame + 88, frame + 80))
      return false;
    auto version = Word(frame + 88), swap = unsigned(m.ReadU8(frame + 80));
    Words(stream, frame + 96, 6, swap);
    unsigned vertices = Word(frame + 96), triangles = Word(frame + 100),
             edges = Word(frame + 104), polygons = Word(frame + 108),
             indices = Word(frame + 112), extra = Word(frame + 116);
    m.WriteU32(self + 12, vertices);
    m.WriteU32(self + 4, triangles);
    m.WriteU32(self + 52, edges);
    m.WriteU32(self + 36, polygons);
    unsigned bytes = 36 * (polygons + 1);
    auto reserve = [&](unsigned count, unsigned stride, unsigned alignment) {
      if (!bytes)
        ++count;
      bytes = Align(bytes, alignment) + count * stride;
    };
    reserve(vertices, 12, 12);
    reserve(triangles, 12, 12);
    reserve(vertices, 12, 12);
    reserve(edges, 12, 12);
    reserve(edges, 8, 8);
    reserve(edges, 2, 2);
    reserve(indices, 2, 2);
    reserve(indices, 1, 1);
    if (!bytes)
      ++extra;
    bytes += extra;
    auto raw = Allocate(bytes, 0, 0x82bc6ea4u);
    m.WriteU32(self + 72, raw);
    if (!raw)
      return false;
    unsigned cursor = Align(raw, 36);
    auto bind = [&](unsigned field, unsigned count, unsigned stride,
                    unsigned alignment) {
      cursor = Align(cursor, alignment);
      m.WriteU32(self + field, cursor);
      cursor += count * stride;
    };
    bind(40, polygons, 36, 36);
    bind(16, vertices, 12, 12);
    bind(8, triangles, 12, 12);
    bind(20, vertices, 12, 12);
    bind(60, edges, 12, 12);
    bind(64, edges, 8, 8);
    bind(56, edges, 2, 2);
    bind(48, indices, 2, 2);
    bind(44, indices, 1, 1);
    bind(68, extra, 1, 1);
    Words(stream, Word(self + 16), 3 * vertices, swap);
    auto maximum = Scalar(stream, swap);
    Adaptive(stream, Word(self + 8), 3 * triangles, maximum, swap, false);
    unsigned normalMode = 0;
    if (version >= 5) {
      s.r[3] = swap;
      s.r[4] = stream;
      Codec(0x82bad858u);
      normalMode = Address(s.r[3]) & 65535u;
    }
    if (version >= 4 && !normalMode)
      Normals(vertices, Word(self + 20), swap, stream);
    else {
      Words(stream, Word(self + 20), 3 * vertices, swap);
      if (version < 2)
        for (unsigned i = 0; i < 3 * vertices; ++i)
          m.WriteU32(Word(self + 20) + 4 * i,
                     Word(Word(self + 20) + 4 * i) ^ 0x80000000u);
    }
    Words(stream, self + 24, 3, swap);
    Block(stream, Word(self + 40), 36 * polygons, 0x82bc7264u);
    if (swap)
      for (unsigned i = 0; i < polygons; ++i) {
        auto p = Word(self + 40) + 36 * i;
        for (unsigned off : {0, 2})
          m.WriteU16(p + off, __builtin_bswap16(m.ReadU16(p + off)));
        for (unsigned off = 4; off < 36; off += 4)
          m.WriteU32(p + off, __builtin_bswap32(Word(p + off)));
      }
    if (version >= 3) {
      Block(stream, Word(self + 44), indices, 0x82bc7454u);
      auto maxIndex = Scalar(stream, swap);
      Adaptive(stream, Word(self + 48), indices, maxIndex & 65535u, swap, true);
    }
    auto byteBase = Scalar(stream, swap), halfBase = Scalar(stream, swap);
    for (unsigned i = 0; i < polygons; ++i) {
      auto p = Word(self + 40) + 36 * i;
      m.WriteU32(p + 4, Word(self + 44) + (Word(p + 4) - byteBase));
      m.WriteU32(p + 8, Word(self + 48) + ((Word(p + 8) - halfBase) & ~1u));
    }
    if (version >= 3)
      Block(stream, Word(self + 56), 2 * edges, 0x82bc75d8u);
    if (version >= 4 && !normalMode)
      Normals(edges, Word(self + 60), swap, stream);
    else
      Words(stream, Word(self + 60), 3 * edges, swap);
    auto oldStack = s.r[1];
    unsigned temporary;
    if (edges <= 256) {
      s.r[12] = std::uint32_t(0 - 4 * edges) & 0xfffffff0u;
      mesh_polygon_collect61::ProbeStack(m, s);
      auto back = Word(Address(s.r[1]));
      s.r[1] += s.r[12];
      m.WriteU32(Address(s.r[1]), back);
      temporary = Address(s.r[1] + 80);
    } else
      temporary = Allocate(4 * edges, 1, 0x82bc7670u);
    for (unsigned column = 0; column < 3; ++column) {
      auto maxValue = Scalar(stream, swap);
      Adaptive(stream, temporary, edges,
               column < 2 ? (maxValue & 65535u) : maxValue, swap, column < 2);
      for (unsigned i = 0; i < edges; ++i) {
        auto p = Word(self + 64) + 8 * i;
        if (column < 2)
          m.WriteU16(p + 2 * column, m.ReadU16(temporary + 2 * i));
        else
          m.WriteU32(p + 4, Word(temporary + 4 * i));
      }
    }
    if (edges > 256)
      Free(temporary, 0x82bc7878u);
    s.r[1] = oldStack;
    Block(stream, Word(self + 68), extra, 0x82bc7894u);
    return true;
  }
  bool Combined(unsigned self, unsigned stream) {
    auto frame = Address(s.r[1]);
    if (!Header(stream, 'C', 'L', 'H', 'L', frame + 88, frame + 80))
      return false;
    auto outer = s.r[1];
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(outer));
    bool ok = Geometry(self, stream);
    s.r[1] = outer;
    if (!ok)
      return false;
    for (unsigned off = 108; off <= 128; off += 4)
      m.WriteU32(self + off, Word(Word(Word(self + 80) + 4)));
    m.WriteU32(self + 84, self + 92);
    s.r[3] = self + 88;
    s.r[4] = stream;
    (void)mesh_valence_stream61::Apply(0x82bc7f98u, m,
                                       {d.lifetime.guest, d.lifetime.fp}, s);
    return (s.r[3] & 255u) != 0;
  }
  void Run(unsigned entry) {
    auto a = s.r;
    auto old = s.r[1];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(old));
    if (entry == 0x82bc69e0u)
      Normals(Address(a[3]), Address(a[4]), Address(a[5]), Address(a[6]));
    else
      s.r[3] = (entry == 0x82bc6c20u ? Geometry(Address(a[3]), Address(a[4]))
                                     : Combined(Address(a[3]), Address(a[4])))
                   ? 1
                   : 0;
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(s.r[1] - 16 - 8 * (31 - i)));
    s.lr = m.ReadU32(Address(s.r[1] - 8));
  }
};
} // namespace
bool Apply(GuestAddress entry, GuestMemory &m, Dependencies d, Registers &s) {
  if (entry != 0x82bc69e0u && entry != 0x82bc6c20u && entry != 0x82bc8638u)
    return false;
  Reader{m, d, s}.Run(entry);
  return true;
}
} // namespace lo::semantic::gpu::mesh_geometry_load61
