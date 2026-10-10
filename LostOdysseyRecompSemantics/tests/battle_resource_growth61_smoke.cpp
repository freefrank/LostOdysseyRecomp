#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_resource_growth61.h"
#include "lo_semantics/recovery_abi.h"
#include "battle_resource_growth_fixture.h"
#include <iostream>
struct GrowthGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress, GuestMemory &,
                  manager_release_context61::Registers &) override {
    throw std::runtime_error("growth direct boundary");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("growth indirect boundary");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    growth_fixture::Regions(regions);
    regions.push_back({0x832ca000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    growth_fixture::Setup(m);
    GrowthGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned resource = 0x80000, owner = 0x74000;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("growth state");
    };
    auto get = [&](unsigned off) {
      return std::bit_cast<float>(m.ReadU32(resource + off));
    };
    auto put = [&](unsigned off, float v) {
      m.WriteU32(resource + off, std::bit_cast<unsigned>(v));
    };
    auto run = [&](unsigned e, unsigned level = 0) {
      s.r[3] = owner;
      s.r[4] = resource;
      s.r[5] = level;
      check(battle_resource_growth61::Apply(e, m, {guest, native}, s));
      check(s.r[1] == initial.r[1] && s.r[27] == initial.r[27] &&
            s.r[31] == initial.r[31]);
    };
    m.WriteU32(resource + 68, 2);
    m.WriteU32(resource + 140, 9);
    m.WriteU32(0x100000 + 408 + 160, 7);
    run(0x82ac0588);
    check(get(2412) == 10 && get(2464) == 10 && m.ReadU32(resource + 152) == 7);
    run(0x82ac25e8);
    check(get(2472) == 100 && get(2500) == 50 && get(2476) == 2 &&
          get(2480) == 2 && get(2492) == 10 && get(2504) == 20);
    put(2412, -100);
    run(0x82ac25e8);
    check(get(2472) == 1);
    run(0x82ac0588);
    m.WriteU32(resource + 140, 100);
    run(0x82ac25e8);
    check(get(2472) == 100);
    auto row = 0x140000 + 280;
    m.WriteU32(row, 7);
    m.WriteU32(row + 56, 0x1234);
    m.WriteU32(row + 68, 0x200);
    m.WriteU32(row + 72, 19);
    m.WriteU32(row + 80, 20);
    m.WriteU32(row + 84, 21);
    m.WriteU32(row + 88, 22);
    m.WriteU32(row + 76, 23);
    m.WriteU32(resource + 5132, 24);
    m.WriteU32(row + 120, 31);
    m.WriteU32(row + 124, 32);
    m.WriteU32(row + 128, 33);
    run(0x82ac3820);
    check(m.ReadU32(resource + 140) == 7 && get(2588) == 100 &&
          get(2616) == 50 && m.ReadU32(resource + 4880) == 0x1234 &&
          m.ReadU32(resource + 4876) == 0x200 && m.ReadU32(owner + 28) == 24 &&
          m.ReadU32(resource + 5156) == 3 && m.ReadU32(resource + 5168) == 33);
    // Optional template plus first-set archetype plus creature adjustments.
    m.WriteU32(row + 60, 0x8001);
    m.WriteU32(row + 4, 2);
    m.WriteU32(row + 8, 3);
    run(0x82ac3820, 9);
    check(m.ReadU32(resource + 140) == 9 && get(2412) == 22 &&
          get(2440) == 23 && get(2416) == 20);
    check(!battle_resource_growth61::Apply(0, m, {guest, native}, s));
    std::cout << "battle resource growth logic smoke passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
