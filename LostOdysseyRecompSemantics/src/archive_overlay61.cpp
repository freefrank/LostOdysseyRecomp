#include "lo_semantics/archive_overlay61.h"
#include "lo_semantics/archive_lookup61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::archive_overlay61 {
namespace {
using recovery_abi::Address;
struct Overlay {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Call(unsigned e, unsigned a) {
    s.r[3] = a;
    d.guest.CallDirect(e, m, s);
    return Address(s.r[3]);
  }
  unsigned Element(unsigned node, unsigned index) {
    s.r[3] = node;
    s.r[4] = index;
    s.r[5] = 4;
    (void)archive_lookup61::Apply(0x822a2600, m, {d.guest, d.fp}, s);
    return Address(s.r[3]);
  }
  unsigned New(unsigned bytes) {
    if (!W(0x8330b608)) {
      s.r[3] = 0;
      d.guest.CallDirect(0x827c5f38, m, s);
    }
    s.r[3] = W(0x8330b608);
    s.r[4] = bytes + 12;
    s.r[5] = 8;
    s.ctr = W(W(Address(s.r[3])) + 4);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    auto node = Address(s.r[3]);
    for (unsigned i = 0; i < 12; ++i)
      m.WriteU8(node + i, 0);
    m.WriteU32(node + 4, bytes);
    return node;
  }
  unsigned Append(unsigned node, unsigned input, unsigned stride) {
    unsigned bytes = 0, last = node;
    for (auto p = node; p; p = W(p))
      bytes += W(p + 8);
    auto index = std::int32_t(bytes) / std::int32_t(stride);
    while (W(last))
      last = W(last);
    auto used = W(last + 8), capacity = W(last + 4), out = last + 12 + used;
    if (std::int32_t(stride) <= std::int32_t(capacity - used))
      m.WriteU32(last + 8, used + stride);
    else {
      auto next = New(std::int32_t(capacity) >= std::int32_t(stride) ? capacity
                                                                     : stride);
      m.WriteU32(last, next);
      m.WriteU32(next + 8, stride);
      out = next + 12;
    }
    for (unsigned i = 0; i < stride; ++i)
      m.WriteU8(out + i, m.ReadU8(input + i));
    return unsigned(index);
  }
  void Order(unsigned root) {
    auto table = W(root + 220);
    if (!table) {
      m.WriteU32(root + 212, 0);
      return;
    }
    auto count = std::int32_t(W(table + 4));
    unsigned n = 0;
    for (std::int32_t i = 0; i < count; ++i)
      if (W(Element(W(W(root + 220)), unsigned(i))))
        m.WriteU8(root + 224 + n++, unsigned(i));
    m.WriteU32(root + 212, n);
    for (unsigned i = 1; i < n; ++i) {
      unsigned id = m.ReadU8(root + 224 + i),
               priority = W(W(Element(W(W(root + 220)), id)) + 340);
      unsigned j = i;
      while (j) {
        auto before = m.ReadU8(root + 224 + j - 1);
        if (W(W(Element(W(W(root + 220)), before)) + 340) >= priority)
          break;
        m.WriteU8(root + 224 + j, before);
        --j;
      }
      if (j != i)
        m.WriteU8(root + 224 + j, id);
    }
  }
  void Load(unsigned owner, unsigned sp) {
    m.WriteU32(sp + 372, owner);
    s.r[3] = owner;
    s.r[4] = sp + 96;
    s.r[5] = owner + 380;
    d.guest.CallDirect(0x82dd1b58, m, s);
    s.r[3] = 1;
    s.r[4] = sp + 96;
    s.r[5] = 0;
    s.r[6] = ~std::uint64_t(0);
    s.r[7] = owner + 352;
    (void)archive_loader61::Apply(0x82854400, m, d, s);
    auto result = s.r[3];
    // Preserve the source nonzero gate, including its negative loader result.
    if (std::int32_t(result) != 0) {
      constexpr unsigned root = 0x83264d90, lock = 0x83264ef0;
      Call(0x830d9c6c, lock);
      auto table = W(root + 220);
      if (!table) {
        table = Call(0x82486c88, 12);
        m.WriteU32(sp + 80, table);
        if (table) {
          m.WriteU32(table, 0);
          m.WriteU32(table + 4, 0);
          m.WriteU32(table + 8, 8);
        }
        m.WriteU32(root + 220, table);
      }
      auto count = std::int32_t(W(table + 4));
      unsigned hole = 0;
      for (std::int32_t i = 0; i < count; ++i) {
        auto slot = Element(W(W(root + 220)), unsigned(i));
        if (!W(slot)) {
          hole = slot;
          break;
        }
      }
      if (hole)
        m.WriteU32(hole, owner);
      else {
        table = W(root + 220);
        m.WriteU32(table + 4, W(table + 4) + 1);
        if (!W(table))
          m.WriteU32(table, New(W(table + 8) * 4));
        (void)Append(W(table), sp + 372, 4);
      }
      Order(root);
      Call(0x830d9c7c, lock);
    }
    s.r[3] = std::int32_t(result) == 1;
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x8285d418 && e != 0x8285d920 && e != 0x828537d0 && e != 0x82854d98)
    return false;
  auto a = Address(s.r[3]), b = Address(s.r[4]), c = Address(s.r[5]),
       old = Address(s.r[1]);
  unsigned frame = e == 0x8285d418   ? 112
                   : e == 0x8285d920 ? 128
                   : e == 0x828537d0 ? 160
                                     : 352;
  unsigned first = e == 0x8285d418   ? 30
                   : e == 0x8285d920 ? 28
                   : e == 0x828537d0 ? 23
                                     : 25;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  Overlay o{m, d, s};
  if (e == 0x8285d418)
    s.r[3] = o.New(a);
  else if (e == 0x8285d920)
    s.r[3] = o.Append(a, b, c);
  else if (e == 0x828537d0)
    o.Order(a);
  else
    o.Load(a, sp);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::archive_overlay61
