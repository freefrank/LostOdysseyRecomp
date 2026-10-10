#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/battle_script_parameters61.h"
#include "lo_semantics/battle_script_core61.h"
#include "lo_semantics/battle_script_dispatch61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_script_extensions61 {
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  using recovery_abi::Address;
  if (e == 0x8238c198) {
    s.r[5] = 0;
    return battle_script_parameters61::Apply(0x8238be38, m, d, s);
  }
  if (e == 0x8238c208) {
    s.r[6] = 0;
    return battle_script_core61::Apply(0x8238c210, m, d, s);
  }
  if (e != 0x8238c590 && e != 0x82a9da28 && e != 0x82af7fb8 && e != 0x82af7fd0)
    return false;
  auto owner = Address(s.r[3]), actor = m.ReadU32(owner + 24),
       pc = m.ReadU32(actor + 52), code = m.ReadU32(actor + 36) + pc;
  if (e == 0x8238c590) {
    auto p = code + Address(s.r[4]);
    auto v = unsigned(m.ReadU8(p)) | (unsigned(m.ReadU8(p + 1)) << 8);
    s.r[3] = std::uint64_t(std::int64_t(std::int16_t(v)));
    return true;
  }
  if (e == 0x82a9da28) {
    auto state = m.ReadU32(owner + 44);
    m.WriteU32(state + 28, m.ReadU32(state + 28) | 0x04000000);
    auto target = m.ReadU32(owner + 4 * (unsigned(m.ReadU8(code + 1)) + 270));
    s.ctr = target;
    battle_script_dispatch61::DispatchOpcode(target, m, d, s);
    return true;
  }
  m.WriteU32(actor + 52, pc + (e == 0x82af7fb8 ? 9 : 2));
  return true;
}
} // namespace lo::semantic::gpu::battle_script_extensions61
