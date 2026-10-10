#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_item_action61.h"
#include <iostream>
#include "battle_action_record_fixture.h"
struct ItemGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (ActionStorageDirectFixture(e, m, s))
      return;

    if (e == 0x82af68d8)
      return;
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x8238e308) {
      s.r[3] = 0x80000;
      return;
    }
    if (e == 0x82b1f1d0)
      return;
    throw std::runtime_error("unexpected item service");
  }
  void CallIndirect(GuestAddress e, GuestMemory &m,
                    manager_release_context61::Registers &s) override {
    if (ActionStorageIndirectFixture(e, m, s))
      return;
    throw std::runtime_error("unexpected item indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83264000, 0x1000});
    regions.push_back({0x83245000, 0x1000});
    regions.push_back({0x832c9000, 0x4000});
    regions.push_back({0x8330b000, 0x1000});
    regions.push_back({0x821a8000, 0x1000});
    regions.push_back({0x83213000, 0x1000});
    regions.push_back({0x831f3000, 0x21000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    SetupActionStorageFixture(m);
    ItemGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       code = 0x69000, vars = 0x6a000, resource = 0x80000,
                       table = 0xa0000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(0x832c9c54 + 44, state);
    m.WriteU32(state + 4, actor);
    m.WriteU32(state + 12, 1);
    m.WriteU32(actor + 8, 24);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(actor + 4, resource);
    m.WriteU32(resource + 64, 24);
    m.WriteU32(state + 4, actor);
    m.WriteU32(state + 12, 1);
    m.WriteU32(actor + 8, 24);
    m.WriteU32(actor + 80, 0x6b000);
    m.WriteU32(actor + 84, 1);
    m.WriteU8(0x6b000, 24);
    m.WriteU32(0x83264978, table);
    m.WriteU32(0x83264558, 0x72000);
    for (unsigned i = 0; i < 4; ++i) {
      m.WriteU8(code + 1 + 2 * i, i);
      m.WriteU8(code + 2 + 2 * i, 0);
    }
    m.WriteU32(vars + 4, 0);
    m.WriteU32(vars + 8, 0);
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("item selection result");
    };
    auto op = [&](bool preserveBusy = false) {
      if (!preserveBusy)
        m.WriteU32(actor + 96, 0);
      s.r[3] = owner;
      m.WriteU32(actor + 52, 0);
      check(battle_script_item_action61::Apply(0x82b009b0, m, {guest, native},
                                               s));
      check(m.ReadU32(actor + 52) == 9 && s.r[1] == initial.r[1] &&
            s.r[26] == initial.r[26] && s.r[31] == initial.r[31]);
    };
    for (unsigned i : {2u, 3u}) {
      m.WriteU32(state + 200 + 4 * i, 100);
      m.WriteU32(table + 196 * i + 148, 1);
      m.WriteU32(table + 196 * i + 152, 20);
    }
    m.WriteU32(vars + 12, 0);
    op();
    check(!m.ReadU32(vars) && m.ReadU32(actor + 92) == 3 &&
          m.ReadU32(state + 200 + 12) == 99);
    m.WriteU32(vars + 12, 1);
    op();
    check(m.ReadU32(actor + 92) == 3);
    m.WriteU32(vars + 12, 2);
    op();
    check(m.ReadU32(actor + 92) == 2 &&
          m.ReadU32(0x72000 + 4 * (128 * 15 + 88 + 3)) == 1);
    m.WriteU32(vars + 12, 3);
    auto prepared = m.ReadU32(actor + 336);
    op();
    check(m.ReadU32(actor + 92) == 0 && m.ReadU32(actor + 336) == prepared &&
          !m.ReadU32(vars));
    m.WriteU32(vars + 8, 3);
    m.WriteU32(vars + 12, 77);
    m.WriteU32(actor + 60, 2);
    op();
    check(m.ReadU32(actor + 92) == 77 &&
          m.ReadU32(resource + 156) == 0xfffffffe);
    m.WriteU32(actor + 60, 3);
    op();
    check(m.ReadU32(resource + 156) != 0xfffffffe);
    m.WriteU32(actor + 96, 1);
    auto executed = m.ReadU32(0x72000 + 4 * (128 * 15 + 110 + 3));
    op(true);
    check(m.ReadU32(vars) == 1 &&
          m.ReadU32(0x72000 + 4 * (128 * 15 + 110 + 3)) == executed);
    m.WriteU32(actor + 64, 0x01000000);
    op(true);
    check(!m.ReadU32(vars));
    m.WriteU32(vars + 4, 2);
    executed = m.ReadU32(0x72000 + 4 * (128 * 15 + 110 + 3));
    op();
    check(m.ReadU32(vars) == 1 &&
          m.ReadU32(0x72000 + 4 * (128 * 15 + 110 + 3)) == executed);
    m.WriteU32(actor + 96, 0);
    m.WriteU32(vars + 4, 1);
    m.WriteU32(vars + 8, 0);
    op();
    check(m.ReadU32(vars) == 1 && (m.ReadU32(actor + 64) & 0x2000));
    m.WriteU32(state + 8260 + 200 + 8, 1);
    m.WriteU32(vars + 12, 0);
    op();
    check(!m.ReadU32(vars) && m.ReadU32(actor + 92) == 2);
    std::cout << "battle_script_item_action61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
