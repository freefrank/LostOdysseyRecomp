#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/battle_script_runtime61.h"
#include "lo_semantics/battle_action_adjustments61.h"
#include "lo_semantics/battle_property_mutation61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_property_mutation61 {
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  using recovery_abi::Address;
  if (e == 0x82ac85a0) {
    auto flags = m.ReadU32(Address(s.r[3]) + 272 * Address(s.r[4]) + 232);
    unsigned result = 0;
    if (flags & Address(s.r[5]))
      for (unsigned i = 0; i < 31; ++i)
        result += (flags >> i) & 1;
    s.r[3] = result;
    return true;
  }
  if (e == 0x82ac90e8) {
    s.r[6] = 0;
    return battle_property_mutation61::Apply(0x82ac90f8, m, d, s);
  }
  if (e == 0x82ac8af8) {
    s.r[7] = s.r[6];
    s.r[6] = 0;
    return battle_property_mutation61::Apply(0x82ac89f0, m, d, s);
  }
  if (e == 0x82ac90f8 || e == 0x82ac89f0) {
    auto old = Address(s.r[1]), resource = Address(s.r[3]),
         bank = Address(s.r[4]), mask = Address(s.r[5]);
    bool adding = e == 0x82ac89f0;
    unsigned first = adding ? 26 : 27, frame = adding ? 144 : 0;
    auto mode = Address(s.r[6]) & 255, payload = Address(s.r[7]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    if (frame) {
      s.r[1] -= frame;
      m.WriteU32(Address(s.r[1]), old);
    }
    auto flagsAddress = resource + 272 * bank + 232,
         flags = m.ReadU32(flagsAddress);
    unsigned result = 0;
    for (unsigned bit = 0; bit < 31; ++bit) {
      auto flag = 1u << bit;
      if (!(mask & flag))
        continue;
      if (adding) {
        bool special = m.ReadU32(0x83213538 + 4 * (32 * bank + bit)) == 1;
        if (special && (flags & flag))
          continue;
        result = 1;
        if (mode == 1) {
          m.WriteU32(flagsAddress, m.ReadU32(flagsAddress) | flag);
          if (payload) {
            unsigned index = 0;
            for (; index < 31; ++index)
              if (mask & (1u << index))
                break;
            m.WriteU32(resource + 4 * (68 * bank + index + 59), payload);
          }
        }
      } else if (flags & flag) {
        result = 1;
        if (mode == 1) {
          m.WriteU32(flagsAddress, m.ReadU32(flagsAddress) & ~flag);
          m.WriteU32(resource + 4 * (68 * bank + bit + 59), 0);
          m.WriteU32(resource + 4 * (68 * bank + bit + 91), 0);
        }
      }
    }
    s.r[3] = result;
    s.r[1] += frame;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82ac8978) {
    s.r[8] = 0;
    s.r[7] = 0;
    return battle_property_mutation61::Apply(0x82ac87d8, m, d, s);
  }
  if (e == 0x82ac87d8) {
    auto old = Address(s.r[1]), resource = Address(s.r[3]),
         bank = Address(s.r[4]), mask = Address(s.r[5]),
         value = Address(s.r[6]);
    auto mode = Address(s.r[7]) & 255, add = Address(s.r[8]) & 255;
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 24; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 160;
    m.WriteU32(Address(s.r[1]), old);
    auto flagAddress = resource + 272 * bank + 232,
         flags = m.ReadU32(flagAddress);
    unsigned result = 0, index = 0;
    for (; index < 31; ++index)
      if (mask & (1u << index))
        break;
    auto payload = resource + 4 * (68 * bank + index + 59),
         other = resource + 4 * (68 * bank + index + 91);
    for (unsigned bit = 0; bit < 31; ++bit) {
      auto flag = 1u << bit;
      if (!(mask & flag))
        continue;
      bool typed = m.ReadU32(0x83213538 + 4 * (32 * bank + bit)) == 1;
      if (typed && (flags & flag)) {
        result = 1;
        auto current = m.ReadU32(payload);
        if (mode == 1) {
          if (add == 1)
            m.WriteU32(payload, current + value);
          else if (std::int32_t(current) < std::int32_t(value))
            m.WriteU32(payload, value);
        } else if (std::int32_t(current) > std::int32_t(value))
          result = 0;
      } else {
        result = 1;
        if (mode == 1) {
          m.WriteU32(flagAddress, m.ReadU32(flagAddress) | flag);
          m.WriteU32(payload, value);
          m.WriteU32(other, 0);
        }
      }
    }
    s.r[3] = result;
    s.r[1] += 160;
    for (unsigned i = 24; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82aca700) {
    s.r[7] = s.r[6];
    s.r[6] = 0;
    return battle_property_mutation61::Apply(0x82aca468, m, d, s);
  }
  if (e == 0x82aca468) {
    auto old = Address(s.r[1]), resource = Address(s.r[3]),
         bank = Address(s.r[4]), mask = Address(s.r[5]);
    auto mode = Address(s.r[6]) & 255, bypass = Address(s.r[7]) & 255;
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 15; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 224;
    m.WriteU32(Address(s.r[1]), old);
    auto flagAddress = resource + 272 * bank + 232,
         flags = m.ReadU32(flagAddress);
    auto property198 = [&]() {
      s.r[3] = resource;
      s.r[4] = 198;
      (void)battle_action_readiness61::Apply(0x8238e368, m, d, s);
      return (Address(s.r[3]) & 255) != 0;
    };
    unsigned result = 0;
    for (unsigned bit = 0; bit < 31; ++bit) {
      auto flag = 1u << bit;
      if (!(mask & flag))
        continue;
      if (m.ReadU32(0x83213538 + 4 * (32 * bank + bit)) == 1 && (flags & flag))
        continue;
      if (bank == 0) {
        if (m.ReadU32(resource + 76348) & 0x80000000u)
          continue;
        if (!bypass && (m.ReadU32(resource + 4876) & mask) &&
            ((m.ReadU32(resource + 124) & 0x10000000u) || mode == 1))
          continue;
        if (m.ReadU32(resource + 5088) & mask)
          continue;
        if (mask & 0x10000) {
          if (!(flags & 4) || property198())
            continue;
          if (mode == 1) {
            s.r[3] = 16;
            (void)battle_action_adjustments61::Apply(0x82ac84e8, m, d, s);
            m.WriteU32(resource + 4 * (Address(s.r[3]) + 59), 3);
            s.r[3] = resource;
            s.r[4] = 2;
            s.r[5] = 1;
            (void)battle_property_mutation61::Apply(0x82ac8ee8, m, d, s);
          }
        }
        if (mask & 4) {
          if ((flags & 0x10000) || property198())
            continue;
        }
      } else if (bank == 7 && (mask & 2)) {
        auto restriction = m.ReadU32(0x832134b4);
        if ((m.ReadU32(resource + 4876) | m.ReadU32(resource + 5088)) &
            restriction)
          continue;
      }
      result = 1;
      if (mode != 1)
        continue;
      m.WriteU32(flagAddress, m.ReadU32(flagAddress) | flag);
      if (bank == 0 && bit == 0) {
        s.r[3] = 0x832c9c54;
        s.r[4] = resource;
        (void)battle_script_runtime61::Apply(0x82a9bdb0, m, d, s);
        auto actor = Address(s.r[3]);
        if (actor && (m.ReadU32(actor + 64) & 0x800000))
          m.WriteU32(actor + 64, m.ReadU32(actor + 64) | 0x400000);
        d.guest.CallDirect(0x82380a18, m, s);
        d.guest.CallDirect(0x82389b78, m, s);
        s.r[4] = resource;
        s.r[5] = 1;
        s.r[6] = 1;
        d.guest.CallDirect(0x82ad0ad0, m, s);
      }
    }
    s.r[3] = result;
    s.r[1] += 224;
    for (unsigned i = 15; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82aca6f0) {
    s.r[7] = s.r[6];
    s.r[6] = 1;
    return battle_property_mutation61::Apply(0x82aca468, m, d, s);
  }
  if (e == 0x82ac8ae8) {
    s.r[7] = s.r[6];
    s.r[6] = 1;
    return battle_property_mutation61::Apply(0x82ac89f0, m, d, s);
  }
  if (e == 0x82ac91d8) {
    s.r[6] = 1;
    return battle_property_mutation61::Apply(0x82ac90f8, m, d, s);
  }
  if (e == 0x82ac8ec8 || e == 0x82ac8ed8) {
    s.r[9] = s.r[8];
    s.r[8] = e == 0x82ac8ec8;
    return battle_property_mutation61::Apply(0x82ac8ce0, m, d, s);
  }
  if (e == 0x82ac8ce0) {
    auto old = Address(s.r[1]), resource = Address(s.r[3]),
         bank = Address(s.r[4]), mask = Address(s.r[5]);
    auto value = Address(s.r[6]), aux = Address(s.r[7]),
         mode = Address(s.r[8]) & 255, option = Address(s.r[9]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 21; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 176;
    m.WriteU32(Address(s.r[1]), old);
    auto flagAddress = resource + 272 * bank + 232,
         flags = m.ReadU32(flagAddress);
    unsigned result = 0, index = 0;
    for (; index < 31; ++index)
      if (mask & (1u << index))
        break;
    auto payload = resource + 4 * (68 * bank + index + 59),
         other = resource + 4 * (68 * bank + index + 91);
    for (unsigned bit = 0; bit < 31; ++bit) {
      auto flag = 1u << bit;
      if (!(mask & flag))
        continue;
      auto restriction = m.ReadU32(0x832134b4);
      if ((mask & 2) && (m.ReadU32(resource + 4876) & restriction)) {
        result = 0;
        continue;
      }
      bool typed = m.ReadU32(0x83213538 + 4 * (32 * bank + bit)) == 1;
      if (typed && (flags & flag)) {
        result = 1;
        if (mode == 1) {
          if (option == 1) {
            m.WriteU32(payload, bank == 7 && (mask & 2)
                                    ? m.ReadU32(payload) + value
                                    : value);
            m.WriteU32(other, aux);
          }
        } else if (option == 0)
          result = 0;
      } else {
        if ((m.ReadU32(0x83213444) & mask) &&
            (m.ReadU32(resource + 4876) & restriction) &&
            (m.ReadU32(resource + 5088) & restriction))
          continue;
        result = 1;
        if (mode == 1) {
          m.WriteU32(flagAddress, m.ReadU32(flagAddress) | flag);
          m.WriteU32(payload, value);
          m.WriteU32(other, aux);
        }
      }
    }
    s.r[3] = result;
    s.r[1] += 176;
    for (unsigned i = 21; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82ac9000) {
    s.r[5] = 1;
    return battle_property_mutation61::Apply(0x82ac8ee8, m, d, s);
  }
  if (e != 0x82ac8ee8)
    return false;
  auto old = Address(s.r[1]), resource = Address(s.r[3]), id = Address(s.r[4]),
       mode = Address(s.r[5]) & 255;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = 29; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  unsigned result = 0;
  if (std::int32_t(id) <= 262) {
    auto n = std::int32_t(id), q = n / 32;
    auto p = 0x83213438u + 8 * unsigned(n - q * 32);
    auto bank = m.ReadU32(p) + unsigned(q), mask = m.ReadU32(p + 4),
         flags = resource + 272 * bank + 232;
    if (m.ReadU32(flags) & mask) {
      result = 1;
      if (mode == 1) {
        m.WriteU32(flags, m.ReadU32(flags) & ~mask);
        for (unsigned base : {59u, 91u}) {
          auto currentMask = m.ReadU32(p + 4);
          unsigned index = 0;
          for (; index < 31; ++index)
            if (currentMask & (1u << index))
              break;
          auto currentBank = m.ReadU32(p) + unsigned(q);
          m.WriteU32(resource + 4 * (68 * currentBank + index + base), 0);
        }
      }
    }
  }
  s.r[3] = result;
  for (unsigned i = 29; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_property_mutation61
