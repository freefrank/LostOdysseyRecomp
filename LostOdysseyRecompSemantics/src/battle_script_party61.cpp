#include "lo_semantics/battle_script_party61.h"
#include "lo_semantics/battle_script_parameters61.h"
#include "lo_semantics/battle_script_core61.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
#include <bit>
namespace lo::semantic::gpu::battle_script_party61 {
namespace {
using recovery_abi::Address;
struct Party {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner, sp;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Actor() { return W(owner + 24); }
  unsigned Code() { return W(Actor() + 36) + W(Actor() + 52); }
  unsigned Get(unsigned off) {
    s.r[3] = owner;
    s.r[4] = off;
    s.r[5] = 0;
    (void)battle_script_parameters61::Apply(0x8238be38, m, d, s);
    return Address(s.r[3]);
  }
  void Set(unsigned off, unsigned value) {
    s.r[3] = owner;
    s.r[4] = off;
    s.r[5] = value;
    s.r[6] = 0;
    (void)battle_script_core61::Apply(0x8238c210, m, d, s);
  }
  void Next(unsigned n) { m.WriteU32(Actor() + 52, W(Actor() + 52) + n); }
  unsigned Mask(unsigned bit) {
    bit &= 63;
    return bit < 32 ? 1u << bit : 0;
  }
  void Inventory(unsigned item, unsigned count) {
    s.r[3] = owner;
    d.guest.CallDirect(0x82380a18, m, s);
    d.guest.CallDirect(0x82ab0110, m, s);
    auto table = W(W(Address(s.r[3])) + 4) + 72, p = table + 4 * item;
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto old = std::bit_cast<float>(W(p)),
         zero = std::bit_cast<float>(W(0x82000e50));
    if (old == zero)
      for (unsigned i = 0; i < 1024; ++i)
        if (!W(table + 4096 + 4 * i)) {
          m.WriteU32(table + 4096 + 4 * i, item);
          break;
        }
    recovery_abi::WriteU64(m, sp + 80,
                           std::uint64_t(std::int64_t(std::int32_t(count))));
    auto value = float(float(std::int32_t(count)) + old),
         cap = std::bit_cast<float>(W(0x8204bc58));
    // Original unordered comparison also takes the cap store.
    m.WriteU32(p, std::bit_cast<unsigned>(value <= cap ? value : cap));
    s.r[3] = W(owner + 28);
    s.r[4] = item;
    s.r[5] = 1;
    s.r[6] = 1;
    s.r[7] = 1;
    s.ctr = W(W(Address(s.r[3])) + 312);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  void Run(unsigned entry) {
    if (entry == 0x82a9e5e0) {
      Inventory(Address(s.r[4]), Address(s.r[5]));
      return;
    }
    if (entry == 0x82a9ea80) {
      auto count = Get(3), item = Get(1);
      s.r[3] = owner;
      s.r[4] = item;
      s.r[5] = count;
      (void)battle_script_party61::Apply(0x82a9e5e0, m, d, s);
      Next(5);
      return;
    }
    if (entry == 0x82a9d1b8) {
      auto index = Get(1), value = Get(3);
      if (std::int32_t(index) <= 16)
        m.WriteU32(W(W(owner + 44) + 52) + 4 * index, value);
      Next(5);
      return;
    }
    auto play = W(owner + 28);
    if (entry == 0x82a9eae8) {
      unsigned item = Get(2),
               mode = m.ReadU8(Code() +
                               ((W(W(owner + 44) + 28) & 0x04000000) ? 2 : 1));
      if (mode < 2) {
        auto p = play + 96 + 4 * mode;
        m.WriteU32(p, W(p) | Mask(item));
      } else if (mode == 2) {
        // Preserve the original loop: it can fill several vacant slots.
        for (unsigned i = 0; i < 5; ++i) {
          auto p = play + 104 + 4 * i, v = W(p);
          if (v == item)
            break;
          if (v == 0xffffffff)
            m.WriteU32(p, item);
        }
      } else if (mode == 3) {
        std::array<unsigned, 5> values{};
        for (unsigned i = 0; i < 5; ++i) {
          auto p = play + 104 + 4 * i, v = W(p);
          values[i] = v == item ? 0xffffffff : v;
          m.WriteU32(sp + 80 + 4 * i, values[i]);
          m.WriteU32(p, 0xffffffff);
        }
        unsigned out = 0;
        for (auto v : values)
          if (v != 0xffffffff)
            m.WriteU32(play + 104 + 4 * out++, v);
      }
      Next(4);
      return;
    }
    if (entry == 0x82a9d4e0) {
      auto mask = Mask(Get(1));
      if (W(play + 96) & mask)
        Next(5);
      else {
        auto p = Code() + 3,
             v = unsigned(m.ReadU8(p)) | (unsigned(m.ReadU8(p + 1)) << 8);
        m.WriteU32(Actor() + 52, unsigned(std::int32_t(std::int16_t(v))));
      }
      return;
    }
    std::array<unsigned, 5> values{};
    if (entry == 0x82a9d3c8) {
      for (unsigned i = 0; i < 5; ++i)
        values[i] = W(play + 104 + 4 * i);
      for (unsigned i = 0; i < 5; ++i)
        Set(1 + 2 * i, values[i]);
    } else {
      for (unsigned i = 0; i < 5; ++i)
        values[i] = Get(1 + 2 * i);
      for (unsigned i = 0; i < 5; ++i)
        m.WriteU32(play + 104 + 4 * i, values[i]);
    }
    Next(11);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82a9da20 || e == 0x822d3068)
    return true;
  if (e != 0x82a9e5e0 && e != 0x82a9ea80 && e != 0x82a9eae8 &&
      e != 0x82a9d4e0 && e != 0x82a9d3c8 && e != 0x82a9d450 && e != 0x82a9d1b8)
    return false;
  auto owner = Address(s.r[3]), old = Address(s.r[1]);
  unsigned frame =
      (e == 0x82a9ea80 || e == 0x82a9d4e0 || e == 0x82a9d1b8) ? 96 : 128;
  unsigned first = e == 0x82a9e5e0   ? 29
                   : e == 0x82a9eae8 ? 30
                   : e == 0x82a9d3c8 ? 28
                   : e == 0x82a9d450 ? 27
                   : e == 0x82a9d4e0 ? 32
                                     : 31;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  Party{m, d, s, owner, sp}.Run(e);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_party61
