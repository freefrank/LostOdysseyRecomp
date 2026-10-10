#include "lo_semantics/battle_settlement61.h"
#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/battle_manager_access61.h"
#include "lo_semantics/battle_random_range61.h"
#include "lo_semantics/battle_script_party61.h"
#include "lo_semantics/battle_resource_growth61.h"
#include "lo_semantics/battle_resource_stats61.h"
#include "lo_semantics/battle_phase_support61.h"
#include "lo_semantics/battle_roster_persistence61.h"
#include "lo_semantics/battle_progression61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <limits>
namespace lo::semantic::gpu::battle_settlement61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_settlement61::Apply(e, m, d, s) &&
      !battle_phase_support61::Apply(e, m, d, s) &&
      !battle_roster_persistence61::Apply(e, m, d, s) &&
      !battle_progression61::Apply(e, m, d, s) &&
      !battle_resource_growth61::Apply(e, m, d, s) &&
      !battle_resource_stats61::Apply(e, m, d, s) &&
      !battle_random_range61::Apply(e, m, d, s) &&
      !battle_script_party61::Apply(e, m, d, s) &&
      !battle_action_readiness61::Apply(e, m, d, s) &&
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
  case 0x82ac6d88:
    frame = 96;
    first = 31;
    break;
  case 0x82ac6c60:
    frame = 160;
    first = 26;
    literal = 80;
    break;
  case 0x82ac6460:
    frame = 144;
    first = 26;
    break;
  case 0x82ac6738:
    frame = 432;
    first = 21;
    break;
  case 0x82ac32c0:
    frame = 144;
    first = 27;
    break;
  case 0x82ac2140:
    frame = 160;
    first = 24;
    break;
  case 0x82ac20b0:
    frame = 144;
    first = 26;
    break;
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
  if (e == 0x82ac32c0) {
    recovery_abi::WriteU64(m, old - 64, s.fpr_bits[30]);
    recovery_abi::WriteU64(m, old - 56, s.fpr_bits[31]);
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
  auto play = [&]() {
    s.r[3] = W(0x83315fb4);
    s.ctr = W(W(Address(s.r[3])) + 352);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    Call(0x8229dfd8, m, d, s);
    return Address(s.r[3]);
  };
  auto achievement = [](unsigned group) {
    constexpr unsigned codes[] = {6, 7, 8, 9, 10, 11, 12, 14, 13, 13};
    return codes[group];
  };
  if (e == 0x82ac6d88) {
    manager();
    auto battle = Address(s.r[3]);
    if (W(battle + 148) & 0x8000u) {
      manager();
      s.r[4] = 13;
      s.r[5] = 1;
      Call(0x82aaa7c8, m, d, s);
    } else {
      s.r[3] = owner;
      Call(0x82ac22f8, m, d, s);
      s.r[3] = owner;
      Call(0x82ac1ce0, m, d, s);
      m.WriteU32(owner + 92, Address(s.r[3]));
      s.r[3] = owner;
      Call(0x82ac31b0, m, d, s);
      m.WriteU32(owner + 88, Address(s.r[3]));
      for (auto entry :
           {0x82ac2140u, 0x82ac1dc8u, 0x82ac1fa0u, 0x82ac32c0u, 0x82ac6c60u}) {
        s.r[3] = owner;
        Call(entry, m, d, s);
      }
      manager();
      Call(0x82af57d8, m, d, s);
      manager();
      m.WriteU8(Address(s.r[3]) + 212, 1);
    }
  } else if (e == 0x82ac6c60) {
    for (unsigned i = 0; i < 5; ++i) {
      auto slot = owner + 564 + 128 * i;
      if (W(slot) && m.ReadU8(slot + 96) == 1) {
        m.WriteU32(owner + 92, W(owner + 92) * 2);
        break;
      }
    }
    for (unsigned i = 0; i < 5; ++i) {
      auto slot = owner + 576 + 128 * i, resource = W(slot - 12);
      if (!resource || blocked(resource))
        continue;
      auto award = std::int32_t(W(owner + 92));
      fp();
      recovery_abi::WriteU64(m, sp + 88, std::uint64_t(std::int64_t(award)));
      m.WriteU32(resource + 144, std::bit_cast<unsigned>(
                                     float(float(award) + F(resource + 144))));
      auto kind = W(resource + 152);
      if (kind > 1)
        continue;
      s.r[3] = owner;
      s.r[4] = resource;
      s.r[5] = slot;
      Call(kind == 1 ? 0x82ac6460 : 0x82ac6738, m, d, s);
    }
  } else if (e == 0x82ac6460) {
    auto resource = Address(s.r[4]), output = Address(s.r[5]);
    Call(0x82380a18, m, d, s);
    Call(0x82ab0110, m, d, s);
    auto inventory = W(W(Address(s.r[3])) + 4) + 72;
    unsigned learned = 0;
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(W(W(0x832ca0d0) + 308));
         ++i) {
      auto row = W(W(0x832ca0d0) + 304) + 20 * i;
      if (W(row) != W(resource + 68) ||
          std::int32_t(W(row + 8)) > std::int32_t(W(resource + 140)))
        continue;
      if (learned >= 20)
        break;
      auto id = W(row + 4), flags = resource + 16 * (id + 330);
      if (W(flags) & 0x80000000u)
        continue;
      m.WriteU32(output + 4 * learned++, id);
      m.WriteU32(flags, W(flags) | 0x80000000u);
      for (unsigned j = 0; std::int32_t(j) < std::int32_t(W(resource + 5156));
           ++j)
        if (!W(resource + 5160 + 4 * j)) {
          m.WriteU32(resource + 5160 + 4 * j, id);
          break;
        }
      m.WriteU32(sp + 80, W(row + 12));
      m.WriteU32(sp + 84, W(row + 16));
      for (unsigned j = 0; j < 2; ++j) {
        auto item = W(sp + 80 + 4 * j), flag = inventory + 4 * (item + 2048);
        if ((W(flag) & 0x80000000u) || !item)
          continue;
        for (unsigned k = 0; k < 512; ++k)
          if (!W(inventory + 10240 + 4 * k)) {
            m.WriteU32(inventory + 10240 + 4 * k, item);
            break;
          }
        m.WriteU32(flag, W(flag) | 0x80000000u);
      }
      // The source checks the word at +28 of a 20-byte table row.
      if (W(W(W(0x832ca0d0) + 304) + 20 * i + 28) == 0) {
        auto group = W(resource + 68);
        (void)play();
        if (group <= 9) {
          s.r[3] = owner;
          s.r[4] = achievement(group);
          Call(0x82ac3498, m, d, s);
        }
        break;
      }
    }
  } else if (e == 0x82ac6738) {
    auto resource = Address(s.r[4]), output = Address(s.r[5]), profile = play();
    for (unsigned i = 0; i < 256; ++i)
      m.WriteU8(sp + 80 + i, 0);
    unsigned learned = 0;
    auto notify = [&](unsigned code) {
      auto p = play();
      s.r[4] = code;
      s.ctr = W(W(p) + 404);
      d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
      s.r[3] = code;
      d.guest.CallDirect(0x828208f8, m, s);
    };
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(W(resource + 9376));
         ++i) {
      auto row = resource + 9384 + 12 * i, group = W(row), id = W(row + 4),
           shift = group & 63;
      auto flag = shift < 32 ? 1u << shift : 0;
      if (W(profile + 100) & flag)
        break;
      bool present = false;
      for (unsigned j = 0; j < 5; ++j) {
        auto peer = W(owner + 564 + 128 * j);
        if (peer && W(peer + 68) == group) {
          present = true;
          break;
        }
      }
      auto skill = resource + 16 * (id + 330),
           definition = W(0x83264978 + 72) + 104 * id;
      if (!present || (W(skill) & 0x80000000u))
        continue;
      auto points = W(skill + 4) + W(owner + 92);
      m.WriteU32(skill + 4, points);
      if (std::int32_t(W(definition + 8)) > std::int32_t(points))
        continue;
      m.WriteU32(skill + 4, W(definition + 8));
      m.WriteU32(skill, W(skill) | 0x80000000u);
      m.WriteU32(output + 4 * learned++, id);
      m.WriteU8(sp + 80 + id, 1);
      (void)play();
      notify(5);
    }
    for (unsigned i = 0; i < 5; ++i) {
      auto gear = W(resource + 5116 + 4 * i),
           id = W(W(0x83264978) + 196 * gear + 24);
      if (!id)
        continue;
      auto skill = resource + 16 * (id + 330),
           definition = W(0x83264978 + 72) + 104 * id;
      if ((W(skill) & 0x80000000u) || m.ReadU8(sp + 80 + id))
        continue;
      auto points = W(skill + 4) + W(owner + 92);
      m.WriteU32(skill + 4, points);
      if (std::int32_t(W(definition + 8)) > std::int32_t(points))
        continue;
      m.WriteU32(skill + 4, W(definition + 8));
      m.WriteU32(skill, W(skill) | 0x80000000u);
      m.WriteU32(output + 4 * learned++, id);
    }
    bool complete = true;
    for (unsigned id = 0; id < 256; ++id)
      if (W(W(0x83264978 + 72) + 104 * id + 12) &&
          !(W(resource + 16 * (id + 330)) & 0x80000000u)) {
        complete = false;
        break;
      }
    if (complete) {
      auto group = W(resource + 68);
      (void)play();
      if (group <= 9)
        notify(achievement(group));
    }
  } else if (e == 0x82ac32c0) {
    Call(0x82380a18, m, d, s);
    Call(0x82ab0110, m, d, s);
    fp();
    auto hpCap = F(0x822184dc), mpCap = F(0x822181e4);
    s.fpr_bits[30] = std::bit_cast<std::uint64_t>(double(mpCap));
    s.fpr_bits[31] = std::bit_cast<std::uint64_t>(double(hpCap));
    for (unsigned i = 0; i < 5; ++i) {
      auto slot = owner + 572 + 128 * i, resource = W(slot - 8);
      if (!resource || m.ReadU8(slot) != 1)
        continue;
      m.WriteU32(slot + 116, W(resource + 140));
      constexpr unsigned offsets[] = {2588, 2616, 2592, 2620};
      for (unsigned j = 0; j < 4; ++j) {
        auto value = F(resource + offsets[j]), cap = (j & 1) ? mpCap : hpCap;
        m.WriteU32(slot + 96 + 4 * j,
                   std::bit_cast<unsigned>(value > cap ? cap : value));
      }
      m.WriteU32(resource + 140, W(resource + 140) + 1);
      auto row = W(0x83264978 + 36) + 204 * W(resource + 68);
      constexpr unsigned from[] = {124, 152, 108, 112, 120, 116, 128,
                                   132, 140, 148, 144, 156, 136};
      constexpr unsigned to[] = {2412, 2440, 2416, 2420, 2428, 2424, 2432,
                                 2444, 2448, 2456, 2452, 2460, 2464};
      for (unsigned j = 0; j < 13; ++j)
        m.WriteU32(resource + to[j], W(row + from[j]));
      m.WriteU32(resource + 152, W(row + 160));
      s.r[3] = owner;
      s.r[4] = resource;
      Call(0x82ac25e8, m, d, s);
      m.WriteU32(owner + 4, W(resource + 5108));
      for (unsigned j = 0; j < 5; ++j)
        m.WriteU32(owner + 12 + 4 * j, W(resource + 5116 + 4 * j));
      s.r[3] = owner;
      s.r[4] = resource;
      Call(0x82ac3058, m, d, s);
      auto hpDelta = float(F(resource + 2592) - F(slot + 104)),
           mpDelta = float(F(resource + 2620) - F(slot + 108));
      m.WriteU32(resource + 2588,
                 std::bit_cast<unsigned>(float(hpDelta + F(resource + 2588))));
      m.WriteU32(resource + 2616,
                 std::bit_cast<unsigned>(float(mpDelta + F(resource + 2616))));
    }
  } else if (e == 0x82ac2140) {
    auto maximum = 99u - W(owner + 1208) * 5u;
    for (unsigned i = 0; i < 32; ++i) {
      auto row = owner + 100 + 12 * i, id = W(row);
      if (!id)
        continue;
      auto resourceID = W(row + 4);
      manager();
      s.r[4] = resourceID;
      Call(0x8238e308, m, d, s);
      auto resource = Address(s.r[3]);
      if (m.ReadU8(resource + 124) & 1)
        continue;
      auto table = W(W(0x832ca0d0) + 120) + 140 * W(row);
      bool drops = W(table + 104) == W(table + 108) &&
                   W(table + 112) == W(table + 116) &&
                   W(table + 104) == W(table + 116);
      if (!drops) {
        s.r[3] = W(0x83264558);
        s.r[4] = 70;
        s.r[5] = 0;
        s.r[6] = W(resource + 64);
        Call(0x82aa0838, m, d, s);
        drops = (Address(s.r[3]) & 255) == 1;
      }
      unsigned item = 0;
      if (drops) {
        bool uniform = (W(table + 68) & 0x80000u) != 0;
        s.r[3] = W(0x83264558);
        s.r[4] = 0;
        s.r[5] = uniform ? 3 : maximum;
        s.r[6] = 1;
        s.r[7] = W(resource + 64);
        Call(0x82aa0740, m, d, s);
        auto roll = Address(s.r[3]);
        if (uniform) {
          if (roll <= 3)
            item = W(table + 104 + 4 * roll);
        } else {
          auto value = std::int32_t(roll);
          item = W(table + (value < 5    ? 116
                            : value < 10 ? 112
                            : value < 50 ? 108
                                         : 104));
        }
      }
      s.r[3] = owner;
      s.r[4] = item;
      Call(0x82ac0068, m, d, s);
    }
    s.r[3] = owner;
    Call(0x82ac20b0, m, d, s);
  } else if (e == 0x82ac20b0) {
    for (unsigned i = 0; i < 10; ++i) {
      auto row = owner + 484 + 8 * i;
      if (!W(row))
        continue;
      auto count = W(row + 4);
      if (m.ReadU8(owner + 1213) == 1) {
        s.r[3] = W(0x83264558);
        s.r[4] = 5;
        s.r[5] = 112;
        s.r[6] = 0;
        Call(0x82aa0838, m, d, s);
        if ((Address(s.r[3]) & 255) == 1) {
          count *= 2;
          m.WriteU32(row + 4, count);
        }
      }
      s.r[3] = 0x832c9c54;
      s.r[4] = W(row);
      s.r[5] = count;
      Call(0x82a9e5e0, m, d, s);
    }
  } else if (e == 0x82ac31b0) {
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
  if (e == 0x82ac32c0) {
    s.fpr_bits[30] = recovery_abi::ReadU64(m, old - 64);
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - 56);
  }
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_settlement61
