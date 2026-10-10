#include "lo_semantics/mesh_triangle_cook61.h"
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/mesh_triangle_clean61.h"
#include "lo_semantics/mesh_triangle_tree61.h"
#include "lo_semantics/mesh_triangle_flags61.h"
#include "lo_semantics/mesh_triangle_bounds61.h"
#include "lo_semantics/mesh_triangle_stream61.h"
#include "lo_semantics/mesh_partition61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::mesh_triangle_cook61 {
namespace {
using recovery_abi::Address;
struct Cook {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  void Invoke(unsigned self, unsigned slot) {
    s.r[3] = self;
    s.ctr = Word(Word(self) + slot);
    d.lifetime.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  unsigned Allocate(unsigned bytes, unsigned tag) {
    s.r[4] = bytes;
    s.r[5] = tag;
    Invoke(Word(0x832df548), 8);
    return Address(s.r[3]);
  }
  void Free(unsigned p) {
    s.r[4] = p;
    Invoke(Word(0x832df548), 20);
  }
  void Copy(unsigned out, unsigned in, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i)
      m.WriteU8(out + i, m.ReadU8(in + i));
  }
  void Storage(unsigned e, unsigned self, unsigned n = 0) {
    s.r[3] = self;
    s.r[4] = n;
    (void)mesh_triangle_storage61::Apply(e, m, d, s);
  }
  void Diagnostic(unsigned code, unsigned line, unsigned message) {
    s.r[3] = code;
    s.r[4] = 0xffffffff820d5ffcull;
    s.r[5] = line;
    s.r[6] = 0;
    s.r[7] = 0xffffffff00000000ull | message;
    (void)diagnostic_format_routes61::Apply(0x82b9c298, m, d.edge.diagnostics,
                                            s);
  }
  bool Valid(unsigned desc) {
    auto count = Word(desc), indices = Word(desc + 20), flags = Word(desc + 24);
    return count >= 3 && (indices || count % 3 == 0) &&
           (!Word(desc + 32) || Word(desc + 28) >= 2) &&
           (count <= 65535 || !(flags & 2)) && Word(desc + 16) &&
           Word(desc + 8) >= 12 &&
           (!indices || Word(desc + 12) >= (flags & 2 ? 6u : 12u));
  }
  bool Import(unsigned self, unsigned desc) {
    auto mesh = self + 4;
    Storage(0x82bc5fa0, mesh);
    Storage(0x82bc5be0, mesh, Word(desc));
    auto positions = Address(s.r[3]);
    Storage(0x82bc5c38, mesh, Word(desc + 4));
    auto triangles = Address(s.r[3]);
    for (unsigned i = 0; i < Word(mesh); ++i)
      Copy(positions + 12 * i, Word(desc + 16) + Word(desc + 8) * i, 12);
    auto flags = Word(desc + 24), width = flags & 2 ? 2u : 4u, flip = flags & 1;
    for (unsigned i = 0; i < Word(mesh + 4); ++i) {
      auto input = Word(desc + 20) + Word(desc + 12) * i;
      unsigned order[]{0, 1 + flip, 2 - flip};
      for (unsigned j = 0; j < 3; ++j) {
        auto p = input + width * order[j];
        m.WriteU32(triangles + 12 * i + 4 * j,
                   width == 2 ? m.ReadU16(p) : Word(p));
      }
    }
    if (Word(desc + 32)) {
      Storage(0x82bc5c90, mesh);
      auto out = Address(s.r[3]);
      for (unsigned i = 0; i < Word(mesh + 4); ++i)
        Copy(out + 2 * i, Word(desc + 32) + Word(desc + 28) * i, 2);
    }
    auto adapter = Address(s.r[1]) + 80;
    m.WriteU32(adapter, mesh);
    s.r[3] = adapter;
    (void)mesh_triangle_clean61::Apply(0x82bb4540, m, d, s);
    if (!(s.r[3] & 255)) {
      Diagnostic(4, 361, 0x820d6028);
      return false;
    }
    if (Word(desc + 36) == 255) {
      s.r[3] = adapter;
      (void)mesh_partition61::Apply(0x82bb4cf0, m, d, s);
      if (!(s.r[3] & 255))
        return false;
    }
    return true;
  }
  bool Build(unsigned self, unsigned input) {
    if (!Valid(input)) {
      Diagnostic(1, 35, 0x820d6048);
      return false;
    }
    auto desc = Address(s.r[1]) + 96;
    Copy(desc, input, 52);
    m.WriteU32(self + 100, Word(input + 48));
    m.WriteU32(self + 176, Word(input + 36));
    m.WriteU32(self + 180, Word(input + 40));
    m.WriteU32(self + 96,
               Word(input + 24) &
                   (Word(input + 36) == 255 ? 0xffffffff : 0xfffffffb));
    unsigned temporary = 0;
    if (!Word(desc + 20)) {
      auto count = Word(desc);
      m.WriteU32(desc + 24, Word(desc + 24) & ~2u);
      m.WriteU32(desc + 12, 12);
      m.WriteU32(desc + 4, count / 3);
      temporary = Allocate(4 * count, 255);
      for (unsigned i = 0; i < count; ++i)
        m.WriteU32(temporary + 4 * i, i);
      m.WriteU32(desc + 20, temporary);
    }
    if (!Import(self, desc))
      return false;
    if (temporary)
      Free(temporary);
    auto adapter = Address(s.r[1]) + 80;
    m.WriteU32(adapter, self + 4);
    s.r[3] = adapter;
    s.r[4] = Word(self + 176);
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(
        double(std::bit_cast<float>(Word(self + 180))));
    (void)mesh_triangle_tree61::Apply(0x82bb4160, m, d, s);
    if (auto callback = Word(input + 44)) {
      s.r[4] = callback;
      Invoke(self, 12);
    }
    s.r[3] = self;
    (void)mesh_triangle_bounds61::Apply(0x82ba6458, m, d, s);
    s.r[3] = adapter;
    (void)mesh_triangle_flags61::Apply(0x82bb5230, m, d, s);
    return true;
  }
  bool Main(unsigned desc, unsigned writer) {
    if (!Word(0x832dc414) || !Valid(desc))
      return false;
    auto owner = Allocate(264, 43);
    if (!owner)
      return false;
    Storage(0x82b9e2d8, owner);
    m.WriteU32(owner, 0x820d58a0);
    bool success = Build(owner, desc);
    if (success) {
      s.r[3] = owner;
      s.r[4] = writer;
      (void)mesh_triangle_stream61::Apply(0x82ba6b18, m, d, s);
    }
    Storage(0x82b9e220, owner);
    Free(owner);
    return success;
  }
  void Run(unsigned entry) {
    auto a = Address(s.r[3]), b = Address(s.r[4]);
    auto old = s.r[1];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(old));
    s.r[3] = entry == 0x82ba6238   ? Import(a, b)
             : entry == 0x82ba65c0 ? Build(a, b)
                                   : Main(a, b);
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82ba6238 && e != 0x82ba65c0 && e != 0x82b9cc00)
    return false;
  Cook{m, d, s}.Run(e);
  return true;
}
} // namespace lo::semantic::gpu::mesh_triangle_cook61
