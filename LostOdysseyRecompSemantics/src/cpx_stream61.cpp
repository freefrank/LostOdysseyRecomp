#include "lo_semantics/cpx_stream61.h"
#include "lo_semantics/cpx_context61.h"
#include "lo_semantics/cpx_decode61.h"
#include "lo_semantics/cpx_lifecycle61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::cpx_stream61 {
namespace {
using recovery_abi::Address;
struct Stream {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned self, budget, sp;
  std::uint64_t start;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Little(unsigned p) { return __builtin_bswap32(W(p)); }
  unsigned Reserve() { return unsigned(m.ReadU8(self + 55)) << 11; }
  void Copy(unsigned out, unsigned in, unsigned n) {
    for (unsigned i = 0; i < n; ++i)
      m.WriteU8(out + i, m.ReadU8(in + i));
  }
  unsigned Direct(unsigned e, unsigned a) {
    s.r[3] = a;
    d.guest.CallDirect(e, m, s);
    return Address(s.r[3]);
  }
  std::uint64_t Elapsed(std::uint64_t since) {
    s.r[3] = d.guest.ReadTimeBase() - since;
    d.guest.CallDirect(0x8284cfa8, m, s);
    return s.r[3];
  }
  unsigned Context(unsigned e, unsigned a) {
    s.r[3] = a;
    (void)cpx_context61::Apply(e, m, {d.guest, d.fp}, s);
    return Address(s.r[3]);
  }
  bool Done(unsigned status) {
    m.WriteU8(self + 30, 255);
    d.guest.ExchangeStatus(m, self + 40, status);
    return true;
  }
  void Read(unsigned out, unsigned bytes) {
    s.r[3] = self;
    s.r[4] = out;
    s.r[5] = bytes;
    s.r[6] = 0;
    s.r[7] = 2;
    s.ctr = W(W(self) + 20);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  bool Run() {
    auto state = m.ReadU8(self + 30);
    if (state == 0) {
      auto compressed = unsigned(m.ReadU8(self + 31));
      if (W(self + 24) & 0x50000)
        compressed = 0;
      auto decoded = Little(self + 64), reserve = Reserve();
      if (!compressed) {
        decoded = W(self + 16);
        reserve = 0;
      }
      if (!W(self + 48)) {
        m.WriteU8(self + 32, 1);
        s.r[4] = 8;
        m.WriteU32(self + 48, Direct(0x823f3298, decoded));
        if (!W(self + 48))
          return Done(5);
      }
      auto stored = W(self + 16), remaining = stored,
           offset = decoded + reserve - stored;
      m.WriteU32(self + 92, offset);
      if (stored <= reserve || (compressed && stored - reserve < 2048)) {
        if (stored > reserve) {
          reserve += 2048;
          m.WriteU8(self + 55, m.ReadU8(self + 55) + 1);
        }
        m.WriteU8(self + 88, 1);
        remaining = 0;
      } else
        remaining = stored - reserve;
      if (reserve) {
        s.r[4] = 8;
        m.WriteU32(self + 96, Direct(0x823f3298, reserve));
        if (!W(self + 96)) {
          Direct(0x823f3340, W(self + 48));
          m.WriteU32(self + 48, 0);
          return Done(5);
        }
      }
      if (m.ReadU8(self + 31) == 2 && (m.ReadU8(self + 56) & 0xf0)) {
        Copy(W(self + 48) + offset, self + 52, 16);
        offset += 16;
        remaining -= 16;
      }
      m.WriteU32(self + 72, offset);
      m.WriteU32(self + 76, remaining);
      m.WriteU8(self + 30, 1);
      state = 1;
    }
    if (state == 1) {
      auto reserve = Reserve();
      while (W(self + 76)) {
        auto n = W(self + 76);
        if (n > 0x500000)
          n = 0x500000;
        Read(W(self + 48) + W(self + 72), n);
        m.WriteU32(self + 76, W(self + 76) - n);
        m.WriteU32(self + 72, W(self + 72) + n);
        if (W(self + 76) &&
            std::int64_t(Elapsed(start)) >= std::int64_t(budget))
          return false;
      }
      if (reserve)
        Read(W(self + 96), reserve);
      auto elapsed = unsigned(Elapsed(recovery_abi::ReadU64(m, self + 80)));
      recovery_abi::WriteU64(m, self + 80, d.guest.ReadTimeBase());
      m.WriteU32(
          self + 72,
          unsigned((std::uint64_t(elapsed + 500u) * 0x10624dd3u) >> 32) >> 6);
      if ((W(self + 24) & 0x50000) || !m.ReadU8(self + 31))
        return Done(4);
      auto context = Direct(0x82486c88, 44);
      m.WriteU32(sp + 80, context);
      if (context)
        Context(0x828571c8, context);
      m.WriteU32(self + 100, context);
      auto input =
          m.ReadU8(self + 88) ? W(self + 96) : W(self + 48) + W(self + 92);
      if (m.ReadU8(self + 88))
        m.WriteU32(self + 92, 0);
      s.r[4] = input;
      s.r[5] = 0;
      Context(0x82857308, context);
      m.WriteU8(self + 30, 16);
      return false;
    }
    if (state != 16)
      return true;
    for (;;) {
      auto context = W(self + 100);
      m.WriteU32(sp + 80, 0);
      s.r[4] = 0xffffffff;
      s.r[5] = sp + 80;
      auto offset = Context(0x828574a8, context);
      auto boundary = W(self + 16) - Reserve();
      auto input =
          (m.ReadU8(self + 88) ? W(self + 96) : W(self + 48) + W(self + 92)) +
          offset;
      if (offset + W(sp + 80) > boundary) {
        if (offset < boundary) {
          auto first = boundary - offset,
               scratch = Context(0x82857438, context);
          Copy(scratch, input, first);
          Copy(scratch + first, W(self + 96), W(sp + 80) - first);
          input = scratch;
        } else
          input = W(self + 96) + offset - boundary;
      }
      s.r[3] = context;
      s.r[4] = W(self + 48) + W(context + 32);
      s.r[5] = input;
      (void)cpx_decode61::Apply(
          (m.ReadU8(self + 56) & 0xf0) == 0x10 ? 0x82857d90 : 0x82857b00, m, s);
      if (Context(0x82857480, W(self + 100)))
        break;
      if (std::int64_t(Elapsed(start)) >= std::int64_t(budget))
        return false;
    }
    m.WriteU32(self + 20, W(W(self + 100) + 32));
    (void)Elapsed(recovery_abi::ReadU64(m, self + 80));
    if (W(self + 96)) {
      Direct(0x823f3340, W(self + 96));
      m.WriteU32(self + 96, 0);
    }
    if (auto context = W(self + 100)) {
      s.r[3] = context;
      (void)cpx_lifecycle61::Apply(0x82857820, m, {d.guest, d.fp}, s);
      Direct(0x823f3340, context);
    }
    m.WriteU32(self + 100, 0);
    return Done(4);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x8284e6b8)
    return false;
  auto self = Address(s.r[3]), budget = Address(s.r[4]), old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = 21; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= 192;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  bool done = Stream{m, d, s, self, budget, sp, d.guest.ReadTimeBase()}.Run();
  s.r[1] += 192;
  for (unsigned i = 21; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  s.r[3] = done;
  return true;
}
} // namespace lo::semantic::gpu::cpx_stream61
