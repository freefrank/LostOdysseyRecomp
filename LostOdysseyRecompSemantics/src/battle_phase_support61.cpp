#include "lo_semantics/battle_phase_support61.h"
#include "lo_semantics/battle_manager_access61.h"
#include "lo_semantics/battle_bootstrap61.h"
#include "lo_semantics/battle_script61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::battle_phase_support61 {
namespace {
using recovery_abi::Address;
void Call(unsigned entry, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_manager_access61::Apply(entry, m, d, s) &&
      !battle_bootstrap61::Apply(entry, m, d, s) &&
      !battle_script61::Apply(entry, m, d, s))
    d.guest.CallDirect(entry, m, s);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  auto owner = Address(s.r[3]);
  if (e == 0x82400a18) {
    recovery_abi::WriteU64(m, owner + 8,
                           recovery_abi::ReadU64(m, owner + 8) &
                               ~std::uint64_t(0x4000));
    return true;
  }
  if (e == 0x82b08a60) {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto value = m.ReadU32(0x82000e50);
    s.fpr_bits[0] =
        std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(value)));
    for (unsigned offset : {8u, 12u, 16u})
      m.WriteU32(owner + offset, value);
    return true;
  }
  if (e == 0x82b2c410) {
    s.r[3] = std::uint64_t(-1);
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(m.ReadU32(owner + 32));
         ++i) {
      auto row = m.ReadU32(owner + 28) + 20 * i, kind = m.ReadU32(row);
      if (kind == 0 || kind == 11) {
        s.r[3] = m.ReadU32(row + 4);
        break;
      }
    }
    return true;
  }
  unsigned first, frame;
  switch (e) {
  case 0x82a9f160:
    first = 31;
    frame = 96;
    break;
  case 0x82389b10:
    first = 32;
    frame = 96;
    break;
  case 0x82ace978:
    first = 29;
    frame = 112;
    break;
  case 0x82af6b48:
    first = 27;
    frame = 0;
    break;
  default:
    return false;
  }
  auto old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  if (frame) {
    s.r[1] -= frame;
    m.WriteU32(Address(s.r[1]), old);
  }
  if (e == 0x82389b10) {
    s.r[3] = m.ReadU32(0x83315fb4);
    s.ctr = m.ReadU32(m.ReadU32(Address(s.r[3])) + 352);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    Call(0x8229dfd8, m, d, s);
  } else if (e == 0x82a9f160) {
    s.r[3] = m.ReadU32(0x83315fb4);
    Call(0x822a7c58, m, d, s);
    if (Address(s.r[3]) && m.ReadU32(owner + 28) &&
        m.ReadU32(m.ReadU32(owner + 28) + 68) != 0xffffffffu) {
      s.r[3] = owner;
      Call(0x82a9e3b8, m, d, s);
    }
  } else if (e == 0x82ace978) {
    auto side = Address(s.r[4]);
    auto list = [&]() {
      Call(0x82380a18, m, d, s);
      Call(0x8238e2f8, m, d, s);
      return Address(s.r[3]);
    };
    auto rows = list();
    for (unsigned i = 0;; ++i) {
      auto current = list();
      if (!(std::int32_t(i) < std::int32_t(m.ReadU32(current + 4))))
        break;
      auto resource = m.ReadU32(m.ReadU32(rows) + 4 * i);
      auto id = std::int32_t(m.ReadU32(resource + 64));
      if (((side & 255) == 1 ? id < 20 : id >= 20) &&
          !(m.ReadU16(resource + 124) & 1) &&
          std::int32_t(m.ReadU32(resource + 88)) > 0)
        m.WriteU32(resource + 88, m.ReadU32(resource + 88) - 1);
    }
  } else {
    // Two actor banks, each containing two 1024-word record arrays.
    for (unsigned bank : {200u, 8460u}) {
      for (unsigned i = 0; i < 256; ++i) {
        for (unsigned word = 0; word < 4; ++word) {
          for (unsigned second = 0; second < 2; ++second) {
            auto offset = bank + 16 * i + 4 * word + second * 4100;
            auto value = m.ReadU32(m.ReadU32(owner + 28) + 180904 + offset);
            m.WriteU32(m.ReadU32(owner + 44) + offset, value);
          }
        }
      }
    }
  }
  if (frame)
    s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_phase_support61
