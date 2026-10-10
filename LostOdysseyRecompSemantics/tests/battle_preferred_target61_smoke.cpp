#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_preferred_target61.h"
#include "lo_semantics/battle_random_range61.h"
#include <iostream>
struct PreferredGuest final : manager_release_context61::GuestServices {
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
      if (s.r[4] != 0)
        throw std::runtime_error("preferred random owner");
      s.r[3] = 0x80000;
      return;
    }
    throw std::runtime_error("preferred direct");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("preferred indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83213000, 0x1000});
    regions.push_back({0x83264000, 0x1000});
    regions.push_back({0x831f3000, 0x21000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    PreferredGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("preferred state");
    };
    m.WriteU32(0x83264558, 0x72000);
    m.WriteU32(0x71000, 0x71100);
    m.WriteU32(0x71004, 4);
    m.WriteU32(0x83213428, 49);
    m.WriteU32(0x83213438, 0);
    m.WriteU32(0x8321343c, 1);
    m.WriteU32(0x831f3300, 1);
    m.WriteU32(0x831f3304, 1);
    for (unsigned i = 0; i < 4; ++i) {
      auto resource = 0x80000 + 0x10000 * i;
      m.WriteU32(0x71100 + 4 * i, resource);
      m.WriteU32(resource + 64, 24 + i);
      m.WriteU32(resource + 124, 0x10000000u | (i % 2 ? 0 : 0x40000000u));
      m.WriteU32(resource + 132, 1);
      m.WriteU32(resource + 2588, 0x3f800000);
    }
    auto run = [&] {
      s.r[3] = 0x60000;
      check(battle_preferred_target61::Apply(0x82ac8228, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[19] == initial.r[19] &&
            s.fpr_bits[31] == initial.fpr_bits[31]);
      return unsigned(s.r[3]);
    };
    check(run() == 27 && m.ReadU32(0x72000 + 4 * (64 + 3)) == 1);
    m.WriteU32(0x83213428, 0);
    check(run() == 26 && m.ReadU32(0x72000 + 4 * (65 + 3)) == 1);
    m.WriteU32(0x90000 + 124, 0);
    m.WriteU32(0xb0000 + 124, 0);
    check(run() == 26 && m.ReadU32(0x72000 + 4 * (62 + 3)) == 1);
    m.WriteU32(0xa0000 + 2588, 0x7fc00000);
    check(run() == 24);
    m.WriteU32(0x80000 + 132, 0);
    m.WriteU32(unsigned(s.r[1]) - 144, 123);
    check(run() == 123);
    // The probability wrapper uses threshold >= draw, including equality.
    s.r[3] = 0x72000;
    s.r[4] = 1;
    s.r[5] = 66;
    s.r[6] = 0;
    check(battle_random_range61::Apply(0x82aa0838, m, {g, native}, s) &&
          s.r[3] == 1);
    check(!battle_preferred_target61::Apply(0, m, {g, native}, s));
    std::cout << "battle_preferred_target61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
