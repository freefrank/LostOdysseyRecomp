#include "lo_semantics/string_storage_context61.h"
#include "lo_semantics/battle_action_destruction61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_action_destruction61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_action_destruction61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned first, frame;
  switch (e) {
  case 0x828ae428:
    first = 30;
    frame = 112;
    break;
  case 0x828ae120:
    first = 31;
    frame = 96;
    break;
  case 0x82b7ae18:
  case 0x82b7aef0:
    first = 27;
    frame = 144;
    break;
  case 0x82b7afa8: {
    auto old = Address(s.r[1]), caller = Address(s.r[12]) - 144;
    for (unsigned i = 27; i <= 31; ++i)
      recovery_abi::WriteU64(m, old - 8 - 8 * (31 - i), s.r[i]);
    m.WriteU32(old - 48, Address(s.lr));
    s.r[1] -= 128;
    m.WriteU32(Address(s.r[1]), old);
    s.r[31] = caller;
    if (!m.ReadU32(caller + 80)) {
      s.r[3] = s.r[29];
      s.r[4] = s.r[30];
      s.r[5] = s.r[28];
      s.r[6] = s.r[27];
      Call(0x82b7ae18, m, d, s);
    }
    s.r[1] = m.ReadU32(Address(s.r[1]));
    for (unsigned i = 27; i <= 31; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 8 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 48);
    return true;
  }
  default:
    return false;
  }
  auto old = Address(s.r[1]), owner = Address(s.r[3]);
  auto originalSp = s.r[1];
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  s.r[31] = sp;
  if (e == 0x828ae428) {
    m.WriteU32(sp + 132, owner);
    s.r[30] = owner;
    for (unsigned offset : {14884u, 36u}) {
      s.r[3] = owner + offset;
      s.r[4] = 464;
      s.r[5] = 32;
      s.r[6] = 0x828ae120;
      Call(0x82b7aef0, m, d, s);
    }
  } else if (e == 0x828ae120) {
    m.WriteU32(sp + 116, owner);
    s.r[3] = owner + 272;
    s.r[4] = 12;
    s.r[5] = 16;
    s.r[6] = 0x82298938;
    Call(0x82b7aef0, m, d, s);
  } else if (e == 0x82b7ae18) {
    auto stride = Address(s.r[4]), count = Address(s.r[5]),
         callback = Address(s.r[6]), end = owner;
    s.r[30] = end;
    s.r[29] = stride;
    s.r[27] = count;
    s.r[28] = callback;
    while (true) {
      --count;
      s.r[27] = count;
      m.WriteU32(sp + 180, count);
      if (std::int32_t(count) < 0)
        break;
      end -= stride;
      s.r[30] = end;
      m.WriteU32(sp + 164, end);
      s.r[3] = end;
      s.ctr = callback;
      if (!string_storage_context61::Apply(callback & ~3u, m, d, s) &&
          !battle_action_destruction61::Apply(callback & ~3u, m, d, s))
        d.guest.CallIndirect(callback & ~3u, m, s);
    }
  } else {
    auto stride = Address(s.r[4]), count = Address(s.r[5]),
         callback = Address(s.r[6]), end = owner + stride * count;
    s.r[30] = stride;
    s.r[28] = count;
    s.r[27] = callback;
    s.r[29] = end;
    m.WriteU32(sp + 172, stride);
    m.WriteU32(sp + 180, count);
    m.WriteU32(sp + 188, callback);
    m.WriteU32(sp + 164, end);
    m.WriteU32(sp + 80, 0);
    while (true) {
      --count;
      s.r[28] = count;
      m.WriteU32(sp + 180, count);
      if (std::int32_t(count) < 0)
        break;
      end -= stride;
      s.r[29] = end;
      m.WriteU32(sp + 164, end);
      s.r[3] = end;
      s.ctr = callback;
      if (!string_storage_context61::Apply(callback & ~3u, m, d, s) &&
          !battle_action_destruction61::Apply(callback & ~3u, m, d, s))
        d.guest.CallIndirect(callback & ~3u, m, s);
    }
    m.WriteU32(sp + 80, 1);
    s.r[12] = sp + 144;
    Call(0x82b7afa8, m, d, s);
  }
  s.r[1] = originalSp;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_action_destruction61
