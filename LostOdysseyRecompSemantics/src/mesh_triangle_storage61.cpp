#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/mesh_triangle_links61.h"
#include "lo_semantics/mesh_triangle_normals61.h"
#include "lo_semantics/mesh_cook_storage61.h"
#include "lo_semantics/grid_transform_support61.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
namespace lo::semantic::gpu::mesh_triangle_storage61 {
namespace {
using recovery_abi::Address;
struct Storage {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  void Invoke(unsigned p, unsigned lr) {
    s.ctr = p;
    s.lr = lr;
    d.lifetime.guest.CallIndirect(p & ~3u, m, s);
  }
  unsigned Allocate(unsigned size, unsigned tag, unsigned lr) {
    s.r[3] = Word(0x832df548);
    s.r[4] = size;
    s.r[5] = tag;
    Invoke(Word(Word(Address(s.r[3])) + 8), lr);
    return Address(s.r[3]);
  }
  void Free(unsigned p, unsigned lr, bool sdk = true) {
    if (sdk)
      s.r[3] = Word(0x832df548);
    else
      (void)crt_close_recursive_buffer_context::Apply(0x82bd0798, m,
                                                      d.lifetime.guest, s);
    s.r[4] = p;
    Invoke(Word(Word(Address(s.r[3])) + (sdk ? 20 : 12)), lr);
  }
  void Destroy(unsigned p, unsigned lr) {
    s.r[3] = p;
    s.r[4] = 1;
    Invoke(Word(Word(p)), lr);
  }
  void Initialize(unsigned self) {
    m.WriteU32(self + 96, Word(0x82000d6c));
    for (unsigned off : {76, 80, 84, 88, 92})
      m.WriteU32(self + off, 0);
    s.r[3] = self + 100;
    (void)grid_transform_support61::Apply(0x82f2b308, m, d.lifetime.fp, s);
    s.r[3] = self;
    s.r[4] = 0;
    s.r[5] = 76;
    crt_reader_chain61::ApplySupport_B7BC40(m, d.edge.engine.sort.accepted, s);
    s.r[3] = self;
  }
  void Construct(unsigned self) {
    m.WriteU32(self, 0x820d5c58);
    Initialize(self + 4);
    for (unsigned off : {128, 132, 136})
      m.WriteU32(self + off, Word(0x82000e0c));
    for (unsigned off : {140, 144, 148})
      m.WriteU32(self + off, Word(0x82000d64));
    for (unsigned off : {168, 180})
      m.WriteU32(self + off, Word(0x82000e50));
    for (unsigned off : {172, 176})
      m.WriteU32(self + off, 255);
    for (unsigned off : {184, 188, 192, 196, 200, 204})
      m.WriteU32(self + off, 0);
    m.WriteU32(self + 208, Word(0x82000e40));
    s.r[3] = self;
  }
  void BaseCleanup(unsigned self) {
    auto p = Word(self + 84);
    if (p && p != 1) {
      s.r[3] = p;
      (void)mesh_triangle_links61::Apply(0x82bc3ec0, m, d, s);
      Free(p, 0x82bc5fe8, false);
    }
    m.WriteU32(self + 84, 0);
    for (auto pair : std::array<std::array<unsigned, 2>, 3>{
             {{40, 0x82bc6010}, {36, 0x82bc603c}, {32, 0x82bc6060}}})
      if (auto q = Word(self + pair[0])) {
        Free(q, pair[1]);
        m.WriteU32(self + pair[0], 0);
      }
    if (auto q = Word(self + 44)) {
      Destroy(q, 0x82bc6084);
      m.WriteU32(self + 44, 0);
    }
    if (auto q = Word(self + 88)) {
      s.r[3] = q;
      (void)mesh_cook_storage61::Apply(0x82bbd4e0, m, d.lifetime, s);
      Free(q, 0x82bc60b4, false);
      m.WriteU32(self + 88, 0);
    }
    for (auto pair : std::array<std::array<unsigned, 2>, 5>{{{16, 0x82bc60d8},
                                                             {80, 0x82bc60fc},
                                                             {76, 0x82bc6120},
                                                             {12, 0x82bc6144},
                                                             {8, 0x82bc6168}}})
      if (auto q = Word(self + pair[0])) {
        Free(q, pair[1]);
        m.WriteU32(self + pair[0], 0);
      }
  }
  void Cleanup(unsigned self) {
    for (auto pair : std::array<std::array<unsigned, 2>, 3>{
             {{192, 0x82b9d390}, {196, 0x82b9d3b4}, {204, 0x82b9d3d8}}})
      if (auto q = Word(self + pair[0])) {
        Free(q, pair[1]);
        m.WriteU32(self + pair[0], 0);
      }
    BaseCleanup(self + 4);
    if (auto q = Word(self + 184)) {
      Destroy(q, 0x82b9d404);
      m.WriteU32(self + 184, 0);
    }
  }
  void Destruct(unsigned self, bool deleting) {
    if (deleting && !self)
      return;
    m.WriteU32(self, 0x820d5c58);
    Cleanup(self);
    BaseCleanup(self + 4);
    m.WriteU32(self, 0x820d5a18);
    if (deleting)
      Free(self, 0x82b9e3e4);
  }
  void Arrays(unsigned entry, unsigned self, unsigned count) {
    unsigned field, size, tag, lr;
    if (entry == 0x82bc5be0 || entry == 0x82bc5c38) {
      bool vertex = entry == 0x82bc5be0;
      m.WriteU32(self + (vertex ? 0 : 4), count);
      field = vertex ? 8 : 12;
      size = count * 12;
      tag = vertex ? 261 : 262;
      lr = vertex ? 0x82bc5c20 : 0x82bc5c78;
    } else {
      count = Word(self + 4);
      if (!count) {
        s.r[3] = 0;
        return;
      }
      bool material = entry == 0x82bc5c90;
      field = material ? 76 : 80;
      size = count * (material ? 2 : 4);
      tag = material ? 263 : 264;
      lr = material ? 0x82bc5ce8 : 0x82bc5d58;
    }
    auto p = Allocate(size, tag, lr);
    m.WriteU32(self + field, p);
  }
  void GenerateNormals(unsigned self) {
    auto out = Allocate(12 * Word(self), 266, 0x82bc5db0);
    m.WriteU32(self + 16, out);
    s.r[3] = Word(self + 4);
    s.r[4] = Word(self);
    s.r[5] = Word(self + 8);
    s.r[6] = Word(self + 12);
    s.r[7] = 0;
    s.r[8] = out;
    s.r[9] = 1;
    (void)mesh_triangle_normals61::Apply(0x82bcc470, m, d, s);
  }
  void View(unsigned self, unsigned part, unsigned selector) {
    if (part || selector >= 3) {
      s.r[3] = 0;
      return;
    }
    if (selector == 2) {
      auto mesh = self + 4;
      if (!Word(mesh + 16))
        GenerateNormals(mesh);
      s.r[3] = Word(mesh + 16);
    } else
      s.r[3] = Word(self + (selector ? 12 : 16));
  }
  void Run(unsigned entry) {
    auto a = s.r;
    auto old = s.r[1];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(old));
    auto self = Address(a[3]);
    switch (entry) {
    case 0x82bc5d70:
      GenerateNormals(self);
      break;
    case 0x82b9e0d8:
      View(self, Address(a[4]), Address(a[5]));
      break;
    case 0x82bc5ab8:
      Initialize(self);
      break;
    case 0x82b9e2d8:
      Construct(self);
      break;
    case 0x82bc5fa0:
    case 0x82bc6178:
      BaseCleanup(self);
      break;
    case 0x82b9d358:
      Cleanup(self);
      break;
    case 0x82b9e220:
      Destruct(self, false);
      break;
    case 0x82b9e388:
      Destruct(self, true);
      break;
    default:
      Arrays(entry, self, Address(a[4]));
      break;
    }
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  switch (e) {
  case 0x82bc5d70:
  case 0x82b9e0d8:
  case 0x82bc5ab8:
  case 0x82bc5be0:
  case 0x82bc5c38:
  case 0x82bc5c90:
  case 0x82bc5d00:
  case 0x82bc5fa0:
  case 0x82bc6178:
  case 0x82b9d358:
  case 0x82b9e220:
  case 0x82b9e2d8:
  case 0x82b9e388:
    Storage{m, d, s}.Run(e);
    return true;
  default:
    return false;
  }
}
} // namespace lo::semantic::gpu::mesh_triangle_storage61
