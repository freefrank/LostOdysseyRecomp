#include "lo_semantics/archive_startup61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::archive_startup61 {
namespace {
using recovery_abi::Address;
unsigned Length(GuestMemory &m, unsigned p) {
  unsigned n = 0;
  while (m.ReadU8(p + n))
    ++n;
  return n;
}
void Copy(GuestMemory &m, unsigned out, unsigned in) {
  for (;; ++out, ++in) {
    auto c = m.ReadU8(in);
    m.WriteU8(out, c);
    if (!c)
      return;
  }
}
unsigned Slashes(GuestMemory &m, unsigned p) {
  while (auto c = m.ReadU8(p)) {
    if ((c >= 128 && c < 160) || c >= 224) {
      p += 2;
      continue;
    }
    if (c == '/')
      m.WriteU8(p, '\\');
    ++p;
  }
  return p;
}
unsigned Split(GuestMemory &m, unsigned directory, unsigned basename,
               unsigned path) {
  auto last = Length(m, path);
  while (last && m.ReadU8(path + last) != '/' && m.ReadU8(path + last) != '\\')
    --last;
  if (last) {
    auto first = m.ReadU8(path);
    if (first != '/' && first != '\\') {
      for (unsigned i = 0; i <= last; ++i)
        m.WriteU8(directory + i, m.ReadU8(path + i));
      m.WriteU8(directory + last + 1, 0);
    } else if (last > 1) {
      for (unsigned i = 0; i < last; ++i)
        m.WriteU8(directory + i, m.ReadU8(path + 1 + i));
      m.WriteU8(directory + last, 0);
    }
    ++last;
  }
  Copy(m, basename, path + last);
  (void)Slashes(m, directory);
  return Slashes(m, basename);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x8284b508) {
    s.r[3] = Slashes(m, Address(s.r[3]));
    return true;
  }
  if (e != 0x82852ae8 && e != 0x82852bc8 && e != 0x82854cf8 && e != 0x8284b468)
    return false;
  auto input = Address(s.r[3]), old = Address(s.r[1]);
  unsigned frame = e == 0x82852ae8   ? 128
                   : e == 0x82854cf8 ? 304
                   : e == 0x8284b468 ? 96
                                     : 112;
  unsigned first = e == 0x82852ae8 ? 28 : e == 0x8284b468 ? 31 : 30;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  constexpr unsigned root = 0x83264d90;
  if (e == 0x82852ae8)
    s.r[3] = Split(m, input, Address(s.r[4]), Address(s.r[5]));
  else if (e == 0x82852bc8) {
    if (!m.ReadU8(root + 240)) {
      for (unsigned i = 0; i < 240; ++i)
        m.WriteU8(root + i, 0);
      m.WriteU8(root + 240, 1);
    }
    m.WriteU16(root + 2, 65535);
    d.guest.ExchangeStatus(m, root + 4, 0);
    Copy(m, root + 8, m.ReadU32(0x831e9168));
    (void)Split(m, root + 8 + Length(m, root + 8), root + 168, input);
    s.r[3] = 1;
  } else if (e == 0x82854cf8) {
    m.WriteU8(root, 0);
    Copy(m, sp + 80, root + 8);
    Copy(m, sp + 80 + Length(m, sp + 80), root + 168);
    s.r[3] = 0;
    s.r[4] = sp + 80;
    s.r[5] = m.ReadU16(root + 2) == 65535;
    s.r[6] = input;
    s.r[7] = root + 216;
    (void)archive_loader61::Apply(0x82854400, m, d, s);
    if (std::int32_t(s.r[3]) > 0) {
      m.WriteU8(root, 1);
      m.WriteU8(root + 1, 1);
    }
    s.r[3] = 1;
  } else {
    s.r[3] = 0x240000;
    d.guest.CallDirect(0x82be20a8, m, s);
    d.guest.CallDirect(0x82be1ae8, m, s);
    m.WriteU32(0x83264c58, Address(s.r[3]));
    s.r[3] = input;
    (void)archive_startup61::Apply(0x82852bc8, m, d, s);
    s.r[3] = ~std::uint64_t(0);
    (void)archive_startup61::Apply(0x82854cf8, m, d, s);
    d.guest.CallDirect(0x82850970, m, s);
  }
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::archive_startup61
