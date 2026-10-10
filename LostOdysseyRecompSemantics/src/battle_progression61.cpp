#include "lo_semantics/battle_progression61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_progression61 {
using recovery_abi::Address;
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82ac3498 && e != 0x82ac34f8 && e != 0x82ac6348)
    return false;
  auto old = Address(s.r[1]), owner = Address(s.r[3]),
       argument = Address(s.r[4]), value = Address(s.r[5]);
  unsigned frame = e == 0x82ac3498 ? 96 : 112,
           first = e == 0x82ac3498 ? 31 : 29;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  auto platform = [&]() {
    s.r[3] = m.ReadU32(0x83315fb4);
    s.ctr = m.ReadU32(m.ReadU32(Address(s.r[3])) + 352);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    d.guest.CallDirect(0x8229dfd8, m, s);
    return Address(s.r[3]);
  };
  auto unlock = [&](unsigned manager, unsigned id) {
    s.r[3] = manager;
    s.r[4] = id;
    (void)battle_progression61::Apply(0x82ac3498, m, d, s);
  };
  auto increment = [&](unsigned p, unsigned amount, std::int32_t cap,
                       unsigned manager, unsigned achievement) {
    auto prior = std::int32_t(m.ReadU32(p));
    if (prior < cap) {
      auto next = unsigned(prior) + amount;
      m.WriteU32(p, next);
      if (std::int32_t(next) >= cap)
        unlock(manager, achievement);
    }
  };
  if (e == 0x82ac3498) {
    auto play = platform();
    s.r[4] = argument;
    s.ctr = m.ReadU32(m.ReadU32(play) + 404);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    s.r[3] = argument;
    d.guest.CallDirect(0x828208f8, m, s);
  } else if (e == 0x82ac34f8) {
    auto play = platform(), base = play + 76;
    switch (argument) {
    case 0:
      if (value == 2)
        increment(base + 170748, 1, 3, owner, 27);
      else
        m.WriteU32(base + 170748, 0);
      break;
    case 1:
      if (value == 2)
        increment(base + 170752, 1, 500, owner, 28);
      break;
    case 2:
      increment(base + 170756, value, 1000000, owner, 29);
      break;
    case 3:
      increment(base + 170760, 1, 1000, owner, 30);
      break;
    case 5:
      if (value <= 9)
        unlock(owner, value <= 6 ? value + 6 : value == 7 ? 14 : 13);
      break;
    case 6:
      unlock(owner, 15);
      break;
    case 7:
      unlock(owner, 5);
      break;
    default:
      break;
    }
  } else {
    auto resource = argument;
    if (!(m.ReadU32(resource + 124) & 0x10000000u)) {
      auto manager = m.ReadU32(0x83291dc0), play = platform();
      increment(play + 170836, 1, 1000, manager, 30);
      if (!(m.ReadU32(resource + 124) & 0x04000000u))
        for (unsigned i = 0; i < 32; ++i) {
          auto p = owner + 100 + 12 * i;
          if (m.ReadU32(p + 4) == m.ReadU32(resource + 64))
            break;
          if (!m.ReadU32(p)) {
            m.WriteU32(p, m.ReadU32(resource + 68));
            m.WriteU32(p + 4, m.ReadU32(resource + 64));
            m.WriteU8(p + 8, (m.ReadU32(resource + 124) >> 25) & 1);
            break;
          }
        }
    }
  }
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_progression61
