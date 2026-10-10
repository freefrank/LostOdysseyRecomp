#include "lo_semantics/mesh_triangle_clean61.h"
#include "lo_semantics/mesh_indexed_workspace61.h"
#include "lo_semantics/mesh_indexed_vertex_output61.h"
#include "lo_semantics/mesh_indexed_channels61.h"
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/mesh_cook_storage61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/recovery_abi.h"
#include <initializer_list>
namespace lo::semantic::gpu::mesh_triangle_clean61 {
namespace {
using recovery_abi::Address;
struct Clean {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  void Zero(unsigned p, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i)
      m.WriteU8(p + i, 0);
  }
  void Copy(unsigned dst, unsigned src, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i)
      m.WriteU8(dst + i, m.ReadU8(src + i));
  }
  void Call(unsigned e, std::initializer_list<unsigned> args) {
    unsigned i = 3;
    for (auto v : args)
      s.r[i++] = v;
    switch (e) {
    case 0x82bbdf60:
    case 0x82bbf628:
    case 0x82bbe310:
    case 0x82bbf590:
      (void)mesh_indexed_workspace61::Apply(e, m, d.edge.engine, s);
      break;
    case 0x82bc0ba8:
      (void)mesh_indexed_vertex_output61::Apply(e, m, d.edge.engine, s);
      break;
    case 0x82bb3c00:
      (void)mesh_indexed_channels61::Apply(e, m, d.edge.engine, s);
      break;
    case 0x82bd2a08:
    case 0x82bd2c08:
      (void)object_sort_support61::Apply(e, m,
                                         {d.lifetime.guest, d.lifetime.fp}, s);
      break;
    case 0x82bc5be0:
    case 0x82bc5c38:
      (void)mesh_triangle_storage61::Apply(e, m, d, s);
      break;
    case 0x82bbd4c0:
      (void)mesh_edge_build61::Apply(e, m, d.edge, s);
      break;
    case 0x82bbddf0:
      (void)mesh_cache_build61::Apply(e, m, d, s);
      break;
    case 0x82bbd4e0:
      (void)mesh_cook_storage61::Apply(e, m, d.lifetime, s);
      break;
    }
  }
  unsigned Allocate(unsigned bytes, unsigned tag, unsigned lr) {
    s.r[3] = Word(0x832df548);
    s.r[4] = bytes;
    s.r[5] = tag;
    s.ctr = Word(Word(Address(s.r[3])) + 8);
    s.lr = lr;
    d.lifetime.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    return Address(s.r[3]);
  }
  void ReleaseField(unsigned mesh, unsigned field, unsigned lr) {
    if (auto p = Word(mesh + field)) {
      s.r[3] = Word(0x832df548);
      s.r[4] = p;
      s.ctr = Word(Word(Address(s.r[3])) + 20);
      s.lr = lr;
      d.lifetime.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
      m.WriteU32(mesh + field, 0);
    }
  }
  bool Replace(unsigned triangle, unsigned from, unsigned to) {
    for (unsigned i = 0; i < 3; ++i)
      if (Word(triangle + 4 * i) == from) {
        m.WriteU32(triangle + 4 * i, to);
        return true;
      }
    return false;
  }
  bool Body(unsigned adapter) {
    auto mesh = Word(adapter), sp = Address(s.r[1]);
    unsigned input = sp + 112, face = sp + 192, topology = sp + 160,
             edges = sp + 224, positions = sp + 256, out = sp + 272,
             workspace = sp + 368;
    Zero(input, 40);
    m.WriteU32(input, Word(mesh));
    m.WriteU32(input + 4, Word(mesh + 4));
    m.WriteU32(input + 16, Word(mesh + 8));
    for (unsigned off : {28, 33, 34, 35, 36, 38})
      m.WriteU8(input + off, 1);
    Call(0x82bbdf60, {workspace});
    Call(0x82bbf628, {workspace, input});
    if (!(s.r[3] & 255)) {
      Call(0x82bbf590, {workspace});
      return false;
    }
    for (unsigned i = 0; i < Word(mesh + 4); ++i) {
      Zero(face, 28);
      m.WriteU32(face, i);
      m.WriteU32(face + 4, 0xffffffff);
      m.WriteU32(face + 8, 1);
      m.WriteU32(face + 12, Word(mesh + 12) + 12 * i);
      Call(0x82bbe310, {workspace, face});
    }
    Zero(out, 96);
    Call(0x82bc0ba8, {workspace, out});
    if (!(s.r[3] & 255)) {
      Call(0x82bbf590, {workspace});
      return false;
    }
    ReleaseField(mesh, 12, 0x82bb46cc);
    ReleaseField(mesh, 8, 0x82bb46f8);
    ReleaseField(mesh, 80, 0x82bb4724);
    auto triangles = Word(out), map = Word(out + 28);
    bool changed = false;
    if (map)
      for (unsigned i = 0; i < triangles; ++i)
        if (Word(map + 4 * i) != i) {
          changed = true;
          break;
        }
    if (changed) {
      auto p = Allocate(triangles * 4, 269, 0x82bb4798);
      m.WriteU32(mesh + 80, p);
      Copy(p, map, triangles * 4);
    }
    if (Word(mesh + 76) && Word(mesh + 80)) {
      auto old = Word(mesh + 76), p = Allocate(triangles * 2, 263, 0x82bb47f4);
      for (unsigned i = 0; i < triangles; ++i)
        m.WriteU16(p + 2 * i,
                   m.ReadU16(old + 2 * Word(Word(mesh + 80) + 4 * i)));
      ReleaseField(mesh, 76, 0x82bb4868);
      m.WriteU32(mesh + 76, p);
    }
    Call(0x82bc5be0, {mesh, Word(out + 44)});
    Call(0x82bc5c38, {mesh, triangles});
    for (unsigned i = 0; i < Word(mesh); ++i)
      Copy(Word(mesh + 8) + 12 * i,
           Word(out + 60) + 12 * Word(Word(out + 48) + 4 * i), 12);
    Copy(Word(mesh + 12), Word(out + 12), 12 * triangles);
    Zero(topology, 24);
    m.WriteU32(topology, Word(mesh + 4));
    m.WriteU32(topology + 4, Word(mesh + 12));
    m.WriteU8(topology + 13, 1);
    m.WriteU32(topology + 20, Word(0x82000dac));
    Call(0x82bbd4c0, {edges});
    Call(0x82bbddf0, {edges, topology});
    if (!(s.r[3] & 255)) {
      Call(0x82bbd4e0, {edges});
      Call(0x82bbf590, {workspace});
      return false;
    }
    Call(0x82bd2a08, {positions});
    unsigned count = Word(mesh);
    for (unsigned i = 0; i < count; ++i)
      Call(0x82bb3c00, {positions, Word(mesh + 8) + 12 * i});
    for (unsigned edge = 0; edge < Word(edges); ++edge) {
      auto adjacency = Word(edges + 16) + 8 * edge;
      unsigned incidence = m.ReadU16(adjacency + 2);
      if (incidence <= 2)
        continue;
      unsigned a = Word(Word(edges + 4) + 8 * edge),
               b = Word(Word(edges + 4) + 8 * edge + 4),
               start = Word(adjacency + 4), mask = 0, perturb = 1, first = 0,
               second = 0;
      for (unsigned j = 0; j < incidence - 2; ++j) {
        if (!(j & 1)) {
          if (++mask == 8) {
            mask = 1;
            ++perturb;
          }
          Copy(sp + 80, Word(mesh + 8) + 12 * a, 12);
          Copy(sp + 96, Word(mesh + 8) + 12 * b, 12);
          for (unsigned axis = 0; axis < 3; ++axis)
            if (mask & (1 << axis)) {
              m.WriteU32(sp + 80 + 4 * axis,
                         Word(sp + 80 + 4 * axis) ^ perturb);
              m.WriteU32(sp + 96 + 4 * axis,
                         Word(sp + 96 + 4 * axis) ^ perturb);
            }
          Call(0x82bb3c00, {positions, sp + 80});
          Call(0x82bb3c00, {positions, sp + 96});
          first = count;
          second = count + 1;
          count += 2;
        }
        auto triangle =
            Word(mesh + 12) + 12 * Word(Word(edges + 20) + 4 * (start + j));
        (void)Replace(triangle, a, first);
        (void)Replace(triangle, b, second);
      }
    }
    if (count != Word(mesh)) {
      ReleaseField(mesh, 8, 0x82bb4c78);
      auto p = Allocate(count * 12, 261, 0x82bb4ca0);
      m.WriteU32(mesh + 8, p);
      Copy(p, Word(positions + 8), count * 12);
      m.WriteU32(mesh, count);
    }
    Call(0x82bd2c08, {positions});
    Call(0x82bbd4e0, {edges});
    Call(0x82bbf590, {workspace});
    return true;
  }
  void Run(unsigned entry) {
    auto a = s.r;
    auto old = s.r[1];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 1536;
    m.WriteU32(Address(s.r[1]), Address(old));
    s.r[3] = (entry == 0x82bd9188
                  ? Replace(Address(a[3]), Address(a[4]), Address(a[5]))
                  : Body(Address(a[3])))
                 ? 1
                 : 0;
    s.r[1] += 1536;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82bb4540 && e != 0x82bd9188)
    return false;
  Clean{m, d, s}.Run(e);
  return true;
}
} // namespace lo::semantic::gpu::mesh_triangle_clean61
