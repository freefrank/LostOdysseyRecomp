#include "lo_semantics/mesh_triangle_tree61.h"
#include "lo_semantics/crt_reader_upper_follow61.h"
#include "lo_semantics/grid_transform_support61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/mesh_attribute_reorder61.h"
#include <array>
#include <bit>
namespace lo::semantic::gpu::mesh_triangle_tree61 {
namespace {
using recovery_abi::Address;
struct Tree {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  void Call(unsigned target, unsigned lr) {
    s.ctr = target;
    s.lr = lr;
    d.lifetime.guest.CallIndirect(target & ~3u, m, s);
  }
  unsigned Allocate(unsigned bytes, unsigned tag, unsigned lr, bool sdk) {
    if (sdk)
      s.r[3] = Word(0x832df548);
    else
      (void)crt_close_recursive_buffer_context::Apply(0x82bd0798, m,
                                                      d.lifetime.guest, s);
    s.r[4] = bytes;
    s.r[5] = tag;
    Call(Word(Word(Address(s.r[3])) + (sdk ? 8 : 0)), lr);
    return Address(s.r[3]);
  }
  void Reorder(unsigned adapter, unsigned order) {
    s.r[3] = adapter;
    s.r[4] = order;
    s.lr = 0x82bb414c;
    (void)mesh_attribute_reorder61::Apply(0x82bb3cf8, m, d.lifetime.guest, s);
  }
  void Bind(unsigned mesh) {
    m.WriteU32(mesh + 112, Word(mesh));
    m.WriteU32(mesh + 108, Word(mesh + 4));
    s.r[3] = mesh + 100;
    s.r[4] = Word(mesh + 12);
    s.r[5] = Word(mesh + 8);
    (void)diagnostic_format_routes61::Apply(0x82bd18c0, m, d.edge.diagnostics,
                                            s);
  }
  void ReleaseTree(unsigned mesh, unsigned lr) {
    if (auto p = Word(mesh + 44)) {
      s.r[3] = p;
      s.r[4] = 1;
      Call(Word(Word(p)), lr);
      m.WriteU32(mesh + 44, 0);
    }
  }
  unsigned Create(unsigned mesh, unsigned lr) {
    auto p = Allocate(36, 12, lr, false);
    if (p) {
      s.r[3] = p;
      (void)crt_reader_upper_follow61::Apply(0x82bd1aa8, m, d.lifetime.guest,
                                             s);
      p = Address(s.r[3]);
    }
    m.WriteU32(mesh + 44, p);
    return p;
  }
  bool Result(unsigned line, unsigned filename, unsigned ret) {
    if (s.r[3] & 255)
      return true;
    s.r[3] = 4;
    s.r[4] = 0xffffffff820d0000ull | filename;
    s.r[5] = line;
    s.r[6] = 0;
    s.r[7] = 0xffffffff820d5da4ull;
    s.lr = ret;
    (void)diagnostic_format_routes61::Apply(0x82b9c298, m, d.edge.diagnostics,
                                            s);
    return false;
  }
  bool Load(unsigned mesh, unsigned stream) {
    ReleaseTree(mesh, 0x82bc5e1c);
    Bind(mesh);
    auto tree = Create(mesh, 0x82bc5e64);
    s.r[3] = tree;
    s.r[4] = mesh + 100;
    s.r[5] = stream;
    Call(Word(Word(tree) + 4), 0x82bc5e94);
    return Result(289, 26892, 0x82bc5ec0);
  }
  bool Build(unsigned adapter, unsigned limit, std::uint64_t threshold) {
    auto mesh = Word(adapter);
    ReleaseTree(mesh, 0x82bb41a4);
    Bind(mesh);
    auto options = Address(s.r[1] + 80);
    s.r[3] = options;
    (void)grid_transform_support61::Apply(0x82bd1278, m, d.lifetime.fp, s);
    m.WriteU32(options, mesh + 100);
    m.WriteU32(options + 4, 1);
    m.WriteU32(options + 8, 34);
    m.WriteU8(options + 24, 1);
    m.WriteU8(options + 25, m.ReadU8(0x832dc188) == 0);
    if (limit != 255) {
      m.WriteU32(options + 12, std::bit_cast<unsigned>(
                                   float(std::bit_cast<double>(threshold))));
      m.WriteU32(options + 16, limit);
    }
    m.WriteU8(options + 26, 0);
    m.WriteU8(options + 27, 1);
    m.WriteU32(mesh + 100, 0x82bb4138);
    m.WriteU32(mesh + 104, adapter);
    auto tree = Create(mesh, 0x82bb4260);
    s.r[3] = tree;
    s.r[4] = options;
    Call(Word(Word(tree) + 8), 0x82bb4298);
    return Result(164, 25292, 0x82bb42c4);
  }
  void Run(unsigned entry) {
    auto a = s.r;
    auto old = s.r[1], f = s.fpr_bits[31];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(old));
    if (entry == 0x82bb4138) {
      Reorder(Address(a[5]), Address(a[4]));
      s.r[3] = 0;
    } else
      s.r[3] = (entry == 0x82bc5de8
                    ? Load(Address(a[3]), Address(a[4]))
                    : Build(Address(a[3]), Address(a[4]), s.fpr_bits[1]))
                   ? 1
                   : 0;
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.fpr_bits[31] = f;
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82bb4138 && e != 0x82bc5de8 && e != 0x82bb4160)
    return false;
  Tree{m, d, s}.Run(e);
  return true;
}
} // namespace lo::semantic::gpu::mesh_triangle_tree61
