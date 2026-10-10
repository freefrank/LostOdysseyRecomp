#include "lo_semantics/archive_names61.h"
#include "lo_semantics/recovery_abi.h"
#include <cstdint>
namespace lo::semantic::gpu::archive_names61 {
namespace {
using recovery_abi::Address;
struct Names {
  GuestMemory &m;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  unsigned Length(unsigned p) {
    unsigned n = 0;
    while (m.ReadU8(p + n))
      ++n;
    return n;
  }
  static unsigned Fold(unsigned ch) {
    return ch >= 'A' && ch <= 'Z' ? ch + 32 : ch;
  }
  int Compare(unsigned a, unsigned b, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
      auto x = Fold(m.ReadU8(a + i)), y = Fold(m.ReadU8(b + i));
      if (x != y || !x)
        return int(x) - int(y);
    }
    return 0;
  }
  unsigned Decode(unsigned out, unsigned input, unsigned words, unsigned mode) {
    unsigned count = words & 255, p = out;
    m.WriteU8(out, 0);
    if (count)
      mode = 0;
    mode &= 255;
    if (mode) {
      auto header = m.ReadU16(input);
      input += 2;
      count = (header % 40) & 255;
      if (mode < 3 && mode == 1) {
        m.WriteU8(p++, m.ReadU8(0x820468dc + (header % 1600) / 40));
        m.WriteU8(p++, m.ReadU8(0x820468dc + header / 1600));
        m.WriteU8(p, 0);
      }
    }
    for (unsigned i = 0; i < count; ++i) {
      unsigned value = m.ReadU16(input);
      input += 2;
      auto ch = m.ReadU8(0x820468dc + value % 40);
      m.WriteU8(p, ch);
      if (!ch)
        break;
      value /= 40;
      ch = m.ReadU8(0x820468dc + value % 40);
      m.WriteU8(p + 1, ch);
      if (!ch)
        break;
      value /= 40;
      m.WriteU8(p + 2, m.ReadU8(0x820468dc + (value & 255)));
      p += 3;
      if (i + 1 == count)
        m.WriteU8(p, 0);
    }
    return count;
  }
  unsigned Lower(unsigned out, unsigned input, unsigned count, unsigned mode) {
    auto result = Decode(out, input, count, mode);
    for (unsigned p = out; m.ReadU8(p); ++p)
      m.WriteU8(p, Fold(m.ReadU8(p)));
    return result;
  }
  unsigned Name(unsigned archive, unsigned record, unsigned out) {
    auto bits = Word(record), index = bits & 0x3ffff;
    if (!index) {
      m.WriteU8(out, 0);
      return 0;
    }
    auto pool = Word(archive + 8);
    Lower(out, pool + 2 * index, 0, 1);
    auto length = Length(out);
    auto suffix = (Word(record) >> 18) & 31;
    if (suffix) {
      auto source = Word(0x831e9170 + 4 * (suffix - 1)), n = Length(source);
      for (unsigned i = 0; i <= n; ++i)
        m.WriteU8(out + length + i, m.ReadU8(source + i));
      length += n;
    }
    auto extension = (Word(record) >> 23) & 31;
    if (extension) {
      m.WriteU8(out + length++, '.');
      auto offset = m.ReadU16(Word(archive + 12) + 2 * (extension - 1));
      Lower(out + length, pool + 2 * offset, 0, 1);
      length += Length(out + length);
    }
    return length;
  }
  unsigned Segment(unsigned out, unsigned input) {
    unsigned n = 0;
    while (m.ReadU8(input + n) && m.ReadU8(input + n) != '\\') {
      m.WriteU8(out + n, m.ReadU8(input + n));
      ++n;
    }
    m.WriteU8(out + n, 0);
    return n;
  }
  void Candidates() {
    auto header = Address(s.r[3]), output = Address(s.r[4]),
         path = Address(s.r[6]), exact = Address(s.r[7]);
    auto old = Address(s.r[1]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 22; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 336;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    unsigned n = Length(path), archives = Word(header + 32),
         count = m.ReadU16(header + 26);
    unsigned found = 0, result = 0;
    bool matched = false;
    for (unsigned i = 0; i < count; ++i) {
      auto archive = archives + 48 * i,
           size = Name(archive, archive + 20, sp + 80);
      unsigned prefix = size;
      if (size <= n) {
        if (size && Compare(path, sp + 80, size))
          continue;
        auto next = m.ReadU8(path + size);
        if (size && next != '\\' && next)
          continue;
        if (!next && exact) {
          m.WriteU32(exact, archive);
          result = 0xffffffff;
          matched = true;
          break;
        }
      } else {
        if (exact || m.ReadU8(sp + 80 + n) != '\\' || Compare(path, sp + 80, n))
          continue;
        prefix = n;
      }
      m.WriteU32(output + 16 * found, archive);
      m.WriteU32(output + 16 * found + 8, prefix);
      ++found;
    }
    if (!matched) {
      // The source sorts using the low byte of the prefix lengths and only
      // moves the pointer/+8 fields, leaving the other candidate words alone.
      for (unsigned i = 1; i < found; ++i) {
        auto pointer = Word(output + 16 * i),
             key = Word(output + 16 * i + 8) & 255, j = i;
        while (j && key < (Word(output + 16 * (j - 1) + 8) & 255)) {
          m.WriteU32(output + 16 * j, Word(output + 16 * (j - 1)));
          m.WriteU32(output + 16 * j + 8,
                     Word(output + 16 * (j - 1) + 8) & 255);
          --j;
        }
        if (j != i) {
          m.WriteU32(output + 16 * j, pointer);
          m.WriteU32(output + 16 * j + 8, key);
        }
      }
      result = found;
    }
    s.r[1] += 336;
    for (unsigned i = 22; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = Word(old - 8);
    s.r[3] = matched ? 0xffffffffffffffffull : result;
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Registers &s) {
  Names n{m, s};
  auto a = Address(s.r[3]), b = Address(s.r[4]), c = Address(s.r[5]);
  switch (e) {
  case 0x8229d798:
    s.r[3] = n.Decode(a, b, c, Address(s.r[6]));
    return true;
  case 0x82376d60:
    s.r[3] = n.Lower(a, b, c, Address(s.r[6]));
    return true;
  case 0x82853030:
    s.r[3] = n.Name(a, b, c);
    return true;
  case 0x82852d68:
    s.r[3] = n.Segment(a, b);
    return true;
  case 0x823564d8:
    s.r[3] = std::uint64_t(std::int64_t(n.Compare(a, b, c)));
    return true;
  case 0x82853da8:
    n.Candidates();
    return true;
  default:
    return false;
  }
}
} // namespace lo::semantic::gpu::archive_names61
