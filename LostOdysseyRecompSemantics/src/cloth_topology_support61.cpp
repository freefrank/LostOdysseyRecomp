#include "lo_semantics/cloth_topology_support61.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
#include <cstdint>
namespace lo::semantic::gpu::cloth_topology_support61 {
namespace {
using recovery_abi::Address;
using Record = std::array<unsigned, 4>;
struct Topology {
  GuestMemory &m;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  Record Read(unsigned p, unsigned words) {
    Record a{};
    for (unsigned i = 0; i < words; ++i)
      a[i] = Word(p + 4 * i);
    return a;
  }
  void Write(unsigned p, Record a, unsigned words) {
    for (unsigned i = 0; i < words; ++i)
      m.WriteU32(p + 4 * i, a[i]);
  }
  static bool Less(Record a, Record b, unsigned keys) {
    for (unsigned i = 0; i < keys; ++i) {
      if (a[i] < b[i])
        return true;
      if (a[i] > b[i])
        return false;
    }
    return false;
  }
  void Sort(unsigned base, std::int32_t left, std::int32_t right,
            unsigned words, unsigned keys) {
    while (true) {
      auto i = left, j = right;
      auto middle = std::int32_t(unsigned(left) + unsigned(right)) / 2;
      auto pivot = Read(base + 4 * words * unsigned(middle), words);
      while (i <= j) {
        while (Less(Read(base + 4 * words * unsigned(i), words), pivot, keys))
          ++i;
        while (Less(pivot, Read(base + 4 * words * unsigned(j), words), keys))
          --j;
        if (i > j)
          break;
        auto a = Read(base + 4 * words * unsigned(i), words),
             b = Read(base + 4 * words * unsigned(j), words);
        Write(base + 4 * words * unsigned(i), b, words);
        Write(base + 4 * words * unsigned(j), a, words);
        ++i;
        --j;
      }
      if (left < j)
        Sort(base, left, j, words, keys);
      if (i >= right)
        break;
      left = i;
    }
  }
  unsigned Find(unsigned vector, unsigned key) {
    auto base = Word(vector);
    auto count = std::int32_t(Word(vector + 4) - base) / 12;
    unsigned lo = 0, hi = unsigned(count - 1);
    auto wanted = Read(key, 3);
    // The caller supplies a nonempty sorted range, as in the original routine.
    do {
      auto mid = (lo + hi) >> 1;
      auto got = Read(base + 12 * mid, 3);
      if (!Less(got, wanted, 3) && !Less(wanted, got, 3)) {
        if (mid) {
          auto prev = Read(base + 12 * (mid - 1), 3);
          if (prev[0] == got[0] && prev[1] == got[1])
            return 0;
        }
        return base + 12 * mid;
      }
      if (Less(wanted, got, 3))
        hi = mid - 1;
      else
        lo = mid + 1;
    } while (lo <= hi);
    return 0;
  }
  void Describe(unsigned self, unsigned out) {
    for (unsigned i = 0; i < 15; ++i)
      m.WriteU32(out + 4 * i, 0);
    m.WriteU32(out + 4,
               unsigned(std::int32_t(Word(self + 8) - Word(self + 4)) / 12));
    m.WriteU32(out + 12, (Word(self + 28) - Word(self + 24)) / 12);
    m.WriteU32(out + 28, Word(self + 4));
    m.WriteU32(out + 52, Word(self + 44));
    m.WriteU32(out + 56, Word(self + 64));
    m.WriteU32(out + 8, 12);
    m.WriteU32(out + 16, 12);
    m.WriteU32(out + 44, 4);
    m.WriteU32(out + 48, 4);
    m.WriteU32(out + 32, Word(self + 24));
    m.WriteU32(out + 40, Word(self + 216));
    if (Word(self + 208)) {
      m.WriteU32(out + 12, 0);
      m.WriteU32(out + 32, 0);
      m.WriteU32(out + 16, 0);
      m.WriteU32(out + 24, 16);
      m.WriteU32(out + 20, (Word(self + 28) - Word(self + 24)) / 16);
      m.WriteU32(out + 36, Word(self + 24));
    }
    s.r[3] = 1;
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Registers &s) {
  Topology t{m, s};
  auto a = Address(s.r[3]), b = Address(s.r[4]), c = Address(s.r[5]);
  switch (e) {
  case 0x82ba7080:
    t.Sort(m.ReadU32(a), std::int32_t(b), std::int32_t(c), 4, 2);
    break;
  case 0x82ba7250:
    t.Sort(m.ReadU32(a), std::int32_t(b), std::int32_t(c), 3, 3);
    break;
  case 0x82ba7438:
    t.Sort(m.ReadU32(a), std::int32_t(b), std::int32_t(c), 4, 3);
    break;
  case 0x82ba7db8:
    s.r[3] = t.Find(a, b);
    break;
  case 0x82ba7648:
    t.Describe(a, b);
    break;
  default:
    return false;
  }
  return true;
}
} // namespace lo::semantic::gpu::cloth_topology_support61
