#include "lo_semantics/battle_script_loading61.h"
#include "lo_semantics/battle_script61.h"
#include "lo_semantics/battle_script_text61.h"
#include "lo_semantics/battle_script_dispatch61.h"
#include "lo_semantics/battle_manager_access61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/recovery_abi.h"
#include <utility>
namespace lo::semantic::gpu::battle_script_loading61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_script_loading61::Apply(e, m, d, s) &&
      !battle_script61::Apply(e, m, d, s) &&
      !battle_script_text61::Apply(e, m, d, s) &&
      !battle_script_dispatch61::Apply(e, m, d, s) &&
      !(e == 0x8229dfd8 && battle_manager_access61::Apply(e, m, d, s)) &&
      !crt_copy_full_context::Apply(e, m, s))
    d.guest.CallDirect(e, m, s);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned frame, first;
  switch (e) {
  case 0x82b024e0:
  case 0x82b025b8:
  case 0x82a9ef80:
    frame = 96;
    first = 31;
    break;
  case 0x82a9f0a0:
  case 0x82a9f028:
    frame = 112;
    first = 29;
    break;
  case 0x82a9de60:
    frame = 160;
    first = 24;
    break;
  case 0x82a9e108:
    frame = 208;
    first = 18;
    break;
  default:
    return false;
  }
  auto old = Address(s.r[1]), owner = Address(s.r[3]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  if (e == 0x82a9de60)
    recovery_abi::WriteU64(m, old - 80, s.fpr_bits[31]);
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  auto state = [&]() { return m.ReadU32(owner + 44); };
  auto allocator = [&]() {
    auto manager = m.ReadU32(0x8330b608);
    if (!manager) {
      Call(0x827c5f38, m, d, s);
      manager = m.ReadU32(0x8330b608);
    }
    return manager;
  };
  auto allocate = [&](unsigned bytes) {
    s.r[3] = allocator();
    s.r[4] = bytes;
    s.r[5] = 8;
    s.ctr = m.ReadU32(m.ReadU32(Address(s.r[3])) + 4);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    return Address(s.r[3]);
  };
  auto release = [&](unsigned p) {
    s.r[3] = allocator();
    s.r[4] = p;
    s.ctr = m.ReadU32(m.ReadU32(Address(s.r[3])) + 12);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  };
  auto fp = [&]() {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
  };
  if (e == 0x82b024e0 || e == 0x82b025b8) {
    s.ctr = m.ReadU32(m.ReadU32(owner) + 116);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    auto index = Address(s.r[3]);
    s.r[3] = std::int32_t(index) < 0
                 ? 0
                 : m.ReadU32(m.ReadU32(m.ReadU32(owner + 8) + 4 * index) +
                             (e == 0x82b024e0 ? 84 : 72));
  } else if (e == 0x82a9de60) {
    fp();
    auto zero = m.ReadU32(0x82000e50);
    for (unsigned i = 0;
         std::int32_t(i) < std::int32_t(m.ReadU32(state() + 24)); ++i) {
      auto actor = m.ReadU32(state() + 4) + 472 * i;
      for (auto [offset, size] : {std::pair{12u, 1024u},
                                  {20u, 128u},
                                  {40u, 384u},
                                  {48u, 128u},
                                  {80u, 32u},
                                  {72u, 32u}})
        m.WriteU32(actor + offset, allocate(size));
      for (unsigned off : {4u, 52u, 56u, 44u, 16u, 24u, 28u, 32u, 36u})
        m.WriteU32(actor + off, 0);
      m.WriteU32(actor + 8, 0xffffffff);
      (void)FillGuestMemory(m, m.ReadU32(actor + 12), 0, 1024);
      for (unsigned j = 0; j < 16; ++j) {
        auto queue = m.ReadU32(actor + 40) + 24 * j;
        m.WriteU32(queue + 4, 0);
        m.WriteU32(queue, 255);
        m.WriteU32(queue + 8, 0xffffffff);
        m.WriteU32(queue + 20, 0);
        m.WriteU32(queue + 16, 0);
      }
      (void)FillGuestMemory(m, m.ReadU32(actor + 48), 0, 128);
      for (unsigned off : {84u, 76u, 100u, 296u})
        m.WriteU32(actor + off, 0);
      m.WriteU32(actor + 468, 0xffffffff);
      m.WriteU32(actor + 64, (m.ReadU32(actor + 64) & 3075) | 256);
      for (unsigned j = 0; j < 8; ++j) {
        auto row = actor + 104 + 24 * j;
        for (unsigned off : {0u, 4u, 8u, 16u, 20u})
          m.WriteU32(row + off, 0);
        m.WriteU32(row + 12, zero);
      }
      for (unsigned off : {96u, 320u, 300u, 316u, 332u})
        m.WriteU32(actor + off, 0);
      m.WriteU8(actor + 312, 0);
      (void)FillGuestMemory(m, actor + 340, 0, 128);
      m.WriteU32(actor + 68, 0);
    }
  } else if (e == 0x82a9e108) {
    Call(0x82a9dbb8, m, d, s);
    s.r[3] = owner;
    Call(0x82a9de60, m, d, s);
    auto input = m.ReadU32(owner + 32), cursor = input;
    bool success = input != 0;
    auto read = [&](unsigned destination, unsigned bytes) {
      s.r[3] = 0x83264560;
      s.r[4] = cursor;
      s.r[5] = destination;
      s.r[6] = bytes;
      Call(0x82834fe0, m, d, s);
      cursor = Address(s.r[3]);
    };
    if (success) {
      read(state() + 8, 4);
      read(state() + 12, 4);
      success = std::int32_t(m.ReadU32(state() + 12)) <=
                std::int32_t(m.ReadU32(state() + 24));
      if (success)
        for (unsigned i = 0;
             std::int32_t(i) < std::int32_t(m.ReadU32(state() + 12)); ++i) {
          auto actor = m.ReadU32(state() + 4) + 472 * i;
          read(actor, 4);
          read(actor + 16, 4);
          for (unsigned j = 0;
               std::int32_t(j) < std::int32_t(m.ReadU32(actor + 16)); ++j)
            read(m.ReadU32(actor + 20) + 4 * j, 2);
          read(actor + 24, 4);
          auto count = m.ReadU32(actor + 24);
          if (count) {
            m.WriteU32(actor + 28,
                       allocate(count > 0x3fffffff ? 0xffffffff : count * 4));
            for (unsigned j = 0;
                 std::int32_t(j) < std::int32_t(m.ReadU32(actor + 24)); ++j)
              read(m.ReadU32(actor + 28) + 4 * j, 4);
          } else
            m.WriteU32(actor + 28, 0);
          read(actor + 32, 4);
          auto bytes = m.ReadU32(actor + 32);
          if (!bytes) {
            m.WriteU32(actor + 36, 0);
            success = false;
            break;
          }
          auto code = allocate(bytes);
          m.WriteU32(actor + 36, code);
          s.r[3] = code;
          s.r[4] = cursor;
          s.r[5] = m.ReadU32(actor + 32);
          Call(0x82b7a0b0, m, d, s);
          cursor += m.ReadU32(actor + 32);
        }
      if (!success)
        release(input);
    }
    m.WriteU32(state() + 28, success
                                 ? (m.ReadU32(state() + 28) | 0x40000000)
                                 : (m.ReadU32(state() + 28) & ~0x40000000u));
    s.r[3] = success;
  } else if (e == 0x82a9ef80) {
    auto actor = m.ReadU32(owner + 24), queue = m.ReadU32(actor + 40);
    m.WriteU32(queue + 4, m.ReadU32(m.ReadU32(actor + 20)));
    m.WriteU32(queue, 128);
    m.WriteU32(actor + 56, 0);
    m.WriteU32(actor + 52, m.ReadU32(queue + 4));
    s.r[3] = owner;
    Call(0x8238bab0, m, d, s);
    actor = m.ReadU32(owner + 24);
    queue = m.ReadU32(actor + 40) + 24 * m.ReadU32(actor + 56);
    m.WriteU32(queue + 4, m.ReadU32(actor + 52));
  } else if (e == 0x82a9f028) {
    if (state() && (m.ReadU32(state() + 28) & 0x40000000))
      for (unsigned i = 0;
           std::int32_t(i) < std::int32_t(m.ReadU32(state() + 12)); ++i) {
        m.WriteU32(owner + 24, m.ReadU32(state() + 4) + 472 * i);
        s.r[3] = owner;
        Call(0x82a9ef80, m, d, s);
      }
  } else {
    s.r[3] = m.ReadU32(0x83315fb4);
    s.ctr = m.ReadU32(m.ReadU32(Address(s.r[3])) + 352);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    Call(0x8229dfd8, m, d, s);
    auto profile = Address(s.r[3]);
    m.WriteU32(owner + 28, profile);
    if (m.ReadU32(profile + 68) == 0xffffffff)
      s.r[3] = 1;
    else {
      m.WriteU32(owner + 8, 0);
      m.WriteU8(owner + 12, 0);
      m.WriteU8(owner + 20, 0);
      m.WriteU8(owner + 21, 0);
      m.WriteU8(owner + 4, 1);
      s.r[3] = 0x832cc05c;
      s.r[4] = m.ReadU32(0x832ca0e0 + 5624);
      Call(0x82b024e0, m, d, s);
      m.WriteU32(owner + 32, Address(s.r[3]));
      s.r[3] = 0x832cc05c;
      s.r[4] = m.ReadU32(0x832ca0e0 + 5624);
      Call(0x82b025b8, m, d, s);
      m.WriteU32(owner + 40, Address(s.r[3]));
      s.r[3] = owner;
      s.r[4] = 128;
      Call(0x82a9e108, m, d, s);
      s.r[3] = (Address(s.r[3]) & 255) != 0;
    }
  }
  s.r[1] += frame;
  if (e == 0x82a9de60)
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - 80);
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_loading61
