#include "lo_semantics/battle_property_mutation61.h"
#include "lo_semantics/battle_manager_access61.h"
#include "lo_semantics/battle_action_adjustments61.h"
#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_action_adjustments61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_manager_access61::Apply(e, m, d, s) &&
      !battle_property_mutation61::Apply(e, m, d, s) &&
      !battle_action_adjustments61::Apply(e, m, d, s) &&
      !battle_action_readiness61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
bool Property(unsigned resource, unsigned id, GuestMemory &m, Dependencies d,
              Registers &s) {
  s.r[3] = resource;
  s.r[4] = id;
  Call(0x8238e368, m, d, s);
  return (Address(s.r[3]) & 255) != 0;
}
unsigned Index(GuestMemory &m, unsigned id) {
  auto n = std::int32_t(id), q = n / 32;
  auto mask = m.ReadU32(0x8321343c + 8 * unsigned(n - q * 32));
  unsigned i = 0;
  for (; i < 31; ++i)
    if (mask & (1u << i))
      break;
  return i;
}
void Scale(GuestMemory &m, Registers &s) {
  if (m.ReadU32(0x832cb778) == 242)
    return;
  auto resource = Address(s.r[4]), groupPtr = Address(s.r[5]),
       valuePtr = Address(s.r[6]), rate = Address(s.r[7]),
       live = Address(s.r[8]) & 255;
  if (live) {
    groupPtr = resource + 88;
    valuePtr = resource + 92;
  }
  auto oldGroup = m.ReadU32(groupPtr),
       total = oldGroup * 25 + m.ReadU32(valuePtr);
  auto scaled = std::int32_t(total * rate) / 100;
  auto group = scaled / 25;
  m.WriteU32(groupPtr, unsigned(group));
  m.WriteU32(valuePtr, unsigned(scaled - group * 25));
  if (live && oldGroup == 1 && group == 0) {
    m.WriteU32(resource + 60, 0);
    m.WriteU32(resource + 100, m.ReadU32(resource + 100) & ~0x40000000u);
  }
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82ace208 || e == 0x82ace260) {
    auto resource = Address(s.r[4]);
    s.r[3] = 0;
    if (m.ReadU32(resource + 100) & 0x80000000u) {
      auto kind = std::int32_t(m.ReadU32(m.ReadU32(resource + 14656)));
      s.r[3] = e == 0x82ace208 ? (kind == 2 || (kind >= 6 && kind <= 9))
                               : (kind == 3 || kind == 10);
    }
    return true;
  }
  if (e == 0x82ac84b8) {
    auto mask = Address(s.r[3]);
    unsigned i = 0;
    for (; i < 31; ++i)
      if (mask & (1u << i))
        break;
    s.r[3] = i;
    return true;
  }
  if (e == 0x82ac84e8) {
    s.r[3] = Index(m, Address(s.r[3]));
    return true;
  }
  if (e == 0x82acd680) {
    Scale(m, s);
    return true;
  }
  unsigned frame = 160, first = 25;
  if (e == 0x82acd770) {
    frame = 176;
    first = 22;
  } else if (e == 0x82acd998) {
    first = 24;
  } else if (e != 0x82acdb50 && e != 0x82acdda0 && e != 0x82acdaa0 &&
             e != 0x82acdc40 && e != 0x82acdcf0)
    return false;
  auto owner = Address(s.r[3]), resource = Address(s.r[4]),
       groupPtr = Address(s.r[5]), valuePtr = Address(s.r[6]),
       mode = Address(s.r[7]), variant = Address(s.r[8]) & 255,
       old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  m.WriteU32(sp + 80, 0x8204a1d8);
  unsigned rate = 0;
  if (e == 0x82acd998) {
    if (!variant && Property(resource, 244, m, d, s)) {
      auto id = m.ReadU32(resource + 4 * (Index(m, 244) + 567));
      Call(0x82380a18, m, d, s);
      Call(0x82389b78, m, d, s);
      s.r[4] = id;
      Call(0x8238e308, m, d, s);
      auto peer = Address(s.r[3]);
      s.ctr = m.ReadU32(m.ReadU32(peer) + 300);
      d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
      if (Address(s.r[3])) {
        s.r[3] = resource;
        s.r[4] = 244;
        Call(0x82ac9000, m, d, s);
        s.r[3] = peer;
        s.r[4] = 243;
        Call(0x82ac9000, m, d, s);
      } else
        rate = 50;
    }
  } else if (e == 0x82acdaa0 || e == 0x82acdc40 || e == 0x82acdcf0) {
    auto id = e == 0x82acdaa0 ? 192u : e == 0x82acdc40 ? 194u : 162u;
    if (!variant && Property(resource, id, m, d, s))
      rate = e == 0x82acdaa0 ? 130 : e == 0x82acdc40 ? 150 : 50;
  } else if (e == 0x82acdda0) {
    if (Property(resource, 2, m, d, s))
      rate = 200;
  } else if (!variant && e == 0x82acdb50) {
    if (Property(resource, 2, m, d, s))
      rate = 200;
    else if (Property(resource, 194, m, d, s))
      rate = 150;
    else if (Property(resource, 162, m, d, s))
      rate = 50;
  } else if (!variant) {
    rate = 100;
    if (Property(resource, 160, m, d, s))
      rate = 70;
    if (Property(resource, 244, m, d, s)) {
      auto id = m.ReadU32(resource + 4 * (Index(m, 244) + 567));
      Call(0x82380a18, m, d, s);
      Call(0x82389b78, m, d, s);
      s.r[4] = id;
      Call(0x8238e308, m, d, s);
      auto peer = Address(s.r[3]);
      s.ctr = m.ReadU32(m.ReadU32(peer) + 300);
      d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
      if (!Address(s.r[3])) {
        if (std::int32_t(rate) > 50)
          rate = 50;
      } else {
        s.r[3] = resource;
        s.r[4] = 244;
        Call(0x82ac9000, m, d, s);
        s.r[3] = peer;
        s.r[4] = 243;
        Call(0x82ac9000, m, d, s);
      }
    }
    if (Property(resource, 235, m, d, s)) {
      auto limit = m.ReadU32(resource + 4 * (Index(m, 235) + 399));
      if (std::int32_t(rate) > std::int32_t(limit))
        rate = limit;
    }
    if (Property(resource, 252, m, d, s)) {
      auto limit = m.ReadU32(resource + 4 * (Index(m, 252) + 535));
      if (std::int32_t(rate) > std::int32_t(limit))
        rate = limit;
    }
    unsigned boost = 0;
    if (!(mode & 255) && Property(resource, 2, m, d, s))
      boost = 200;
    if (Property(resource, 192, m, d, s) && std::int32_t(boost) < 130)
      boost = 130;
    if (boost)
      rate = rate == 100 ? boost : unsigned(std::int32_t(rate + boost) / 2);
  }
  if ((e == 0x82acd770 && !variant) ? rate != 100 : rate != 0) {
    s.r[3] = owner;
    s.r[4] = resource;
    s.r[5] = groupPtr;
    s.r[6] = valuePtr;
    s.r[7] = rate;
    s.r[8] = mode;
    Scale(m, s);
  }
  m.WriteU32(sp + 80, 0x8204a1d8);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_action_adjustments61
