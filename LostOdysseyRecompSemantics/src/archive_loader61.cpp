#include "lo_semantics/archive_loader61.h"
#include "lo_semantics/archive_index_fields61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::archive_loader61 {
namespace {
using recovery_abi::Address;
unsigned Swap16(unsigned x) { return ((x & 255) << 8) | (x >> 8); }
void Header(GuestMemory &m, unsigned p) {
  auto lo = __builtin_bswap32(m.ReadU32(p)),
       hi = __builtin_bswap32(m.ReadU32(p + 4));
  recovery_abi::WriteU64(m, p, (std::uint64_t(hi) << 32) | lo);
  for (unsigned off : {8u, 16u, 28u, 32u, 36u, 40u, 44u})
    m.WriteU32(p + off, __builtin_bswap32(m.ReadU32(p + off)));
  for (unsigned off : {12u, 14u, 24u, 26u})
    m.WriteU16(p + off, Swap16(m.ReadU16(p + off)));
}
struct Loader {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  static constexpr unsigned root = 0x83264d90, loading = root + 248,
                            lock = root + 308;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Call(unsigned e, unsigned a) {
    s.r[3] = a;
    d.guest.CallDirect(e, m, s);
    return Address(s.r[3]);
  }
  void Copy(unsigned out, unsigned in, unsigned n) {
    for (unsigned i = 0; i < n; ++i)
      m.WriteU8(out + i, m.ReadU8(in + i));
  }
  void Relocate(unsigned object, unsigned off) {
    if (auto p = W(object + off))
      m.WriteU32(object + off, p + object);
  }
  std::uint64_t Read(unsigned file, unsigned out, unsigned bytes,
                     unsigned actual, unsigned overlapped) {
    s.r[3] = file;
    s.r[4] = out;
    s.r[5] = bytes;
    s.r[6] = actual;
    s.r[7] = overlapped;
    d.guest.CallDirect(0x82be2dd8, m, s);
    return s.r[3];
  }
  void LockedRead() {
    auto file = Address(s.r[3]), out = Address(s.r[4]), bytes = Address(s.r[5]),
         actual = Address(s.r[6]), overlapped = Address(s.r[7]),
         mode = Address(s.r[8]);
    Call(0x830d9c6c, lock);
    if (std::int32_t(mode) < 2)
      d.guest.ExchangeStatus(m, root + 4, mode);
    auto result = Read(file, out, bytes, actual, overlapped);
    Call(0x830d9c7c, lock);
    s.r[3] = result;
  }
  void Load(unsigned sp) {
    auto mode = Address(s.r[3]), path = Address(s.r[4]),
         report = Address(s.r[5]), target = Address(s.r[7]);
    d.guest.ExchangeStatus(m, loading, 1);
    s.r[3] = path;
    s.r[4] = 0xffffffff80000000ull;
    s.r[5] = 1;
    s.r[6] = 0;
    s.r[7] = 3;
    s.r[8] = 128;
    s.r[9] = 0;
    d.guest.CallDirect(0x82be2be0, m, s);
    auto file = Address(s.r[3]);
    auto fail = [&](bool close) {
      if (close) {
        if (report)
          d.guest.CallDirect(0x828212e0, m, s);
        Call(0x82be1b80, file);
      }
      d.guest.ExchangeStatus(m, loading, 0);
      s.r[3] = ~std::uint64_t(0);
    };
    if (file == 0xffffffff) {
      fail(false);
      return;
    }
    m.WriteU32(sp + 80, 0);
    Call(0x830d9c6c, lock);
    d.guest.ExchangeStatus(m, root + 4, 0);
    (void)Read(file, sp + 96, 64, sp + 80, 0);
    Call(0x830d9c7c, lock);
    if (W(sp + 80) != 64) {
      fail(true);
      return;
    }
    unsigned native = m.ReadU16(sp + 120) & 1;
    if (!native)
      Header(m, sp + 96);
    auto sectors = unsigned(m.ReadU16(sp + 108));
    m.WriteU32(sp + 80, sectors);
    if (!sectors || sectors > 512) {
      fail(true);
      return;
    }
    if (!mode) {
      m.WriteU16(root + 2, m.ReadU8(sp + 116));
      d.guest.ExchangeStatus(m, root + 4, 0);
    }
    if (W(target))
      Call(0x823f3340, W(target));
    auto size = W(sp + 80) << 11;
    m.WriteU32(sp + 80, size);
    s.r[4] = 8;
    auto index = Call(0x823f3298, size);
    m.WriteU32(target, index);
    Copy(index, sp + 96, 64);
    s.r[3] = file;
    s.r[4] = index + 64;
    s.r[5] = size - 64;
    s.r[6] = sp + 80;
    s.r[7] = 0;
    s.r[8] = 0;
    (void)archive_loader61::Apply(0x82852c90, m, d, s);
    Call(0x82be1b80, file);
    Relocate(index, 32);
    Relocate(index, 36);
    if (!native) {
      if (W(index + 44)) {
        Relocate(index, 44);
        auto table = W(index + 44);
        for (unsigned i = 0; i < m.ReadU8(index + 23); ++i)
          m.WriteU16(table + 2 * i, Swap16(m.ReadU16(table + 2 * i)));
      }
      if (W(index + 40)) {
        Relocate(index, 40);
        auto table = W(index + 40);
        if (!(m.ReadU16(table) & 1)) {
          auto count = Swap16(m.ReadU16(table + 2)) << 3;
          for (unsigned i = 0; i < count; ++i)
            m.WriteU16(table + 2 * i, Swap16(m.ReadU16(table + 2 * i)));
        }
      }
    }
    unsigned count = m.ReadU16(index + 26), archive = W(index + 32);
    for (unsigned i = 0; i < count; ++i, archive += 48) {
      if (!native) {
        s.r[3] = archive;
        (void)archive_index_fields61::Apply(0x82853208, m, s);
      }
      for (unsigned off : {4u, 8u, 12u})
        Relocate(archive, off);
      s.r[3] = W(archive + 4);
      s.r[4] = m.ReadU16(archive + 2);
      s.r[5] = archive;
      s.r[6] = native;
      (void)archive_index_fields61::Apply(0x82853a10, m, s);
    }
    d.guest.ExchangeStatus(m, loading, 0);
    s.r[3] = 1;
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82853130) {
    Header(m, Address(s.r[3]));
    return true;
  }
  if (e != 0x82852c90 && e != 0x82854400)
    return false;
  auto old = Address(s.r[1]), frame = e == 0x82854400 ? 240u : 144u,
       first = e == 0x82854400 ? 23u : 25u;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  Loader l{m, d, s};
  if (e == 0x82854400)
    l.Load(Address(s.r[1]));
  else
    l.LockedRead();
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::archive_loader61
