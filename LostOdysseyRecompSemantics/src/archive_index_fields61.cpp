#include "lo_semantics/archive_index_fields61.h"
#include "lo_semantics/recovery_abi.h"
#include <cstdint>
#include <initializer_list>
namespace lo::semantic::gpu::archive_index_fields61 {
namespace {
using recovery_abi::Address;
unsigned Swap(unsigned x) {
  return ((x & 255) << 24) | ((x & 0xff00) << 8) | ((x >> 8) & 0xff00) |
         (x >> 24);
}
void Half(GuestMemory &m, unsigned p) {
  auto x = m.ReadU16(p);
  m.WriteU16(p, (x >> 8) | (x << 8));
}
void Word(GuestMemory &m, unsigned p) { m.WriteU32(p, Swap(m.ReadU32(p))); }
void Entries(GuestMemory &m, Registers &s, unsigned base, std::int32_t count,
             unsigned archive, bool native) {
  for (std::int32_t i = 0; i < count; ++i) {
    auto p = base + 24 * unsigned(i);
    if (!native) {
      for (unsigned off : {0, 4, 8})
        Word(m, p + off);
      Half(m, p + 12);
      if (m.ReadU32(p) & 0x10000000) {
        Half(m, p + 14);
        Half(m, p + 16);
      } else
        Word(m, p + 16);
      Word(m, p + 20);
    }
    if (m.ReadU32(p) & 0x10000000) {
      auto children = m.ReadU32(archive + 4) + 24 * m.ReadU32(p + 20);
      m.WriteU32(p + 20, children);
      s.r[3] = children;
      Entries(m, s, children, m.ReadU16(p + 14), archive, native);
    }
  }
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Registers &s) {
  auto p = Address(s.r[3]);
  switch (e) {
  case 0x82853208:
    Half(m, p + 2);
    for (unsigned off : {4, 8, 12, 16, 20, 36, 24, 28, 32})
      Word(m, p + off);
    s.r[3] = m.ReadU32(p + 4);
    return true;
  case 0x82853a10:
    Entries(m, s, p, std::int32_t(s.r[4]), Address(s.r[5]), Address(s.r[6]) != 0);
    return true;
  case 0x828571b8:
    s.r[11] = m.ReadU8(p + 3);
    s.r[3] = s.r[11] << 11;
    return true;
  default:
    return false;
  }
}
} // namespace lo::semantic::gpu::archive_index_fields61
