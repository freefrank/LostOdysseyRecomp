#include "lo_semantics/battle_manager_access61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/string_storage_context61.h"
namespace lo::semantic::gpu::battle_manager_access61 {
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  using recovery_abi::Address;
  if (e == 0x82ab01d0) {
    auto old = Address(s.r[1]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 29; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 144;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    auto call = [&](unsigned a) {
      if (!string_storage_context61::Apply(a, m, d, s))
        d.guest.CallDirect(a, m, s);
    };
    s.r[3] = sp + 88;
    s.r[4] = 0x820bdc70;
    call(0x822d02f8);
    auto name = m.ReadU32(sp + 92) ? m.ReadU32(sp + 88) : 0x821a83d0;
    s.r[3] = m.ReadU32(0x83315f9c);
    if (!s.r[3]) {
      s.r[3] = 0x8218c210;
      call(0x82410b90);
      m.WriteU32(0x83315f9c, Address(s.r[3]));
      call(0x82410c48);
      s.r[3] = m.ReadU32(0x83315f9c);
    }
    s.r[4] = std::uint64_t(-1);
    s.r[5] = name;
    s.r[6] = 0;
    call(0x8229c7b8);
    unsigned result = 0;
    if (Address(s.r[3])) {
      auto manager = m.ReadU32(0x832c9c44);
      m.WriteU32(sp + 80, 0);
      m.WriteU32(sp + 84, 0);
      if (!manager) {
        s.r[3] = 0x8202050c;
        call(0x82a9b5a8);
        m.WriteU32(0x832c9c44, Address(s.r[3]));
        call(0x82a9b2c0);
        manager = m.ReadU32(0x832c9c44);
      }
      call(0x82300100);
      s.r[4] = s.r[3];
      s.r[3] = manager;
      s.r[5] = recovery_abi::ReadU64(m, sp + 80);
      s.r[6] = 0;
      s.r[7] = 0;
      s.r[8] = m.ReadU32(0x8330b5f4);
      s.r[9] = 0;
      s.r[10] = 0;
      call(0x82401a10);
      m.WriteU32(0x832cb788, Address(s.r[3]));
      if (Address(s.r[3])) {
        call(0x82400a08);
        result = m.ReadU32(0x832cb788);
      }
    }
    s.r[3] = sp + 88;
    call(0x82298938);
    s.r[3] = result;
    s.r[1] += 144;
    for (unsigned i = 29; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x8229dfd8 || e == 0x82389aa0) {
    auto old = Address(s.r[1]), object = Address(s.r[3]);
    unsigned frame = e == 0x8229dfd8 ? 112 : 96;
    m.WriteU32(old - 8, Address(s.lr));
    if (e == 0x8229dfd8) {
      recovery_abi::WriteU64(m, old - 24, s.r[30]);
      recovery_abi::WriteU64(m, old - 16, s.r[31]);
    }
    s.r[1] -= frame;
    m.WriteU32(Address(s.r[1]), old);
    if (e == 0x8229dfd8) {
      s.r[3] = 0;
      if (object) {
        auto type = m.ReadU32(0x832c1764);
        if (!type) {
          s.r[3] = 0x821ab904;
          d.guest.CallDirect(0x829086b0, m, s);
          m.WriteU32(0x832c1764, Address(s.r[3]));
          d.guest.CallDirect(0x82907948, m, s);
          type = m.ReadU32(0x832c1764);
        }
        auto current = m.ReadU32(object + 52);
        while (current && current != type)
          current = m.ReadU32(current + 60);
        s.r[3] = (current == type) ? object : 0;
      }
    } else {
      s.r[3] = m.ReadU32(0x83315fb4);
      auto method = m.ReadU32(m.ReadU32(Address(s.r[3])) + 352);
      s.ctr = method;
      d.guest.CallIndirect(method & ~3u, m, s);
      (void)battle_manager_access61::Apply(0x8229dfd8, m, d, s);
      if (s.r[3])
        s.r[3] =
            m.ReadU32(0x83264978 + 128) + 160 * m.ReadU32(Address(s.r[3]) + 68);
    }
    s.r[1] += frame;
    if (e == 0x8229dfd8) {
      s.r[30] = recovery_abi::ReadU64(m, old - 24);
      s.r[31] = recovery_abi::ReadU64(m, old - 16);
    }
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82380a18) {
    s.r[3] = m.ReadU32(0x832cb788);
    if (!s.r[3])
      (void)battle_manager_access61::Apply(0x82ab01d0, m, d, s);
  } else if (e == 0x82389b78) {
    s.r[3] = 0x832ca0e8;
  } else if (e == 0x83081540 || e == 0x82df7558) {
    s.r[3] = m.ReadU32(Address(s.r[3]) + (e == 0x83081540 ? 20 : 48));
  } else if (e == 0x8238e2f8 || e == 0x82ab0110) {
    s.r[3] = 0x832ca0e8;
    return battle_manager_access61::Apply(
        e == 0x8238e2f8 ? 0x83081540 : 0x82df7558, m, d, s);
  } else if (e == 0x8238e308) {
    auto list = m.ReadU32(Address(s.r[3]) + 20), id = Address(s.r[4]);
    s.r[3] = 0;
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(m.ReadU32(list + 4));
         ++i) {
      auto resource = m.ReadU32(m.ReadU32(list) + 4 * i);
      if (m.ReadU32(resource + 64) == id) {
        s.r[3] = resource;
        break;
      }
    }
  } else if (e == 0x82380d30) {
    auto list = 0x832cb550u, id = Address(s.r[4]), count = m.ReadU32(list + 8),
         data = m.ReadU32(list + 4);
    s.r[3] = 0;
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(count); ++i) {
      auto item = m.ReadU32(data + 4 * i);
      if (m.ReadU32(item + 552) == id) {
        s.r[3] = item;
        break;
      }
    }
  } else
    return false;
  return true;
}
} // namespace lo::semantic::gpu::battle_manager_access61
