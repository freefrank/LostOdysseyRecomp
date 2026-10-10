#include "lo_semantics/battle_bootstrap61.h"
#include "lo_semantics/battle_manager_access61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_bootstrap61 {
namespace {
using recovery_abi::Address;
void Call(unsigned entry, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_bootstrap61::Apply(entry, m, d, s) &&
      !(entry == 0x8229dfd8 && battle_manager_access61::Apply(entry, m, d, s)))
    d.guest.CallDirect(entry, m, s);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned frame = 0, first = 32;
  switch (e) {
  case 0x82b08a30:
  case 0x82acd398:
  case 0x82ac1b90:
    break;
  case 0x82ad1c88:
  case 0x822a7c58:
    frame = 112;
    first = 30;
    break;
  case 0x82aab200:
    frame = 128;
    first = 27;
    break;
  default:
    return false;
  }
  auto old = Address(s.r[1]), owner = Address(s.r[3]);
  if (frame) {
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= frame;
    m.WriteU32(Address(s.r[1]), old);
  }
  auto fp = [&]() {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
  };
  if (e == 0x82acd398) {
    for (unsigned off : {12u, 8u, 4u, 16u})
      m.WriteU32(owner + off, 0);
  } else if (e == 0x82b08a30) {
    fp();
    auto zero = m.ReadU32(0x82000e50);
    m.WriteU32(owner + 4, zero);
    m.WriteU8(owner + 24, 1);
    for (unsigned off : {8u, 12u, 16u})
      m.WriteU32(owner + off, zero);
    m.WriteU32(owner + 20, m.ReadU32(0x82007784));
  } else if (e == 0x82ad1c88) {
    m.WriteU32(old + 20, owner);
    m.WriteU32(owner, 0x820c0bac);
    m.WriteU32(0x8324570c, owner);
    m.WriteU32(owner, 0x820c0eb0);
    Call(0x82acd398, m, d, s);
    s.r[3] = owner;
  } else if (e == 0x822a7c58) {
    unsigned result = 0;
    if (owner) {
      auto type = m.ReadU32(0x83263198);
      if (!type) {
        s.r[3] = 0x821c4bdc;
        Call(0x82829368, m, d, s);
        m.WriteU32(0x83263198, Address(s.r[3]));
        Call(0x82820f70, m, d, s);
        type = m.ReadU32(0x83263198);
      }
      auto current = m.ReadU32(owner + 52);
      while (current && current != type)
        current = m.ReadU32(current + 60);
      if (current == type)
        result = owner;
    }
    s.r[3] = result;
  } else if (e == 0x82aab200) {
    m.WriteU32(owner + 144, 0);
    m.WriteU32(owner + 20, owner + 8);
    m.WriteU32(owner + 48, owner + 36);
    m.WriteU32(owner + 56, 0);
    s.r[3] = owner + 76;
    Call(0x82b08a30, m, d, s);
    s.r[3] = owner + 104;
    Call(0x82b08a30, m, d, s);
    for (auto header : {m.ReadU32(owner + 20), owner + 132}) {
      m.WriteU32(header + 4, 0);
      if (!m.ReadU32(header + 8))
        continue;
      auto data = m.ReadU32(header);
      m.WriteU32(header + 8, 0);
      if (!data)
        continue;
      auto manager = m.ReadU32(0x8330b608);
      if (!manager) {
        Call(0x827c5f38, m, d, s);
        manager = m.ReadU32(0x8330b608);
      }
      s.r[3] = manager;
      s.r[4] = data;
      s.r[5] = 0;
      s.r[6] = 8;
      s.ctr = m.ReadU32(m.ReadU32(manager) + 8);
      d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
      m.WriteU32(header, Address(s.r[3]));
    }
    m.WriteU32(owner + 4, 20);
    for (unsigned i = 0; i < 14; ++i)
      m.WriteU8(owner + 60 + i, 0);
    m.WriteU8(owner + 61, 1);
    m.WriteU8(owner + 65, 1);
    s.r[3] = m.ReadU32(0x83315fb4);
    Call(0x822a7c58, m, d, s);
    unsigned profile = 0;
    if (Address(s.r[3])) {
      s.r[3] = m.ReadU32(0x83315fb4);
      s.ctr = m.ReadU32(m.ReadU32(Address(s.r[3])) + 352);
      d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
      Call(0x8229dfd8, m, d, s);
      profile = Address(s.r[3]);
    }
    m.WriteU32(owner + 32, profile);
    m.WriteU32(owner + 52, 0);
  } else {
    for (unsigned i = 0; i < 32; ++i) {
      auto row = owner + 100 + 12 * i;
      m.WriteU32(row, 0);
      m.WriteU32(row + 4, 0xffffffff);
      m.WriteU8(row + 8, 0);
    }
    for (unsigned i = 0; i < 20; ++i)
      m.WriteU32(owner + 484 + 4 * i, 0);
    for (unsigned off : {92u, 88u, 84u, 1208u})
      m.WriteU32(owner + off, 0);
    m.WriteU8(owner + 1212, 0);
    fp();
    auto zero = m.ReadU32(0x82000e50);
    m.WriteU8(owner + 1213, 0);
    for (unsigned i = 0; i < 5; ++i) {
      auto row = owner + 564 + 128 * i;
      m.WriteU32(row, 0);
      m.WriteU32(row + 4, 0);
      m.WriteU8(row + 8, 0);
      for (unsigned j = 0; j < 5; ++j)
        m.WriteU8(row + 92 + j, 0);
      for (unsigned j = 0; j < 20; ++j)
        m.WriteU32(row + 12 + 4 * j, 0);
      m.WriteU32(row + 112, zero);
      m.WriteU32(row + 116, zero);
      for (unsigned off : {100u, 120u, 124u})
        m.WriteU32(row + off, 0);
    }
    m.WriteU32(owner + 1204, 0);
    auto source = m.ReadU32(m.ReadU32(0x832ca0d0) + 292);
    for (unsigned i = 0; i < 13; ++i)
      m.WriteU32(owner + 32 + 4 * i, m.ReadU32(source + 4 * i));
  }
  if (frame) {
    s.r[1] += frame;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
  }
  return true;
}
} // namespace lo::semantic::gpu::battle_bootstrap61
