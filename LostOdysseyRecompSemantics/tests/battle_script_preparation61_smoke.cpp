#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_preparation61.h"
#include <iostream>
struct PreparationGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x8238e2f8) {
      s.r[3] = 0x71000;
      return;
    }
    if (e == 0x8238e308) {
      s.r[3] = s.r[4] ? 0x80000 + 0x10000 * (unsigned(s.r[4]) - 24) : 0x80000;
      return;
    }
    throw std::runtime_error("unexpected preparation service");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("unexpected preparation indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83213000, 0x1000});
    regions.push_back({0x83264000, 0x1000});
    regions.push_back({0x832ae000, 0x1000});
    regions.push_back({0x831f3000, 0x21000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    for (unsigned i = 0; i < 32768; ++i)
      m.WriteU32(0x831f3300 + 4 * i, i % 2);
    PreparationGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       targets = 0x64000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(actor + 4, 0x80000);
    m.WriteU32(actor + 8, 24);
    m.WriteU32(actor + 80, targets);
    m.WriteU32(actor + 72, 0x65000);
    m.WriteU32(actor + 76, 1);
    m.WriteU8(0x65000, 26);
    m.WriteU32(0x71000, 0x71100);
    m.WriteU32(0x71004, 4);
    m.WriteU32(0x83264558, 0x72000);
    for (unsigned i = 0; i < 4; ++i) {
      auto resource = 0x80000 + 0x10000 * i;
      m.WriteU32(0x71100 + 4 * i, resource);
      m.WriteU32(resource + 64, 24 + i);
      m.WriteU32(resource + 124, (i < 2 ? 0x08000000 : 0) |
                                     (i % 2 == 0 ? 0x40000000 : 0) |
                                     (i == 3 ? 0x00400000 : 0));
      m.WriteU32(resource + 132, 1);
      m.WriteU32(resource + 2588, 0x42c80000);
    }
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("preparation result");
    };
    auto run = [&](unsigned mode, unsigned mask = 0) {
      s.r[3] = owner;
      s.r[4] = mode;
      s.r[5] = mask;
      check(battle_script_preparation61::Apply(0x82afde70, m, {guest, native},
                                               s));
      check(s.r[1] == initial.r[1] && s.r[24] == initial.r[24] &&
            s.r[31] == initial.r[31]);
    };
    run(0);
    check(s.r[3] == 0 && m.ReadU32(actor + 84) == 1 && m.ReadU8(targets) == 24);
    m.WriteU32(0x90000 + 124, m.ReadU32(0x90000 + 124) | 0x10000000);
    run(1);
    check(m.ReadU8(targets) == 25);
    m.WriteU32(0x83213438 + 16, 0);
    m.WriteU32(0x8321343c + 16, 4);
    m.WriteU32(0x80000 + 7 * 272 + 232, 4);
    m.WriteU32(0x80000 + 4 * (567 + 2), 26);
    run(1);
    check(m.ReadU8(targets) == 26);
    m.WriteU32(0x80000 + 7 * 272 + 232, 0);
    run(2);
    check(m.ReadU32(actor + 84) == 2 && m.ReadU8(targets) == 24 &&
          m.ReadU8(targets + 1) == 25 && m.ReadU32(actor + 336) == 1);
    run(3);
    check(m.ReadU32(0x72000 + 4 * (128 * 15 + 80 + 3)) == 1 &&
          m.ReadU8(targets) == 26);
    run(4);
    check(m.ReadU32(actor + 84) == 2 && m.ReadU32(actor + 336) == 4);
    for (unsigned mode : {5u, 7u, 9u, 11u}) {
      run(mode);
      check(s.r[3] == 0 && m.ReadU32(actor + 84) == 1 &&
            m.ReadU8(targets) == (mode == 5   ? 24
                                  : mode == 7 ? 25
                                  : mode == 9 ? 26
                                              : 27));
    }
    for (unsigned mode : {6u, 8u, 10u, 12u}) {
      run(mode);
      check(m.ReadU32(actor + 84) == 1 &&
            m.ReadU32(actor + 336) == (mode == 6    ? 2
                                       : mode == 8  ? 3
                                       : mode == 10 ? 5
                                                    : 6));
    }
    run(13);
    check(m.ReadU8(targets) == 26 && m.ReadU32(actor + 336) == 0);
    run(14);
    check(m.ReadU8(targets) == 24 && m.ReadU32(actor + 336) == 5);
    run(15);
    check(m.ReadU32(actor + 84) == 1 && m.ReadU8(targets) == 27);
    run(16);
    check(m.ReadU32(actor + 16) == 1 && m.ReadU32(actor + 84) == 1 &&
          m.ReadU8(targets) == 25);
    run(16, 5);
    check(!m.ReadU32(actor + 16) && m.ReadU32(actor + 84) == 1 &&
          m.ReadU8(targets) == 25 && m.ReadU8(targets + 1) == 2);
    run(17);
    check(m.ReadU32(0x72000 + 4 * (128 * 15 + 83 + 3)) == 6 &&
          m.ReadU8(targets) == 25);
    m.WriteU32(0x71004, 0);
    m.WriteU32(actor + 336, 77);
    run(5);
    check(s.r[3] == 1 && !m.ReadU32(actor + 84) &&
          m.ReadU32(actor + 336) == 77);
    run(4);
    check(s.r[3] == 0 && !m.ReadU32(actor + 84) && m.ReadU32(actor + 336) == 4);
    run(99);
    check(!s.r[3] && !m.ReadU32(actor + 84));
    std::cout << "battle_script_preparation61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
