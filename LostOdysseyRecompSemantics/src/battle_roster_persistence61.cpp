#include "lo_semantics/battle_roster_persistence61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_roster_persistence61 {
namespace {
using recovery_abi::Address;
void Call(unsigned entry, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_roster_persistence61::Apply(entry, m, d, s) &&
      !crt_copy_full_context::Apply(entry, m, s))
    d.guest.CallDirect(entry, m, s);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned frame = 0, first = 32;
  switch (e) {
  case 0x82af52f0:
    break;
  case 0x82abfc50:
  case 0x82af5400:
    frame = 112;
    first = 30;
    break;
  case 0x82af56a8:
  case 0x82af5810:
    frame = 128;
    first = 27;
    break;
  case 0x82af57d8:
    frame = 96;
    first = 31;
    break;
  default:
    return false;
  }
  auto old = Address(s.r[1]), owner = Address(s.r[3]);
  if (frame) {
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= frame;
    m.WriteU32(Address(s.r[1]), old);
  }
  auto copy = [&](unsigned destination, unsigned source, unsigned size) {
    s.r[3] = destination;
    s.r[4] = source;
    s.r[5] = size;
    Call(0x82b7a0b0, m, d, s);
  };
  auto invoke = [&](unsigned resource, unsigned offset) {
    s.r[3] = resource;
    s.ctr = m.ReadU32(m.ReadU32(resource) + offset);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    return Address(s.r[3]);
  };
  if (e == 0x82abfc50) {
    auto row = Address(s.r[4]), resource = Address(s.r[5]);
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    m.WriteU32(resource + 136, m.ReadU32(row + 16));
    m.WriteU32(resource + 144, m.ReadU32(row + 20));
    m.WriteU32(resource + 140, m.ReadU32(row + 12));
    copy(resource + 2468, row + 128, 60);
    copy(resource + 2528, row + 188, 60);
    copy(resource + 2588, row + 248, 2352);
    m.WriteU32(resource + 4876, 0);
    copy(resource + 5108, row + 2600, 48);
    copy(resource + 5156, row + 2648, 5388);
    copy(resource + 10544, row + 8036, 4096);
    copy(resource + 232, row + 12132, 2176);
  } else if (e == 0x82af52f0) {
    auto groups = m.ReadU32(m.ReadU32(owner + 48));
    auto group = m.ReadU32(groups + (((Address(s.r[4]) & 255) == 1) ? 4 : 0));
    m.WriteU32(group + 12676, 0);
  } else if (e == 0x82af5400) {
    auto list = m.ReadU32(owner + 48);
    if (m.ReadU32(list + 4) != 0) {
      auto group = m.ReadU32(m.ReadU32(list) + 4),
           state = m.ReadU32(0x832c9c54 + 44);
      if (!(m.ReadU32(state + 28) & 0x18000)) {
        auto target = m.ReadU32(owner + 32) + 76, source = group + 72;
        copy(target + 157436, source, 8192);
        copy(target + 165628, source + 8192, 4096);
      }
    }
  } else if (e == 0x82af56a8) {
    auto list = m.ReadU32(owner + 20);
    for (unsigned i = 0;
         std::int32_t(i) < std::int32_t(m.ReadU32(m.ReadU32(owner + 20) + 4));
         ++i) {
      auto resource = m.ReadU32(m.ReadU32(list) + 4 * i);
      if (!(m.ReadU32(resource + 124) & 0x10000000))
        continue;
      invoke(resource, 440);
      auto target =
          m.ReadU32(owner + 32) + 124 + 14308 * m.ReadU32(resource + 68);
      if (s.cached_fp_control & 0x8040) {
        s.cached_fp_control &= ~0x8040u;
        d.fp.SetHostFpControl(s.cached_fp_control);
      }
      m.WriteU32(target + 16, m.ReadU32(resource + 136));
      m.WriteU32(target + 20, m.ReadU32(resource + 144));
      m.WriteU32(target + 12, m.ReadU32(resource + 140));
      copy(target + 128, resource + 2468, 60);
      copy(target + 188, resource + 2528, 60);
      copy(target + 248, resource + 2588, 2352);
      copy(target + 12132, resource + 232, 2176);
      copy(target + 2600, resource + 5108, 48);
      copy(target + 2648, resource + 5156, 5388);
      copy(target + 8036, resource + 10544, 4096);
      m.WriteU32(target + 8,
                 (m.ReadU32(target + 8) & 0x7fffffff) |
                     ((~m.ReadU32(resource + 124) << 1) & 0x80000000));
    }
  } else if (e == 0x82af57d8) {
    Call(0x82af56a8, m, d, s);
    s.r[3] = owner;
    Call(0x82af5400, m, d, s);
  } else {
    for (unsigned side : {1u, 0u}) {
      auto group = m.ReadU32(m.ReadU32(m.ReadU32(owner + 48)) + 4 * side),
           count = m.ReadU32(group + 12676);
      m.WriteU32(group + 12688, count);
      m.WriteU32(group + 12684, count);
      m.WriteU32(group + 12680, count);
    }
    auto list = m.ReadU32(owner + 20);
    for (unsigned i = 0;
         std::int32_t(i) < std::int32_t(m.ReadU32(m.ReadU32(owner + 20) + 4));
         ++i) {
      auto resource = m.ReadU32(m.ReadU32(list) + 4 * i);
      auto side = (m.ReadU32(resource + 124) & 0x10000000) ? 1u : 0u;
      auto group = m.ReadU32(m.ReadU32(m.ReadU32(owner + 48)) + 4 * side);
      auto decrement = [&](unsigned offset) {
        m.WriteU32(group + offset, m.ReadU32(group + offset) - 1);
      };
      bool unavailable = invoke(resource, 292) != 0;
      if (!unavailable)
        unavailable = invoke(resource, 380) != 0;
      if (unavailable)
        decrement(12680);
      if (unavailable || (m.ReadU32(resource + 124) & 0x400000)) {
        decrement(12684);
        decrement(12688);
      } else if (!m.ReadU32(resource + 132))
        decrement(12688);
    }
  }
  if (frame) {
    s.r[1] += frame;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
  }
  return true;
}
} // namespace lo::semantic::gpu::battle_roster_persistence61
