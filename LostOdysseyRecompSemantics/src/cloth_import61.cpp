#include "lo_semantics/cloth_import61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::cloth_import61 {
namespace {
using recovery_abi::Address;
struct Import {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  void Invoke(unsigned slot) {
    s.r[3] = Word(0x832df548);
    s.ctr = Word(Word(Address(s.r[3])) + slot);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  unsigned Append(unsigned v, unsigned stride) {
    auto start = Word(v), end = Word(v + 4);
    if (Word(v + 8) <= end) {
      auto bytes = 2 * ((end - start) / stride + 1) * stride;
      s.r[4] = bytes;
      s.r[5] = 282;
      Invoke(8);
      auto p = Address(s.r[3]);
      for (unsigned i = 0; i < end - start; ++i)
        m.WriteU8(p + i, m.ReadU8(start + i));
      if (start) {
        s.r[4] = start;
        Invoke(20);
      }
      m.WriteU32(v, p);
      m.WriteU32(v + 8, p + bytes);
      end = p + (end - start);
    }
    m.WriteU32(v + 4, end + stride);
    return end;
  }
  bool Build(unsigned self, unsigned desc, bool tetra) {
    auto count = Word(desc + 4), faces = Word(desc + (tetra ? 20 : 12));
    if (!count || !faces)
      return false;
    auto flags = Word(desc + 40);
    m.WriteU32(self + 216, flags);
    auto points = Word(desc + 28), weights = Word(desc + 52),
         attributes = Word(desc + 56);
    for (unsigned i = 0; i < count; ++i) {
      auto out = Append(self + 4, 12);
      for (unsigned j = 0; j < 3; ++j)
        m.WriteU32(out + 4 * j, Word(points + 4 * j));
      points += Word(desc + 8);
      if (weights) {
        auto value = Word(weights);
        m.WriteU32(Append(self + 44, 4), value);
        weights += Word(desc + 44);
      }
      if (attributes) {
        auto value = Word(attributes);
        m.WriteU32(Append(self + 64, 4), value);
        attributes += Word(desc + 48);
      }
    }
    auto input = Word(desc + (tetra ? 36 : 32)),
         stride = Word(desc + (tetra ? 24 : 16));
    unsigned width = flags & 2 ? 2 : 4;
    for (unsigned i = 0; i < faces; ++i) {
      for (unsigned j = 0; j < (tetra ? 4u : 3u); ++j) {
        unsigned corner = tetra || !j ? j : ((flags & 1) ? 3 - j : j);
        auto p = input + width * corner,
             value = width == 2 ? m.ReadU16(p) : Word(p);
        m.WriteU32(Append(self + 24, 4), value);
      }
      input += stride;
    }
    return true;
  }
  void Run(unsigned e) {
    auto self = Address(s.r[3]), desc = Address(s.r[4]);
    auto old = s.r[1];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(old));
    s.r[3] = Build(self, desc, e == 0x82ba9530);
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82ba8ae8 && e != 0x82ba9530)
    return false;
  Import{m, d, s}.Run(e);
  return true;
}
} // namespace lo::semantic::gpu::cloth_import61
