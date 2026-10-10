#include "lo_semantics/cloth_storage61.h"
#include "lo_semantics/recovery_abi.h"
#include <initializer_list>
namespace lo::semantic::gpu::cloth_storage61 {
namespace {
using recovery_abi::Address;
struct Storage {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  void Clear(unsigned vector) {
    for (unsigned i = 0; i < 3; ++i)
      m.WriteU32(vector + 4 * i, 0);
  }
  void Free(unsigned p) {
    s.r[3] = Word(0x832df548);
    s.r[4] = p;
    s.ctr = Word(Word(Address(s.r[3])) + 20);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  void Drop(unsigned vector) {
    if (auto p = Word(vector))
      Free(p);
    Clear(vector);
  }
  void Mesh(unsigned self) {
    Drop(self + 24);
    Drop(self + 4);
  }
  void Children(unsigned self) {
    unsigned i = 0;
    while (i < (Word(self + 188) - Word(self + 184)) / 4) {
      auto p = Word(Word(self + 184) + 4 * i);
      if (p) {
        Mesh(p);
        Free(p);
      }
      m.WriteU32(Word(self + 184) + 4 * i++, 0);
    }
  }
  void Run(unsigned entry) {
    auto a = s.r;
    auto self = Address(a[3]);
    if (entry == 0x82b9cdd8) {
      m.WriteU32(self, 0x820d59a4);
      Clear(self + 4);
      for (unsigned off = 24; off <= 184; off += 20)
        Clear(self + off);
      for (unsigned off = 16; off <= 32; off += 4)
        m.WriteU32(Address(s.r[1] - off), 0);
      return;
    }
    if (entry == 0x82ba7f90) {
      for (unsigned off : {0, 20, 40})
        Clear(self + off);
      m.WriteU32(self + 60, Address(a[4]));
      m.WriteU8(self + 64, Address(a[5]));
      for (unsigned i = 0; i < 32; ++i)
        Clear(self + 68 + 20 * i);
      m.WriteU32(self + 708, 0);
      m.WriteU32(self + 712, 0);
      for (unsigned i = 0; i < 5; ++i)
        m.WriteU32(self + 716 + 4 * i, Address(a[6 + i]));
      return;
    }
    auto old = s.r[1];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(old));
    if (entry == 0x82ba7f10)
      Mesh(self);
    else if (entry == 0x82ba8018) {
      for (int i = 31; i >= 0; --i)
        Drop(self + 68 + 20 * unsigned(i));
      for (unsigned off : {40, 20, 0})
        Drop(self + off);
    } else if (entry == 0x82ba8108) {
      for (unsigned off : {212, 204, 208})
        m.WriteU32(self + off, 0);
      for (unsigned off : {4, 24, 44, 64, 104, 84})
        m.WriteU32(self + off + 4, Word(self + off));
      Children(self);
      for (unsigned off : {184, 124, 144, 164})
        m.WriteU32(self + off + 4, Word(self + off));
    } else {
      m.WriteU32(self, 0x820d59a4);
      Children(self);
      for (int off = 184; off >= 4; off -= 20)
        Drop(self + unsigned(off));
    }
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82b9cdd8 && e != 0x82ba7f10 && e != 0x82ba7f90 &&
      e != 0x82ba8018 && e != 0x82ba8108 && e != 0x82ba8858)
    return false;
  Storage{m, d, s}.Run(e);
  return true;
}
} // namespace lo::semantic::gpu::cloth_storage61
