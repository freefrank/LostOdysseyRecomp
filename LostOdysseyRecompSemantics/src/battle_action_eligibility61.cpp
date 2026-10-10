#include "lo_semantics/battle_action_results61.h"
#include "lo_semantics/battle_evaluation_effects61.h"
#include "lo_semantics/battle_evaluation_gates61.h"
#include "lo_semantics/battle_script_party61.h"
#include "lo_semantics/battle_action_eligibility61.h"
#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/battle_script_runtime61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/battle_script_skill_cost61.h"
#include "lo_semantics/battle_random_range61.h"
#include <bit>
namespace lo::semantic::gpu::battle_action_eligibility61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_action_eligibility61::Apply(e, m, d, s) &&
      !battle_script_skill_cost61::Apply(e, m, d, s) &&
      !battle_random_range61::Apply(e, m, d, s) &&
      !battle_script_actions61::Apply(e, m, d, s) &&
      !battle_script_runtime61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82b120e0) {
    auto receiver = Address(s.r[3]), kind = m.ReadU32(receiver + 20);
    unsigned record = 0, offset = 32;
    if (kind == 2 || (kind >= 6 && kind <= 9) || kind == 11)
      record = m.ReadU32(receiver + 44);
    else if (kind == 3) {
      record = m.ReadU32(receiver + 48);
      offset = 40;
    } else if (kind == 10) {
      record = m.ReadU32(receiver + 52);
      offset = 16;
    } else {
      m.WriteU8(receiver + 208, 0);
      return true;
    }
    auto callback =
        m.ReadU32(receiver + 4 * (m.ReadU32(record + offset) + 124));
    s.ctr = callback;
    if (!battle_action_results61::Apply(callback, m, d, s) &&
        !battle_evaluation_effects61::Apply(callback, m, d, s) &&
        !battle_evaluation_gates61::Apply(callback, m, d, s) &&
        !battle_script_party61::Apply(callback, m, d, s))
      d.guest.CallIndirect(callback, m, s);
    return true;
  }
  unsigned first = 31, frame = 96;
  if (e == 0x82ad0c10) {
    first = 27;
    frame = 144;
  } else if (e == 0x82b14168) {
    first = 24;
    frame = 160;
  } else if (e != 0x8238abe0 && e != 0x82b143b0 && e != 0x82b121b0)
    return false;
  auto old = Address(s.r[1]), owner = Address(s.r[3]), source = Address(s.r[4]),
       target = Address(s.r[5]), kind = Address(s.r[6]),
       detail = Address(s.r[7]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  if (e == 0x82b121b0) {
    // Parameter rows retain source field ordering, including untouched 84/164
    // fields on categories that never write them.
    auto category = source;
    bool skill =
        category == 2 || (category >= 6 && category <= 9) || category == 15;
    bool item =
        category == 3 || category == 5 || (category >= 25 && category <= 29);
    bool special = category == 10, inventory = category == 11;
    auto fcopy = [&](unsigned from, unsigned to, unsigned reg = 0) {
      if (s.cached_fp_control & 0x8040) {
        s.cached_fp_control &= ~0x8040u;
        d.fp.SetHostFpControl(s.cached_fp_control);
      }
      auto value = double(std::bit_cast<float>(m.ReadU32(from)));
      s.fpr_bits[reg] = std::bit_cast<std::uint64_t>(value);
      m.WriteU32(owner + to, std::bit_cast<unsigned>(float(value)));
    };
    if (skill || item || special || inventory) {
      auto row = m.ReadU32(owner + (item ? 48 : special ? 52 : 44));
      unsigned start = item ? 48 : special ? 20 : 36;
      constexpr unsigned destinations[] = {92,  100, 96,  104,
                                           108, 112, 116, 120};
      for (unsigned i = 0; i < 8; ++i) {
        auto value = m.ReadU32(row + start + 4 * i);
        m.WriteU32(owner + 132 + 4 * i, value);
        m.WriteU32(owner + destinations[i], value);
      }
      if (item) {
        fcopy(0x82000e50, 84);
        fcopy(row + 80, 80, 13);
        fcopy(row + 84, 88);
      } else {
        unsigned base = special ? 52 : 68;
        fcopy(row + base, 80);
        if (!inventory)
          fcopy(row + base + 4, 84);
        fcopy(row + base + 8, 88);
      }
      m.WriteU32(owner + 124, item ? 0 : m.ReadU32(row + (special ? 4 : 12)));
      m.WriteU32(owner + 176, item || special ? 16 : m.ReadU32(row + 32));
      if (skill || item) {
        s.r[3] = m.ReadU32(0x83291dc0);
        s.r[4] = m.ReadU32(owner + 4);
        s.r[5] = m.ReadU32(row + (item ? 24 : 16));
        Call(0x82ac1af0, m, d, s);
        m.WriteU32(owner + 212, Address(s.r[3]));
        row = m.ReadU32(owner + (item ? 48 : 44));
      } else
        m.WriteU32(owner + 212, 0);
      m.WriteU32(owner + 128, special ? 0 : m.ReadU32(row + (item ? 92 : 88)));
      m.WriteU32(owner + 164, m.ReadU32(row + (item ? 24 : special ? 4 : 12)));
      if (m.ReadU32(owner + 124) & 64) {
        s.r[3] = m.ReadU32(0x83264558);
        s.r[4] = 0;
        s.r[5] = 3;
        s.r[6] = 102;
        s.r[7] = m.ReadU32(m.ReadU32(owner + 4) + 64);
        Call(0x82aa0740, m, d, s);
        auto shift = Address(s.r[3]) & 63;
        m.WriteU32(owner + 124, shift < 32 ? 1u << shift : 0);
      }
    } else {
      fcopy(0x82000e50, 80);
      fcopy(0x82000e50, 88);
      for (unsigned offset = 92; offset <= 160; offset += 4)
        m.WriteU32(owner + offset, 0);
      m.WriteU32(owner + 176, 0);
      m.WriteU32(owner + 212, 0);
    }
  } else if (e == 0x82b14168) {
    m.WriteU32(sp + 80, 0x8204a1d8);
    for (auto offset : {4u, 12u})
      m.WriteU32(owner + offset, source);
    for (auto offset : {8u, 16u})
      m.WriteU32(owner + offset, target);
    m.WriteU32(owner + 20, kind);
    m.WriteU32(owner + 24, detail);
    for (auto offset : {203u, 76u, 60u})
      m.WriteU8(owner + offset, 1);
    m.WriteU32(owner + 196, 0);
    m.WriteU32(owner + 184, 0);
    auto skills = m.ReadU32(0x83264984), items = m.ReadU32(0x832649c0),
         special = m.ReadU32(m.ReadU32(0x832ca0d0) + 132),
         inventory = m.ReadU32(0x83264978);
    m.WriteU32(owner + 44, skills);
    m.WriteU32(owner + 48, items);
    m.WriteU32(owner + 52, special);
    m.WriteU32(owner + 56, inventory);
    unsigned descriptor = 0;
    if (kind == 2 || (kind >= 6 && kind <= 9)) {
      m.WriteU8(owner + 76, 0);
      skills += 96 * detail;
      m.WriteU32(owner + 44, skills);
      descriptor = m.ReadU32(skills + 32);
    } else if (kind == 3) {
      m.WriteU8(owner + 76, 0);
      items += 104 * detail;
      m.WriteU32(owner + 48, items);
      descriptor = m.ReadU32(items + 40);
    } else if (kind == 10) {
      m.WriteU8(owner + 76, m.ReadU32(special + 64) != 0);
      special += 68 * detail;
      m.WriteU32(owner + 52, special);
      descriptor = m.ReadU32(special + 16);
    } else if (kind == 11) {
      m.WriteU8(owner + 76, 0);
      inventory += 196 * detail;
      m.WriteU32(owner + 56, inventory);
      skills += 96 * m.ReadU32(inventory + 48);
      m.WriteU32(owner + 44, skills);
      descriptor = m.ReadU32(skills + 32);
    }
    s.r[3] = target;
    Call(0x8238abe0, m, d, s);
    bool valid = (!Address(s.r[3]) || descriptor == 7 || descriptor == 18) &&
                 m.ReadU32(target + 132) != 0;
    if (valid) {
      s.r[3] = owner;
      s.r[4] = m.ReadU32(owner + 20);
      Call(0x82b121b0, m, d, s);
      m.WriteU8(owner + 70, 0);
      m.WriteU8(owner + 69, 0);
      m.WriteU8(owner + 208, 1);
      m.WriteU8(owner + 200, 0);
    }
    s.r[3] = valid;
    m.WriteU32(sp + 80, 0x8204a1d8);
  } else if (e == 0x8238abe0) {
    s.r[3] = 0;
    Call(0x8238aa80, m, d, s);
    auto bank = Address(s.r[3]);
    s.r[3] = 0;
    Call(0x8238aab0, m, d, s);
    s.r[3] = Address(s.r[3]) & m.ReadU32(owner + 272 * bank + 232);
  } else if (e == 0x82b143b0) {
    Call(0x82b14168, m, d, s);
    if (!(Address(s.r[3]) & 255))
      s.r[3] = 0;
    else {
      s.r[3] = owner;
      Call(0x82b120e0, m, d, s);
      s.r[3] = m.ReadU8(owner + 208) != 0 || m.ReadU8(owner + 76) != 0;
    }
  } else {
    m.WriteU32(sp + 80, 0x8204a1d8);
    bool result = kind <= 31 && kind != 17;
    bool basic = kind == 1 || kind == 12 || kind == 16 || kind == 22 ||
                 kind == 30 || kind == 31;
    if (result && basic) {
      s.r[3] = target;
      Call(0x8238abe0, m, d, s);
      if (Address(s.r[3]))
        result = false;
    }
    if (result && (basic || kind == 3)) {
      Call(0x82380a18, m, d, s);
      Call(0x82389b78, m, d, s);
      if (m.ReadU16(Address(s.r[3]) + 148) & 32) {
        s.r[3] = 0x832c9c54;
        s.r[4] = target;
        Call(0x82a9bdb0, m, d, s);
        auto actor = Address(s.r[3]);
        if (actor && (m.ReadU32(actor + 64) & 0x4000u) &&
            (kind == 3 || m.ReadU32(source + 68) != 7))
          result = false;
      }
    }
    if (result && (kind == 2 || kind == 3 || (kind >= 6 && kind <= 11))) {
      s.r[3] = m.ReadU32(0x832ca0d8);
      s.r[4] = source;
      s.r[5] = target;
      s.r[6] = 3;
      s.r[7] = detail;
      Call(0x82b143b0, m, d, s);
      result = (Address(s.r[3]) & 255) != 0;
    }
    s.r[3] = result;
    m.WriteU32(sp + 80, 0x8204a1d8);
  }
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_action_eligibility61
