#include "lo_semantics/archive_metadata61.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
namespace lo::semantic::gpu::archive_metadata61 {
namespace {
using recovery_abi::Address;
void Date(GuestMemory &m, unsigned out, unsigned input) {
  auto packed = m.ReadU32(input), year = 2000 + (packed >> 25),
       value = packed & 0x1ffffff;
  auto month = value / 2678400 + 1, day = (value / 86400) % 31 + 1;
  auto previous = (year - 1) % 400;
  constexpr unsigned january[]{1, 6, 4, 2, 0, 5, 3};
  unsigned weekday = (january[(previous / 4) % 7] - (previous / 100) % 4 +
                      (previous % 4) + 7) %
                     7;
  bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  if (month > 1) {
    // Preserve the original single table load outside the accumulation loop.
    // This helper's weekday field is not passed by the FILETIME bridge.
    auto days = m.ReadU8(0x82046150 + month - 1);
    for (unsigned i = 1; i < month; ++i)
      weekday += i == 2 && leap ? 29u : unsigned(days);
  }
  weekday = (weekday + day - 1) % 7;
  unsigned fields[]{
      year,       month, weekday, day, (value / 3600) % 24, (value / 60) % 60,
      value % 60, 0};
  for (unsigned i = 0; i < 8; ++i)
    m.WriteU16(out + 2 * i, fields[i]);
}
unsigned Fold(unsigned c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
void Rewrite(GuestMemory &m, Registers &s) {
  auto p = Address(s.r[3]);
  unsigned length = 0;
  while (m.ReadU8(p + length))
    ++length;
  auto a = p + length - 7, b = 0x8204615cu;
  int difference;
  do {
    auto x = m.ReadU8(a++), y = m.ReadU8(b++);
    difference = int(x) - int(y);
    if (!y)
      break;
    if (difference)
      difference = int(Fold(x)) - int(Fold(y));
    if (difference)
      break;
  } while (true);
  if (!difference)
    m.WriteU8(p + length - 5, '1');
  s.r[3] = std::uint64_t(std::int64_t(difference));
}
void FileTime(GuestMemory &m, Dependencies d, Registers &s) {
  auto input = Address(s.r[3]), out = Address(s.r[4]), old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  recovery_abi::WriteU64(m, old - 16, s.r[31]);
  s.r[1] -= 128;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  unsigned offsets[]{0, 2, 6, 8, 10, 12, 14};
  for (unsigned i = 0; i < 7; ++i)
    m.WriteU16(sp + 96 + 2 * i, m.ReadU16(input + offsets[i]));
  s.r[3] = sp + 96;
  s.r[4] = sp + 80;
  d.guest.CallDirect(0x830da31c, m, s);
  unsigned result = 1;
  if (!(s.r[3] & 255)) {
    s.r[3] = 0xffffffffc000000dull;
    d.guest.CallDirect(0x827ca628, m, s);
    result = 0;
  } else {
    m.WriteU32(out, m.ReadU32(sp + 80));
    m.WriteU32(out + 4, m.ReadU32(sp + 84));
  }
  s.r[1] += 128;
  s.r[31] = recovery_abi::ReadU64(m, old - 16);
  s.lr = m.ReadU32(old - 8);
  s.r[3] = result;
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x828527a0) {
    Date(m, Address(s.r[3]), Address(s.r[4]));
    return true;
  }
  if (e == 0x82852db8) {
    Rewrite(m, s);
    return true;
  }
  if (e == 0x82be2f60) {
    FileTime(m, d, s);
    return true;
  }
  return false;
}
} // namespace lo::semantic::gpu::archive_metadata61
