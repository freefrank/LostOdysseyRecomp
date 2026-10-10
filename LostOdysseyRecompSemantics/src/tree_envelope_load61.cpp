#include "lo_semantics/tree_envelope_load61.h"
#include "lo_semantics/mesh_stream_codec61.h"
#include "lo_semantics/transform_owner_initialize61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::tree_envelope_load61 {
namespace {
using recovery_abi::Address;
struct Reader {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  void Call(unsigned target, unsigned lr) {
    s.ctr = target;
    s.lr = lr;
    d.guest.CallIndirect(target & ~3u, m, s);
  }
  unsigned Read(unsigned stream, unsigned slot, unsigned lr) {
    s.r[3] = stream;
    Call(Word(Word(stream) + slot), lr);
    return Address(s.r[3]);
  }
  unsigned Scalar(unsigned stream, bool swap, unsigned lr) {
    auto v = Read(stream, 12, lr);
    return swap ? __builtin_bswap32(v) : v;
  }
  bool Header(unsigned stream, const char *tag, bool &swap, unsigned lr) {
    unsigned v[4];
    for (unsigned i = 0; i < 4; ++i)
      v[i] = Read(stream, 4, lr + 24 * i) & 255;
    swap = v[3] != 0;
    return v[0] == unsigned(tag[0]) && v[1] == unsigned(tag[1]) &&
           v[2] == unsigned(tag[2]);
  }
  unsigned Allocate(unsigned bytes, unsigned tag, unsigned lr) {
    (void)crt_close_recursive_buffer_context::Apply(0x82bd0798, m, d.guest, s);
    s.r[4] = bytes;
    s.r[5] = tag;
    Call(Word(Word(Address(s.r[3]))), lr);
    return Address(s.r[3]);
  }
  void Free(unsigned p, unsigned lr) {
    (void)crt_close_recursive_buffer_context::Apply(0x82bd0798, m, d.guest, s);
    s.r[4] = p;
    Call(Word(Word(Address(s.r[3])) + 12), lr);
  }
  void Indices(unsigned stream, bool swap, unsigned count, unsigned out,
               unsigned lr) {
    auto maximum = Scalar(stream, swap, lr);
    s.r[3] = maximum;
    s.r[4] = count;
    s.r[5] = out;
    s.r[6] = stream;
    s.r[7] = swap;
    (void)mesh_stream_codec61::Apply(0x82bd8748, m, {d.guest, d.fp}, s);
  }
  bool Base(unsigned self, unsigned stream) {
    s.r[3] = self;
    (void)transform_owner_routes61::Apply(0x82bd1558, m, d, s);
    bool swap = false;
    if (!Header(stream, "OPC", swap, 0x82bd15f4))
      return false;
    if (Scalar(stream, swap, 0x82bd1684) < 1)
      return false;
    auto flags = Scalar(stream, swap, 0x82bd16d4);
    m.WriteU32(self + 8, flags);
    if (flags & 4)
      return true;
    s.r[3] = self;
    s.r[4] = (flags >> 1) & 1;
    s.r[5] = flags & 1;
    (void)transform_owner_initialize61::Apply(0x82bd12f8, m, d, s);
    if (!(s.r[3] & 255))
      return false;
    auto strategy = Word(self + 16);
    s.r[3] = strategy;
    s.r[4] = swap;
    s.r[5] = stream;
    Call(Word(Word(strategy) + 24), 0x82bd1758);
    return (s.r[3] & 255) != 0;
  }
  bool Mesh(unsigned self, unsigned stream) {
    if (!Base(self, stream))
      return false;
    if (auto p = Word(self + 32)) {
      Free(p, 0x82bd1d54);
      m.WriteU32(self + 32, 0);
    }
    if (auto p = Word(self + 24)) {
      Free(p - 4, 0x82bd1d7c);
      m.WriteU32(self + 24, 0);
    }
    m.WriteU32(self + 20, 0);
    m.WriteU32(self + 28, 0);
    bool swap = false;
    if (!Header(stream, "HBM", swap, 0x82bd1d9c))
      return false;
    (void)Read(stream, 12, 0x82bd1e2c);
    auto count = Scalar(stream, swap, 0x82bd1e40);
    m.WriteU32(self + 20, count);
    if (count > 1) {
      unsigned size = count <= 0x3fffffff && count * 4 <= 0xfffffffb
                          ? count * 4 + 4
                          : 0xffffffff;
      auto raw = Allocate(size, 39, 0x82bd1ec8);
      auto out = raw ? raw + 4 : 0;
      if (raw)
        m.WriteU32(raw, count);
      m.WriteU32(self + 24, out);
      if (!out)
        return false;
      Indices(stream, swap, count, out, 0x82bd1f00);
    }
    count = Scalar(stream, swap, 0x82bd1f58);
    m.WriteU32(self + 28, count);
    if (count) {
      auto out = Allocate(count * 4, 62, 0x82bd1fb4);
      m.WriteU32(self + 32, out);
      if (!out)
        return false;
      Indices(stream, swap, count, out, 0x82bd1fd4);
    }
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
    s.r[3] =
        (entry == 0x82bd15c8 ? Base(self, stream) : Mesh(self, stream)) ? 1 : 0;
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82bd15c8 && e != 0x82bd1d08)
    return false;
  Reader{m, d, s}.Run(e);
  return true;
}
} // namespace lo::semantic::gpu::tree_envelope_load61
