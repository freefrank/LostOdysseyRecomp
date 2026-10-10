#include "lo_semantics/cloth_load61.h"
#include "lo_semantics/mesh_stream_codec61.h"
#include "lo_semantics/cloth_storage61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::cloth_load61 {
namespace {
using recovery_abi::Address;
struct Loader {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  void Invoke(unsigned slot) {
    s.r[3] = Word(0x832df548);
    s.ctr = Word(Word(Address(s.r[3])) + slot);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  unsigned Allocate(unsigned bytes, unsigned tag) {
    s.r[4] = bytes;
    s.r[5] = tag;
    Invoke(8);
    return Address(s.r[3]);
  }
  void Free(unsigned p) {
    s.r[4] = p;
    Invoke(20);
  }
  void Copy(unsigned out, unsigned in, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i)
      m.WriteU8(out + i, m.ReadU8(in + i));
  }
  void Reserve(unsigned v, unsigned elements, unsigned stride) {
    auto start = Word(v), end = Word(v + 4), cap = Word(v + 8);
    if (start && (cap - start) / stride >= elements)
      return;
    auto bytes = elements * stride, p = Allocate(bytes, 282);
    Copy(p, start, end - start);
    if (start)
      Free(start);
    m.WriteU32(v, p);
    m.WriteU32(v + 4, p + (end - start));
    m.WriteU32(v + 8, p + bytes);
  }
  unsigned Append(unsigned v, unsigned stride) {
    auto end = Word(v + 4);
    if (Word(v + 8) <= end) {
      auto used = (end - Word(v)) / stride;
      Reserve(v, 2 * (used + 1), stride);
      end = Word(v + 4);
    }
    m.WriteU32(v + 4, end + stride);
    return end;
  }
  unsigned Scalar(unsigned stream, unsigned swap, bool half = false) {
    s.r[3] = swap;
    s.r[4] = stream;
    (void)mesh_stream_codec61::Apply(half ? 0x82bad858 : 0x82bad8b8, m,
                                     {d.guest, d.fp}, s);
    return Address(s.r[3]);
  }
  void Words(unsigned v, unsigned count, unsigned stream, unsigned swap) {
    for (unsigned i = 0; i < count; ++i) {
      auto value = Scalar(stream, swap);
      m.WriteU32(Append(v, 4), value);
    }
  }
  bool Read(unsigned self, unsigned stream) {
    auto sp = Address(s.r[1]);
    s.r[3] = 'C';
    s.r[4] = 'L';
    s.r[5] = 'T';
    s.r[6] = 'H';
    s.r[7] = sp + 84;
    s.r[8] = sp + 80;
    s.r[9] = stream;
    (void)mesh_stream_codec61::Apply(0x82bade98, m, {d.guest, d.fp}, s);
    if (!(s.r[3] & 255))
      return false;
    s.r[3] = self;
    (void)cloth_storage61::Apply(0x82ba8108, m, {d.guest, d.fp}, s);
    auto swap = m.ReadU8(sp + 80);
    auto type = Scalar(stream, swap);
    m.WriteU32(self + 212, type);
    m.WriteU32(self + 216, Scalar(stream, swap));
    auto count = Scalar(stream, swap);
    for (unsigned i = 0; i < count; ++i) {
      unsigned values[3];
      for (auto &v : values)
        v = Scalar(stream, swap);
      auto out = Append(self + 4, 12);
      for (unsigned j = 0; j < 3; ++j)
        m.WriteU32(out + 4 * j, values[j]);
    }
    if (type != 1 && type != 2)
      return false;
    auto faces = Scalar(stream, swap);
    Words(self + 24, faces * (type == 1 ? 3 : 4), stream, swap);
    Words(self + 44, Scalar(stream, swap), stream, swap);
    Words(self + 64, Scalar(stream, swap), stream, swap);
    if (type == 1)
      Words(self + 104, faces, stream, swap);
    auto permutations = Scalar(stream, swap);
    Words(self + 124, permutations, stream, swap);
    auto inverse = self + 144, start = Word(inverse), end = Word(inverse + 4),
         used = (end - start) / 4;
    if (permutations > used) {
      if (!start || (Word(inverse + 8) - start) / 4 < permutations)
        Reserve(inverse, 2 * permutations, 4);
      for (unsigned i = used; i < permutations; ++i)
        m.WriteU32(Word(inverse) + 4 * i, 0);
    }
    m.WriteU32(inverse + 4, Word(inverse) + 4 * permutations);
    if (Word(inverse) == Word(inverse + 4)) {
      if (Word(inverse))
        Free(Word(inverse));
      for (unsigned i = 0; i < 3; ++i)
        m.WriteU32(inverse + 4 * i, 0);
    }
    if (Word(inverse + 8) > Word(inverse + 4)) {
      s.r[4] = Word(inverse);
      s.r[5] = 4 * permutations;
      Invoke(16);
      auto p = Address(s.r[3]);
      m.WriteU32(inverse, p);
      m.WriteU32(inverse + 4, p + 4 * permutations);
      m.WriteU32(inverse + 8, p + 4 * permutations);
    }
    for (unsigned i = 0; i < permutations; ++i)
      m.WriteU32(Word(inverse) + 4 * Word(Word(self + 124) + 4 * i), i);
    Words(self + 164, Scalar(stream, swap), stream, swap);
    auto children = Scalar(stream, swap);
    for (unsigned i = 0; i < children; ++i) {
      auto child = Allocate(44, 285);
      if (child) {
        m.WriteU32(child, 0);
        for (unsigned off : {4, 24})
          for (unsigned j = 0; j < 3; ++j)
            m.WriteU32(child + off + 4 * j, 0);
      }
      m.WriteU32(Append(self + 184, 4), child);
      m.WriteU32(child, Scalar(stream, swap));
      Words(child + 4, Scalar(stream, swap), stream, swap);
      auto records = Scalar(stream, swap);
      for (unsigned j = 0; j < records; ++j) {
        auto record = sp + 96;
        for (unsigned k = 0; k < 4; ++k)
          m.WriteU16(record + 2 * k, Scalar(stream, swap, true));
        if (type == 1) {
          m.WriteU16(record + 8, 65535);
          m.WriteU16(record + 10, 65535);
          for (unsigned off : {12, 16, 20})
            m.WriteU32(record + off, Scalar(stream, swap));
        } else {
          m.WriteU32(record + 8, Scalar(stream, swap));
          for (unsigned k = 0; k < 6; ++k)
            m.WriteU16(record + 12 + 2 * k, Scalar(stream, swap, true));
        }
        m.WriteU16(record + 28, Scalar(stream, swap, true));
        Copy(Append(child + 24, 32), record, 32);
      }
    }
    return true;
  }
  void Run() {
    auto self = Address(s.r[3]), stream = Address(s.r[4]);
    auto old = s.r[1];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(old));
    s.r[3] = Read(self, stream);
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82baa130)
    return false;
  Loader{m, d, s}.Run();
  return true;
}
} // namespace lo::semantic::gpu::cloth_load61
