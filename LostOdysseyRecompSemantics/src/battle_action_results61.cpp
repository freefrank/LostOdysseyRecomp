#include "lo_semantics/battle_action_results61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_action_results61 {
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82b11230) {
    s.r[3] = m.ReadU32(0x832cb790);
    return battle_action_results61::Apply(0x82b2b248, m, d, s);
  }
  unsigned offset, value = 1;
  switch (e) {
  case 0x82b2b248:
    offset = 3722;
    break;
  case 0x82b2b270:
    offset = 10;
    break;
  case 0x82b2b298:
    offset = 3722;
    value = 0;
    break;
  case 0x82b2b2c0:
    offset = 3754;
    break;
  case 0x82b2b2e8:
    offset = 3766;
    break;
  case 0x82b2b310:
    offset = 3758;
    break;
  case 0x82b2b438:
    offset = 3782;
    value = 0;
    break;
  default:
    return false;
  }
  auto owner = recovery_abi::Address(s.r[3]);
  auto index = 116 * m.ReadU32(owner + 12) + m.ReadU32(owner + 24) + offset;
  m.WriteU32(m.ReadU32(owner + 20) + 4 * index, value);
  return true;
}
} // namespace lo::semantic::gpu::battle_action_results61
