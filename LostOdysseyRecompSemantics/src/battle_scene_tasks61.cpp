#include "lo_semantics/battle_scene_tasks61.h"
#include "lo_semantics/battle_manager_access61.h"
#include "lo_semantics/string_storage_context61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_scene_tasks61 {
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  using recovery_abi::Address;
  if (e != 0x82b035e0 && e != 0x82b04c50 && e != 0x82b1a560)
    return false;
  auto old = Address(s.r[1]), owner = Address(s.r[3]),
       argument = Address(s.r[4]);
  unsigned frame = e == 0x82b04c50   ? 192
                   : e == 0x82b035e0 ? 112
                                     : 96,
           first = e == 0x82b04c50   ? 20
                   : e == 0x82b035e0 ? 29
                                     : 31;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  auto call = [&](unsigned a) {
    if (a == 0x82389aa0 && battle_manager_access61::Apply(a, m, d, s))
      return;
    if (!battle_scene_tasks61::Apply(a, m, d, s) &&
        !string_storage_context61::Apply(a, m, d, s))
      d.guest.CallDirect(a, m, s);
  };
  auto virt = [&](unsigned p, unsigned off) {
    auto a = m.ReadU32(m.ReadU32(p) + off);
    s.r[3] = p;
    s.ctr = a;
    d.guest.CallIndirect(a & ~3u, m, s);
  };
  auto grow = [&](unsigned header, unsigned extra, unsigned size) {
    auto before = m.ReadU32(header + 4), count = before + extra;
    m.WriteU32(header + 4, count);
    if (std::int32_t(count) > std::int32_t(m.ReadU32(header + 8))) {
      auto capacity =
          count + unsigned(std::int32_t(count + count * 2) / 8) + 32;
      m.WriteU32(header + 8, capacity);
      s.r[3] = header;
      s.r[4] = size;
      s.r[5] = 8;
      call(0x8229f678);
    }
    return before;
  };
  if (e == 0x82b1a560) {
    s.r[3] = grow(owner, argument, 4);
  } else if (e == 0x82b035e0) {
    auto id = m.ReadU32(owner + 116);
    if (std::int32_t(id) >= 0) {
      s.r[4] = id;
      virt(0x832cb670, 36);
    }
    m.WriteU32(owner + 116, 0xffffffff);
    id = m.ReadU32(owner + 120);
    if (std::int32_t(id) >= 0 && id != m.ReadU32(0x832cb6f0)) {
      s.r[3] = 0x832cb68c;
      s.r[4] = id;
      call(0x82b08410);
    }
    m.WriteU32(owner + 120, 0xffffffff);
    for (unsigned off : {124u, 136u}) {
      while (std::int32_t(m.ReadU32(owner + off + 4)) > 0) {
        auto row = m.ReadU32(owner + off), task = m.ReadU32(row + 4);
        if (std::int32_t(task) >= 0) {
          s.r[4] = task;
          if (off == 124)
            virt(owner, 48);
          else {
            s.r[3] = 0x832cc0fc;
            s.r[5] = 0;
            s.r[6] = m.ReadU32(row);
            call(0x82b1a2d0);
          }
        }
        s.r[3] = owner + off;
        s.r[4] = 0;
        s.r[5] = 1;
        s.r[6] = 8;
        s.r[7] = 8;
        call(0x82298af8);
      }
    }
    m.WriteU32(owner + 148, 0);
  } else {
    s.r[3] = owner;
    call(0x82b035e0);
    call(0x82380a18);
    call(0x82389aa0);
    auto profile = Address(s.r[3]);
    auto rank = std::int8_t(m.ReadU8(profile + 133));
    if (rank > 0 &&
        rank < std::int32_t(m.ReadU32(m.ReadU32(0x832ca0d0) + 452))) {
      auto row = m.ReadU32(m.ReadU32(0x832ca0d0) + 448) + 60 * unsigned(rank);
      auto name = [&](unsigned p) {
        return m.ReadU32(p + 4) ? m.ReadU32(p) : 0x821a83d0;
      };
      auto nonempty = [&](unsigned p) {
        s.r[3] = p;
        s.r[4] = 0x821a83d0;
        call(0x822971e0);
        return Address(s.r[3]) != 0;
      };
      if (nonempty(name(row + 12))) {
        s.r[4] = 0;
        s.r[5] = 999;
        s.r[6] = name(row + 12);
        s.r[7] = 11;
        virt(0x832cb670, 16);
        m.WriteU32(owner + 116, Address(s.r[3]));
      }
      unsigned secondary = 0;
      if (nonempty(name(row)))
        secondary = name(row);
      else if (nonempty(name(profile + 136)))
        secondary = name(profile + 136);
      if (secondary) {
        s.r[3] = 0x832cb68c;
        s.r[4] = secondary;
        s.r[5] = 0;
        call(0x82b08318);
        m.WriteU32(owner + 120, Address(s.r[3]));
      }
      auto task = m.ReadU32(owner + 120);
      if (std::int32_t(task) >= 0 && task != m.ReadU32(0x832cb6f0))
        m.WriteU32(0x832cb6f0, task);
      for (unsigned kind = 0; kind < 2; ++kind) {
        auto header = owner + (kind ? 136 : 124),
             listField = row + (kind ? 36 : 24);
        for (unsigned i = 0;
             std::int32_t(i) < std::int32_t(m.ReadU32(listField + 4)); ++i) {
          auto index = grow(header, 1, 8), dest = m.ReadU32(header) + 8 * index;
          for (unsigned j = 0; j < 8; ++j)
            m.WriteU8(dest + j, 0);
          auto def = m.ReadU32(listField) + 16 * i, key = m.ReadU32(def);
          m.WriteU32(m.ReadU32(header) + 8 * i, key);
          auto text = m.ReadU32(def + 8) ? m.ReadU32(def + 4) : 0x821a83d0;
          auto resultSlot = m.ReadU32(header) + 8 * i + 4;
          if (!kind) {
            s.r[4] = 8;
            s.r[5] = 0;
            s.r[6] = key;
            s.r[7] = text;
            s.r[8] = 11;
            virt(owner, 16);
          } else {
            s.r[3] = 0x832cc0fc;
            s.r[4] = 0;
            s.r[5] = key;
            s.r[6] = text;
            s.r[7] = 2;
            s.r[8] = 11;
            s.r[9] = 0;
            call(0x82b1a7f0);
          }
          m.WriteU32(resultSlot, Address(s.r[3]));
        }
      }
      m.WriteU32(owner + 148, row + 48);
    }
  }
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_scene_tasks61
