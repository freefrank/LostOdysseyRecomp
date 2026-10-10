#include "lo_semantics/battle_action_finalize61.h"
#include "lo_semantics/battle_script_runtime61.h"
#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/battle_action_adjustments61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_action_finalize61 {
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82acde40)
    return false;
  using recovery_abi::Address;
  auto manager = Address(s.r[3]), resource = Address(s.r[4]),
       reset = Address(s.r[5]) & 255, variant = Address(s.r[6]) & 255,
       old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = 21; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= 192;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  m.WriteU32(sp + 80, 0x8204a1d8);
  s.r[3] = 0x832c9c54;
  s.r[4] = resource;
  (void)battle_script_runtime61::Apply(0x82a9bdb0, m, d, s);
  auto actor = Address(s.r[3]);
  bool blend = actor && (m.ReadU8(actor + 64) & 1) && variant != 1;
  if ((m.ReadU32(resource + 124) & 0x10000000u) && m.ReadU32(resource + 76316))
    blend = true;
  if (blend) {
    auto current = m.ReadU32(resource + 88) * 25 + m.ReadU32(resource + 92),
         incoming = m.ReadU32(manager + 4) * 25 + m.ReadU32(manager + 8);
    if (std::int32_t(current) > std::int32_t(incoming)) {
      auto half = std::int32_t(incoming) / 2;
      incoming = half > 0 ? unsigned(half) : 1;
    } else {
      auto half = std::int32_t(current) / 2;
      current = half > 0 ? unsigned(half) : 1;
    }
    auto total = current + incoming;
    auto group = std::int32_t(total) / 25;
    auto remainder = total - unsigned(group) * 25;
    m.WriteU32(manager + 4, unsigned(group));
    m.WriteU32(manager + 8, remainder ? remainder : 1);
  }
  if (blend || !m.ReadU32(resource + 92) || reset == 1) {
    m.WriteU32(resource + 88, m.ReadU32(manager + 4));
    m.WriteU32(resource + 92, m.ReadU32(manager + 8));
    m.WriteU32(resource + 96, m.ReadU32(manager + 12));
  }
  s.r[3] = 235;
  (void)battle_script_actions61::Apply(0x8238aa80, m, d, s);
  auto bank = Address(s.r[3]);
  s.r[3] = 235;
  (void)battle_script_actions61::Apply(0x8238aab0, m, d, s);
  if (m.ReadU32(resource + 272 * bank + 232) & Address(s.r[3])) {
    s.r[3] = 235;
    (void)battle_script_actions61::Apply(0x8238aa80, m, d, s);
    bank = Address(s.r[3]);
    s.r[3] = 235;
    (void)battle_action_adjustments61::Apply(0x82ac84e8, m, d, s);
    auto factor = m.ReadU32(resource + 4 * (68 * bank + Address(s.r[3]) + 59));
    if (factor) {
      auto total =
          (m.ReadU32(resource + 88) * 25 + m.ReadU32(resource + 92)) * 600 +
          m.ReadU32(resource + 96);
      auto scaled = unsigned(std::int32_t(total * factor) / 100);
      // Preserve the source's word arithmetic and its explicit 600/15000 steps.
      auto group = unsigned(std::int32_t(scaled) / 25) * 600;
      m.WriteU32(resource + 88, group);
      auto rest = scaled - group * 15000;
      auto part = std::int32_t(rest) / 600;
      m.WriteU32(resource + 92, unsigned(part));
      m.WriteU32(resource + 96, rest - unsigned(part) * 600);
    }
  }
  if (m.ReadU32(0x832cb778) == 242)
    m.WriteU32(resource + 4956, m.ReadU32(resource + 4956) | 4096);
  if (m.ReadU32(resource + 4956) & 4096)
    m.WriteU32(resource + 88, 0);
  if (m.ReadU32(m.ReadU32(0x832c9c54 + 44) + 28) & 0x00100000u) {
    auto id = m.ReadU32(resource + 64);
    m.WriteU32(resource + 88, 0);
    m.WriteU32(resource + 96, 0);
    m.WriteU32(resource + 92, std::int32_t(id) < 20 ? id + 1 : id - 14);
  }
  if (actor) {
    auto flags = m.ReadU32(actor + 64);
    if (flags & 4096) {
      m.WriteU32(resource + 92, 0);
      m.WriteU32(resource + 96, 0);
      m.WriteU32(resource + 88, 0);
    } else if (flags & 128) {
      m.WriteU32(resource + 92, 25);
      m.WriteU32(resource + 96, 0);
      m.WriteU32(resource + 88, 0);
    }
  }
  m.WriteU32(resource + 100,
             (m.ReadU32(resource + 100) & 0x3fffffffu) | 0x80000000u);
  m.WriteU32(sp + 80, 0x8204a1d8);
  s.r[1] += 192;
  for (unsigned i = 21; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_action_finalize61
