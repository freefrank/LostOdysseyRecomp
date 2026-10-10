#include "lo_semantics/battle_phase_support61.h"
#include "lo_semantics/battle_manager_access61.h"
#include "lo_semantics/battle_bootstrap61.h"
#include "lo_semantics/battle_script61.h"
#include "lo_semantics/battle_resource_stats61.h"
#include "lo_semantics/battle_roster_persistence61.h"
#include "lo_semantics/battle_random_range61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::battle_phase_support61 {
namespace {
using recovery_abi::Address;
void Call(unsigned entry, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_phase_support61::Apply(entry, m, d, s) &&
      !battle_resource_stats61::Apply(entry, m, d, s) &&
      !battle_roster_persistence61::Apply(entry, m, d, s) &&
      !battle_random_range61::Apply(entry, m, d, s) &&
      !battle_manager_access61::Apply(entry, m, d, s) &&
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
  case 0x82ad40d0:
    first = 23;
    frame = 160;
    break;
  case 0x82aaa7c8:
    first = 24;
    frame = 160;
    break;
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
  if (e == 0x82ad40d0) {
    // The composed runtime routes these services to their recovered units.
    auto call = [&](unsigned entry, unsigned receiver) {
      s.r[3] = receiver;
      d.guest.CallDirect(entry, m, s);
    };
    auto header = [&]() { return m.ReadU32(owner + 20); };
    for (unsigned i = 0;
         std::int32_t(i) < std::int32_t(m.ReadU32(header() + 4));) {
      auto resource = m.ReadU32(m.ReadU32(header()) + 4 * i);
      if (m.ReadU32(resource + 124) & 0x08000000u) {
        auto kind = m.ReadU32(resource + 72);
        if ((kind == 0 || kind == 11) && m.ReadU32(0x832cc0cc)) {
          call(0x82b2c410, m.ReadU32(0x832cc0cc));
          auto replacement = Address(s.r[3]);
          if (std::int32_t(replacement) >= 0 &&
              m.ReadU32(resource + 72) != replacement)
            m.WriteU32(resource + 72, replacement);
        }
        call(0x82ab31e0, resource);
        m.WriteU32(resource + 60, 0);
        ++i;
      } else {
        call(0x82400a18, resource);
        m.WriteU32(m.ReadU32(header()) + 4 * i, 0);
        s.r[4] = i;
        s.r[5] = 1;
        s.r[6] = 4;
        s.r[7] = 8;
        call(0x82298af8, header());
      }
    }
    d.guest.CallDirect(0x82380a18, m, s);
    d.guest.CallDirect(0x82389aa0, m, s);
    s.r[4] = 1;
    s.r[5] = m.ReadU8(Address(s.r[3]) + 56);
    call(0x82af5ba8, owner);
    m.WriteU32(owner + 4, 20);
    s.r[4] = 0;
    call(0x82af52f0, owner);
    call(0x82af6448, owner);
    call(0x82a9f160, 0x832c9c54);
    for (unsigned side : {1u, 0u}) {
      auto gauge = m.ReadU32(0x832aeb00);
      m.WriteU8(gauge + (side ? 48 : 24), 0);
      s.r[4] = side;
      s.r[5] = 1;
      call(0x82ac7b08, gauge);
      for (auto entry : {0x82ac7fc8u, 0x82ac6e60u, 0x82ac6f08u}) {
        s.r[4] = side;
        call(entry, gauge);
      }
    }
    m.WriteU16(owner + 148, 0);
    m.WriteU32(owner + 156, 0);
    m.WriteU32(owner + 148, m.ReadU32(owner + 148) & 0xffff0007u);
    call(0x82ac1b90, m.ReadU32(0x83291dc0));
    call(0x82ac3118, m.ReadU32(0x83291dc0));
    auto rows = header();
    for (unsigned i = 0;
         std::int32_t(i) < std::int32_t(m.ReadU32(header() + 4)); ++i) {
      auto resource = m.ReadU32(m.ReadU32(rows) + 4 * i);
      call(0x82ab31e0, resource);
      if (m.ReadU32(resource + 124) & 0x10000000u) {
        s.r[4] = resource;
        call(0x82ac3058, m.ReadU32(0x83291dc0));
      }
      s.r[4] = resource;
      call(0x82acd3c0, m.ReadU32(0x8324570c));
    }
    call(0x82a9f0a0, 0x832c9c54);
    call(0x82a9f028, 0x832c9c54);
    for (unsigned i = 0; i < 14; ++i)
      m.WriteU8(owner + 60 + i, 0);
    m.WriteU8(owner + 61, 1);
    m.WriteU8(owner + 65, 1);
    m.WriteU32(owner + 52, 0);
    s.r[4] = 0;
    s.r[5] = 1;
    call(0x82aaa7c8, owner);
  } else if (e == 0x82aaa7c8) {
    auto request = Address(s.r[4]), forced = Address(s.r[5]) & 255;
    auto phase = m.ReadU32(owner + 56);
    auto setPhase = [&](unsigned value) {
      m.WriteU32(owner + 144, value);
      m.WriteU32(owner + 56, value);
    };
    auto timer = [&]() {
      s.r[3] = owner + 104;
      Call(0x82b08a60, m, d, s);
    };
    auto transition = [&](unsigned value) {
      setPhase(value);
      timer();
    };
    auto manager = [&]() {
      Call(0x82380a18, m, d, s);
      Call(0x82389b78, m, d, s);
    };
    auto roster = [&]() {
      manager();
      Call(0x82af5810, m, d, s);
    };
    auto visit = [&](bool activeOnly) {
      for (unsigned i = 0;
           std::int32_t(i) < std::int32_t(m.ReadU32(m.ReadU32(owner + 20) + 4));
           ++i) {
        auto resource = m.ReadU32(m.ReadU32(m.ReadU32(owner + 20)) + 4 * i);
        s.r[3] = resource;
        if (activeOnly) {
          if ((m.ReadU32(resource + 60) & 255) == 1)
            Call(0x82ab0b10, m, d, s);
        } else {
          s.ctr = m.ReadU32(m.ReadU32(resource) + 440);
          d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        }
      }
    };
    auto virtualPhase = [&]() {
      s.r[3] = owner;
      s.ctr = m.ReadU32(m.ReadU32(owner) + 12);
      d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    };
    auto outcome = [&]() {
      s.r[3] = owner;
      Call(0x82acf108, m, d, s);
      if (Address(s.r[3]) & 255) {
        setPhase(12);
        return true;
      }
      s.r[3] = owner;
      Call(0x82acf280, m, d, s);
      if (Address(s.r[3]) & 255) {
        setPhase(11);
        s.r[3] = m.ReadU32(0x83291dc0);
        Call(0x82ac6d88, m, d, s);
        return true;
      }
      return false;
    };
    auto effects = [&](unsigned offset, unsigned mode) {
      auto p = Address(s.r[1]) + offset;
      m.WriteU32(p, 0x8204a1d8);
      s.r[3] = p;
      s.r[4] = mode;
      Call(0x82acb120, m, d, s);
      m.WriteU32(p, 0x8204a1d8);
    };
    if (forced)
      transition(request);
    else
      switch (phase) {
      case 0:
        if (request == 1)
          transition(1);
        break;
      case 1:
        if (request != 2)
          break;
        if (m.ReadU32(0x832cb778) == 255 || m.ReadU32(0x832cb778) == 256) {
          manager();
          s.r[4] = 20;
          Call(0x8238e308, m, d, s);
          auto resource = Address(s.r[3]);
          for (unsigned i = 0; i < 2; ++i) {
            s.r[3] = m.ReadU32(0x83264558);
            s.r[4] = 0;
            s.r[5] = i ? 7 : 3;
            s.r[6] = 1;
            s.r[7] = 32;
            Call(0x82aa0740, m, d, s);
            auto choice = Address(s.r[3]);
            if (choice <= (i ? 7u : 3u))
              m.WriteU32(resource + (i ? 4888 : 4880), 1u << choice);
          }
        }
        Call(0x82380a18, m, d, s);
        Call(0x82389b10, m, d, s);
        transition(2);
        virtualPhase();
        s.r[3] = m.ReadU32(0x8324570c);
        Call(0x82acd398, m, d, s);
        for (unsigned side : {1u, 0u}) {
          s.r[3] = m.ReadU32(0x8324570c);
          s.r[4] = side;
          Call(0x82ace978, m, d, s);
        }
        roster();
        s.r[3] = 0x832c9c54;
        Call(0x82af6b48, m, d, s);
        visit(false);
        break;
      case 2:
        if (request == 3) {
          transition(3);
          m.WriteU32(owner + 52, m.ReadU32(owner + 52) + 1);
        }
        break;
      case 3:
        if (request == 4) {
          transition(4);
          visit(true);
        } else if (request == 2) {
          timer();
          setPhase(2);
          virtualPhase();
        }
        break;
      case 4:
        if (request == 5)
          transition(5);
        break;
      case 5:
        if (request == 6) {
          transition(6);
          effects(80, 0);
        }
        break;
      case 6:
        if (request == 7) {
          timer();
          if (!outcome())
            setPhase(7);
        }
        break;
      case 7:
        if (request == 8) {
          transition(8);
          roster();
          visit(false);
        }
        break;
      case 8:
        if (request == 9)
          transition(9);
        break;
      case 9:
        if (request == 10) {
          roster();
          timer();
          if (!outcome()) {
            visit(false);
            setPhase(10);
            effects(84, 1);
            roster();
          }
        }
        break;
      case 10:
        if (request == 1) {
          timer();
          if (!outcome())
            setPhase(1);
        }
        break;
      case 11:
      case 12:
        if (request == 13)
          transition(13);
        break;
      case 13:
        Call(0x82380a18, m, d, s);
        Call(0x82389b10, m, d, s);
        break;
      default:
        break;
      }
  } else if (e == 0x82389b10) {
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
