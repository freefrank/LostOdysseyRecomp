#include "lo_semantics/cloth_face_map61.h"
#include "lo_semantics/cloth_topology_support61.h"
#include "lo_semantics/recovery_abi.h"
#include <algorithm>
#include <array>
namespace lo::semantic::gpu::cloth_face_map61 {
namespace {
using recovery_abi::Address;
struct Mapping {
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
  void Run() {
    auto self = Address(s.r[3]);
    auto old = s.r[1];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(old));
    auto count = (Word(self + 28) - Word(self + 24)) / 12,
         records = count ? Allocate(16 * count) : 0;
    for (unsigned i = 0; i < count; ++i) {
      std::array<unsigned, 3> key;
      for (unsigned j = 0; j < 3; ++j)
        key[j] = Word(Word(self + 24) + 12 * i + 4 * j);
      std::sort(key.begin(), key.end());
      for (unsigned j = 0; j < 3; ++j)
        m.WriteU32(records + 16 * i + 4 * j, key[j]);
      m.WriteU32(records + 16 * i + 12, i);
    }
    auto descriptor = Address(s.r[1]) + 96;
    m.WriteU32(descriptor, records);
    m.WriteU32(descriptor + 4, records + 16 * count);
    m.WriteU32(descriptor + 8, records + 16 * count);
    s.r[3] = descriptor;
    s.r[4] = 0;
    s.r[5] = count - 1;
    (void)cloth_topology_support61::Apply(0x82ba7438, m, s);
    auto vector = self + 104, start = Word(vector),
         used = (Word(vector + 4) - start) / 4,
         cap = start ? (Word(vector + 8) - start) / 4 : 0;
    if (count > used && cap < count) {
      auto p = Allocate(8 * count);
      for (unsigned i = 0; i < 4 * used; ++i)
        m.WriteU8(p + i, m.ReadU8(start + i));
      if (start)
        Free(start);
      start = p;
      cap = 2 * count;
      m.WriteU32(vector, p);
      m.WriteU32(vector + 8, p + 4 * cap);
    }
    if (count > used)
      for (unsigned i = used; i < count; ++i)
        m.WriteU32(start + 4 * i, 0);
    m.WriteU32(vector + 4, start + 4 * count);
    if (!count) {
      if (start)
        Free(start);
      for (unsigned i = 0; i < 3; ++i)
        m.WriteU32(vector + 4 * i, 0);
    } else if (cap > count) {
      s.r[4] = start;
      s.r[5] = 4 * count;
      Invoke(16);
      start = Address(s.r[3]);
      m.WriteU32(vector, start);
      m.WriteU32(vector + 4, start + 4 * count);
      m.WriteU32(vector + 8, start + 4 * count);
    }
    for (unsigned i = 0; i < count;) {
      auto end = i + 1, minimum = Word(records + 16 * i + 12);
      while (end < count) {
        bool equal = true;
        for (unsigned j = 0; j < 3; ++j)
          equal &= Word(records + 16 * end + 4 * j) ==
                   Word(records + 16 * i + 4 * j);
        if (!equal)
          break;
        minimum = std::min(minimum, Word(records + 16 * end + 12));
        ++end;
      }
      for (unsigned j = i; j < end; ++j)
        m.WriteU32(start + 4 * Word(records + 16 * j + 12), minimum);
      i = end;
    }
    if (records)
      Free(records);
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82ba8208)
    return false;
  Mapping{m, d, s}.Run();
  return true;
}
} // namespace lo::semantic::gpu::cloth_face_map61
