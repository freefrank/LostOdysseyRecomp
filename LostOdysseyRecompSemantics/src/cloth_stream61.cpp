#include "lo_semantics/cloth_stream61.h"
#include "lo_semantics/recovery_abi.h"
#include <initializer_list>
namespace lo::semantic::gpu::cloth_stream61 {
namespace {
using recovery_abi::Address;
struct Writer {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  unsigned Count(unsigned vector, unsigned stride = 4) {
    return unsigned(std::int32_t(Word(vector + 4) - Word(vector)) /
                    std::int32_t(stride));
  }
  void Put(unsigned stream, unsigned swap, unsigned value, bool half = false) {
    s.r[3] = value;
    s.r[4] = swap;
    s.r[5] = stream;
    (void)mesh_stream_codec61::Apply(half ? 0x82bad9a0 : 0x82bada00, m, d, s);
  }
  void Vector(unsigned object, unsigned offset, unsigned stream, unsigned swap,
              bool count = true) {
    if (count)
      Put(stream, swap, Count(object + offset));
    for (unsigned i = 0; i < Count(object + offset); ++i)
      Put(stream, swap, Word(Word(object + offset) + 4 * i));
  }
  bool Write(unsigned self, unsigned stream, unsigned swap) {
    s.r[3] = 'C';
    s.r[4] = 'L';
    s.r[5] = 'T';
    s.r[6] = 'H';
    s.r[7] = 3;
    s.r[8] = swap;
    s.r[9] = stream;
    (void)mesh_stream_codec61::Apply(0x82badd60, m, d, s);
    if (!(s.r[3] & 255))
      return false;
    Put(stream, swap, Word(self + 212));
    Put(stream, swap, Word(self + 216));
    Put(stream, swap, Count(self + 4, 12));
    Vector(self, 4, stream, swap, false);
    auto type = Word(self + 212);
    if (type != 1 && type != 2)
      return false;
    Put(stream, swap, Count(self + 24) / (type == 1 ? 3 : 4));
    Vector(self, 24, stream, swap, false);
    Vector(self, 44, stream, swap);
    Vector(self, 64, stream, swap);
    if (type == 1)
      for (unsigned i = 0; i < Count(self + 24) / 3; ++i)
        Put(stream, swap, Word(Word(self + 104) + 4 * i));
    Vector(self, 124, stream, swap);
    Vector(self, 164, stream, swap);
    Put(stream, swap, Count(self + 184));
    for (unsigned child = 0; child < Count(self + 184); ++child) {
      auto object = Word(Word(self + 184) + 4 * child);
      Put(stream, swap, Word(object));
      Vector(object, 4, stream, swap);
      Put(stream, swap, Count(object + 24, 32));
      for (unsigned i = 0; i < Count(object + 24, 32); ++i) {
        auto p = Word(object + 24) + 32 * i;
        for (unsigned j = 0; j < 4; ++j)
          Put(stream, swap, m.ReadU16(p + 2 * j), true);
        if (type == 1) {
          for (unsigned off : {12, 16, 20})
            Put(stream, swap, Word(p + off));
        } else {
          Put(stream, swap, Word(p + 8));
          for (unsigned j = 0; j < 6; ++j)
            Put(stream, swap, m.ReadU16(p + 12 + 2 * j), true);
        }
        Put(stream, swap, m.ReadU16(p + 28), true);
      }
    }
    return true;
  }
  void Run() {
    auto self = Address(s.r[3]), stream = Address(s.r[4]),
         swap = Address(s.r[5]);
    auto old = s.r[1];
    m.WriteU32(Address(old - 8), Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
    s.r[1] -= 512;
    m.WriteU32(Address(s.r[1]), Address(old));
    s.r[3] = Write(self, stream, swap);
    s.r[1] += 512;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, Address(old - 16 - 8 * (31 - i)));
    s.lr = Word(Address(old - 8));
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82ba7760)
    return false;
  Writer{m, d, s}.Run();
  return true;
}
} // namespace lo::semantic::gpu::cloth_stream61
