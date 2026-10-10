#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_semantic_runtime61.h"
#include <iostream>
struct RuntimeGuest final : manager_release_context61::GuestServices {
  unsigned direct = 0, indirect = 0;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e != 0x82b5cf80 || s.r[3] != 0xb0000 || s.r[4] != 3)
      throw std::runtime_error("unexpected runtime direct escape");
    ++direct;
    s.r[3] = 0;
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e != 0x123450 || s.r[3] != 0x90000)
      throw std::runtime_error("unexpected runtime indirect escape");
    ++indirect;
    s.r[3] = 0x92000;
    s.r[10] = 77;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (auto p : {0x832ca000u, 0x832cb000u, 0x832c1000u, 0x83264000u,
                   0x83315000u, 0x832d2000u})
      regions.push_back({p, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    RuntimeGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("battle runtime state");
    };
    auto run = [&](unsigned e, unsigned owner) {
      s.r[3] = owner;
      check(battle_semantic_runtime61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[31] == initial.r[31]);
    };
    m.WriteU32(0x60000 + 24, 0x62000);
    m.WriteU32(0x62000 + 64, 8);
    m.WriteU32(0x62000 + 468, 42);
    m.WriteU32(0x62000 + 16, 2);
    m.WriteU32(0x62000 + 20, 0x65000);
    m.WriteU32(0x65008, 123);
    m.WriteU32(0x62000 + 40, 0x66000);
    for (unsigned i = 0; i < 16; ++i)
      m.WriteU32(0x66000 + 24 * i, 255);
    m.WriteU32(0x832cb788, 0x70000);
    m.WriteU32(0x832ca0e8 + 20, 0x71000);
    m.WriteU32(0x71000, 0x72000);
    m.WriteU32(0x71004, 1);
    m.WriteU32(0x72000, 0x80000);
    m.WriteU32(0x80000 + 64, 42);
    m.WriteU32(0x80000 + 60, 5);
    // Completion formerly escaped through three independently recovered
    // getters.
    run(0x8238b850, 0x60000);
    check(!m.ReadU32(0x62000 + 64) && m.ReadU32(0x62000 + 468) == 0xffffffff &&
          m.ReadU32(0x66000 + 24 * 15) == 16 &&
          m.ReadU32(0x66000 + 24 * 15 + 4) == 123 && !g.direct && !g.indirect);
    m.WriteU32(0x83315fb4, 0x90000);
    m.WriteU32(0x90000, 0x91000);
    m.WriteU32(0x90000 + 20, 0x92000);
    m.WriteU32(0x91000 + 352, 0x83081540);
    m.WriteU32(0x92000 + 52, 0x94000);
    m.WriteU32(0x832c1764, 0x94000);
    m.WriteU32(0x92000 + 68, 2);
    m.WriteU32(0x83264978 + 128, 0x95000);
    // A known guest vtable target is intercepted as well as direct calls.
    run(0x82389aa0, 0);
    check(s.r[3] == 0x95140 && !g.indirect);
    m.WriteU32(0x91000 + 352, 0x123450);
    run(0x82389aa0, 0);
    check(s.r[3] == 0x95140 && s.r[10] == 77 && g.indirect == 1);
    m.WriteU32(0x832d268c, 0xb0000);
    m.WriteU32(0xa0000, 99);
    run(0x82b63828, 0xa0000);
    check(!s.r[3] && !m.ReadU32(0xa0000) && g.direct == 1);
    auto before = s.r[3];
    check(!battle_semantic_runtime61::Apply(0xdeadc0de, m, {g, native}, s) &&
          s.r[3] == before && s.r[1] == initial.r[1]);
    std::cout << "battle semantic runtime composition smoke passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
