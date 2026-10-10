#include "lo_semantics/battle_script_text61.h"
#include "lo_semantics/battle_script_parameters61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_script_text61 {
namespace {
using recovery_abi::Address;
unsigned Little(GuestMemory &m, unsigned p, unsigned count) {
  unsigned value = m.ReadU8(p);
  for (unsigned i = 1; std::int32_t(i) < std::int32_t(count); ++i) {
    unsigned shift = (8 * (i - 1)) & 63;
    unsigned weight = shift < 32 ? 256u << shift : 0;
    value += unsigned(m.ReadU8(p + i)) * weight;
  }
  return value;
}
void Append(GuestMemory &m, unsigned out, unsigned source) {
  while (m.ReadU16(out))
    out += 2;
  unsigned c;
  do {
    c = m.ReadU16(source);
    source += 2;
    m.WriteU16(out, c);
    out += 2;
  } while (c);
}
void Decimal(GuestMemory &m, unsigned value, unsigned out) {
  if (std::int32_t(value) < 0) {
    m.WriteU16(out, 45);
    out += 2;
    value = 0u - value;
  }
  auto begin = out;
  do {
    m.WriteU16(out, 48 + value % 10);
    out += 2;
    value /= 10;
  } while (value);
  m.WriteU16(out, 0);
  for (unsigned end = out - 2; begin < end; begin += 2, end -= 2) {
    auto x = m.ReadU16(begin);
    m.WriteU16(begin, m.ReadU16(end));
    m.WriteU16(end, x);
  }
}
struct Text {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner, sp;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Scratch(unsigned index) {
    auto state = W(owner + 44);
    return state && std::int32_t(index) < 16 ? W(W(state + 52) + 4 * index) : 0;
  }
  unsigned Actors() {
    d.guest.CallDirect(0x82380a18, m, s);
    d.guest.CallDirect(0x8238e2f8, m, s);
    return Address(s.r[3]);
  }
  unsigned Format(unsigned base, unsigned index, unsigned size, unsigned out,
                  unsigned capacity) {
    if (!base)
      return 0;
    auto offset = Little(m, base + 4 * index, 4);
    m.WriteU32(sp + 88, offset);
    if (std::int32_t(offset) > std::int32_t(size))
      return 0;
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(capacity); ++i)
      m.WriteU16(out + 2 * i, 0);
    m.WriteU16(sp + 84, 0);
    m.WriteU16(sp + 86, 0);
    auto source = base + offset;
    for (;;) {
      auto token = Little(m, source, 2);
      m.WriteU16(sp + 84, token);
      if (token == 0xe0)
        break;
      unsigned low = token & 255;
      if (low < 0xe0 || low > 0xf9) {
        // The original swaps ordinary units back before raw halfword append.
        m.WriteU16(sp + 84, m.ReadU16(source));
        Append(m, out, sp + 84);
      } else if (token == 0xe1) {
        source += 2;
        auto param = Little(m, source, 2);
        m.WriteU32(sp + 80, param);
        Decimal(m, Scratch(param), sp + 96);
        Append(m, out, sp + 96);
      } else if (token == 0x1e1 || token == 0x2e1) {
        auto id = W(W(owner + 44) + (token == 0x1e1 ? 60 : 64));
        auto list = Actors();
        for (unsigned i = 0; std::int32_t(i) < std::int32_t(W(Actors() + 4));
             ++i) {
          auto actor = W(W(list) + 4 * i);
          if (W(actor + 64) == id) {
            Append(m, out, W(actor + 80) ? W(actor + 76) : 0x821a83d0);
            break;
          }
        }
      } else if (token == 0x3e1 || token == 0x4e1 || token == 0x5e1 ||
                 token == 0x6e1) {
        source += 2;
        auto param = Little(m, source, 2);
        m.WriteU32(sp + 80, param);
        if (std::int32_t(param) >= 10000) {
          param -= 10000;
          m.WriteU32(sp + 80, param);
          param = Scratch(param);
        }
        if (token != 0x6e1) {
          unsigned field = token == 0x3e1 ? 60 : token == 0x4e1 ? 0 : 48;
          auto string = W(0x83264978 + 116) + 84 * param + field;
          auto ptr = W(string + 4) ? W(string) : 0x821a83d0;
          if (ptr)
            Append(m, out, ptr);
        }
      } else if (token == 0xbe1) {
        source += 2;
        auto count = Little(m, source, 2);
        m.WriteU32(sp + 80, count);
        for (unsigned i = 0; std::int32_t(i) < std::int32_t(count); ++i)
          Append(m, out, 0x82000b8c);
      }
      source += 2;
    }
    return 1;
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82834fe0) {
    auto p = Address(s.r[4]), n = Address(s.r[6]);
    m.WriteU32(Address(s.r[5]), Little(m, p, n));
    s.r[3] = p + n;
    return true;
  }
  if (e == 0x823a46c8) {
    Append(m, Address(s.r[3]), Address(s.r[4]));
    return true;
  }
  if (e != 0x82a9f1c0 && e != 0x82a9f718)
    return false;
  auto owner = Address(s.r[3]), old = Address(s.r[1]);
  unsigned frame = e == 0x82a9f1c0 ? 704 : 1136,
           first = e == 0x82a9f1c0 ? 21 : 29;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  if (e == 0x82a9f1c0) {
    s.r[3] = Text{m, d, s, owner, sp}.Format(Address(s.r[4]), Address(s.r[5]),
                                             Address(s.r[6]), Address(s.r[7]),
                                             Address(s.r[8]));
  } else {
    auto state = m.ReadU32(owner + 44);
    s.r[6] = m.ReadU32(state + 44);
    s.r[4] = 1;
    s.r[5] = 0;
    (void)battle_script_parameters61::Apply(0x8238be38, m, d, s);
    s.r[5] = s.r[3];
    s.r[4] = m.ReadU32(state + 48);
    s.r[3] = owner;
    s.r[7] = sp + 80;
    s.r[8] = 512;
    (void)battle_script_text61::Apply(0x82a9f1c0, m, d, s);
    d.guest.CallDirect(0x823a5058, m, s);
    s.r[4] = 0;
    s.r[5] = 30;
    s.r[6] = sp + 80;
    s.r[7] = 1;
    d.guest.CallDirect(0x82b079e8, m, s);
    auto actor = m.ReadU32(owner + 24);
    m.WriteU32(actor + 52, m.ReadU32(actor + 52) + 3);
  }
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_text61
