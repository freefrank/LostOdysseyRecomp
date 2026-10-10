#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_roster_persistence61.h"
#include "lo_semantics/battle_resource_growth61.h"
struct PersistGuest final : manager_release_context61::GuestServices {
  unsigned synchronized = 0;
  void CallDirect(GuestAddress, GuestMemory &,
                  manager_release_context61::Registers &) override {
    throw std::runtime_error("unexpected persistence direct service");
  }
  void CallIndirect(GuestAddress e, GuestMemory &m,
                    manager_release_context61::Registers &s) override {
    auto resource = recovery_abi::Address(s.r[3]);
    if (e == 0x2000) {
      ++synchronized;
      m.WriteU32(resource + 140, m.ReadU32(resource + 140) + 1);
      return;
    }
    if (e == 0x2004 || e == 0x2008) {
      s.r[3] = m.ReadU32(resource + (e == 0x2004 ? 160 : 164));
      return;
    }
    throw std::runtime_error("unexpected persistence virtual service");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x832c9000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    PersistGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x50000, list = 0x51000, data = 0x52000,
                       groups = 0x53000, groupData = 0x54000, state = 0x55000,
                       vt = 0x56000, profile = 0x80000, left = 0xc0000,
                       right = 0xd0000;
    unsigned actors[] = {0x100000, 0x120000, 0x140000, 0x160000, 0x180000};
    m.WriteU32(owner + 20, list);
    m.WriteU32(list, data);
    m.WriteU32(list + 4, 5);
    m.WriteU32(owner + 32, profile);
    m.WriteU32(owner + 48, groups);
    m.WriteU32(groups, groupData);
    m.WriteU32(groups + 4, 2);
    m.WriteU32(groupData, left);
    m.WriteU32(groupData + 4, right);
    m.WriteU32(0x832c9c54 + 44, state);
    m.WriteU32(vt + 440, 0x2000);
    m.WriteU32(vt + 292, 0x2004);
    m.WriteU32(vt + 380, 0x2008);
    for (unsigned i = 0; i < 5; ++i) {
      auto r = actors[i];
      m.WriteU32(data + 4 * i, r);
      m.WriteU32(r, vt);
      m.WriteU32(r + 68, i);
      m.WriteU32(r + 124, i == 4 ? 0 : 0x10000000);
      m.WriteU32(r + 132, 1);
      m.WriteU32(r + 140, 10 + i);
    }
    m.WriteU32(actors[0] + 124, 0x50000000);
    m.WriteU32(actors[1] + 160, 1);
    m.WriteU32(actors[2] + 124, 0x10400000);
    m.WriteU32(actors[3] + 132, 0);
    m.WriteU32(right + 12676, 4);
    m.WriteU32(left + 12676, 1);
    auto call = [&](unsigned e) {
      s.r[3] = owner;
      if (!battle_roster_persistence61::Apply(e, m, {guest, native}, s))
        throw std::runtime_error("persistence entry");
      if (s.r[1] != initial.r[1])
        throw std::runtime_error("persistence stack");
      for (unsigned i = 27; i < 32; ++i)
        if (s.r[i] != initial.r[i])
          throw std::runtime_error("persistence nonvolatile");
    };
    auto check = [&](bool b, const char *msg) {
      if (!b)
        throw std::runtime_error(msg);
    };
    call(0x82af5810);
    check(m.ReadU32(right + 12680) == 3 && m.ReadU32(right + 12684) == 2 &&
              m.ReadU32(right + 12688) == 1,
          "availability tiers");
    check(m.ReadU32(left + 12680) == 1 && m.ReadU32(left + 12688) == 1,
          "other side tiers");
    // Distinct public synthetic payloads detect incorrect source/destination
    // spans.
    unsigned offsets[] = {2468, 2528, 2588, 232, 5108, 5156, 10544},
             destinations[] = {128, 188, 248, 12132, 2600, 2648, 8036},
             sizes[] = {60, 60, 2352, 2176, 48, 5388, 4096};
    for (unsigned i = 0; i < 7; ++i)
      for (unsigned j = 0; j < sizes[i]; ++j)
        m.WriteU8(actors[0] + offsets[i] + j, (i * 31 + j) & 255);
    for (unsigned j = 0; j < 12288; ++j)
      m.WriteU8(right + 72 + j, (j * 7 + 3) & 255);
    call(0x82af57d8);
    check(guest.synchronized == 4, "party persistence filter");
    check(m.ReadU32(profile + 124 + 12) == 11, "virtual refresh before save");
    for (unsigned i = 0; i < 7; ++i)
      for (unsigned j = 0; j < sizes[i]; ++j)
        check(m.ReadU8(profile + 124 + destinations[i] + j) ==
                  ((i * 31 + j) & 255),
              "persisted resource spans");
    check((m.ReadU32(profile + 124 + 8) & 0x80000000) == 0 &&
              (m.ReadU32(profile + 124 + 14308 + 8) & 0x80000000) != 0,
          "saved inverse class flag");
    for (unsigned j = 0; j < 12288; ++j)
      check(m.ReadU8(profile + 157512 + j) == ((j * 7 + 3) & 255),
            "shared profile ranges");
    constexpr unsigned restored = 0x1a0000;
    s.r[4] = profile + 124;
    s.r[5] = restored;
    call(0x82abfc50);
    for (unsigned i = 0; i < 7; ++i)
      for (unsigned j = 0; j < sizes[i]; ++j) {
        unsigned offset = offsets[i] + j;
        auto expected =
            (offset >= 4876 && offset < 4880) ? 0u : ((i * 31 + j) & 255);
        check(m.ReadU8(restored + offset) == expected,
              "profile restore spans and passive reset");
      }
    check(m.ReadU32(restored + 140) == 11, "profile restore level");
    s.r[3] = owner;
    s.r[4] = restored;
    check(battle_resource_growth61::Apply(0x82abfe38, m, {guest, native}, s),
          "baseline entry");
    check(m.ReadU32(restored + 4940) == 3 && m.ReadU32(restored + 5088) == 0 &&
              m.ReadU32(restored + 5100) == 0,
          "baseline fields");
    for (unsigned i = 0; i < 32; ++i)
      check(m.ReadU32(restored + 4960 + 4 * i) == 99, "baseline level limits");
    for (unsigned group : {0u, 1u, 2u, 3u, 4u, 5u, 6u}) {
      m.WriteU32(restored + 68, group);
      m.WriteU32(restored + 124, 0xffffffff);
      m.WriteU32(restored + 200, 0xffffffff);
      m.WriteU32(restored + 208, 0xffffffff);
      m.WriteU32(restored + 100, 0xffffffff);
      s.r[4] = restored;
      check(battle_resource_growth61::Apply(0x82abfe90, m, {guest, native}, s),
            "active reset entry");
      unsigned expected = (group == 1 || group == 2 || group == 3 || group == 5)
                              ? 0xf8dfffffu
                              : 0xf8cfffffu;
      check(m.ReadU32(restored + 124) == expected &&
                m.ReadU32(restored + 132) == 1 &&
                m.ReadU32(restored + 200) == 0x7fffffff &&
                m.ReadU32(restored + 208) == 0x3fffffff &&
                m.ReadU32(restored + 100) == 0x7fffffff,
            "active reset masks and group bit");
    }
    m.WriteU32(profile + 157512, 0x12345678);
    m.WriteU32(state + 28, 0x18000);
    call(0x82af5400);
    check(m.ReadU32(profile + 157512) == 0x12345678,
          "script mode copy suppression");
    s.r[4] = 257;
    call(0x82af52f0);
    check(m.ReadU32(right + 12676) == 0 && m.ReadU32(left + 12676) == 1,
          "selected group count reset");
    std::puts("PASS roster persistence spans, party filtering, availability "
              "tiers and copy suppression");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
