#include "lo_semantics/string_storage_context61.h"
#include "lo_semantics/string_conversion_context61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::string_conversion_context61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!string_storage_context61::Apply(e, m, d, s) &&
      !string_conversion_context61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
unsigned Length(GuestMemory &m, unsigned p) {
  unsigned n = 0;
  while (m.ReadU8(p + n))
    ++n;
  return n;
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x8229c538) {
    s.r[3] = Length(m, Address(s.r[3]));
    return true;
  }
  unsigned frame, first;
  switch (e) {
  case 0x823227c8:
    frame = 128;
    first = 28;
    break;
  case 0x8229c560:
    frame = 96;
    first = 31;
    break;
  default:
    return false;
  }
  auto old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  if (e == 0x823227c8) {
    auto header = Address(s.r[3]), source = Address(s.r[4]);
    if (!source)
      m.WriteU32(header + 256, 0);
    else {
      auto count = Length(m, source) + 1, output = header;
      if (count > 128) {
        auto twice = count * 2;
        s.r[3] = twice > 0x7fffffffu ? 0xffffffffu : twice * 2;
        Call(0x82486c88, m, d, s);
        output = Address(s.r[3]);
      }
      s.r[3] = 0;
      s.r[4] = 0;
      s.r[5] = source;
      s.r[6] = count;
      s.r[7] = output;
      s.r[8] = count;
      Call(0x8229c560, m, d, s);
      m.WriteU32(header + 256, output);
    }
    s.r[3] = header;
  } else {
    auto codepage = Address(s.r[3]), source = Address(s.r[5]),
         count = Address(s.r[6]), output = Address(s.r[7]),
         capacity = Address(s.r[8]);
    s.r[4] = count;
    s.r[3] = output;
    if (codepage == 65001) {
      s.r[3] = source;
      s.r[5] = output;
      s.r[6] = capacity;
      Call(0x827ca660, m, d, s);
    } else {
      if (count == 0xffffffffu)
        count = Length(m, source) + 1;
      if (!capacity)
        s.r[3] = count;
      else if (std::int32_t(count) < 0 ||
               std::int32_t(capacity) < std::int32_t(count)) {
        s.r[3] = 122;
        Call(0x822ca180, m, d, s);
        s.r[3] = 0;
      } else {
        s.r[3] = output;
        s.r[4] = capacity * 2;
        s.r[5] = 0;
        s.r[6] = source;
        s.r[7] = count;
        Call(0x830d9cdc, m, d, s);
        if (std::int32_t(Address(s.r[3])) >= 0)
          s.r[3] = count;
        else {
          Call(0x830d9efc, m, d, s);
          Call(0x822ca180, m, d, s);
          s.r[3] = 0;
        }
      }
    }
  }
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::string_conversion_context61
