#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/battle_script_runtime61.h"
#include "lo_semantics/battle_script_dispatch61.h"
#include "lo_semantics/battle_action_adjustments61.h"
#include "lo_semantics/battle_property_mutation61.h"
#include "lo_semantics/battle_progression61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/battle_manager_access61.h"
#include <bit>
#include <initializer_list>
namespace lo::semantic::gpu::battle_property_mutation61 {
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  using recovery_abi::Address;
  if (e == 0x82ac92b0) {
    auto resource = Address(s.r[3]), preserved = m.ReadU32(0x832134cc);
    unsigned preservedBit = 0;
    while (preservedBit < 31 && !(preserved & (1u << preservedBit)))
      ++preservedBit;
    for (unsigned bank = 0; bank < 8; ++bank) {
      auto base = resource + 272 * bank;
      m.WriteU32(base + 232, bank == 3 ? m.ReadU32(base + 232) & preserved : 0);
      for (unsigned bit = 0; bit < 32; ++bit)
        if (bank != 3 || bit != preservedBit) {
          m.WriteU32(base + 236 + 4 * bit, 0);
          m.WriteU32(base + 364 + 4 * bit, 0);
        }
    }
    return true;
  }
  if (e == 0x82ac8608) {
    auto old = Address(s.r[1]), resource = Address(s.r[3]),
         value = Address(s.r[5]), mode = Address(s.r[6]) & 255;
    auto id = std::int32_t(Address(s.r[4]));
    recovery_abi::WriteU64(m, old - 16, s.r[30]);
    recovery_abi::WriteU64(m, old - 8, s.r[31]);
    if (id <= 262) {
      auto quotient = id / 32, remainder = id - quotient * 32;
      auto table = 0x83213438u + 8 * unsigned(remainder),
           mask = m.ReadU32(table + 4);
      auto bank = m.ReadU32(table) + unsigned(quotient),
           flagsAddress = resource + 272 * bank + 232;
      unsigned bit = 0;
      while (bit < 31 && !(mask & (1u << bit)))
        ++bit;
      auto payload = resource + 4 * (68 * bank + bit + 59);
      if (m.ReadU32(flagsAddress) & mask) {
        auto previous = m.ReadU32(payload);
        if (mode == 1)
          m.WriteU32(payload, previous + value);
        else if (std::int32_t(previous) < std::int32_t(value))
          m.WriteU32(payload, value);
      } else {
        m.WriteU32(flagsAddress, m.ReadU32(flagsAddress) | mask);
        m.WriteU32(payload, value);
        m.WriteU32(resource + 4 * (68 * bank + bit + 91), 0);
      }
    }
    s.r[30] = recovery_abi::ReadU64(m, old - 16);
    s.r[31] = recovery_abi::ReadU64(m, old - 8);
    return true;
  }
  if (e == 0x82ac9548) {
    auto resource = Address(s.r[3]), bank = Address(s.r[4]),
         mask = Address(s.r[5]);
    auto slot = resource + 272 * bank + 232, initial = m.ReadU32(slot);
    for (unsigned bit = 0; bit < 31; ++bit) {
      auto flag = 1u << bit;
      if (!(mask & flag))
        continue;
      if (initial & flag)
        m.WriteU32(slot, m.ReadU32(slot) & ~flag);
      else if (!((m.ReadU32(resource + 4876) | m.ReadU32(resource + 5088)) &
                 flag))
        m.WriteU32(slot, m.ReadU32(slot) | flag);
    }
    return true;
  }
  if (e == 0x82ac8988) {
    auto resource = Address(s.r[3]);
    auto id = std::int32_t(Address(s.r[4]));
    if (id <= 262) {
      auto quotient = id / 32, remainder = id - quotient * 32;
      auto table = 0x83213438u + 8 * unsigned(remainder);
      auto slot =
          resource + 272 * (m.ReadU32(table) + unsigned(quotient)) + 232;
      m.WriteU32(slot, m.ReadU32(slot) | m.ReadU32(table + 4));
    }
    return true;
  }
  if (e == 0x82aca830) {
    s.r[7] = 1;
    return battle_property_mutation61::Apply(0x82aca710, m, d, s);
  }
  if (e == 0x82aca710 || e == 0x82aca838) {
    bool clear = e == 0x82aca838;
    auto old = Address(s.r[1]), resource = Address(s.r[3]),
         bankOrId = Address(s.r[4]), mask = Address(s.r[5]),
         payload = Address(s.r[6]), mode = Address(s.r[7]) & 255;
    unsigned frame = clear ? 112 : 144, first = clear ? 30 : 25;
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= frame;
    m.WriteU32(Address(s.r[1]), old);
    unsigned result = 0;
    if (!clear || std::int32_t(bankOrId) <= 262) {
      (void)battle_manager_access61::Apply(0x82380a18, m, d, s);
      (void)battle_manager_access61::Apply(0x82ab0110, m, d, s);
      auto list = m.ReadU32(Address(s.r[3]));
      auto group =
          m.ReadU32(list + ((m.ReadU32(resource + 124) >> 26) & 4)) + 72;
      auto flagsAddress = group + 12288, flags = m.ReadU32(flagsAddress);
      auto index = [](unsigned bits) {
        unsigned bit = 0;
        while (bit < 31 && !(bits & (1u << bit)))
          ++bit;
        return bit;
      };
      if (clear) {
        auto id = std::int32_t(bankOrId), remainder = id - (id / 32) * 32;
        auto bitMask = m.ReadU32(0x8321343cu + 8 * unsigned(remainder));
        // The original returns early for an already-set flag.
        if (!(flags & bitMask)) {
          m.WriteU32(flagsAddress, flags & ~bitMask);
          auto bit = index(bitMask);
          m.WriteU32(group + 4 * (bit + 3073), 0);
          m.WriteU32(group + 4 * (bit + 3105), 0);
        }
      } else if (std::int32_t(bankOrId) >= 8) {
        for (unsigned bit = 0; bit < 31; ++bit) {
          auto bitMask = 1u << bit;
          if (!(mask & bitMask))
            continue;
          bool unique = m.ReadU32(0x83213538 + 4 * (32 * bankOrId + bit)) == 1;
          if (unique && (flags & bitMask))
            continue;
          result = 1;
          if (mode == 1) {
            m.WriteU32(flagsAddress, m.ReadU32(flagsAddress) | bitMask);
            // Source stores every payload at the first bit of the complete
            // mask.
            auto slot = index(mask);
            m.WriteU32(group + 4 * (slot + 3073), payload);
            m.WriteU32(group + 4 * (slot + 3105), 0);
          }
        }
      }
    }
    if (!clear)
      s.r[3] = result;
    s.r[1] += frame;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82ac91e0) {
    auto old = Address(s.r[1]), resource = Address(s.r[3]),
         bank = Address(s.r[4]), ordinal = Address(s.r[5]);
    recovery_abi::WriteU64(m, old - 8, s.r[31]);
    auto address = resource + 272 * bank + 232, flags = m.ReadU32(address);
    unsigned seen = 0;
    s.r[6] = 0;
    for (unsigned bit = 0; bit < 31; ++bit) {
      auto mask = 1u << bit;
      if (!(flags & mask))
        continue;
      if (seen++ != ordinal)
        continue;
      m.WriteU32(address, flags & ~mask);
      m.WriteU32(resource + 4 * (68 * bank + bit + 59), 0);
      m.WriteU32(resource + 4 * (68 * bank + bit + 91), 0);
      break;
    }
    s.r[31] = recovery_abi::ReadU64(m, old - 8);
    return true;
  }
  if (e == 0x82ac85e8) {
    s.r[3] = (m.ReadU32(Address(s.r[4]) + 504) & 127) != 0;
    return true;
  }
  if (e == 0x82ad0ad0) {
    auto old = Address(s.r[1]), manager = Address(s.r[3]),
         resource = Address(s.r[4]), notify = Address(s.r[5]) & 255,
         direct = Address(s.r[6]) & 255;
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 25; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 160;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    m.WriteU32(sp + 80, 0x8204a1d8);
    s.r[3] = 0x832c9c54;
    s.r[4] = resource;
    (void)battle_script_runtime61::Apply(0x82a9bdb0, m, d, s);
    auto actor = Address(s.r[3]);
    if (notify) {
      auto flags = m.ReadU32(actor + 64);
      if (!(flags & 4)) {
        if (direct) {
          m.WriteU32(actor + 64, flags | 8);
          m.WriteU32(actor + 64, flags | 12);
          m.WriteU32(actor + 468, m.ReadU32(manager + 208));
        } else {
          s.r[3] = 0x832c9c54;
          s.r[4] = actor;
          s.r[5] = actor;
          s.r[6] = 2;
          s.r[7] = 16;
          (void)battle_script_dispatch61::Apply(0x82a9bdf8, m, d, s);
          m.WriteU32(actor + 64, m.ReadU32(actor + 64) | 4);
        }
      }
    }
    s.r[3] = m.ReadU32(0x83291dc0);
    s.r[4] = resource;
    (void)battle_progression61::Apply(0x82ac6348, m, d, s);
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto zero = std::bit_cast<float>(m.ReadU32(0x82000e50));
    s.fpr_bits[0] = std::bit_cast<std::uint64_t>(double(zero));
    m.WriteU32(resource + 2588, std::bit_cast<unsigned>(zero));
    m.WriteU32(resource + 156, 0xffffffff);
    m.WriteU32(resource + 124, m.ReadU32(resource + 124) & ~0x800000u);
    m.WriteU32(resource + 188, 0);
    for (auto id : {164u, 242u}) {
      s.r[3] = resource;
      s.r[4] = id;
      (void)battle_property_mutation61::Apply(0x82ac9000, m, d, s);
    }
    m.WriteU32(sp + 80, 0x8204a1d8);
    s.r[1] += 160;
    for (unsigned i = 25; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82ac9be0 || e == 0x82ac9ee8 || e == 0x82aca1b8) {
    auto old = Address(s.r[1]), resource = Address(s.r[3]),
         id = Address(s.r[4]);
    auto argument5 = s.r[5], argument6 = s.r[6];
    bool permissive = e == 0x82ac9ee8, scriptInsertion = e == 0x82aca1b8;
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 24; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 160;
    m.WriteU32(Address(s.r[1]), old);
    auto manager = [&]() {
      d.guest.CallDirect(0x82380a18, m, s);
      d.guest.CallDirect(0x82389b78, m, s);
      return Address(s.r[3]);
    };
    auto table = [&](unsigned value) {
      auto n = std::int32_t(value), q = n / 32;
      return 0x83213438u + 8 * unsigned(n - q * 32);
    };
    auto flags = [&](unsigned value) {
      return resource +
             272 * (m.ReadU32(table(value)) +
                    unsigned(std::int32_t(value) / 32)) +
             232;
    };
    auto present = [&](unsigned value) {
      return (m.ReadU32(flags(value)) & m.ReadU32(table(value) + 4)) != 0;
    };
    auto execute = [&]() {
      if (std::int32_t(id) > 262)
        return;
      if (id == 1 && (m.ReadU16(manager() + 148) & 1))
        return;
      auto slot = flags(id), mask = m.ReadU32(table(id) + 4),
           bank = m.ReadU32(table(id)) + unsigned(std::int32_t(id) / 32);
      if (m.ReadU32(slot) & mask)
        return;
      bool notify = false;
      if (id == 0) {
        if (!scriptInsertion && (m.ReadU32(resource + 76348) & 0x80000000u))
          return;
        auto immune = m.ReadU32(resource + 4876) & m.ReadU32(0x8321343c);
        if (!scriptInsertion &&
            (permissive ? bool(immune & 0xfffffffeu) : bool(immune)))
          return;
        if (!permissive && (m.ReadU32(resource + 5088) & m.ReadU32(0x8321343c)))
          return;
        notify = true;
      } else {
        if (bank == 0) {
          auto immune = m.ReadU32(resource + 4876);
          if (!scriptInsertion && (immune & mask))
            return;
          if (!permissive) {
            if (!scriptInsertion && id == 225 &&
                (immune & m.ReadU32(0x832134b4)))
              return;
            if (m.ReadU32(resource + 5088) & mask)
              return;
          }
        }
        if (id == 16) {
          if (!present(2) || present(198))
            return;
          s.r[3] = resource;
          s.r[4] = 2;
          s.r[5] = 1;
          (void)battle_property_mutation61::Apply(0x82ac8ee8, m, d, s);
          s.r[3] = 16;
          (void)battle_action_adjustments61::Apply(0x82ac84e8, m, d, s);
          m.WriteU32(resource + 4 * (Address(s.r[3]) + 59), 3);
        } else if (id == 2 && (present(16) || present(198)))
          return;
        slot = flags(id);
        mask = m.ReadU32(table(id) + 4);
        m.WriteU32(slot, m.ReadU32(slot) | mask);
        notify = id == 15 && !(m.ReadU32(resource + 124) & 0x10000000u);
      }
      if (notify) {
        s.r[3] = 0x832c9c54;
        s.r[4] = resource;
        (void)battle_script_runtime61::Apply(0x82a9bdb0, m, d, s);
        auto actor = Address(s.r[3]);
        if (actor) {
          auto state = m.ReadU32(actor + 64);
          if (state & 0x800000)
            m.WriteU32(actor + 64, state | 0x400000);
          else {
            auto p = resource +
                     272 * m.ReadU32(id == 0 ? 0x83213438 : 0x832134b0) + 232;
            m.WriteU32(p, m.ReadU32(p) | m.ReadU32(0x8321343c));
          }
        }
        manager();
        s.r[4] = resource;
        s.r[5] = argument5;
        s.r[6] = argument6;
        (void)battle_property_mutation61::Apply(0x82ad0ad0, m, d, s);
      }
    };
    execute();
    s.r[1] += 160;
    for (unsigned i = 24; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
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
        (void)battle_property_mutation61::Apply(0x82ad0ad0, m, d, s);
      }
    }
    s.r[3] = result;
    s.r[1] += 224;
    for (unsigned i = 15; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82ac8968) {
    s.r[8] = s.r[7];
    s.r[7] = 1;
    return battle_property_mutation61::Apply(0x82ac87d8, m, d, s);
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
