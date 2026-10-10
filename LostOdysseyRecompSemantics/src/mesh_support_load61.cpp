#include "lo_semantics/mesh_support_load61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::mesh_support_load61 {
namespace {
using recovery_abi::Address;
struct Reader {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  bool Header(const char *tag, unsigned stream) {
    auto sp = Address(s.r[1]);
    for (unsigned i = 0; i < 4; ++i)
      s.r[3 + i] = tag[i];
    s.r[7] = sp + 88;
    s.r[8] = sp + 80;
    s.r[9] = stream;
    (void)mesh_stream_codec61::Apply(0x82bd81b0, m, d, s);
    return (s.r[3] & 255) != 0;
  }
  bool Counts(unsigned self, unsigned stream) {
    if (!Header("GAUS", stream))
      return false;
    auto swap = m.ReadU8(Address(s.r[1] + 80));
    for (unsigned i = 0; i < 2; ++i) {
      s.r[3] = swap;
      s.r[4] = stream;
      (void)mesh_stream_codec61::Apply(0x82bad8b8, m, d, s);
      m.WriteU32(self + 4 + 4 * i, Address(s.r[3]));
    }
    return true;
  }
  bool Load(unsigned self, unsigned stream) {
    if (!Header("SUPM", stream) || !Counts(self, stream))
      return false;
    m.WriteU32(self + 16, Word(self + 4));
    m.WriteU32(self + 20, Word(self + 8));
    (void)crt_close_recursive_buffer_context::Apply(0x82bd0798, m, d.guest, s);
    s.r[4] = 2 * Word(self + 8);
    s.r[5] = 55;
    s.ctr = Word(Word(Address(s.r[3])));
    s.lr = 0x82bc6370;
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    auto out = Address(s.r[3]);
    m.WriteU32(self + 12, out);
    if (!out)
      return false;
    m.WriteU32(self + 24, out);
    m.WriteU32(self + 28, out + Word(self + 8));
    s.r[3] = stream;
    s.r[4] = out;
    s.r[5] = 2 * Word(self + 8);
    s.ctr = Word(Word(stream) + 24);
    s.lr = 0x82bc63a8;
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    return true;
  }
  void Run(unsigned entry) {
    auto old = s.r[1];
    auto self = Address(s.r[3]), stream = Address(s.r[4]);
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(old));
    s.r[3] = (entry == 0x82bc8438 ? Counts(self, stream) : Load(self, stream))
                 ? 1
                 : 0;
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82bc8438 && e != 0x82bc62d8)
    return false;
  Reader{m, d, s}.Run(e);
  return true;
}
} // namespace lo::semantic::gpu::mesh_support_load61
