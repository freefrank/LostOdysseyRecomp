#include "lo_semantics/battle_resource_creation61.h"
#include "lo_semantics/battle_manager_access61.h"
#include "lo_semantics/battle_formation61.h"
#include "lo_semantics/battle_action_storage61.h"
#include "lo_semantics/battle_resource_growth61.h"
#include "lo_semantics/battle_resource_stats61.h"
#include "lo_semantics/battle_roster_persistence61.h"
#include "lo_semantics/string_storage_context61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::battle_resource_creation61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_resource_creation61::Apply(e, m, d, s) &&
      !battle_formation61::Apply(e, m, d, s) &&
      !battle_action_storage61::Apply(e, m, d, s) &&
      !battle_resource_growth61::Apply(e, m, d, s) &&
      !battle_resource_stats61::Apply(e, m, d, s) &&
      !battle_roster_persistence61::Apply(e, m, d, s) &&
      !string_storage_context61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82af60d8 || e == 0x82af63e0) {
    auto old = Address(s.r[1]), owner = Address(s.r[3]);
    unsigned frame = e == 0x82af60d8 ? 192 : 96,
             first = e == 0x82af60d8 ? 20 : 31;
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= frame;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    if (e == 0x82af60d8) {
      auto resize = [&](unsigned data, unsigned bytes) {
        auto manager = m.ReadU32(0x8330b608);
        if (!manager) {
          Call(0x827c5f38, m, d, s);
          manager = m.ReadU32(0x8330b608);
        }
        s.r[3] = manager;
        s.r[4] = data;
        s.r[5] = bytes;
        s.r[6] = 8;
        s.ctr = m.ReadU32(m.ReadU32(manager) + 8);
        d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        return Address(s.r[3]);
      };
      auto list = m.ReadU32(owner + 48);
      m.WriteU32(list + 4, 0);
      if (m.ReadU32(list + 8)) {
        auto data = m.ReadU32(list);
        m.WriteU32(list + 8, 0);
        if (data)
          m.WriteU32(list, resize(data, 0));
      }
      m.WriteU32(sp + 80, 0);
      m.WriteU32(sp + 84, 0);
      auto objectArgument = recovery_abi::ReadU64(m, sp + 80);
      for (unsigned i = 0; i < 2; ++i) {
        auto type = m.ReadU32(0x832c99f8);
        if (!type) {
          s.r[3] = 0x820205bc;
          Call(0x82a79de0, m, d, s);
          m.WriteU32(0x832c99f8, Address(s.r[3]));
          Call(0x82a795b0, m, d, s);
          type = m.ReadU32(0x832c99f8);
        }
        Call(0x82300100, m, d, s);
        s.r[4] = s.r[3];
        s.r[3] = type;
        s.r[5] = objectArgument;
        s.r[6] = 0;
        s.r[7] = 0;
        s.r[8] = m.ReadU32(0x8330b5f4);
        s.r[9] = 0;
        s.r[10] = 0;
        Call(0x82401a10, m, d, s);
        auto group = Address(s.r[3]);
        Call(0x82400a08, m, d, s);
        list = m.ReadU32(owner + 48);
        auto previous = m.ReadU32(list + 4), count = previous + 1;
        m.WriteU32(list + 4, count);
        if (std::int32_t(count) > std::int32_t(m.ReadU32(list + 8))) {
          auto data = m.ReadU32(list),
               capacity = count + unsigned(std::int32_t(count * 3) / 8) + 32;
          m.WriteU32(list + 8, capacity);
          if (data || capacity)
            m.WriteU32(list, resize(data, capacity * 4));
        }
        auto slot = m.ReadU32(list) + 4 * previous;
        if (slot)
          m.WriteU32(slot, group);
      }
      auto group = m.ReadU32(m.ReadU32(m.ReadU32(owner + 48)) + 4);
      s.r[3] = m.ReadU32(0x83291dc0);
      s.r[4] = m.ReadU32(owner + 32) + 76;
      s.r[5] = group + 72;
      Call(0x82abfdd8, m, d, s);
    } else {
      Call(0x82af60d8, m, d, s);
      auto group = m.ReadU32(m.ReadU32(m.ReadU32(owner + 48)) + 4);
      m.WriteU32(group + 12676, 0);
      s.r[3] = owner;
      Call(0x82af6290, m, d, s);
      Call(0x82380a18, m, d, s);
      (void)battle_manager_access61::Apply(0x82389aa0, m, d, s);
      auto profile = Address(s.r[3]);
      s.r[3] = owner;
      s.r[4] = 1;
      s.r[5] = m.ReadU8(profile + 56);
      Call(0x82af5ba8, m, d, s);
    }
    s.r[1] += frame;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82af6290 || e == 0x82af6448) {
    auto old = Address(s.r[1]), owner = Address(s.r[3]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 25; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    recovery_abi::WriteU64(m, old - 72, s.fpr_bits[31]);
    s.r[1] -= 160;
    m.WriteU32(Address(s.r[1]), old);
    auto clearCoordinates = [&]() {
      if (s.cached_fp_control & 0x8040) {
        s.cached_fp_control &= ~0x8040u;
        d.fp.SetHostFpControl(s.cached_fp_control);
      }
      auto zero = std::bit_cast<std::uint64_t>(
          double(std::bit_cast<float>(m.ReadU32(0x82000e50))));
      for (unsigned i = 1; i < 5; ++i)
        s.fpr_bits[i] = zero;
    };
    if (e == 0x82af6290) {
      auto list = m.ReadU32(owner + 20);
      m.WriteU32(list + 4, 0);
      if (m.ReadU32(list + 8)) {
        auto data = m.ReadU32(list);
        m.WriteU32(list + 8, 0);
        if (data) {
          auto manager = m.ReadU32(0x8330b608);
          if (!manager) {
            Call(0x827c5f38, m, d, s);
            manager = m.ReadU32(0x8330b608);
          }
          s.r[3] = manager;
          s.r[4] = data;
          s.r[5] = 0;
          s.r[6] = 8;
          s.ctr = m.ReadU32(m.ReadU32(manager) + 8);
          d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
          m.WriteU32(list, Address(s.r[3]));
        }
      }
      auto profile = m.ReadU32(owner + 32),
           group = m.ReadU32(m.ReadU32(m.ReadU32(owner + 48)) + 4);
      auto formation = profile + (m.ReadU32(profile + 204704) ? 204684 : 76);
      for (unsigned i = 0; i < 5; ++i)
        m.WriteU32(group + 12656 + 4 * i, 0xffffffff);
      unsigned count = 0;
      for (unsigned slot = 0; slot < 5; ++slot) {
        auto index = m.ReadU32(formation + 28 + 4 * slot);
        if (index == 0xffffffff)
          continue;
        auto row = formation + 14308 * index;
        clearCoordinates();
        s.r[3] = owner;
        s.r[4] = m.ReadU32(row + 48);
        s.r[5] = (~m.ReadU32(row + 56) >> 31) & 1;
        s.r[6] = slot;
        s.r[7] = m.ReadU32(row + 52);
        Call(0x82af5d18, m, d, s);
        m.WriteU32(group + 12656 + 4 * count,
                   m.ReadU32(formation + 28 + 4 * slot));
        ++count;
      }
      m.WriteU32(group + 12676, count);
    } else {
      auto group = m.ReadU32(m.ReadU32(m.ReadU32(owner + 48)));
      Call(0x82380a18, m, d, s);
      (void)battle_manager_access61::Apply(0x82389aa0, m, d, s);
      auto profile = Address(s.r[3]);
      for (unsigned i = 0;
           std::int32_t(i) < std::int32_t(m.ReadU32(profile + 152)); ++i) {
        auto row = m.ReadU32(profile + 148) + 32 * i;
        if (!m.ReadU8(row + 13))
          continue;
        clearCoordinates();
        s.r[3] = owner;
        s.r[4] = m.ReadU32(row + 8);
        s.r[5] = m.ReadU8(row + 12) == 1;
        s.r[6] = m.ReadU8(row);
        s.r[7] = m.ReadU32(row + 4);
        Call(0x82af5d18, m, d, s);
        m.WriteU32(Address(s.r[3]) + 76312, m.ReadU8(row + 29));
        m.WriteU32(group + 12676, m.ReadU32(group + 12676) + 1);
      }
      s.r[3] = 0x832ca0e0;
      s.r[4] = 0;
      s.r[5] = 0;
      s.r[6] = m.ReadU8(profile + 132);
      Call(0x82aac1e0, m, d, s);
    }
    s.r[1] += 160;
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - 72);
    for (unsigned i = 25; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e != 0x82ab3b08 && e != 0x82af5d18)
    return false;
  unsigned frame = e == 0x82ab3b08 ? 112 : 224,
           first = e == 0x82ab3b08 ? 30 : 22;
  auto old = Address(s.r[1]), owner = Address(s.r[3]), group = Address(s.r[4]),
       kind = Address(s.r[5]), id = Address(s.r[6]), tag = Address(s.r[7]);
  double coordinates[4];
  for (unsigned i = 0; i < 4; ++i)
    coordinates[i] = std::bit_cast<double>(s.fpr_bits[i + 1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  if (e == 0x82af5d18)
    for (unsigned i = 28; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 88 - 8 * (32 - i), s.fpr_bits[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  auto fp = [&]() {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
  };
  if (e == 0x82ab3b08) {
    for (unsigned off : {64u, 68u, 72u})
      m.WriteU32(owner + off, 0);
    s.r[3] = owner + 76;
    s.r[4] = 0x821a83d0;
    Call(0x8229f5e0, m, d, s);
    auto flags = (m.ReadU32(owner + 124) & 0x3760041f) | 0x80000000;
    for (unsigned off : {14680u, 76352u, 76376u, 76380u, 76312u})
      m.WriteU32(owner + off, 0);
    fp();
    for (unsigned off : {108u, 112u, 116u})
      m.WriteU32(owner + off, m.ReadU32(0x82000e50));
    m.WriteU32(owner + 120, m.ReadU32(0x82007784));
    for (unsigned off : {156u, 148u, 14652u})
      m.WriteU32(owner + off, 0xffffffff);
    m.WriteU32(owner + 124, flags);
    s.r[3] = owner + 14656;
    s.r[4] = 0;
    Call(0x82a9b698, m, d, s);
    s.r[3] = owner;
    s.r[4] = 0;
    Call(0x82ab2d88, m, d, s);
    m.WriteU32(owner + 124, m.ReadU32(owner + 124) & ~0x20000u);
    m.WriteU32(owner + 76316, 0);
    for (unsigned i = 0; i < 32; ++i)
      m.WriteU32(owner + 76124 + 4 * i, 0);
    m.WriteU32(owner + 60, 0);
  } else {
    fp();
    s.r[3] = sp + 88;
    s.r[4] = 0x820c40a0;
    Call(0x822d02f8, m, d, s);
    auto name = [&]() {
      return m.ReadU32(sp + 92) ? m.ReadU32(sp + 88) : 0x821a83d0u;
    };
    auto registry = [&]() {
      auto p = m.ReadU32(0x83315f9c);
      if (!p) {
        s.r[3] = 0x8218c210;
        Call(0x82410b90, m, d, s);
        m.WriteU32(0x83315f9c, Address(s.r[3]));
        Call(0x82410c48, m, d, s);
        p = m.ReadU32(0x83315f9c);
      }
      return p;
    };
    s.r[3] = registry();
    s.r[4] = std::uint64_t(-1);
    s.r[5] = name();
    s.r[6] = 0;
    Call(0x8229c7b8, m, d, s);
    if (!Address(s.r[3])) {
      s.r[3] = registry();
      s.r[4] = 0;
      s.r[5] = name();
      s.r[6] = 0;
      s.r[7] = 0;
      s.r[8] = 0;
      s.r[9] = 1;
      Call(0x823ffdd8, m, d, s);
    }
    unsigned result = 0;
    if (Address(s.r[3])) {
      m.WriteU32(sp + 80, 0);
      m.WriteU32(sp + 84, 0);
      auto type = m.ReadU32(0x832c99f0);
      if (!type) {
        s.r[3] = 0x820205bc;
        Call(0x82a79ee8, m, d, s);
        m.WriteU32(0x832c99f0, Address(s.r[3]));
        Call(0x82a79c28, m, d, s);
        type = m.ReadU32(0x832c99f0);
      }
      Call(0x82300100, m, d, s);
      s.r[4] = s.r[3];
      s.r[3] = type;
      s.r[5] = recovery_abi::ReadU64(m, sp + 80);
      s.r[6] = 0;
      s.r[7] = 0;
      s.r[8] = m.ReadU32(0x8330b5f4);
      s.r[9] = 0;
      s.r[10] = 0;
      Call(0x82401a10, m, d, s);
      auto resource = Address(s.r[3]);
      Call(0x82400a08, m, d, s);
      s.r[3] = resource;
      Call(0x82ab3b08, m, d, s);
      m.WriteU32(resource + 68, group);
      m.WriteU32(resource + 72, tag);
      m.WriteU32(resource + 124, (m.ReadU32(resource + 124) & ~0x40000000u) |
                                     ((kind & 1) << 30));
      auto growth = [&](unsigned entry) {
        s.r[3] = m.ReadU32(0x83291dc0);
        s.r[4] = resource;
        Call(entry, m, d, s);
      };
      auto names = m.ReadU32(0x83264978 + 116) + 84 * group;
      if (std::int32_t(id) < 11) {
        m.WriteU32(resource + 64, id);
        s.r[3] = resource + 76;
        s.r[4] = names + 12;
        Call(0x822b3f50, m, d, s);
        m.WriteU32(resource + 124, m.ReadU32(resource + 124) | 0x18000000);
        auto row = m.ReadU32(owner + 32) + 124 + 14308 * group;
        growth(0x82abfe38);
        s.r[3] = m.ReadU32(0x83291dc0);
        s.r[4] = row;
        s.r[5] = resource;
        Call(0x82abfc50, m, d, s);
        growth(0x82ac0588);
        growth(0x82ac25e8);
        auto manager = m.ReadU32(0x83291dc0);
        m.WriteU32(manager + 4, m.ReadU32(resource + 5108));
        for (unsigned i = 0; i < 5; ++i)
          m.WriteU32(manager + 12 + 4 * i, m.ReadU32(resource + 5116 + 4 * i));
      } else {
        if (std::int32_t(id) < 0) {
          auto next = m.ReadU32(owner + 4);
          m.WriteU32(resource + 64, next);
          m.WriteU32(owner + 4, next + 1);
        } else
          m.WriteU32(resource + 64, id);
        s.r[3] = resource + 76;
        s.r[4] = names + 24;
        Call(0x822b3f50, m, d, s);
        m.WriteU32(resource + 124, m.ReadU32(resource + 124) & ~0x18000000u);
        growth(0x82abfe38);
        s.r[5] = 0;
        growth(0x82ac3820);
      }
      growth(0x82ac0888);
      growth(0x82ac2468);
      fp();
      for (unsigned i = 0; i < 3; ++i)
        m.WriteU32(resource + 76324 + 4 * i,
                   std::bit_cast<unsigned>(float(coordinates[i])));
      m.WriteU32(resource + 76344,
                 std::bit_cast<unsigned>(float(
                     coordinates[3] *
                     double(std::bit_cast<float>(m.ReadU32(0x82000bb8))))));
      s.r[5] = (m.ReadU32(resource + 124) >> 28) & 1;
      growth(0x82ac0620);
      growth(0x82abfe90);
      s.r[3] = 4;
      s.r[4] = m.ReadU32(owner + 20);
      Call(0x828080a8, m, d, s);
      if (Address(s.r[3]))
        m.WriteU32(Address(s.r[3]), resource);
      auto list = m.ReadU32(owner + 20);
      result = m.ReadU32(m.ReadU32(list) + 4 * (m.ReadU32(list + 4) - 1));
    }
    s.r[3] = sp + 88;
    Call(0x82298938, m, d, s);
    s.r[3] = result;
  }
  s.r[1] += frame;
  if (e == 0x82af5d18)
    for (unsigned i = 28; i < 32; ++i)
      s.fpr_bits[i] = recovery_abi::ReadU64(m, old - 88 - 8 * (32 - i));
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_resource_creation61
