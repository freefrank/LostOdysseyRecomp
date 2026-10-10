#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_action_effects61.h"
#include <iostream>
struct EffectGuest final : manager_release_context61::GuestServices {
  unsigned kind = 0, route = 0, finished = 0, reset = 0, special = 0;
  void Need(bool b) {
    if (!b)
      throw std::runtime_error("action effect ABI");
  }
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82a9b698) {
      Need(s.r[3] == 0x80000 + 14656 && s.r[4] == 0);
      m.WriteU32(unsigned(s.r[3]) + 4, 0);
      return;
    }
    if (e == 0x82ab2d88) {
      Need(s.r[3] == 0x80000 && s.r[4] == 0);
      m.WriteU32(0x80000 + 14660, 1);
      return;
    }
    if (e == 0x82acde40) {
      Need(s.r[3] == 0x70000 && s.r[4] == 0x80000 && s.r[6] == 1);
      reset = unsigned(s.r[5]);
      special = unsigned(s.r[7]);
      ++finished;
      return;
    }
    route = e;
    if (e == 0x82acd3b0) {
      Need(s.r[3] == 0x70000 && s.r[4] == 0 &&
           s.r[5] == (kind == 14                 ? 1
                      : kind >= 25 && kind <= 29 ? 55
                                                 : 24) &&
           unsigned(s.r[6]) == (kind >= 25 && kind <= 29 ? 0xffffffffu : 0));
      return;
    }
    Need((e == 0x82b21340 && s.r[3] == 0x71000) ||
         (e == 0x82b11df0 && s.r[3] == 0x72000) ||
         (e == 0x82b1f798 && s.r[3] == 0x73000));
    Need(s.r[4] == 0x80000 && s.r[5] == kind && s.r[6] == 9 && s.r[7] == 1);
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
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
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    EffectGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    m.WriteU32(0x8324570c, 0x70000);
    m.WriteU32(0x70000 + 16, 55);
    m.WriteU32(0x832ca0cc, 0x71000);
    m.WriteU32(0x832ca0d8, 0x72000);
    m.WriteU32(0x832cb78c, 0x73000);
    constexpr unsigned routes[]{
        0x82acd3b0, 0x82b21340, 0x82b11df0, 0x82b11df0, 0x82b1f798, 0x82b1f798,
        0x82b11df0, 0x82b11df0, 0x82b11df0, 0x82b11df0, 0x82b11df0, 0x82b11df0,
        0x82b21340, 0x82acd3b0, 0x82acd3b0, 0x82acd3b0, 0x82b21340, 0,
        0x82acd3b0, 0x82acd3b0, 0,          0,          0x82b21340, 0,
        0,          0x82acd3b0, 0x82acd3b0, 0x82acd3b0, 0x82acd3b0, 0x82acd3b0,
        0x82b21340, 0x82b21340, 0};
    for (unsigned kind = 0; kind < 33; ++kind) {
      guest.kind = kind;
      guest.route = 0;
      s.r[3] = 0x80000;
      s.r[4] = kind;
      s.r[5] = 9;
      s.r[6] = 1;
      guest.Need(
          battle_action_effects61::Apply(0x82ab0d50, m, {guest, native}, s));
      guest.Need(
          guest.route == routes[kind] &&
          guest.reset == unsigned(kind == 0 || kind == 13 || kind == 14 ||
                                  kind == 15 || kind == 18 || kind == 19) &&
          guest.special == unsigned(kind == 2 || (kind >= 6 && kind <= 9)) &&
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
