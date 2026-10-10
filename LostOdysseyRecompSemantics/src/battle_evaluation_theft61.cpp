#include "lo_semantics/battle_evaluation_theft61.h"
#include "lo_semantics/battle_action_eligibility61.h"
#include "lo_semantics/battle_evaluation_chance61.h"
#include "lo_semantics/battle_effect_followups61.h"
#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/battle_action_results61.h"
#include "lo_semantics/battle_script_party61.h"
#include "lo_semantics/battle_script_runtime61.h"
#include "lo_semantics/battle_random_range61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/string_storage_context61.h"
#include <bit>
namespace lo::semantic::gpu::battle_evaluation_theft61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_evaluation_theft61::Apply(e, m, d, s) &&
      !battle_action_eligibility61::Apply(e, m, d, s) &&
      !battle_evaluation_chance61::Apply(e, m, d, s) &&
      !battle_effect_followups61::Apply(e, m, d, s) &&
      !string_storage_context61::Apply(e, m, d, s) &&
      !battle_action_readiness61::Apply(e, m, d, s) &&
      !battle_action_results61::Apply(e, m, d, s) &&
      !battle_script_party61::Apply(e, m, d, s) &&
      !battle_script_runtime61::Apply(e, m, d, s) &&
      !battle_random_range61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82b13380 && e != 0x82aa11f0 && e != 0x82aa1268 &&
      e != 0x82a9b288 && e != 0x82af52b0)
    return false;
  auto old = Address(s.r[1]), owner = Address(s.r[3]), id = Address(s.r[4]);
  unsigned frame = e == 0x82b13380                        ? 4288
                   : (e == 0x82a9b288 || e == 0x82af52b0) ? 96
                                                          : 112,
           first = e == 0x82b13380                        ? 22
                   : (e == 0x82a9b288 || e == 0x82af52b0) ? 31
                                                          : 30;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  if (e == 0x82b13380)
    (void)recovery_abi::ReadU64(m, old - 4096);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  auto manager = [&]() {
    Call(0x82380a18, m, d, s);
    Call(0x82389b78, m, d, s);
  };
  if (e == 0x82af52b0) {
    Call(0x82380a18, m, d, s);
    Call(0x82ab0110, m, d, s);
    s.r[3] = m.ReadU32(m.ReadU32(Address(s.r[3])) + 4 * id);
  } else if (e == 0x82a9b288) {
    manager();
    s.r[4] = id;
    Call(0x82af52b0, m, d, s);
  } else if (e != 0x82b13380) {
    manager();
    s.r[4] = id;
    Call(0x8238e308, m, d, s);
    auto resource = Address(s.r[3]);
    s.r[4] = resource && m.ReadU32(resource + 80) ? m.ReadU32(resource + 76)
                                                  : 0x821a83d0;
    s.r[3] = m.ReadU32(owner + 16) + (e == 0x82aa11f0 ? 272 : 284);
    Call(0x8229f5e0, m, d, s);
  } else {
    m.WriteU32(sp + 80, 0x8204a1d8);
    auto source = [&]() { return m.ReadU32(owner + 4); };
    auto target = [&]() { return m.ReadU32(owner + 8); };
    auto report = [&](unsigned code) {
      auto p = m.ReadU32(0x832cb798);
      m.WriteU32(p + 20, 2);
      m.WriteU32(m.ReadU32(p + 16) + 264, 2);
      m.WriteU32(p + 24, code);
      m.WriteU32(m.ReadU32(p + 16) + 268, code);
    };
    auto platform = [&]() {
      s.r[3] = m.ReadU32(0x83315fb4);
      s.ctr = m.ReadU32(m.ReadU32(Address(s.r[3])) + 352);
      d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
      Call(0x8229dfd8, m, d, s);
      return Address(s.r[3]);
    };
    auto chance = [&](unsigned threshold, unsigned tag) {
      s.r[3] = m.ReadU32(0x83264558);
      s.r[4] = threshold;
      s.r[5] = tag;
      s.r[6] = m.ReadU32(source() + 64);
      Call(0x82aa0838, m, d, s);
      return (Address(s.r[3]) & 255) == 1;
    };
    auto property = [&](unsigned id) {
      s.r[3] = source();
      s.r[4] = id;
      Call(0x8238e368, m, d, s);
      return (Address(s.r[3]) & 255) == 1;
    };
    auto mark = [&]() {
      s.r[3] = m.ReadU32(0x832cb790);
      Call(0x82b2b248, m, d, s);
    };
    auto label = [&](unsigned item) {
      s.r[3] = m.ReadU32(0x832cb798);
      s.r[4] = item;
      s.r[5] = 2;
      Call(0x82aa12e0, m, d, s);
    };
    auto load = [&](unsigned p, unsigned reg) {
      if (s.cached_fp_control & 0x8040) {
        s.cached_fp_control &= ~0x8040u;
        d.fp.SetHostFpControl(s.cached_fp_control);
      }
      auto value = std::bit_cast<float>(m.ReadU32(p));
      s.fpr_bits[reg] = std::bit_cast<std::uint64_t>(double(value));
      return value;
    };
    auto work = [&]() {
      s.r[3] = m.ReadU32(0x8324570c);
      s.r[4] = source();
      s.r[5] = target();
      s.r[6] = s.r[7] = 1;
      s.r[8] = m.ReadU8(owner + 200);
      Call(0x82ace408, m, d, s);
      if (!(Address(s.r[3]) & 255)) {
        m.WriteU8(owner + 208, 0);
        return;
      }
      s.r[3] = m.ReadU32(0x832cb798);
      s.r[4] = m.ReadU32(source() + 64);
      Call(0x82aa11f0, m, d, s);
      s.r[3] = m.ReadU32(0x832cb798);
      s.r[4] = m.ReadU32(target() + 64);
      Call(0x82aa1268, m, d, s);
      if (m.ReadU32(source() + 124) & 0x10000000u) {
        if (m.ReadU8(target() + 124) & 1) {
          report(255);
          return;
        }
        s.r[3] = owner;
        Call(0x82b08f88, m, d, s);
        if (!(Address(s.r[3]) & 255)) {
          report(254);
          return;
        }
        auto row = m.ReadU32(m.ReadU32(0x832ca0d0) + 120) +
                   140 * m.ReadU32(target() + 68);
        if (!m.ReadU32(row + 132) && !m.ReadU32(row + 136)) {
          report(255);
          return;
        }
        auto rare = chance(property(110) ? 29 : 9, 18);
        auto item = rare ? m.ReadU32(row + 136) : 0;
        if (!item)
          item = m.ReadU32(row + 132);
        if (!item) {
          report(254);
          return;
        }
        platform();
        s.r[3] = 0x832c9c54;
        s.r[4] = item;
        s.r[5] = 1;
        Call(0x82a9e5e0, m, d, s);
        mark();
        m.WriteU32(target() + 124, m.ReadU32(target() + 124) | 0x01000000u);
        label(item);
        report(2);
        return;
      }
      m.WriteU32(source() + 132, 0);
      m.WriteU32(source() + 124, m.ReadU32(source() + 124) | 0x00200000u);
      mark();
      platform();
      s.r[3] = 0x832c9c54;
      s.r[4] = source();
      Call(0x82a9bdb0, m, d, s);
      auto actor = Address(s.r[3]);
      Call(0x82380a18, m, d, s);
      s.r[4] = 1;
      Call(0x82a9b288, m, d, s);
      auto inventory = Address(s.r[3]);
      auto item = m.ReadU32(actor + 320), definitions = m.ReadU32(0x83264978);
      m.WriteU32(owner + 56, definitions);
      bool equipment = false;
      if (item) {
        for (unsigned i = 0; i < 5; ++i)
          if (m.ReadU32(target() + 5116 + 4 * i) == item) {
            m.WriteU32(target() + 5116 + 4 * i, 0);
            s.r[3] = m.ReadU32(0x83291dc0);
            s.r[4] = target();
            Call(0x82ac3058, m, d, s);
            equipment = true;
            break;
          }
        if (!equipment &&
            load(inventory + 4 * (item + 18), 13) == load(0x82000e50, 0)) {
          report(252);
          return;
        }
      } else {
        unsigned count = 0;
        auto zero = load(0x82000e50, 0);
        for (unsigned i = 0; i < 1024; ++i)
          if (!(m.ReadU32(definitions + 196 * i + 20) & 0x400u) &&
              load(inventory + 72 + 4 * i, 13) != zero)
            m.WriteU32(sp + 96 + 4 * count++, i);
        if (!count) {
          report(253);
          return;
        }
        s.r[3] = m.ReadU32(0x83264558);
        s.r[4] = 0;
        s.r[5] = count - 1;
        s.r[6] = 19;
        s.r[7] = m.ReadU32(source() + 64);
        Call(0x82aa0740, m, d, s);
        item = m.ReadU32(sp + 96 + 4 * Address(s.r[3]));
        if (!item) {
          report(253);
          return;
        }
      }
      if (!equipment) {
        s.r[3] = 0x832c9c54;
        s.r[4] = item;
        s.r[5] = 1;
        Call(0x82a9e6b8, m, d, s);
      }
      auto play = platform();
      auto index = m.ReadU32(actor + 324) * 2065 + item;
      auto state = m.ReadU32(0x832c9c54 + 44);
      auto local = state + 4 * (index + 50);
      m.WriteU32(local, m.ReadU32(local) + 1);
      auto persisted =
          play + 4 * (m.ReadU32(actor + 324) * 2065 + item + 45276);
      m.WriteU32(persisted, m.ReadU32(persisted) + 1);
      label(item);
      report(251);
      m.WriteU32(target() + 124, m.ReadU32(target() + 124) | 0x01000000u);
    };
    work();
    m.WriteU32(sp + 80, 0x8204a1d8);
  }
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_evaluation_theft61
