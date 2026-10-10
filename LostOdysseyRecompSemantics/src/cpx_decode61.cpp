#include "lo_semantics/cpx_decode61.h"
#include "lo_semantics/recovery_abi.h"
#include <cstdint>
namespace lo::semantic::gpu::cpx_decode61 {
namespace {
using recovery_abi::Address;
struct Decoder {
  GuestMemory &m;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  static unsigned ShiftLeft(unsigned v, unsigned n) {
    n &= 63;
    return n < 32 ? v << n : 0;
  }
  static unsigned ShiftRight(unsigned v, unsigned n) {
    n &= 63;
    return n < 32 ? v >> n : 0;
  }
  unsigned Bit(unsigned p) {
    auto remaining = m.ReadU16(p + 6), bits = m.ReadU16(p + 4);
    if (remaining > 1) {
      --remaining;
      m.WriteU16(p + 6, remaining);
      return ShiftRight(bits, remaining) & 1;
    }
    auto next = Word(p);
    m.WriteU16(p + 6, 16);
    m.WriteU16(p + 4, m.ReadU16(next));
    m.WriteU32(p, next + 2);
    return bits & 1;
  }
  unsigned Bits(unsigned p, unsigned count) {
    auto remaining = m.ReadU16(p + 6), bits = m.ReadU16(p + 4);
    if (std::int32_t(count) < std::int32_t(remaining)) {
      auto left = (remaining - count) & 65535;
      m.WriteU16(p + 6, left);
      return ShiftRight(bits, left) & (ShiftLeft(1, count) - 1);
    }
    auto extra = count - remaining,
         result = unsigned(bits) & (ShiftLeft(1, remaining) - 1),
         next = Word(p);
    auto value = m.ReadU16(next);
    m.WriteU32(p, next + 2);
    m.WriteU16(p + 4, value);
    if (!extra) {
      m.WriteU16(p + 6, 16);
      return result;
    }
    auto left = (16 - extra) & 65535;
    m.WriteU16(p + 6, left);
    if (std::int32_t(extra) > 0)
      result = ShiftLeft(result, extra) +
               (ShiftRight(value, left) & (ShiftLeft(1, extra) - 1));
    return result;
  }
  unsigned Bit8(unsigned p) {
    unsigned remaining = m.ReadU8(p + 5), bits = m.ReadU8(p + 4);
    if (remaining > 1) {
      m.WriteU8(p + 5, --remaining);
      return ShiftRight(bits, remaining) & 1;
    }
    auto next = Word(p);
    m.WriteU8(p + 5, 8);
    m.WriteU8(p + 4, m.ReadU8(next));
    m.WriteU32(p, next + 1);
    return bits & 1;
  }
  unsigned Bits8(unsigned p, unsigned count) {
    unsigned result = 0;
    for (unsigned i = 0; i < count; ++i)
      result = (result << 1) | Bit8(p);
    return result;
  }
  void Header(unsigned out, unsigned in, bool alternative) {
    unsigned first = m.ReadU8(in), distance = (first >> 4) & 3,
             variant = (first >> 2) & 3;
    m.WriteU8(out, first >> 6);
    m.WriteU8(out + 1, distance);
    if (distance != 3) {
      auto second = m.ReadU8(in + 1);
      m.WriteU8(out + 2, second & 15);
      m.WriteU8(out + 3, second >> 4);
      m.WriteU8(out + 4, 0);
      m.WriteU8(out + 2 + distance, 16);
      for (unsigned j = 8; j < 11; ++j)
        m.WriteU8(out + j, 2);
      if (alternative) {
        for (unsigned j = 5; j < 8; ++j)
          m.WriteU8(out + j, 3);
        if (variant < 3) {
          m.WriteU8(out + 5 + variant, 2);
          for (unsigned j = 0, k = 0; j < 3; ++j)
            if (j != variant)
              m.WriteU8(out + 8 + j, 6 + k++);
        }
      } else if (variant == 0) {
        m.WriteU8(out + 8, 0);
        m.WriteU8(out + 9, 1);
      } else if (variant == 1) {
        m.WriteU8(out + 9, 0);
        m.WriteU8(out + 8, 1);
      } else if (variant == 2) {
        m.WriteU8(out + 9, 0);
        m.WriteU8(out + 10, 1);
      }
    }
    m.WriteU32(out + 12, unsigned(m.ReadU8(in + 2)) +
                             (unsigned(m.ReadU8(in + 3)) << 8) + 1);
  }
  unsigned Length(unsigned params, unsigned bits, bool byte = false) {
    auto bit = [&]() { return byte ? Bit8(bits) : Bit(bits); };
    auto mode = m.ReadU8(params);
    unsigned width = 9;
    if (mode == 1) {
      if (!bit())
        width = 2;
    } else if (mode == 2) {
      if (!bit())
        width = 3;
    }
    return (byte ? Bits8(bits, width) : Bits(bits, width)) + 3;
  }
  unsigned Block(unsigned context, unsigned out, unsigned input, unsigned sp,
                 bool byte = false) {
    m.WriteU16(context + 16, m.ReadU16(context + 16) + 1);
    auto count = unsigned(m.ReadU8(input + 2)) +
                 (unsigned(m.ReadU8(input + 3)) << 8) + 1;
    m.WriteU32(sp + 80, Word(input));
    if (m.ReadU8(input) == 255) {
      if (!m.ReadU8(input + 1))
        for (unsigned i = 0; i < count; ++i)
          m.WriteU8(out + i, m.ReadU8(input + 4 + i));
      m.WriteU32(context + 32, Word(context + 32) + count);
      return count;
    }
    auto params = sp + 96, bits = sp + 80;
    Header(params, bits, false);
    if (byte) {
      m.WriteU32(bits, input + 5);
      m.WriteU8(bits + 4, m.ReadU8(input + 4));
      m.WriteU8(bits + 5, 8);
    } else {
      m.WriteU32(bits, input + 6);
      m.WriteU16(bits + 4, m.ReadU16(input + 4));
      m.WriteU16(bits + 6, 16);
    }
    auto bit = [&]() { return byte ? Bit8(bits) : Bit(bits); };
    auto take = [&](unsigned n) {
      return byte ? Bits8(bits, n) : Bits(bits, n);
    };
    auto limit = Word(params + 12);
    unsigned produced = 0;
    do {
      if (!bit()) {
        m.WriteU8(out + produced, take(8));
        ++produced;
        continue;
      }
      unsigned distance = 0;
      auto mode = m.ReadU8(params + 1);
      if (mode == 0) {
        if (byte)
          distance = take(16);
        else {
          auto p = Word(bits);
          distance = m.ReadU16(p);
          m.WriteU32(bits, p + 2);
        }
      } else if (mode == 1) {
        auto selector = bit();
        distance = take(m.ReadU8(params + 2 + selector));
      } else if (mode == 2) {
        auto selector =
            !bit() ? m.ReadU8(params + 8) : m.ReadU8(params + 9 + bit());
        distance = take(m.ReadU8(params + 2 + selector));
      }
      auto length = Length(params, bits, byte);
      for (unsigned i = 0; i < length; ++i) {
        m.WriteU8(out + produced, m.ReadU8(out + produced - distance - 1));
        ++produced;
      }
    } while (produced < limit);
    m.WriteU32(context + 32, Word(context + 32) + produced);
    return produced;
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Registers &s) {
  Decoder d{m, s};
  auto a = Address(s.r[3]), b = Address(s.r[4]), c = Address(s.r[5]);
  switch (e) {
  case 0x82857088:
    s.r[3] = d.Bit(a);
    return true;
  case 0x828570e0:
    s.r[3] = d.Bit8(a);
    return true;
  case 0x828575e0:
    s.r[3] = d.Bits8(a, b);
    return true;
  case 0x82857a38:
    s.r[3] = d.Length(b, c, true);
    return true;
  case 0x82857520:
    s.r[3] = d.Bits(a, b);
    return true;
  case 0x828576e8:
    d.Header(a, b, c != 0);
    return true;
  case 0x82857970:
    s.r[3] = d.Length(b, c);
    return true;
  case 0x82857d90:
  case 0x82857b00: {
    auto old = Address(s.r[1]);
    unsigned first = e == 0x82857d90 ? 24 : 23;
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 192;
    m.WriteU32(Address(s.r[1]), old);
    auto result = d.Block(a, b, c, Address(s.r[1]), e == 0x82857d90);
    s.r[1] += 192;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    s.r[3] = result;
    return true;
  }
  default:
    return false;
  }
}
} // namespace lo::semantic::gpu::cpx_decode61
