#include "lo_semantics/battle_action_eligibility61.h"
#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/battle_script_runtime61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_action_eligibility61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_action_eligibility61::Apply(e, m, d, s) &&
      !battle_script_actions61::Apply(e, m, d, s) &&
      !battle_script_runtime61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned first = 31, frame = 96;
  if (e == 0x82ad0c10) {
    first = 27;
    frame = 144;
  } else if (e != 0x8238abe0 && e != 0x82b143b0)
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
  if (e == 0x8238abe0) {
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
