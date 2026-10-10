#include "lo_semantics/battle_completion61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/string_storage_context61.h"
namespace lo::semantic::gpu::battle_completion61 {
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  using recovery_abi::Address;
  auto owner = Address(s.r[3]), id = Address(s.r[4]);
  if (e == 0x82b02f08) {
    s.r[3] = std::uint64_t(-1);
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(m.ReadU32(owner + 140));
         ++i)
      if (m.ReadU32(m.ReadU32(owner + 136) + 8 * i) == id) {
        s.r[3] = i;
        break;
      }
    return true;
  }
  if (e == 0x82b07e80) {
    auto count = m.ReadU32(owner + 8), data = m.ReadU32(owner + 4);
    s.r[3] = 0;
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(count); ++i) {
      auto row = data + 18044 * i;
      if (m.ReadU32(row + 4) != id)
        continue;
      auto state = m.ReadU8(row);
      s.r[3] = state == 2 || state == 3 || state == 4;
      break;
    }
    return true;
  }
  if (e == 0x82b1a100 || e == 0x82b1a168) {
    auto count = m.ReadU32(owner + 12), data = m.ReadU32(owner + 8);
    s.r[3] = 0;
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(count); ++i) {
      auto row = m.ReadU32(data + 4 * i);
      if (m.ReadU32(row + 8) != id)
        continue;
      s.r[3] = m.ReadU8(row + 6) == (e == 0x82b1a100 ? 3 : 6);
      break;
    }
    return true;
  }
  if (e != 0x82b03428 && e != 0x82aad200)
    return false;
  auto old = Address(s.r[1]);
  unsigned frame = e == 0x82b03428 ? 128 : 96,
           first = e == 0x82b03428 ? 27 : 31;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  auto call = [&](unsigned a) {
    if (!battle_completion61::Apply(a, m, d, s) &&
        !string_storage_context61::Apply(a, m, d, s))
      d.guest.CallDirect(a, m, s);
  };
  if (e == 0x82aad200) {
    s.r[3] = 0x832cc05c;
    s.r[4] = id;
    call(0x82b02f08);
    auto index = std::int32_t(Address(s.r[3]));
    s.r[3] = 0;
    if (index >= 0) {
      auto value =
          m.ReadU32(m.ReadU32(0x832cc05c + 136) + 8 * unsigned(index) + 4);
      if (std::int32_t(value) >= 0) {
        s.r[3] = 0x832cc0fc;
        s.r[4] = value;
        call(0x82b1a168);
      }
    }
  } else {
    call(0x82380a18);
    call(0x82389aa0);
    auto rank = std::int8_t(m.ReadU8(Address(s.r[3]) + 133));
    bool ready = rank > 0;
    if (ready)
      ready = rank < std::int32_t(m.ReadU32(m.ReadU32(0x832ca0d0) + 452));
    if (ready && std::int32_t(m.ReadU32(owner + 116)) >= 0) {
      s.r[3] = 0x832cb670;
      s.r[4] = m.ReadU32(owner + 116);
      d.guest.CallIndirect(m.ReadU32(m.ReadU32(0x832cb670) + 28), m, s);
      ready = (Address(s.r[3]) & 255) != 0;
    }
    if (ready && std::int32_t(m.ReadU32(owner + 120)) >= 0) {
      s.r[3] = 0x832cb68c;
      s.r[4] = m.ReadU32(owner + 120);
      call(0x82b07e80);
      ready = (Address(s.r[3]) & 255) != 0;
    }
    for (unsigned off : {124u, 136u}) {
      if (!ready)
        break;
      unsigned i = 0;
      while (std::int32_t(i) < std::int32_t(m.ReadU32(owner + off + 4))) {
        auto value = m.ReadU32(m.ReadU32(owner + off) + 8 * i + 4);
        if (std::int32_t(value) < 0) {
          s.r[3] = owner + off;
          s.r[4] = i;
          s.r[5] = 1;
          s.r[6] = 8;
          s.r[7] = 8;
          call(0x82298af8);
          continue;
        }
        s.r[4] = value;
        if (off == 124) {
          s.r[3] = owner;
          d.guest.CallIndirect(m.ReadU32(m.ReadU32(owner) + 28), m, s);
        } else {
          s.r[3] = 0x832cc0fc;
          call(0x82b1a100);
        }
        if (!(Address(s.r[3]) & 255)) {
          ready = false;
          break;
        }
        ++i;
      }
    }
    s.r[3] = ready;
  }
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_completion61
