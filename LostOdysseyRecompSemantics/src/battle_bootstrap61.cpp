#include "lo_semantics/battle_bootstrap61.h"
#include "lo_semantics/battle_script_loading61.h"
#include "lo_semantics/battle_manager_access61.h"
#include "lo_semantics/battle_resource_creation61.h"
#include "lo_semantics/battle_roster_persistence61.h"
#include "lo_semantics/battle_resource_stats61.h"
#include "lo_semantics/battle_group_gauge61.h"
#include <utility>
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_bootstrap61 {
namespace {
using recovery_abi::Address;
void Call(unsigned entry, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_bootstrap61::Apply(entry, m, d, s) &&
      !battle_script_loading61::Apply(entry, m, d, s) &&
      !battle_resource_creation61::Apply(entry, m, d, s) &&
      !battle_roster_persistence61::Apply(entry, m, d, s) &&
      !battle_resource_stats61::Apply(entry, m, d, s) &&
      !battle_group_gauge61::Apply(entry, m, d, s) &&
      !(entry == 0x8229dfd8 && battle_manager_access61::Apply(entry, m, d, s)))
    d.guest.CallDirect(entry, m, s);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned frame = 0, first = 32;
  switch (e) {
  case 0x82ad1378:
    frame = 176;
    first = 14;
    break;
  case 0x82ad20c0:
    frame = 160;
    first = 24;
    break;
  case 0x82b08a30:
  case 0x82acd398:
  case 0x82ac1b90:
    break;
  case 0x82ad1c88:
  case 0x822a7c58:
    frame = 112;
    first = 30;
    break;
  case 0x82aab200:
    frame = 128;
    first = 27;
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
  auto fp = [&]() {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
  };
  if (e == 0x82ad1378) {
    m.WriteU32(old + 20, owner);
    m.WriteU32(owner, 0x820c0ba8);
    m.WriteU32(0x832ca0d8, owner);
    m.WriteU32(owner, 0x820c0e28);
    // The constructor leaves slot 856 untouched. These are callable code
    // addresses, not copied guest data or translated instruction bodies.
    for (unsigned offset = 216; offset < 888; offset += 4)
      if (offset != 856)
        m.WriteU32(owner + offset, 0x82acf878);
    for (unsigned offset :
         {508u, 568u, 584u, 580u, 540u, 560u, 564u, 612u, 616u, 620u,
          624u, 628u, 632u, 636u, 640u, 644u, 648u, 660u, 664u, 668u,
          672u, 676u, 680u, 684u, 688u, 692u, 696u, 704u})
      m.WriteU32(owner + offset, 0x82b08b20);
    for (unsigned offset : {500u, 516u, 588u, 592u, 552u, 572u, 700u})
      m.WriteU32(owner + offset, 0x82b08b30);
    for (unsigned offset : {596u, 808u, 816u, 824u, 880u})
      m.WriteU32(owner + offset, 0x822d3068);
    constexpr std::pair<unsigned, unsigned> callbacks[]{
        {220u, 0x82b0a568u}, {224u, 0x82b0ea88u}, {228u, 0x82b0b630u},
        {232u, 0x82b0f920u}, {236u, 0x82b0fbd0u}, {240u, 0x82b0f3e0u},
        {244u, 0x82b0fff0u}, {252u, 0x82b0e798u}, {256u, 0x82b0ec18u},
        {260u, 0x82b0b178u}, {264u, 0x82b11660u}, {268u, 0x82b0ef68u},
        {272u, 0x82b0f7d0u}, {276u, 0x82b0ee40u}, {280u, 0x82b0c880u},
        {284u, 0x82b0c9e0u}, {288u, 0x82b12870u}, {292u, 0x82b0a928u},
        {296u, 0x82b0aa70u}, {300u, 0x82b0d7f0u}, {304u, 0x82b0c4e8u},
        {308u, 0x82b0a698u}, {312u, 0x82b0a7a0u}, {316u, 0x82b11758u},
        {320u, 0x82b11878u}, {324u, 0x82b0ad38u}, {328u, 0x82b0af88u},
        {332u, 0x82b0fcf0u}, {336u, 0x82b0d378u}, {340u, 0x82b0d418u},
        {344u, 0x82b11230u}, {348u, 0x82b0db98u}, {352u, 0x82b10368u},
        {356u, 0x82b0dd20u}, {360u, 0x82b0ddf8u}, {364u, 0x82b0deb0u},
        {368u, 0x82b104a8u}, {372u, 0x82b0bfd0u}, {376u, 0x82b10548u},
        {380u, 0x82b12d08u}, {384u, 0x82b10aa8u}, {388u, 0x82b13220u},
        {392u, 0x82b10dd8u}, {396u, 0x82b10e98u}, {400u, 0x82b106b8u},
        {404u, 0x82b12a98u}, {408u, 0x82b110b8u}, {412u, 0x82b0fdb0u},
        {416u, 0x82b0e300u}, {420u, 0x82b11a20u}, {424u, 0x82b0ba98u},
        {504u, 0x82b0eb68u}, {512u, 0x82b0fb08u}, {520u, 0x82b0f6a8u},
        {524u, 0x82b102d8u}, {532u, 0x82b0e9d0u}, {536u, 0x82b0ed68u},
        {544u, 0x82b11690u}, {548u, 0x82b0f310u}, {556u, 0x82b0eb68u},
        {576u, 0x82b0ac68u}, {600u, 0x82b08ab8u}, {604u, 0x82b08ab8u},
        {608u, 0x82b0b0f0u}, {652u, 0x82b0c430u}, {656u, 0x82b10618u},
        {780u, 0x82b0cd88u}, {784u, 0x82b0ce98u}, {788u, 0x82b0cf20u},
        {792u, 0x82b0cf78u}, {796u, 0x82b0cfd0u}, {800u, 0x82b0d028u},
        {804u, 0x82b0d088u}, {812u, 0x82b0d148u}, {820u, 0x82b0d1a0u},
        {828u, 0x82b0d210u}, {832u, 0x82b0d2c8u}, {864u, 0x82b11230u},
        {868u, 0x82b11248u}, {872u, 0x82b13380u}, {876u, 0x82b11350u},
        {884u, 0x82b114b8u}};
    for (auto [offset, target] : callbacks)
      m.WriteU32(owner + offset, target);
    m.WriteU32(Address(s.r[1]) + 16, 0x82ad0000);
    m.WriteU32(Address(s.r[1]) + 20, 0x82ad0000);
  } else if (e == 0x82ad20c0) {
    auto allocate = [&](unsigned bytes) {
      auto manager = m.ReadU32(0x8330b608);
      if (!manager) {
        Call(0x827c5f38, m, d, s);
        manager = m.ReadU32(0x8330b608);
      }
      s.r[3] = manager;
      s.r[4] = bytes;
      s.r[5] = 8;
      s.ctr = m.ReadU32(m.ReadU32(manager) + 4);
      d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
      auto result = Address(s.r[3]);
      m.WriteU32(Address(s.r[1]) + 80, result);
      return result;
    };
    struct Descriptor {
      unsigned bytes, baseVtable, global, vtable, constructor;
    };
    constexpr Descriptor managers[]{
        {1216, 0x8204a9d0, 0x83291dc0, 0x8204a9f4, 0},
        {40, 0x820c0b98, 0x832cb790, 0x820c0d28, 0},
        {28, 0x820c0b9c, 0x832cb798, 0x820c0d68, 0},
        {4, 0x82056158, 0x832c09c4, 0x82056200, 0},
        {32, 0x820c0ba0, 0x832cb78c, 0x820c0da8, 0},
        {124, 0x820c0ba4, 0x832ca0cc, 0x820c0de8, 0},
        {888, 0, 0, 0, 0x82ad1378},
        {20, 0, 0, 0, 0x82ad1c88},
        {52, 0x820c0bb0, 0x832aeb00, 0x820c0f30, 0},
        {24, 0x820c0bb4, 0x832cb784, 0x820c0f70, 0}};
    for (auto entry : managers) {
      auto object = allocate(entry.bytes);
      if (!object)
        continue;
      if (entry.constructor) {
        Call(entry.constructor, m, d, s);
        continue;
      }
      m.WriteU32(object, entry.baseVtable);
      m.WriteU32(entry.global, object);
      if (entry.bytes == 28) {
        m.WriteU32(object + 4, 0x832c9c54);
        m.WriteU32(object + 20, 0xffffffff);
      }
      if (entry.bytes == 24) {
        m.WriteU32(object + 4, 1);
        m.WriteU32(object + 8, 0);
      }
      m.WriteU32(object, entry.vtable);
    }
    s.r[3] = owner;
    Call(0x82aab200, m, d, s);
    s.r[3] = owner;
    Call(0x82af63e0, m, d, s);
    m.WriteU8(owner + 212, 0);
    auto initializeGauge = [&](unsigned mode) {
      auto gauge = m.ReadU32(0x832aeb00);
      m.WriteU8(gauge + 24 * (mode + 1), 0);
      for (unsigned entry :
           {0x82ac7b08u, 0x82ac7fc8u, 0x82ac6e60u, 0x82ac6f08u}) {
        s.r[3] = gauge;
        s.r[4] = mode;
        if (entry == 0x82ac7b08)
          s.r[5] = 1;
        Call(entry, m, d, s);
      }
    };
    initializeGauge(1);
    s.r[3] = owner;
    Call(0x82af6448, m, d, s);
    initializeGauge(0);
    Call(0x82380a18, m, d, s);
    Call(0x82389b78, m, d, s);
    Call(0x82af5810, m, d, s);
    m.WriteU16(owner + 148, 0);
    m.WriteU32(owner + 156, 0);
    m.WriteU32(owner + 148, m.ReadU32(owner + 148) & 0xffff0007);
    s.r[3] = m.ReadU32(0x83291dc0);
    Call(0x82ac1b90, m, d, s);
    s.r[3] = m.ReadU32(0x83291dc0);
    Call(0x82ac3118, m, d, s);
    s.r[3] = 0x832c9c54;
    Call(0x82a9f0a0, m, d, s);
    s.r[3] = 0x832c9c54;
    Call(0x82a9f028, m, d, s);
    s.r[3] = 1;
    m.WriteU32(owner + 164, 1);
  } else if (e == 0x82acd398) {
    for (unsigned off : {12u, 8u, 4u, 16u})
      m.WriteU32(owner + off, 0);
  } else if (e == 0x82b08a30) {
    fp();
    auto zero = m.ReadU32(0x82000e50);
    m.WriteU32(owner + 4, zero);
    m.WriteU8(owner + 24, 1);
    for (unsigned off : {8u, 12u, 16u})
      m.WriteU32(owner + off, zero);
    m.WriteU32(owner + 20, m.ReadU32(0x82007784));
  } else if (e == 0x82ad1c88) {
    m.WriteU32(old + 20, owner);
    m.WriteU32(owner, 0x820c0bac);
    m.WriteU32(0x8324570c, owner);
    m.WriteU32(owner, 0x820c0eb0);
    Call(0x82acd398, m, d, s);
    s.r[3] = owner;
  } else if (e == 0x822a7c58) {
    unsigned result = 0;
    if (owner) {
      auto type = m.ReadU32(0x83263198);
      if (!type) {
        s.r[3] = 0x821c4bdc;
        Call(0x82829368, m, d, s);
        m.WriteU32(0x83263198, Address(s.r[3]));
        Call(0x82820f70, m, d, s);
        type = m.ReadU32(0x83263198);
      }
      auto current = m.ReadU32(owner + 52);
      while (current && current != type)
        current = m.ReadU32(current + 60);
      if (current == type)
        result = owner;
    }
    s.r[3] = result;
  } else if (e == 0x82aab200) {
    m.WriteU32(owner + 144, 0);
    m.WriteU32(owner + 20, owner + 8);
    m.WriteU32(owner + 48, owner + 36);
    m.WriteU32(owner + 56, 0);
    s.r[3] = owner + 76;
    Call(0x82b08a30, m, d, s);
    s.r[3] = owner + 104;
    Call(0x82b08a30, m, d, s);
    for (auto header : {m.ReadU32(owner + 20), owner + 132}) {
      m.WriteU32(header + 4, 0);
      if (!m.ReadU32(header + 8))
        continue;
      auto data = m.ReadU32(header);
      m.WriteU32(header + 8, 0);
      if (!data)
        continue;
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
      m.WriteU32(header, Address(s.r[3]));
    }
    m.WriteU32(owner + 4, 20);
    for (unsigned i = 0; i < 14; ++i)
      m.WriteU8(owner + 60 + i, 0);
    m.WriteU8(owner + 61, 1);
    m.WriteU8(owner + 65, 1);
    s.r[3] = m.ReadU32(0x83315fb4);
    Call(0x822a7c58, m, d, s);
    unsigned profile = 0;
    if (Address(s.r[3])) {
      s.r[3] = m.ReadU32(0x83315fb4);
      s.ctr = m.ReadU32(m.ReadU32(Address(s.r[3])) + 352);
      d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
      Call(0x8229dfd8, m, d, s);
      profile = Address(s.r[3]);
    }
    m.WriteU32(owner + 32, profile);
    m.WriteU32(owner + 52, 0);
  } else {
    for (unsigned i = 0; i < 32; ++i) {
      auto row = owner + 100 + 12 * i;
      m.WriteU32(row, 0);
      m.WriteU32(row + 4, 0xffffffff);
      m.WriteU8(row + 8, 0);
    }
    for (unsigned i = 0; i < 20; ++i)
      m.WriteU32(owner + 484 + 4 * i, 0);
    for (unsigned off : {92u, 88u, 84u, 1208u})
      m.WriteU32(owner + off, 0);
    m.WriteU8(owner + 1212, 0);
    fp();
    auto zero = m.ReadU32(0x82000e50);
    m.WriteU8(owner + 1213, 0);
    for (unsigned i = 0; i < 5; ++i) {
      auto row = owner + 564 + 128 * i;
      m.WriteU32(row, 0);
      m.WriteU32(row + 4, 0);
      m.WriteU8(row + 8, 0);
      for (unsigned j = 0; j < 5; ++j)
        m.WriteU8(row + 92 + j, 0);
      for (unsigned j = 0; j < 20; ++j)
        m.WriteU32(row + 12 + 4 * j, 0);
      m.WriteU32(row + 112, zero);
      m.WriteU32(row + 116, zero);
      for (unsigned off : {100u, 120u, 124u})
        m.WriteU32(row + off, 0);
    }
    m.WriteU32(owner + 1204, 0);
    auto source = m.ReadU32(m.ReadU32(0x832ca0d0) + 292);
    for (unsigned i = 0; i < 13; ++i)
      m.WriteU32(owner + 32 + 4 * i, m.ReadU32(source + 4 * i));
  }
  if (frame) {
    s.r[1] += frame;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
  }
  return true;
}
} // namespace lo::semantic::gpu::battle_bootstrap61
