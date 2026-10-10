#include "lo_semantics/archive_lookup61.h"
#include "lo_semantics/archive_member61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::archive_lookup61 {
namespace {
using recovery_abi::Address;
unsigned Normalize(GuestMemory &m, unsigned out, unsigned reference) {
  auto input = m.ReadU32(reference);
  auto first = m.ReadU8(input);
  if (first == '\\' || first == '/') {
    ++input;
    m.WriteU32(reference, input);
  }
  unsigned n = 0;
  while (m.ReadU8(input + n))
    ++n;
  for (unsigned i = 0; i <= n; ++i)
    m.WriteU8(out + i, m.ReadU8(input + i));
  for (unsigned i = 0; i < n; ++i) {
    auto c = m.ReadU8(out + i);
    if (c == '-')
      c = '_';
    else if (c == '/')
      c = '\\';
    else if (c >= 'A' && c <= 'Z')
      c |= 32;
    m.WriteU8(out + i, c);
  }
  return n;
}
unsigned Element(GuestMemory &m, unsigned node, unsigned index,
                 unsigned stride) {
  while (node) {
    auto count = m.ReadU32(node + 8) / stride;
    if (std::int32_t(index) < std::int32_t(count))
      return node + 12 + index * stride;
    index -= count;
    node = m.ReadU32(node);
  }
  return 0;
}
void Copy(GuestMemory &m, unsigned out, unsigned input) {
  unsigned i = 0;
  for (;; ++i) {
    auto c = m.ReadU8(input + i);
    m.WriteU8(out + i, c);
    if (!c)
      return;
  }
}
void Lookup(GuestMemory &m, Dependencies d, Registers &s) {
  auto input = Address(s.r[3]), out = Address(s.r[4]),
       archiveOut = Address(s.r[5]), old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = 22; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= 368;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  m.WriteU32(sp + 388, input);
  auto length = Normalize(m, sp + 80, sp + 388), original = m.ReadU32(sp + 388);
  for (unsigned i = 0; i < 240; ++i)
    m.WriteU8(out + i, 0);
  constexpr unsigned root = 0x83264d90, lock = 0x83264ef0;
  auto call = [&](unsigned target, unsigned arg) {
    s.r[3] = arg;
    d.guest.CallDirect(target, m, s);
  };
  auto find = [&](unsigned header) {
    s.r[3] = header;
    s.r[4] = original;
    s.r[5] = sp + 80;
    s.r[6] = length;
    s.r[7] = out;
    s.r[8] = archiveOut;
    (void)archive_member61::Apply(0x828548a0, m, d, s);
    return s.r[3] != 0;
  };
  bool result = false;
  if (m.ReadU32(root + 220)) {
    call(0x830d9c6c, lock);
    auto count = std::int32_t(m.ReadU32(root + 212));
    for (std::int32_t i = 0; i < count; ++i) {
      auto node = m.ReadU32(m.ReadU32(root + 220));
      auto slot = Element(m, node, m.ReadU8(root + 224 + unsigned(i)), 4),
           owner = m.ReadU32(slot);
      s.r[3] = owner;
      s.r[4] = out + 48;
      s.r[5] = 0;
      // Overlay path formatting stays at the existing guest service boundary.
      d.guest.CallDirect(0x82dd1b58, m, s);
      if (find(m.ReadU32(owner + 352))) {
        m.WriteU32(out + 32, owner);
        result = true;
        break;
      }
    }
    call(0x830d9c7c, lock);
  }
  if (!result) {
    Copy(m, out + 48, m.ReadU32(0x831e9168));
    result = find(m.ReadU32(root + 216));
  }
  s.r[1] += 368;
  for (unsigned i = 22; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  s.r[3] = result;
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82852e08) {
    s.r[3] = Normalize(m, Address(s.r[3]), Address(s.r[4]));
    return true;
  }
  if (e == 0x822a2600) {
    s.r[3] = Element(m, Address(s.r[3]), Address(s.r[4]), Address(s.r[5]));
    return true;
  }
  if (e == 0x828551e8) {
    Lookup(m, d, s);
    return true;
  }
  return false;
}
} // namespace lo::semantic::gpu::archive_lookup61
