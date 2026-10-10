#include "lo_semantics/cpx_context61.h"
#include "lo_semantics/recovery_abi.h"
#include <initializer_list>
namespace lo::semantic::gpu::cpx_context61 {
namespace {
using recovery_abi::Address;
struct Context {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  unsigned Little(unsigned p) {
    return unsigned(m.ReadU8(p)) | (unsigned(m.ReadU8(p + 1)) << 8) |
           (unsigned(m.ReadU8(p + 2)) << 16) |
           (unsigned(m.ReadU8(p + 3)) << 24);
  }
  unsigned Blocks(unsigned p) {
    return unsigned(m.ReadU8(p + 6)) | (unsigned(m.ReadU8(p + 7)) << 8);
  }
  void Copy(unsigned out, unsigned in, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
      m.WriteU8(out + i, m.ReadU8(in + i));
  }
  void Invoke(unsigned slot, unsigned argument) {
    if (!Word(0x8330b608)) {
      s.r[3] = 0;
      d.guest.CallDirect(0x827c5f38, m, s);
    }
    s.r[3] = Word(0x8330b608);
    s.r[4] = argument;
    if (slot == 4)
      s.r[5] = 8;
    s.ctr = Word(Word(Address(s.r[3])) + slot);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  void Free(unsigned p) { Invoke(12, p); }
  void Run(unsigned entry) {
    auto self = Address(s.r[3]), arg = Address(s.r[4]), out = Address(s.r[5]);
    if (entry == 0x82857480) {
      s.r[3] = m.ReadU16(self + 16) >= Blocks(Word(self + 4));
      return;
    }
    if (entry == 0x828574a8) {
      auto header = Word(self + 4), count = Blocks(header),
           index = std::int32_t(arg) < 0 ? unsigned(m.ReadU16(self + 16)) : arg;
      if (std::int32_t(index) >= std::int32_t(count)) {
        s.r[3] = 0;
        return;
      }
      auto offset = Little(header + 16 + 4 * index),
           end = index == count - 1 ? Little(header + 8)
                                    : Little(header + 20 + 4 * index);
      m.WriteU32(out, end - offset);
      s.r[3] = offset;
      return;
    }
    unsigned frame = entry == 0x82857308   ? 144
                     : entry == 0x82857200 ? 128
                     : entry == 0x82857290 ? 112
                                           : 96;
    unsigned first = entry == 0x82857308   ? 27
                     : entry == 0x82857200 ? 28
                     : entry == 0x82857290 ? 29
                                           : 31;
    auto old = Address(s.r[1]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= frame;
    m.WriteU32(Address(s.r[1]), old);
    if (entry == 0x828571c8) {
      for (unsigned i = 0; i < 44; ++i)
        m.WriteU8(self + i, 0);
      s.r[3] = self;
    } else if (entry == 0x82857308) {
      auto table = out ? out : arg + 16, size = Little(table);
      m.WriteU32(self + 12, size);
      Invoke(4, size);
      auto header = Address(s.r[3]);
      m.WriteU32(self + 4, header);
      Copy(header, arg, 16);
      Copy(header + 16, table, size - 16);
      m.WriteU32(self + 24, Little(header + 12));
      s.r[3] = 1;
    } else if (entry == 0x82857438) {
      if (!Word(self + 8)) {
        s.r[3] = 65552;
        d.guest.CallDirect(0x82486c88, m, s);
        m.WriteU32(self + 8, Address(s.r[3]));
      }
      s.r[3] = Word(self + 8);
    } else if (entry == 0x82857200) {
      for (unsigned off : {4, 8})
        if (auto p = Word(self + off)) {
          Free(p);
          m.WriteU32(self + off, 0);
        }
    } else if (entry == 0x82857290) {
      if (m.ReadU8(self + 20) && Word(self + 28)) {
        s.r[3] = Word(self + 28);
        d.guest.CallDirect(0x823f3340, m, s);
      }
      if (auto p = Word(self + 8)) {
        Free(p);
        m.WriteU32(self + 8, 0);
      }
    }
    s.r[1] += frame;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = Word(old - 8);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x828571c8 && e != 0x82857200 && e != 0x82857290 &&
      e != 0x82857308 && e != 0x82857438 && e != 0x82857480 && e != 0x828574a8)
    return false;
  Context{m, d, s}.Run(e);
  return true;
}
} // namespace lo::semantic::gpu::cpx_context61
