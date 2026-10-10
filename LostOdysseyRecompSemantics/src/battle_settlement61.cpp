#include "lo_semantics/battle_settlement61.h"
#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/battle_manager_access61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <limits>
namespace lo::semantic::gpu::battle_settlement61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_action_readiness61::Apply(e, m, d, s) &&
      !battle_manager_access61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
std::int32_t Trunc(float value) {
  return value > double(std::numeric_limits<std::int32_t>::max())
             ? std::numeric_limits<std::int32_t>::max()
         : !(value >= -2147483648.) ? std::numeric_limits<std::int32_t>::min()
                                    : std::int32_t(value);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  auto owner = Address(s.r[3]);
  auto W = [&](unsigned p) { return m.ReadU32(p); };
  if (e == 0x82ac0068) {
    auto id = Address(s.r[4]);
    if (!id)
      return true;
    unsigned index = 10;
    for (unsigned i = 0; i < 10; ++i)
      if (W(owner + 484 + 8 * i) == id) {
        index = i;
        break;
      }
    if (index == 10)
      for (unsigned i = 0; i < 10; ++i)
        if (!W(owner + 484 + 8 * i)) {
          index = i;
          m.WriteU32(owner + 484 + 8 * i, id);
          break;
        }
    if (index < 10)
      m.WriteU32(owner + 488 + 8 * index, W(owner + 488 + 8 * index) + 1);
    return true;
  }
  if (e == 0x82ac1ce0) {
    unsigned sum = 0, table = W(W(0x832ca0d0) + 120);
    for (unsigned i = 0; i < 32; ++i) {
      auto row = owner + 100 + 12 * i, id = W(row);
      if (id && m.ReadU8(row + 8) != 1)
        sum += W(table + 140 * id + 96);
    }
    s.r[3] = sum;
    return true;
  }
  unsigned frame, first, literal = 0;
  switch (e) {
  case 0x82ac31b0:
    frame = 160;
    first = 23;
    break;
  case 0x82ac22f8:
    frame = 176;
    first = 23;
    literal = 80;
    break;
  case 0x82ac1dc8:
    frame = 192;
    first = 23;
    literal = 84;
    break;
  case 0x82ac1fa0:
    frame = 176;
    first = 26;
    literal = 80;
    break;
  default:
    return false;
  }
  auto old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  if (e == 0x82ac1dc8)
    recovery_abi::WriteU64(m, old - 88, s.fpr_bits[31]);
  if (e == 0x82ac1fa0) {
    recovery_abi::WriteU64(m, old - 72, s.fpr_bits[30]);
    recovery_abi::WriteU64(m, old - 64, s.fpr_bits[31]);
  }
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  if (literal)
    m.WriteU32(sp + literal, 0x8204a1d8);
  auto blocked = [&](unsigned resource) {
    s.r[3] = resource;
    Call(0x82ab0a60, m, d, s);
    return (Address(s.r[3]) & 255) != 0;
  };
  auto has = [&](unsigned resource, unsigned id) {
    s.r[3] = resource;
    s.r[4] = id;
    Call(0x8238e368, m, d, s);
    return (Address(s.r[3]) & 255) == 1;
  };
  auto manager = [&]() {
    Call(0x82380a18, m, d, s);
    Call(0x82389b78, m, d, s);
  };
  auto fp = [&]() {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
  };
  auto F = [&](unsigned p) { return std::bit_cast<float>(W(p)); };
  if (e == 0x82ac31b0) {
    s.r[3] = W(0x83315fb4);
    s.ctr = W(W(Address(s.r[3])) + 352);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    Call(0x8229dfd8, m, d, s);
    auto play = Address(s.r[3]);
    m.WriteU32(owner + 96, W(play + 76));
    unsigned total = 0;
    for (unsigned i = 0; i < 32; ++i) {
      auto row = owner + 100 + 12 * i, id = W(row);
      if (!id)
        continue;
      auto value = W(W(W(0x832ca0d0) + 120) + 140 * id + 100),
           resourceID = W(row + 4);
      manager();
      s.r[4] = resourceID;
      Call(0x8238e308, m, d, s);
      auto flags = W(Address(s.r[3]) + 124);
      if (flags & 0x80)
        value *= 2;
      else if (flags & 0x100)
        value = unsigned(std::int32_t(value * 110) / 100);
      total += value;
    }
    if (m.ReadU8(owner + 1212) == 1)
      total *= 2;
    auto balance = W(play + 76) + total;
    m.WriteU32(play + 76, std::int32_t(balance) > 9999999 ? 9999999 : balance);
    s.r[3] = total;
  } else if (e == 0x82ac22f8) {
    Call(0x82380a18, m, d, s);
    Call(0x8238e2f8, m, d, s);
    auto list = Address(s.r[3]);
    unsigned slot = owner + 656;
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(W(list + 4)); ++i) {
      auto resource = W(W(list) + 4 * i);
      if (!(W(resource + 124) & 0x10000000u))
        continue;
      m.WriteU32(slot - 92, resource);
      if (!blocked(resource))
        for (unsigned j = 0; j < 5; ++j)
          if (has(resource, 145 + j)) {
            m.WriteU8(slot + j, 1);
            if (j == 0)
              m.WriteU32(owner + 1208, W(owner + 1208) + 1);
            if (j == 1)
              m.WriteU8(owner + 1212, 1);
            if (j == 2)
              m.WriteU8(owner + 1213, 1);
          }
      slot += 128;
    }
  } else if (e == 0x82ac1dc8) {
    bool doubled = false;
    for (unsigned i = 0; i < 5; ++i) {
      auto slot = owner + 564 + 128 * i, resource = W(slot);
      if (resource && !blocked(resource) && m.ReadU8(slot + 95) == 1) {
        doubled = true;
        break;
      }
    }
    fp();
    s.fpr_bits[31] = std::bit_cast<std::uint64_t>(double(F(0x82000e50)));
    for (unsigned i = 0; i < 5; ++i) {
      auto slot = owner + 568 + 128 * i, resource = W(slot - 4);
      if (!resource || blocked(resource))
        continue;
      for (unsigned j = 0; j < 32; ++j) {
        auto row = owner + 100 + 12 * j, id = W(row);
        float value = F(0x82000e50);
        if (id && m.ReadU8(row + 8) != 1) {
          auto raw = std::int32_t(W(W(W(0x832ca0d0) + 120) + 140 * id + 92));
          recovery_abi::WriteU64(m, sp + 88, std::uint64_t(std::int64_t(raw)));
          value = float(raw);
        }
        auto amount = Trunc(value);
        s.fpr_bits[0] = std::bit_cast<std::uint64_t>(std::int64_t(amount));
        m.WriteU32(sp + 80, unsigned(amount));
        m.WriteU32(owner + 84, unsigned(amount));
        if (!amount)
          continue;
        auto delta = std::int32_t(unsigned(amount) - W(resource + 140));
        if (delta < 0)
          m.WriteU32(slot, W(slot) + 1);
        else if (delta < 13)
          m.WriteU32(slot, W(slot) + W(owner + 4 * (unsigned(delta) + 8)));
        else {
          m.WriteU32(slot, 100);
          break;
        }
      }
      if (doubled)
        m.WriteU32(slot, W(slot) * 2);
      if (std::int32_t(W(slot)) > 100)
        m.WriteU32(slot, 100);
    }
  } else {
    fp();
    auto zero = F(0x82000e50), cap = F(0x8201dd2c);
    s.fpr_bits[30] = std::bit_cast<std::uint64_t>(double(zero));
    s.fpr_bits[31] = std::bit_cast<std::uint64_t>(double(cap));
    for (unsigned i = 0; i < 5; ++i) {
      auto slot = owner + 568 + 128 * i, resource = W(slot - 4);
      if (!resource)
        continue;
      bool unavailable = blocked(resource);
      auto previous = F(resource + 136);
      m.WriteU32(slot + 116, unsigned(Trunc(previous)));
      if (unavailable)
        continue;
      auto award = std::int32_t(W(slot));
      recovery_abi::WriteU64(m, sp + 88, std::uint64_t(std::int64_t(award)));
      auto next = float(float(award) + F(resource + 136));
      m.WriteU32(resource + 136, std::bit_cast<unsigned>(next));
      if (!(next < cap)) {
        if (std::int32_t(W(resource + 140)) >= 99)
          m.WriteU32(resource + 136, std::bit_cast<unsigned>(cap));
        else {
          m.WriteU32(resource + 136, std::bit_cast<unsigned>(zero));
          m.WriteU8(slot + 4, 1);
        }
      }
    }
  }
  if (literal)
    m.WriteU32(sp + literal, 0x8204a1d8);
  s.r[1] += frame;
  if (e == 0x82ac1dc8)
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - 88);
  if (e == 0x82ac1fa0) {
    s.fpr_bits[30] = recovery_abi::ReadU64(m, old - 72);
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - 64);
  }
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_settlement61
