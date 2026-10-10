#include "lo_semantics/cloth_cook61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/cloth_schedule61.h"
#include "lo_semantics/cloth_permutation61.h"
#include "lo_semantics/cloth_import61.h"
#include "lo_semantics/cloth_face_map61.h"
#include "lo_semantics/cloth_triangle_edges61.h"
#include "lo_semantics/cloth_tetra_constraints61.h"
#include "lo_semantics/cloth_storage61.h"
#include "lo_semantics/cloth_stream61.h"
#include <array>
#include <cstdint>
#include <utility>
namespace lo::semantic::gpu::cloth_cook61 {
namespace {
using recovery_abi::Address;
struct Cook {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  void Invoke(unsigned slot) {
    s.r[3] = Word(0x832df548);
    s.ctr = Word(Word(Address(s.r[3])) + slot);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  unsigned Allocate(unsigned bytes) {
    s.r[4] = bytes;
    s.r[5] = 282;
    Invoke(8);
    return Address(s.r[3]);
  }
  void Free(unsigned p) {
    s.r[4] = p;
    Invoke(20);
  }
  void Copy(unsigned out, unsigned in, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i)
      m.WriteU8(out + i, m.ReadU8(in + i));
  }
  void Resize(unsigned v, unsigned count) {
    auto start = Word(v), end = Word(v + 4), used = (end - start) / 4;
    if (count > used) {
      if ((Word(v + 8) - start) / 4 < count) {
        auto p = Allocate(2 * count * 4);
        Copy(p, start, end - start);
        if (start)
          Free(start);
        start = p;
        m.WriteU32(v, p);
        m.WriteU32(v + 8, p + 2 * count * 4);
      }
      for (unsigned i = 4 * used; i < 4 * count; ++i)
        m.WriteU8(start + i, 0);
    }
    end = start + 4 * count;
    m.WriteU32(v + 4, end);
    if (end == start) {
      if (start)
        Free(start);
      m.WriteU32(v, 0);
      m.WriteU32(v + 4, 0);
      m.WriteU32(v + 8, 0);
    } else if (Word(v + 8) > end) {
      s.r[4] = start;
      s.r[5] = count * 4;
      Invoke(16);
      start = Address(s.r[3]);
      m.WriteU32(v, start);
      m.WriteU32(v + 4, start + count * 4);
      m.WriteU32(v + 8, start + count * 4);
    }
  }
  unsigned Append(unsigned vector, unsigned stride) {
    auto start = Word(vector), end = Word(vector + 4),
         used = (end - start) / stride;
    if (Word(vector + 8) <= end) {
      auto bytes = 2 * (used + 1) * stride, p = Allocate(bytes);
      Copy(p, start, end - start);
      if (start)
        Free(start);
      m.WriteU32(vector, p);
      m.WriteU32(vector + 8, p + bytes);
      end = p + used * stride;
    }
    m.WriteU32(vector + 4, end + stride);
    return end;
  }
  void Push(unsigned vector, unsigned value) {
    m.WriteU32(Append(vector, 4), value);
  }
  unsigned Temporary(unsigned count, unsigned stride) {
    if (!count)
      return 0;
    auto p = Allocate(2 * count * stride);
    s.r[4] = p;
    s.r[5] = count * stride;
    Invoke(16);
    return Address(s.r[3]);
  }
  bool Validate(unsigned desc) {
    auto kind = Word(desc), flags = Word(desc + 40);
    if (kind != 1 && kind != 2)
      return false;
    if (Word(desc + 52) && Word(desc + 44) < 4)
      return false;
    if (Word(desc + 56) && Word(desc + 48) < 4)
      return false;
    if (Word(desc + 4) > 65535 && (flags & 2))
      return false;
    if (!Word(desc + 28) || Word(desc + 8) < 12)
      return false;
    auto indices = Word(desc + (kind == 1 ? 32 : 36)),
         stride = Word(desc + (kind == 1 ? 16 : 24));
    if (kind == 2 && !indices)
      return false;
    if (indices && stride < ((flags & 2) ? 6u : 12u))
      return false;
    return true;
  }
  bool Build(unsigned self, unsigned desc, unsigned workspace) {
    if (!Validate(desc))
      return false;
    auto kind = Word(desc), count = Word(desc + 4);
    s.r[3] = self;
    (void)cloth_storage61::Apply(0x82ba8108, m, d, s);
    m.WriteU32(self + 212, kind);
    s.r[3] = self;
    s.r[4] = desc;
    (void)cloth_import61::Apply(kind == 1 ? 0x82ba8ae8 : 0x82ba9530, m, d, s);
    if (!(s.r[3] & 255))
      return false;
    if (kind == 1) {
      s.r[3] = self;
      (void)cloth_face_map61::Apply(0x82ba8208, m, d, s);
      s.r[3] = self;
      (void)cloth_triangle_edges61::Apply(0x82bab468, m, d, s);
    } else {
      s.r[3] = self;
      (void)cloth_tetra_constraints61::Apply(0x82babe50, m, d, s);
    }
    s.r[3] = workspace;
    s.r[4] = kind;
    s.r[5] = (Word(self + 216) >> 8) & 1;
    s.r[6] = self + 4;
    s.r[7] = self + 84;
    s.r[8] = self + 124;
    s.r[9] = self + 164;
    s.r[10] = self + 184;
    (void)cloth_storage61::Apply(0x82ba7f90, m, d, s);
    s.r[3] = workspace;
    (void)cloth_schedule61::Apply(0x82bb6290, m, d, s);
    s.r[3] = workspace;
    (void)cloth_permutation61::Apply(0x82bb7b48, m, d, s);
    Resize(self + 144, count);
    auto permutation = Word(self + 124);
    for (unsigned i = 0; i < count; ++i)
      m.WriteU32(Word(self + 144) + 4 * Word(permutation + 4 * i), i);
    m.WriteU32(self + 88, Word(self + 84));
    auto xyz = Temporary(count, 12), weights = Temporary(count, 4),
         attributes = Temporary(count, 4);
    for (unsigned i = 0; i < count; ++i)
      Copy(xyz + 12 * Word(permutation + 4 * i), Word(self + 4) + 12 * i, 12);
    Copy(Word(self + 4), xyz, count * 12);
    for (auto channel : std::array<std::pair<unsigned, unsigned>, 2>{
             {{44, weights}, {64, attributes}}}) {
      auto off = channel.first, tmp = channel.second;
      if (Word(self + off + 4) != Word(self + off)) {
        for (unsigned i = 0; i < count; ++i)
          m.WriteU32(tmp + 4 * Word(permutation + 4 * i),
                     Word(Word(self + off) + 4 * i));
        Copy(Word(self + off), tmp, count * 4);
      }
    }
    for (unsigned p = Word(self + 24); p < Word(self + 28); p += 4)
      m.WriteU32(p, Word(permutation + 4 * Word(p)));
    auto childCount = (Word(self + 188) - Word(self + 184)) / 4;
    for (unsigned i = 0; i < childCount; ++i) {
      auto child = Word(Word(self + 184) + 4 * i);
      bool consecutive = true;
      unsigned previous = 0, j = 0;
      for (unsigned p = Word(child + 4); p < Word(child + 8); p += 4, ++j) {
        auto v = Word(permutation + 4 * Word(p));
        m.WriteU32(p, v);
        if (j && v != previous + 1)
          consecutive = false;
        previous = v;
      }
      if (consecutive)
        m.WriteU32(child, Word(child) | 2);
    }
    auto savedChildren = Temporary(childCount, 4);
    Copy(savedChildren, Word(self + 184), childCount * 4);
    m.WriteU32(self + 188, Word(self + 184));
    auto tierCount = (Word(self + 168) - Word(self + 164)) / 4,
         savedRanges = Temporary(tierCount, 4);
    Copy(savedRanges, Word(self + 164), tierCount * 4);
    m.WriteU32(self + 168, Word(self + 164));
    for (unsigned tier = tierCount; tier-- > 0;) {
      Push(self + 164, (Word(self + 188) - Word(self + 184)) / 4);
      auto begin = Word(savedRanges + 4 * tier),
           end = tier + 1 == tierCount ? childCount
                                       : Word(savedRanges + 4 * (tier + 1));
      for (unsigned i = begin; i < end; ++i) {
        auto child = Word(savedChildren + 4 * i);
        Push(self + 184, child);
        if (!tier)
          m.WriteU32(child, Word(child) | 1);
      }
    }
    for (unsigned p : {savedRanges, savedChildren, attributes, weights, xyz})
      if (p)
        Free(p);
    s.r[3] = workspace;
    (void)cloth_storage61::Apply(0x82ba8018, m, d, s);
    return true;
  }
  void Public(bool tetra) {
    auto input = Address(s.r[3]), stream = Address(s.r[4]),
         old = Address(s.r[1]), first = tetra ? 29u : 30u;
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 416;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    auto desc = sp + 96, owner = sp + 160;
    unsigned result = 0;
    if (Word(0x832dc414)) {
      for (unsigned i = 0; i < 15; ++i)
        m.WriteU32(desc + 4 * i, 0);
      m.WriteU32(desc, tetra ? 2 : 1);
      m.WriteU32(desc + 4, Word(input));
      m.WriteU32(desc + 8, Word(input + 8));
      m.WriteU32(desc + (tetra ? 20 : 12), Word(input + 4));
      m.WriteU32(desc + (tetra ? 24 : 16), Word(input + 12));
      m.WriteU32(desc + 28, Word(input + 16));
      m.WriteU32(desc + (tetra ? 36 : 32), Word(input + 20));
      auto flags = Word(input + 24);
      m.WriteU32(desc + 40,
                 tetra ? ((flags & 4 ? 256u : 0u) | (flags & 2)) : flags);
      for (unsigned i = 0; i < 4; ++i)
        m.WriteU32(desc + 44 + 4 * i, Word(input + 28 + 4 * i));
      if (Validate(desc)) {
        s.r[3] = owner;
        (void)cloth_storage61::Apply(0x82b9cdd8, m, d, s);
        s.r[3] = owner;
        s.r[4] = desc;
        Run();
        if (s.r[3] & 255) {
          auto platform = Word(0x832dc180);
          s.r[3] = owner;
          s.r[4] = stream;
          s.r[5] = platform == 1 ? 1u : platform == 2 ? 0u : m.ReadU8(sp + 80);
          (void)cloth_stream61::Apply(0x82ba7760, m, {d.guest, d.fp}, s);
          result = Address(s.r[3]);
        }
        s.r[3] = owner;
        (void)cloth_storage61::Apply(0x82ba8858, m, d, s);
      }
    }
    s.r[1] += 416;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = Word(old - 8);
    s.r[3] = result;
  }
  void Run() {
    auto self = Address(s.r[3]), desc = Address(s.r[4]), old = Address(s.r[1]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 15; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 976;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    auto result = Build(self, desc, sp + 96);
    s.r[1] += 976;
    for (unsigned i = 15; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = Word(old - 8);
    s.r[3] = result;
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  Cook t{m, d, s};
  if (e == 0x82bac6d0)
    t.Run();
  else if (e == 0x82b9ce98)
    t.Public(false);
  else if (e == 0x82b9d0a0)
    t.Public(true);
  else
    return false;
  return true;
}
} // namespace lo::semantic::gpu::cloth_cook61
