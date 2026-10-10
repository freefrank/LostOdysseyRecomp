#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_property_mutation61.h"
#include <iostream>
#include "battle_progression_fixture.h"
struct MutationGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (progression_fixture::Direct(e, s))
      return;
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x70000;
      return;
    }

    throw std::runtime_error("property direct");
  }
  void CallIndirect(GuestAddress e, GuestMemory &m,
                    manager_release_context61::Registers &s) override {
    if (progression_fixture::Indirect(e, s))
      return;
    throw std::runtime_error("property indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x832c9000, 0x4000});
    regions.push_back({0x83213000, 0x1000});
    regions.push_back({0x83291000, 0x1000});
    regions.push_back({0x83315000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    progression_fixture::Setup(m);
    MutationGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("property state");
    };
    m.WriteU32(0x83213438 + 8 * 20, 0);
    m.WriteU32(0x8321343c + 8 * 20, 1u << 20);
    constexpr unsigned resource = 0x80000, flag = resource + 7 * 272 + 232,
                       first = resource + 4 * (7 * 68 + 20 + 59),
                       second = resource + 4 * (7 * 68 + 20 + 91);
    m.WriteU32(flag, (1u << 20) | 7);
    m.WriteU32(first, 5);
    m.WriteU32(second, 9);
    auto run = [&](unsigned e, unsigned id, unsigned mode) {
      s.r[3] = resource;
      s.r[4] = id;
      s.r[5] = mode;
      check(battle_property_mutation61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[29] == initial.r[29] &&
            s.r[31] == initial.r[31]);
      return unsigned(s.r[3]);
    };
    check(run(0x82ac8ee8, 244, 0) == 1 && m.ReadU32(first) == 5 &&
          m.ReadU32(second) == 9);
    check(run(0x82ac8ee8, 244, 2) == 1 && (m.ReadU32(flag) & (1u << 20)));
    check(run(0x82ac9000, 244, 0) == 1 && m.ReadU32(flag) == 7 &&
          !m.ReadU32(first) && !m.ReadU32(second));
    check(run(0x82ac9000, 244, 0) == 0);
    check(run(0x82ac8ee8, 263, 1) == 0);
    auto bankRun = [&](unsigned e, unsigned mask, unsigned mode,
                       unsigned value = 0) {
      s.r[3] = resource;
      s.r[4] = 2;
      s.r[5] = mask;
      s.r[6] = mode;
      s.r[7] = value;
      check(battle_property_mutation61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[26] == initial.r[26] &&
            s.r[27] == initial.r[27]);
      return unsigned(s.r[3]);
    };
    auto bankFlags = resource + 2 * 272 + 232;
    m.WriteU32(bankFlags, 0x80000007);
    check(bankRun(0x82ac85a0, 1, 0) ==
          3); // gated whole-bank count, low 31 bits
    check(bankRun(0x82ac85a0, 8, 0) == 0);
    check(bankRun(0x82ac90e8, 3, 1) == 1 && m.ReadU32(bankFlags) == 0x80000007);
    m.WriteU32(resource + 4 * (136 + 59), 99);
    m.WriteU32(resource + 4 * (136 + 91), 88);
    check(bankRun(0x82ac90f8, 3, 1) == 1 && m.ReadU32(bankFlags) == 0x80000004);
    check(!m.ReadU32(resource + 4 * (136 + 59)) &&
          !m.ReadU32(resource + 4 * (136 + 91)));
    check(bankRun(0x82ac90f8, 0x80000000, 1) == 0);
    m.WriteU32(0x83213538 + 4 * (64 + 2), 1);
    check(bankRun(0x82ac8af8, 4, 17) == 0);
    check(bankRun(0x82ac89f0, 8, 0, 17) == 1 &&
          m.ReadU32(bankFlags) == 0x80000004);
    check(bankRun(0x82ac89f0, 24, 1, 17) == 1 &&
          m.ReadU32(bankFlags) == 0x8000001c);
    check(m.ReadU32(resource + 4 * (136 + 3 + 59)) ==
          17); // first bit of the original mask
    check(bankRun(0x82ac89f0, 0x80000000, 1) == 0);
    auto payloadRun = [&](unsigned e, unsigned mask, unsigned value,
                          unsigned mode, unsigned add) {
      s.r[3] = resource;
      s.r[4] = 2;
      s.r[5] = mask;
      s.r[6] = value;
      s.r[7] = mode;
      s.r[8] = add;
      check(battle_property_mutation61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[24] == initial.r[24]);
      return unsigned(s.r[3]);
    };
    auto payload = resource + 4 * (136 + 2 + 59),
         other = resource + 4 * (136 + 2 + 91);
    m.WriteU32(payload, 10);
    m.WriteU32(other, 55);
    check(!payloadRun(0x82ac8978, 4, 9, 1, 1) && m.ReadU32(payload) == 10);
    check(payloadRun(0x82ac8978, 4, 10, 0, 0));
    check(payloadRun(0x82ac87d8, 4, 12, 1, 0) && m.ReadU32(payload) == 12 &&
          m.ReadU32(other) == 55);
    check(payloadRun(0x82ac87d8, 4, 3, 1, 1) && m.ReadU32(payload) == 15);
    check(payloadRun(0x82ac87d8, 4, 5, 1, 0) && m.ReadU32(payload) == 15);
    m.WriteU32(bankFlags, 0);
    check(payloadRun(0x82ac87d8, 4, 7, 1, 0) && m.ReadU32(payload) == 7 &&
          !m.ReadU32(other));
    m.WriteU32(payload, 0xffffffff);
    check(payloadRun(0x82ac8978, 4, 0, 0, 0));
    check(!payloadRun(0x82ac87d8, 0x80000000, 1, 1, 0));
    auto admission = [&](unsigned bank, unsigned mask, unsigned mode,
                         unsigned bypass) {
      s.r[3] = resource;
      s.r[4] = bank;
      s.r[5] = mask;
      s.r[6] = mode;
      s.r[7] = bypass;
      check(battle_property_mutation61::Apply(0x82aca468, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[15] == initial.r[15]);
      return unsigned(s.r[3]);
    };
    m.WriteU32(resource + 232, 0);
    check(admission(0, 8, 0, 0));
    m.WriteU32(resource + 76348, 0x80000000);
    check(!admission(0, 8, 0, 0));
    m.WriteU32(resource + 76348, 0);
    m.WriteU32(resource + 5088, 8);
    check(!admission(0, 8, 0, 1));
    m.WriteU32(resource + 5088, 0);
    m.WriteU32(resource + 4876, 8);
    check(admission(0, 8, 0, 0) && !admission(0, 8, 1, 0));
    check(admission(0, 8, 1, 1) && m.ReadU32(resource + 232) == 8);
    m.WriteU32(resource + 124, 0x10000000);
    check(!admission(0, 8, 0, 0));
    m.WriteU32(resource + 124, 0);
    m.WriteU32(resource + 4876, 0);
    m.WriteU32(resource + 232, 0);
    check(!admission(0, 0x10000, 0, 0));
    m.WriteU32(resource + 232, 4);
    check(admission(0, 0x10000, 0, 0));
    m.WriteU32(resource + 232, 0x10000);
    check(!admission(0, 4, 0, 0));
    m.WriteU32(0x832134b4, 8);
    m.WriteU32(resource + 4876, 8);
    check(!admission(7, 2, 0, 0));
    m.WriteU32(resource + 4876, 0);
    check(admission(7, 2, 1, 0));
    m.WriteU32(0x83213438 + 8 * 16, 0);
    m.WriteU32(0x8321343c + 8 * 16, 1u << 16);
    m.WriteU32(0x83213438 + 8 * 2, 0);
    m.WriteU32(0x8321343c + 8 * 2, 4);
    m.WriteU32(resource + 232, 4);
    check(admission(0, 0x10000, 1, 0));
    check(m.ReadU32(resource + 232) == 0x10000 &&
          m.ReadU32(resource + 4 * (16 + 59)) == 3);
    m.WriteU32(0x832c9c54 + 44, 0x63000);
    m.WriteU32(0x63004, 0x62000);
    m.WriteU32(0x6300c, 1);
    m.WriteU32(0x62008, 24);
    m.WriteU32(resource + 64, 24);
    m.WriteU32(0x62000 + 64, 0x800000);
    m.WriteU32(resource + 232, 0);
    check(admission(0, 1, 1, 0));
    check(m.ReadU32(progression_fixture::Play + 170836) == 1 &&
          m.ReadU32(0x62000 + 64) == 0xc0000c);
    auto stateRun = [&](unsigned e, unsigned bank, unsigned mask,
                        unsigned value, unsigned aux, unsigned mode,
                        unsigned option) {
      s.r[3] = resource;
      s.r[4] = bank;
      s.r[5] = mask;
      s.r[6] = value;
      s.r[7] = aux;
      s.r[8] = mode;
      s.r[9] = option;
      check(battle_property_mutation61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[21] == initial.r[21]);
      return unsigned(s.r[3]);
    };
    m.WriteU32(resource + 4876, 0);
    m.WriteU32(resource + 5088, 0);
    m.WriteU32(bankFlags, 4);
    check(!stateRun(0x82ac8ed8, 2, 4, 20, 30, 0, 99));
    check(stateRun(0x82ac8ed8, 2, 4, 20, 30, 1, 0));
    m.WriteU32(payload, 7);
    check(stateRun(0x82ac8ec8, 2, 4, 20, 30, 0, 0) && m.ReadU32(payload) == 7);
    check(stateRun(0x82ac8ec8, 2, 4, 20, 30, 1, 0) &&
          m.ReadU32(payload) == 20 && m.ReadU32(other) == 30);
    m.WriteU32(resource + 7 * 272 + 232, 2);
    m.WriteU32(0x83213538 + 4 * (7 * 32 + 1), 1);
    auto specialPayload = resource + 4 * (7 * 68 + 1 + 59);
    m.WriteU32(specialPayload, 5);
    check(stateRun(0x82ac8ec8, 7, 2, 3, 4, 1, 0) &&
          m.ReadU32(specialPayload) == 8);
    m.WriteU32(resource + 4876, 8);
    check(!stateRun(0x82ac8ed8, 7, 2, 3, 4, 1, 0));
    m.WriteU32(resource + 4876, 0);
    m.WriteU32(bankFlags, 0);
    check(stateRun(0x82ac8ec8, 2, 4, 21, 31, 0, 0) &&
          m.ReadU32(payload) == 21 && m.ReadU32(other) == 31);
    for (unsigned i = 0; i < 32; ++i) {
      m.WriteU32(0x83213438 + 8 * i, 0);
      m.WriteU32(0x8321343c + 8 * i, 1u << i);
    }
    auto insert = [&](unsigned e, unsigned id) {
      s.r[3] = resource;
      s.r[4] = id;
      s.r[5] = s.r[6] = 1;
      check(battle_property_mutation61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[24] == initial.r[24]);
    };
    m.WriteU32(resource + 232, 0);
    m.WriteU32(resource + 5088, 0);
    m.WriteU32(resource + 4876, 1);
    m.WriteU32(0x62000 + 64, 0);
    insert(0x82ac9be0, 0);
    check(m.ReadU32(resource + 232) == 0 &&
          m.ReadU32(progression_fixture::Play + 170836) == 1);
    insert(0x82ac9ee8, 0);
    check(m.ReadU32(resource + 232) == 1 &&
          m.ReadU32(progression_fixture::Play + 170836) == 2);
    insert(0x82ac9ee8, 0);
    check(m.ReadU32(progression_fixture::Play + 170836) == 2);
    m.WriteU32(resource + 232, 0);
    m.WriteU32(resource + 4876, 0);
    m.WriteU32(resource + 5088, 1);
    insert(0x82ac9be0, 0);
    check(!m.ReadU32(resource + 232));
    m.WriteU32(0x62000 + 64, 0x800000);
    insert(0x82ac9ee8, 0);
    check(!m.ReadU32(resource + 232) && m.ReadU32(0x62000 + 64) == 0xc0000c &&
          m.ReadU32(progression_fixture::Play + 170836) == 3);
    m.WriteU32(resource + 5088, 0);
    m.WriteU16(0x70000 + 148, 1);
    insert(0x82ac9be0, 1);
    check(!m.ReadU32(resource + 232));
    m.WriteU16(0x70000 + 148, 0);
    insert(0x82ac9be0, 1);
    check(m.ReadU32(resource + 232) == 2);
    m.WriteU32(resource + 232, 4);
    m.WriteU32(resource + 6 * 272 + 232, 0);
    insert(0x82ac9be0, 16);
    check(m.ReadU32(resource + 232) == 0x10000 &&
          m.ReadU32(resource + 4 * (59 + 16)) == 3);
    insert(0x82ac9ee8, 2);
    check(m.ReadU32(resource + 232) == 0x10000);
    m.WriteU32(resource + 232, 4);
    m.WriteU32(resource + 6 * 272 + 232, 1u << 6);
    insert(0x82ac9ee8, 16);
    check(m.ReadU32(resource + 232) == 4);
    m.WriteU32(resource + 6 * 272 + 232, 0);
    m.WriteU32(resource + 232, 0);
    m.WriteU32(resource + 5088, 8);
    insert(0x82ac9be0, 3);
    check(!m.ReadU32(resource + 232));
    insert(0x82ac9ee8, 3);
    check(m.ReadU32(resource + 232) == 8);
    m.WriteU32(resource + 5088, 0);
    m.WriteU32(resource + 124, 0);
    m.WriteU32(0x62000 + 64, 0);
    insert(0x82ac9be0, 15);
    check(m.ReadU32(resource + 232) == 0x8009 &&
          m.ReadU32(progression_fixture::Play + 170836) == 4);
    m.WriteU32(0x62000 + 16, 2);
    m.WriteU32(0x62000 + 20, 0x65000);
    m.WriteU32(0x65000 + 8, 777);
    m.WriteU32(0x62000 + 40, 0x66000);
    m.WriteU32(0x62000 + 64, 0);
    for (unsigned i = 0; i < 16; ++i)
      m.WriteU32(0x66000 + 24 * i, 255);
    m.WriteU32(resource + 124, 0x800000);
    m.WriteU32(resource + 2588, 0x425c0000);
    m.WriteU32(resource + 188, 12);
    m.WriteU32(resource + 5 * 272 + 232, 1u << 4);
    m.WriteU32(resource + 7 * 272 + 232, 1u << 18);
    s.r[3] = 0x70000;
    s.r[4] = resource;
    s.r[5] = 1;
    s.r[6] = 0;
    check(battle_property_mutation61::Apply(0x82ad0ad0, m, {g, native}, s));
    check(m.ReadU32(0x62000 + 64) == 4 && m.ReadU32(0x66000 + 15 * 24) == 16 &&
          m.ReadU32(0x66000 + 15 * 24 + 4) == 777 &&
          m.ReadU32(0x66000 + 15 * 24 + 16) == 2 &&
          !m.ReadU32(resource + 2588) && !m.ReadU32(resource + 188) &&
          m.ReadU32(resource + 156) == 0xffffffff &&
          !m.ReadU32(resource + 124) && !m.ReadU32(resource + 5 * 272 + 232) &&
          !m.ReadU32(resource + 7 * 272 + 232));
    m.WriteU32(0x62000 + 64, 0);
    m.WriteU32(0x70000 + 208, 77);
    s.r[3] = 0x70000;
    s.r[4] = resource;
    s.r[5] = s.r[6] = 1;
    check(battle_property_mutation61::Apply(0x82ad0ad0, m, {g, native}, s) &&
          m.ReadU32(0x62000 + 64) == 12 && m.ReadU32(0x62000 + 468) == 77 &&
          s.r[1] == initial.r[1]);
    // Script insertion ignores passive immunity and the special death lock,
    // but still respects the active immunity mask.
    m.WriteU32(resource + 232, 0);
    m.WriteU32(resource + 4876, 0xffffffff);
    m.WriteU32(resource + 76348, 0x80000000);
    m.WriteU32(resource + 5088, 8);
    insert(0x82aca1b8, 3);
    check(!m.ReadU32(resource + 232));
    m.WriteU32(resource + 5088, 0);
    insert(0x82aca1b8, 3);
    check(m.ReadU32(resource + 232) == 8);
    m.WriteU32(0x62000 + 64, 0);
    insert(0x82aca1b8, 0);
    check(m.ReadU32(resource + 232) == 9);
    m.WriteU32(resource + 232, 4);
    m.WriteU32(resource + 6 * 272 + 232, 0);
    insert(0x82aca1b8, 16);
    check(m.ReadU32(resource + 232) == 0x10000 &&
          m.ReadU32(resource + 4 * (59 + 16)) == 3);
    check(!battle_property_mutation61::Apply(0, m, {g, native}, s));
    std::cout << "battle_property_mutation61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
