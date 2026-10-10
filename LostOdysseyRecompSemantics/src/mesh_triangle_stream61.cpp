#include "lo_semantics/mesh_triangle_stream61.h"
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/mesh_triangle_cache61.h"
#include "lo_semantics/mesh_triangle_tree61.h"
#include "lo_semantics/mesh_triangle_bounds61.h"
#include "lo_semantics/mesh_stream_codec61.h"
#include "lo_semantics/mesh_stream_write61.h"
#include "lo_semantics/crt_reader_object_chain61.h"
#include "lo_semantics/serialization_control61.h"
#include "lo_semantics/recovery_abi.h"
#include <algorithm>
#include <bit>
namespace lo::semantic::gpu::mesh_triangle_stream61 {
namespace {
using recovery_abi::Address;
struct Stream {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  void Codec(unsigned e) {
    (void)mesh_stream_codec61::Apply(e, m, {d.lifetime.guest, d.lifetime.fp},
                                     s);
  }
  void Virtual(unsigned stream, unsigned slot) {
    s.r[3] = stream;
    s.ctr = Word(Word(stream) + slot);
    d.lifetime.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  unsigned Scalar(unsigned stream, bool swap, unsigned width = 4) {
    Virtual(stream, width == 1 ? 4 : width == 2 ? 8 : 12);
    unsigned v = Address(s.r[3]);
    if (width == 1)
      return v & 255;
    return swap ? (width == 2 ? __builtin_bswap16(std::uint16_t(v))
                              : __builtin_bswap32(v))
                : v;
  }
  void Raw(unsigned stream, unsigned p, unsigned bytes, bool write) {
    s.r[4] = p;
    s.r[5] = bytes;
    Virtual(stream, write ? 48 : 24);
  }
  void Words(unsigned stream, unsigned p, unsigned count, bool swap,
             bool write) {
    s.r[3] = p;
    s.r[4] = count;
    s.r[5] = swap;
    s.r[6] = stream;
    Codec(write ? 0x82badcd0 : 0x82badae0);
  }
  void Put(unsigned stream, unsigned value, bool swap, unsigned width = 4) {
    s.r[3] = value;
    s.r[4] = swap;
    s.r[5] = stream;
    if (width == 1) {
      s.r[4] = value;
      Virtual(stream, 28);
    } else
      Codec(width == 2 ? 0x82bad9a0 : 0x82bada00);
  }
  unsigned Allocate(unsigned bytes, unsigned tag) {
    s.r[3] = Word(0x832df548);
    s.r[4] = bytes;
    s.r[5] = tag;
    Virtual(Address(s.r[3]), 8);
    return Address(s.r[3]);
  }
  void Storage(unsigned e, unsigned self, unsigned arg = 0) {
    s.r[3] = self;
    s.r[4] = arg;
    (void)mesh_triangle_storage61::Apply(e, m, d, s);
  }
  void Adaptive(unsigned maximum, unsigned count, unsigned out, unsigned stream,
                bool swap) {
    if (maximum > 65535) {
      Words(stream, out, count, swap, false);
      return;
    }
    for (unsigned i = 0; i < count; ++i) {
      auto v = Scalar(stream, swap, maximum <= 255 ? 1 : 2);
      m.WriteU32(out + 4 * i, v);
      s.r[3] = v;
    }
  }
  void Read(unsigned self, unsigned stream) {
    Storage(0x82bc5fa0, self + 4);
    auto sp = Address(s.r[1]);
    s.r[3] = 'M';
    s.r[4] = 'E';
    s.r[5] = 'S';
    s.r[6] = 'H';
    s.r[7] = sp + 100;
    s.r[8] = sp + 80;
    s.r[9] = stream;
    Codec(0x82bade98);
    if (!(s.r[3] & 255)) {
      s.r[3] = 0;
      return;
    }
    if (Word(sp + 100) < Word(0x8321614c)) {
      s.r[3] = 4;
      s.r[4] = 0xffffffff820d5a68ull;
      s.r[5] = 149;
      s.r[6] = 0;
      s.r[7] = 0xffffffff820d5b58ull;
      (void)diagnostic_format_routes61::Apply(0x82b9c298, m, d.edge.diagnostics,
                                              s);
      s.r[3] = 0;
      return;
    }
    bool swap = m.ReadU8(sp + 80) != 0;
    auto flags = Scalar(stream, swap);
    m.WriteU32(self + 96, 0);
    for (unsigned off : {100, 176, 180})
      m.WriteU32(self + off, Scalar(stream, swap));
    auto vertices = Scalar(stream, swap);
    Storage(0x82bc5be0, self + 4, vertices);
    auto triangles = Scalar(stream, swap);
    Storage(0x82bc5c38, self + 4, triangles);
    Words(stream, Word(self + 12), 3 * vertices, swap, false);
    auto out = Word(self + 16);
    unsigned width = flags & 8 ? 1 : flags & 16 ? 2 : 4;
    if (width == 4)
      Words(stream, out, 3 * triangles, swap, false);
    else
      for (unsigned i = 0; i < 3 * triangles; ++i)
        m.WriteU32(out + 4 * i, Scalar(stream, swap, width));
    if (flags & 1) {
      Storage(0x82bc5c90, self + 4);
      out = Word(self + 80);
      Raw(stream, out, 2 * triangles, false);
      if (swap)
        for (unsigned i = 0; i < triangles; ++i)
          m.WriteU16(out + 2 * i, __builtin_bswap16(m.ReadU16(out + 2 * i)));
    }
    if (flags & 2) {
      Storage(0x82bc5d00, self + 4);
      auto maximum = Scalar(stream, swap);
      Adaptive(maximum, triangles, Word(self + 84), stream, swap);
    }
    auto groups = Scalar(stream, swap), categories = Scalar(stream, swap);
    m.WriteU32(self + 28, groups);
    m.WriteU32(self + 32, categories);
    if (groups) {
      out = Allocate(2 * triangles, 274);
      m.WriteU32(self + 36, out);
      Raw(stream, out, 2 * triangles, false);
      if (swap)
        for (unsigned i = 0; i < triangles; ++i)
          m.WriteU16(out + 2 * i, __builtin_bswap16(m.ReadU16(out + 2 * i)));
    }
    if (categories) {
      unsigned bytes = triangles * (categories < 256 ? 1 : 2);
      out = Allocate(bytes, categories < 256 ? 275 : 274);
      m.WriteU32(self + 40, out);
      Raw(stream, out, bytes, false);
    }
    (void)Scalar(stream, swap);
    m.WriteU32(sp + 104, 0x820d5b24);
    m.WriteU32(sp + 108, stream);
    s.r[3] = self + 4;
    s.r[4] = sp + 104;
    (void)mesh_triangle_tree61::Apply(0x82bc5de8, m, d, s);
    m.WriteU32(sp + 104, 0x820d59e0);
    for (unsigned off : {168, 152, 156, 160, 164, 128, 132, 136, 140, 144, 148})
      m.WriteU32(self + off, Scalar(stream, swap));
    s.r[3] = self;
    (void)mesh_triangle_bounds61::Apply(0x82b9d410, m, d, s);
    m.WriteU32(self + 52, Word(self + 168));
    for (unsigned i = 0; i < 6; ++i)
      m.WriteU32(self + 56 + 4 * i, Word(self + 128 + 4 * i));
    auto mass = Scalar(stream, swap);
    m.WriteU32(self + 208, mass);
    if (std::bit_cast<float>(mass) != std::bit_cast<float>(Word(0x82000e40))) {
      Words(stream, self + 212, 9, swap, false);
      Words(stream, self + 248, 3, swap, false);
    }
    auto bytes = Scalar(stream, swap);
    if (bytes) {
      out = Allocate(bytes, 20);
      m.WriteU32(self + 44, out);
      Raw(stream, out, bytes, false);
    }
    s.r[3] = 1;
  }
  void Write(unsigned self, unsigned stream) {
    (void)serialization_control61::Apply(0x82b9cb70, m,
                                         {d.lifetime.guest, d.lifetime.fp}, s);
    bool swap = Address(s.r[3]) != 0;
    s.r[3] = 'M';
    s.r[4] = 'E';
    s.r[5] = 'S';
    s.r[6] = 'H';
    s.r[7] = Word(0x83216174);
    s.r[8] = swap;
    s.r[9] = stream;
    (void)mesh_stream_write61::Apply(0x82badd60, m,
                                     {d.lifetime.guest, d.lifetime.fp}, s);
    if (!(s.r[3] & 255)) {
      s.r[3] = 0;
      return;
    }
    auto vertices = Word(self + 4), triangles = Word(self + 8),
         indices = Word(self + 16);
    unsigned maximum = 0;
    for (unsigned i = 0; i < 3 * triangles; ++i)
      maximum = std::max(maximum, Word(indices + 4 * i));
    unsigned flags = (Word(self + 80) ? 1 : 0) | (Word(self + 84) ? 2 : 0) |
                     (Word(self + 96) & 4) |
                     (maximum <= 255     ? 8
                      : maximum <= 65535 ? 16
                                         : 0);
    Put(stream, flags, swap);
    for (unsigned off : {100, 176, 180, 4, 8})
      Put(stream, Word(self + off), swap);
    Words(stream, Word(self + 12), 3 * vertices, swap, true);
    unsigned width = flags & 8 ? 1 : flags & 16 ? 2 : 4;
    if (width == 4)
      Words(stream, indices, 3 * triangles, swap, true);
    else
      for (unsigned i = 0; i < 3 * triangles; ++i)
        Put(stream, Word(indices + 4 * i), swap, width);
    if (auto p = Word(self + 80)) {
      s.r[3] = p;
      s.r[4] = triangles;
      s.r[5] = swap;
      s.r[6] = stream;
      Codec(0x82badc58);
    }
    if (auto p = Word(self + 84)) {
      maximum = 0;
      for (unsigned i = 0; i < triangles; ++i)
        maximum = std::max(maximum, Word(p + 4 * i));
      Put(stream, maximum, swap);
      for (unsigned i = 0; i < triangles; ++i)
        Put(stream, Word(p + 4 * i), swap,
            maximum <= 255     ? 1
            : maximum <= 65535 ? 2
                               : 4);
    }
    Put(stream, Word(self + 28), swap);
    Put(stream, Word(self + 32), swap);
    if (Word(self + 28)) {
      s.r[3] = Word(self + 36);
      s.r[4] = triangles;
      s.r[5] = swap;
      s.r[6] = stream;
      Codec(0x82badc58);
    }
    if (Word(self + 32))
      Raw(stream, Word(self + 40), triangles * (Word(self + 32) < 256 ? 1 : 2),
          true);
    s.r[3] = Word(self + 48);
    s.r[4] = swap;
    s.r[5] = stream;
    (void)crt_reader_object_chain61::Apply(
        0x82bb44a8, m, {d.lifetime.guest, d.edge.engine.sort.accepted}, s);
    for (unsigned off : {168, 152, 156, 160, 164, 128, 132, 136, 140, 144, 148})
      Put(stream, Word(self + off), swap);
    s.r[3] = self;
    (void)mesh_triangle_cache61::Apply(0x82ba6868, m, d, s);
    auto mass = Address(s.r[3]);
    Put(stream, Word(mass ? mass : 0x82000e40), swap);
    if (mass) {
      Words(stream, mass + 4, 9, swap, true);
      Words(stream, mass + 40, 3, swap, true);
    }
    auto bits = Word(self + 44);
    if (bits) {
      if (!Word(self + 92)) {
        s.r[3] = self + 4;
        (void)mesh_triangle_cache61::Apply(0x82bc5ed8, m, d, s);
      }
      auto cache = Word(self + 92);
      if (cache && Word(cache + 8) == triangles)
        for (unsigned i = 0; i < triangles; ++i)
          for (unsigned j = 0; j < 3; ++j)
            if (Word(Word(cache + 12) + 12 * i + 4 * j) & 0x80000000u)
              m.WriteU8(bits + i, m.ReadU8(bits + i) | (8u << j));
      Put(stream, triangles, swap);
      Raw(stream, bits, triangles, true);
    } else
      Put(stream, 0, swap);
    s.r[3] = 1;
  }
  void Run(unsigned entry) {
    auto a = Address(s.r[3]), b = Address(s.r[4]), c = Address(s.r[5]),
         e = Address(s.r[6]), f = Address(s.r[7]);
    auto old = s.r[1];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(old));
    if (entry == 0x82ba6b18)
      Write(a, b);
    else if (entry == 0x82b9d4f8)
      Read(a, b);
    else
      Adaptive(a, b, c, e, (f & 255) != 0);
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82ba6b18 && e != 0x82b9d4f8 && e != 0x82bae0c0)
    return false;
  Stream{m, d, s}.Run(e);
  return true;
}
} // namespace lo::semantic::gpu::mesh_triangle_stream61
