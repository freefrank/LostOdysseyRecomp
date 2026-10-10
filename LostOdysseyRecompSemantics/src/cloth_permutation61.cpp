#include "lo_semantics/cloth_permutation61.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
#include <cstdint>
namespace lo::semantic::gpu::cloth_permutation61 {
namespace {
using recovery_abi::Address;
struct Permutation {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  void Invoke(unsigned slot) {
    s.r[3] = Word(0x832df548);
    s.ctr = Word(Word(Address(s.r[3])) + slot);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  unsigned Allocate(unsigned bytes) {
    s.r[4] = bytes;
    s.r[5] = 282;
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
  void Resize(unsigned v, unsigned count) {
    auto start = Word(v), end = Word(v + 4), used = (end - start) / 4;
    if (count > used) {
      if ((Word(v + 8) - start) / 4 < count) {
        auto p = Allocate(2 * count * 4);
        Copy(p, start, end - start);
        if (start)
          Free(start);
        start = p;
        m.WriteU32(v, p);
        m.WriteU32(v + 8, p + 2 * count * 4);
      }
      for (unsigned i = 4 * used; i < 4 * count; ++i)
        m.WriteU8(start + i, 0);
    }
    end = start + 4 * count;
    m.WriteU32(v + 4, end);
    if (end == start) {
      if (start)
        Free(start);
      m.WriteU32(v, 0);
      m.WriteU32(v + 4, 0);
      m.WriteU32(v + 8, 0);
    } else if (Word(v + 8) > end) {
      s.r[4] = start;
      s.r[5] = count * 4;
      Invoke(16);
      start = Address(s.r[3]);
      m.WriteU32(v, start);
      m.WriteU32(v + 4, start + count * 4);
      m.WriteU32(v + 8, start + count * 4);
    }
  }
  using Record = std::array<unsigned, 3>;
  Record Read(unsigned p) { return {Word(p), Word(p + 4), Word(p + 8)}; }
  void Write(unsigned p, Record a) {
    for (unsigned i = 0; i < 3; ++i)
      m.WriteU32(p + 4 * i, a[i]);
  }
  static bool Less(Record a, Record b) {
    return std::int32_t(a[0]) < std::int32_t(b[0]) ||
           (a[0] == b[0] && std::int32_t(a[1]) < std::int32_t(b[1]));
  }
  void Sort(unsigned base, std::int32_t left, std::int32_t right) {
    while (true) {
      auto i = left, j = right;
      auto pivot = Read(
          base +
          12 * unsigned(std::int32_t(unsigned(left) + unsigned(right)) / 2));
      while (i <= j) {
        while (Less(Read(base + 12 * unsigned(i)), pivot))
          ++i;
        while (Less(pivot, Read(base + 12 * unsigned(j))))
          --j;
        if (i > j)
          break;
        auto a = Read(base + 12 * unsigned(i)),
             b = Read(base + 12 * unsigned(j));
        Write(base + 12 * unsigned(i), b);
        Write(base + 12 * unsigned(j), a);
        ++i;
        --j;
      }
      if (left < j)
        Sort(base, left, j);
      if (i >= right)
        break;
      left = i;
    }
  }
  void Run() {
    auto self = Address(s.r[3]), old = Address(s.r[1]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 20; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 208;
    m.WriteU32(Address(s.r[1]), old);
    auto input = Word(self + 716), count = (Word(input + 4) - Word(input)) / 12;
    unsigned records = 0, capacity = 0;
    for (unsigned i = 0; i < count; ++i) {
      unsigned bucket = 0xffffffff;
      for (unsigned j = 0;
           j < 3 && std::int32_t(j) < std::int32_t(Word(self + 712)); ++j) {
        bucket = Word(Word(self + 68 + 20 * j) + 4 * i);
        if (bucket != 0xffffffff)
          break;
      }
      if (i == capacity) {
        auto p = Allocate(24 * (i + 1));
        Copy(p, records, 12 * i);
        if (records)
          Free(records);
        records = p;
        capacity = 2 * (i + 1);
      }
      Write(records + 12 * i, {bucket, 0xffffffff, i});
    }
    auto children = Word(self + 732);
    for (unsigned bucket = 0;
         bucket < (Word(children + 4) - Word(children)) / 4; ++bucket) {
      auto child = Word(Word(children) + 4 * bucket), list = Word(child + 4);
      for (unsigned j = 0; j < (Word(child + 8) - list) / 4; ++j) {
        auto record = records + 12 * Word(list + 4 * j);
        if (Word(record) == bucket)
          m.WriteU32(record + 4, j);
      }
    }
    Sort(records, 0, std::int32_t(count - 1));
    auto out = Word(self + 724);
    Resize(out, count);
    for (unsigned i = 0; i < count; ++i)
      m.WriteU32(Word(out) + 4 * Word(records + 12 * i + 8), i);
    if (records)
      Free(records);
    s.r[1] += 208;
    for (unsigned i = 20; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = Word(old - 8);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  Permutation t{m, d, s};
  if (e == 0x82bb5518)
    t.Sort(m.ReadU32(Address(s.r[3])), std::int32_t(s.r[4]),
           std::int32_t(s.r[5]));
  else if (e == 0x82bb7b48)
    t.Run();
  else
    return false;
  return true;
}
} // namespace lo::semantic::gpu::cloth_permutation61
