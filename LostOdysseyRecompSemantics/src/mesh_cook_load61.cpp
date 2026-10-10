#include "lo_semantics/mesh_cook_load61.h"
#include "lo_semantics/mesh_geometry_load61.h"
#include "lo_semantics/mesh_cook_scale61.h"
#include "lo_semantics/mesh_cook_storage61.h"
#include "lo_semantics/mesh_cook_support61.h"
#include "lo_semantics/mesh_stream_codec61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::mesh_cook_load61 {
namespace {
using recovery_abi::Address;
struct Loader {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  void Call(unsigned target, unsigned lr) {
    s.ctr = target;
    s.lr = lr;
    d.lifetime.guest.CallIndirect(target & ~3u, m, s);
  }
  void Codec(unsigned entry) {
    (void)mesh_stream_codec61::Apply(entry, m,
                                     {d.lifetime.guest, d.lifetime.fp}, s);
  }
  void Diagnostic(unsigned line, unsigned text, unsigned lr) {
    s.r[3] = 4;
    s.r[4] = 0xffffffff820d67d0ull;
    s.r[5] = line;
    s.r[6] = 0;
    s.r[7] = 0xffffffff00000000ull | text;
    s.lr = lr;
    (void)diagnostic_format_routes61::Apply(0x82b9c298, m, d.edge.diagnostics,
                                            s);
  }
  bool Tree(unsigned self, unsigned stream) {
    s.r[3] = self + 8;
    (void)owned_tree_reorder_support61::Apply(0x82bd20f0, m, d.lifetime, s);
    s.r[3] = self;
    Call(Word(Word(self) + 48), 0x82bc4db4);
    m.WriteU32(self + 96, Address(s.r[3]));
    s.r[3] = self;
    Call(Word(Word(self) + 52), 0x82bc4dd0);
    m.WriteU32(self + 92, Address(s.r[3]));
    s.r[3] = self + 84;
    s.r[4] = Word(self + 164);
    s.r[5] = Word(self + 172);
    (void)diagnostic_format_routes61::Apply(0x82bd18c0, m, d.edge.diagnostics,
                                            s);
    s.r[3] = self + 8;
    s.r[4] = self + 84;
    s.r[5] = stream;
    Call(Word(Word(self + 8) + 4), 0x82bc4e04);
    if (s.r[3] & 255)
      return true;
    Diagnostic(352, 0x820d5da4, 0x82bc4e30);
    return false;
  }
  bool Load(unsigned self, unsigned stream) {
    auto sp = Address(s.r[1]);
    s.r[3] = 'C';
    s.r[4] = 'V';
    s.r[5] = 'X';
    s.r[6] = 'M';
    s.r[7] = sp + 84;
    s.r[8] = sp + 80;
    s.r[9] = stream;
    Codec(0x82bade98);
    if (!(s.r[3] & 255))
      return false;
    if (Word(sp + 84) < Word(0x832161cc)) {
      Diagnostic(141, 0x820d6828, 0x82bc52e4);
      return false;
    }
    unsigned swap = m.ReadU8(sp + 80), version = Word(sp + 84);
    s.r[3] = swap;
    s.r[4] = stream;
    Codec(0x82bad8b8);
    m.WriteU32(sp + 88, 0x820d5b24);
    m.WriteU32(sp + 92, stream);
    s.r[3] = self + 156;
    s.r[4] = sp + 88;
    Call(Word(Word(self + 156) + 4), 0x82bc5328);
    m.WriteU32(sp + 88, 0x820d59e0);
    if (!(s.r[3] & 255))
      return false;
    m.WriteU32(self + 44, self + 236);
    s.r[3] = swap;
    s.r[4] = stream;
    Codec(0x82bad8b8);
    m.WriteU32(sp + 96, 0x820d5b24);
    m.WriteU32(sp + 100, stream);
    // Nested logical frame keeps header and scalar scratch separate.
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), sp);
    bool tree = Tree(self, sp + 96);
    s.r[1] += 512;
    if (!tree)
      return false;
    m.WriteU32(sp + 96, 0x820d59e0);
    s.r[3] = sp + 112;
    s.r[4] = 12;
    s.r[5] = swap;
    s.r[6] = stream;
    Codec(0x82badae0);
    constexpr unsigned offsets[]{152, 136, 140, 144, 148, 112,
                                 116, 120, 124, 128, 132, 292};
    for (unsigned i = 0; i < 12; ++i)
      m.WriteU32(self + offsets[i], Word(sp + 112 + 4 * i));
    m.WriteU32(self + 76, Word(self + 152));
    for (unsigned i = 0; i < 3; ++i) {
      m.WriteU32(self + 52 + 4 * i, Word(self + 112 + 4 * i));
      m.WriteU32(self + 64 + 4 * i, Word(self + 124 + 4 * i));
    }
    if (std::bit_cast<float>(Word(self + 292)) !=
        std::bit_cast<float>(Word(0x82000e40))) {
      s.r[3] = self + 296;
      s.r[4] = 9;
      s.r[5] = swap;
      s.r[6] = stream;
      Codec(0x82badae0);
      s.r[3] = self + 332;
      s.r[4] = 3;
      s.r[5] = swap;
      s.r[6] = stream;
      Codec(0x82badae0);
    }
    if (version < 2 || Word(self + 168) > 32) {
      if (auto p = Word(self + 288)) {
        s.r[3] = p;
        s.r[4] = 1;
        Call(Word(Word(p)), 0x82bc54a0);
        m.WriteU32(self + 288, 0);
      }
      (void)crt_close_recursive_buffer_context::Apply(0x82bd0798, m,
                                                      d.lifetime.guest, s);
      s.r[4] = 36;
      s.r[5] = 10;
      Call(Word(Word(Address(s.r[3]))), 0x82bc54c4);
      if (s.r[3]) {
        s.r[4] = self + 156;
        (void)mesh_cook_support61::Apply(0x82bc61b0, m,
                                         {d.lifetime, d.edge.diagnostics}, s);
      }
      auto p = Address(s.r[3]);
      m.WriteU32(self + 288, p);
      if (p) {
        m.WriteU32(sp + 104, 0x820d5b24);
        m.WriteU32(sp + 108, stream);
        s.r[3] = p;
        s.r[4] = sp + 104;
        Call(Word(Word(p) + 4), 0x82bc5504);
        m.WriteU32(self + 48, Word(self + 288) + 16);
      }
    }
    return true;
  }
  bool ScaleExport(unsigned stream, unsigned output, std::uint64_t scale) {
    // Preserve source NaN behavior: only ordered <= zero rejects.
    if (std::bit_cast<double>(scale) <=
        double(std::bit_cast<float>(Word(0x82000e50))))
      return false;
    auto owner = Address(s.r[1] + 80);
    s.r[3] = owner;
    (void)mesh_cook_storage61::Apply(0x82b9e4b0, m, d.lifetime, s);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(s.r[1] + 512));
    bool ok = Load(owner, stream);
    s.r[1] += 512;
    if (ok) {
      s.r[3] = owner;
      s.fpr_bits[1] = scale;
      (void)mesh_cook_scale61::Apply(0x82b9ec98, m,
                                     {d.lifetime, d.edge.diagnostics}, s);
      ok = (s.r[3] & 255) != 0;
    }
    if (ok) {
      s.r[3] = owner;
      s.r[4] = output;
      s.r[5] = 0;
      (void)mesh_cook_stream61::Apply(0x82b9f6f0, m, d, s);
      ok = (s.r[3] & 255) != 0;
    }
    s.r[3] = owner;
    (void)mesh_cook_storage61::Apply(0x82b9e518, m, d.lifetime, s);
    return ok;
  }
  void Run(unsigned entry) {
    auto old = s.r[1];
    auto a = s.r;
    auto f = s.fpr_bits[31];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 768;
    m.WriteU32(Address(s.r[1]), Address(old));
    s.r[3] = (entry == 0x82bc4d80 ? Tree(Address(a[3]), Address(a[4]))
              : entry == 0x82bc5270
                  ? Load(Address(a[3]), Address(a[4]))
                  : ScaleExport(Address(a[3]), Address(a[5]), s.fpr_bits[1]))
                 ? 1
                 : 0;
    s.r[1] += 768;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.fpr_bits[31] = f;
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82bc4d80 && e != 0x82bc5270 && e != 0x82b9c670)
    return false;
  Loader{m, d, s}.Run(e);
  return true;
}
} // namespace lo::semantic::gpu::mesh_cook_load61
