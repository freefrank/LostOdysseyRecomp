#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_action_effects61.h"
#include "battle_action_record_fixture.h"
#include <iostream>
struct EffectGuest final : manager_release_context61::GuestServices {
  void Need(bool b) {
    if (!b)
      throw std::runtime_error("action effect state");
  }
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (ActionStorageDirectFixture(e, m, s))
      return;
    throw std::runtime_error("unexpected effect direct");
  }
  void CallIndirect(GuestAddress e, GuestMemory &m,
                    manager_release_context61::Registers &s) override {
    if (ActionStorageIndirectFixture(e, m, s))
      return;
    throw std::runtime_error("unexpected effect indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83245000, 0x1000});
    regions.push_back({0x832c9000, 0x4000});
    regions.push_back({0x8330b000, 0x1000});
    regions.push_back({0x821a8000, 0x1000});
    regions.push_back({0x83213000, 0x1000});
    regions.push_back({0x83264000, 0x1000});
    regions.push_back({0x831f3000, 0x21000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    SetupActionStorageFixture(m);
    EffectGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    m.WriteU32(0x8324570c, 0x70000);
    m.WriteU32(0x70000 + 16, 55);
    m.WriteU32(0x832ca0cc, 0x71000);
    m.WriteU32(0x832ca0d8, 0x72000);
    m.WriteU32(0x832cb78c, 0x73000);
    m.WriteU32(0x80000 + 64, 24);
    for (unsigned kind = 0; kind < 33; ++kind) {
      m.WriteU32(0x70004, 6);
      m.WriteU32(0x70008, 7);
      m.WriteU32(0x7000c, 8);
      m.WriteU32(0x80000 + 88, 0);
      m.WriteU32(0x80000 + 92, 0);
      m.WriteU32(0x80000 + 96, 0);
      s.r[3] = 0x80000;
      s.r[4] = kind;
      s.r[5] = 9;
      s.r[6] = 1;
      guest.Need(
          battle_action_effects61::Apply(0x82ab0d50, m, {guest, native}, s));
      unsigned group = 6, value = 7, extra = 8;
      if (kind == 0 || kind == 13 || kind == 15 || kind == 18 || kind == 19) {
        group = 0;
        value = 24;
        extra = 0;
      } else if (kind == 14) {
        group = 0;
        value = 1;
        extra = 0;
      } else if (kind == 2 || kind == 3 || (kind >= 6 && kind <= 11)) {
        group = 2;
        value = 5;
        extra = 0xffffffff;
      } else if (kind == 1 || kind == 4 || kind == 5 || kind == 12 ||
                 kind == 16 || kind == 22 || (kind >= 25 && kind <= 31)) {
        group = 0;
        value = 55;
        extra = 0xffffffff;
      }
      guest.Need(m.ReadU32(0x80000 + 88) == group &&
                 m.ReadU32(0x80000 + 92) == value &&
                 m.ReadU32(0x80000 + 96) == extra &&
                 (m.ReadU32(0x80000 + 100) & 0xc0000000u) == 0x80000000u &&
                 s.r[1] == initial.r[1] && s.r[27] == initial.r[27] &&
                 s.r[31] == initial.r[31]);
    }
    m.WriteU32(0x80000 + 124, 0x20009);
    m.WriteU32(0x80000 + 76316, 99);
    m.WriteU32(0x80000 + 14680, 9);
    s.r[3] = 0x80000;
    guest.Need(
        battle_action_effects61::Apply(0x82ab31e0, m, {guest, native}, s));
    guest.Need(m.ReadU32(0x80000 + 124) == 9 && !m.ReadU32(0x80000 + 76316) &&
               !m.ReadU32(0x80000 + 14680) && m.ReadU32(0x80000 + 14660) == 1);
    m.WriteU32(0x60000 + 44, 0x63000);
    m.WriteU32(0x63000 + 4, 0x62000);
    m.WriteU32(0x63000 + 12, 1);
    m.WriteU32(0x62000, 7);
    m.WriteU32(0x80000 + 148, 7);
    s.r[3] = 0x60000;
    s.r[4] = 0x80000;
    guest.Need(
        battle_action_effects61::Apply(0x82af68d8, m, {guest, native}, s));
    guest.Need(m.ReadU32(0x62000 + 64) == 0x20000000);
    m.WriteU32(0x80000 + 148, 8);
    s.r[3] = 0x60000;
    s.r[4] = 0x80000;
    guest.Need(
        battle_action_effects61::Apply(0x82af68d8, m, {guest, native}, s));
    for (unsigned state : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 99u, 0xffffffffu}) {
      m.WriteU32(0x80000 + 60, state);
      s.r[3] = 0x80000;
      guest.Need(
          battle_action_effects61::Apply(0x82b1f1d0, m, {guest, native}, s));
      guest.Need(s.r[3] == (state < 6 ? state + 1 : 0) &&
                 m.ReadU32(0x80000 + 60) == s.r[3]);
    }
    for (unsigned i = 0; i < 8; ++i)
      guest.Need(m.ReadU8(unsigned(s.r[1]) - 16 + i) == i);
    guest.Need(!m.ReadU16(unsigned(s.r[1]) - 8));
    std::cout << "battle_action_effects61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
