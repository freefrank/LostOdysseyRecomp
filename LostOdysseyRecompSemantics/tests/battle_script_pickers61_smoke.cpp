#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_pickers61.h"
#include <iostream>
struct PickerGuest final : manager_release_context61::GuestServices {
  unsigned gate = 1, skip = 0, enabled = 0, predicate = 1;
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
      s.r[3] = s.r[4] == 20 ? 0x90000 : 0x80000;
      return;
    }
    if (e == 0x82ad0c10) {
      if (s.r[3] != 0x70000 || s.r[4] != 0x80000 || s.r[6] != 2)
        throw std::runtime_error("picker action eligibility ABI");
      s.r[3] = predicate;
      return;
    }
    if (e == 0x82b08b80) {
      if (s.r[3] != 0x73000)
        throw std::runtime_error("picker kind lookup ABI");
      s.r[3] = 7;
      return;
    }
    throw std::runtime_error("unexpected picker service");
  }
  void CallIndirect(GuestAddress e, GuestMemory &m,
                    manager_release_context61::Registers &s) override {
    if (skip)
      m.WriteU32(0x80000 + 2616, 0x41200000);
    if (s.r[3] != 0x90000)
      throw std::runtime_error("picker target virtual");
    s.r[3] = e == 0x2000 + enabled;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83213000, 0x1000});
    regions.push_back({0x83291000, 0x1000});
    regions.push_back({0x83264000, 0x1000});
    regions.push_back({0x832c9000, 0x4000});
    regions.push_back({0x831f3000, 0x21000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    m.WriteU32(0x831f3304, 1);
    PickerGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       output = 0x64000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(actor + 4, 0x80000);
    m.WriteU32(actor + 8, 24);
    m.WriteU32(actor + 80, 0x65000);
    m.WriteU32(0x71000, 0x71100);
    m.WriteU32(0x71004, 2);
    m.WriteU32(0x71100, 0x80000);
    m.WriteU32(0x71104, 0x90000);
    m.WriteU32(0x83264558, 0x72000);
    m.WriteU32(0x832ca0d8, 0x73000);
    m.WriteU32(0x83264984, 0xa0000);
    m.WriteU32(0x90000, 0x74000);
    for (unsigned slot : {284u, 352u, 288u, 344u, 380u, 420u})
      m.WriteU32(0x74000 + slot, 0x2000 + slot);
    for (unsigned i = 0; i < 2; ++i) {
      auto p = 0x80000 + 0x10000 * i;
      m.WriteU32(p + 64, 24 + i);
      m.WriteU32(p + 124, 0x08000000 | (i == 0 ? 0x40000000 : 0));
      m.WriteU32(p + 132, 1);
      m.WriteU32(p + 2588, 0x42c80000);
      m.WriteU32(p + 2616, 0x41200000);
    }
    for (unsigned id = 0; id < 400; ++id)
      m.WriteU32(0xa0000 + 96 * id + 16, 1);
    unsigned slot = 0;
    for (unsigned i = 0; i < 25; ++i) {
      m.WriteU32(0x832139e8 + 4 * i, 100 + i);
      m.WriteU32(0x80000 + 15212 + 44 * slot++, 100 + i);
    }
    for (unsigned i = 0; i < 6; ++i) {
      m.WriteU32(0x83213a4c + 4 * i, 200 + i);
      m.WriteU32(0x80000 + 15212 + 44 * slot++, 200 + i);
    }
    for (unsigned i = 0; i < 53; ++i) {
      m.WriteU32(0x83213a68 + 4 * i, 300 + i);
      m.WriteU32(0x80000 + 15212 + 44 * slot++, 300 + i);
    }
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("picker result");
    };
    auto run = [&](unsigned e) {
      s.r[3] = owner;
      s.r[4] = 0;
      s.r[5] = output;
      s.r[6] = output + 4;
      m.WriteU32(0x80000 + 2616,
                 (guest.gate != 1 || guest.skip) ? 0 : 0x41200000);
      for (unsigned tag = 0; tag < 128; ++tag)
        m.WriteU32(0x72000 + 4 * (128 * 15 + tag + 3), tag == 83 ? 1 : 0);
      check(battle_script_pickers61::Apply(e, m, {guest, native}, s));
      check(s.r[1] == initial.r[1] && s.r[21] == initial.r[21] &&
            s.r[31] == initial.r[31]);
    };
    m.WriteU32(0x90000 + 232, 3);
    run(0x82aff4e8);
    check(s.r[3] == 1 && m.ReadU32(0x72000 + 4 * (128 * 15 + 90 + 3)) == 1 &&
          m.ReadU32(output) == 8 && m.ReadU32(output + 4) == 120);
    m.WriteU32(0x90000 + 232, 1);
    for (unsigned i = 0; i < 5; ++i) {
      guest.enabled = i == 0   ? 284
                      : i == 1 ? 352
                      : i == 2 ? 288
                      : i == 3 ? 344
                               : 420;
      run(0x82aff4e8);
      check(s.r[3] == 1 &&
            m.ReadU32(0x72000 + 4 * (128 * 15 +
                                     (i < 2    ? 91
                                      : i == 2 ? 92
                                      : i == 3 ? 93
                                               : 94) +
                                     3)) == 1 &&
            m.ReadU32(output + 4) == (i < 2    ? 100
                                      : i == 2 ? 105
                                      : i == 3 ? 110
                                               : 115));
    }
    m.WriteU32(0x90000 + 232, 3);
    guest.enabled = 0;
    guest.skip = 5;
    run(0x82aff4e8);
    check(s.r[3] == 1 && m.ReadU32(0x72000 + 4 * (128 * 15 + 95 + 3)) == 1);
    guest.skip = 0;
    run(0x82aff8e0);
    check(s.r[3] == 1 && m.ReadU32(0x72000 + 4 * (128 * 15 + 100 + 3)) == 1 &&
          m.ReadU32(output + 4) == 200 && m.ReadU32(actor + 336) == 4);
    m.WriteU32(0xa0000 + 96 * 300 + 8, 8);
    run(0x82aff9e8);
    check(s.r[3] == 1 && m.ReadU32(0x72000 + 4 * (128 * 15 + 101 + 3)) == 1 &&
          m.ReadU32(output + 4) == 300 && m.ReadU32(actor + 336) == 1 &&
          m.ReadU32(actor + 84) == 2);
    m.WriteU32(0xa0000 + 96 * 300 + 8, 0);
    run(0x82aff9e8);
    check(s.r[3] == 1 && m.ReadU32(0x72000 + 4 * (128 * 15 + 83 + 3)) == 2 &&
          m.ReadU32(actor + 84) == 1 && m.ReadU8(0x65000) == 25);
    guest.gate = 2;
    run(0x82aff8e0);
    check(s.r[3] == 0);
    guest.gate = 1;
    guest.predicate = 0;
    run(0x82aff8e0);
    check(s.r[3] == 0);
    m.WriteU32(actor + 4, 0);
    run(0x82aff4e8);
    check(s.r[3] == 0);
    std::cout << "battle_script_pickers61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
