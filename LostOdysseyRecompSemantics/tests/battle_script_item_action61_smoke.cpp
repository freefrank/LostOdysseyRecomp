#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_item_action61.h"
#include <iostream>
#include "battle_action_record_fixture.h"
struct ItemGuest final : manager_release_context61::GuestServices {
  unsigned selected = 0, prepared = 0, executed = 0, last = 0, random = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (ActionStorageDirectFixture(e, m, s))
      return;

    if (e == 0x82acd3b0 || e == 0x82b21340 || e == 0x82b11df0 ||
        e == 0x82b1f798)
      return;
    if (e == 0x82aa0740) {
      if (s.r[3] != 0x72000 || s.r[4] != 0 || s.r[5] != 1 || s.r[6] != 88 ||
          s.r[7] != 24)
        throw std::runtime_error("item random ABI");
      ++random;
      s.r[3] = 0;
      return;
    }

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
    if (e == 0x82ac9a28) {
      s.r[3] = 0;
      return;
    }
    if (e == 0x82acee70)
      return;
    if (e == 0x82acde40) {
      if (s.r[4] != 0x80000 || m.ReadU32(0x62000 + 88) != 11)
        throw std::runtime_error("item configuration ABI");
      selected = m.ReadU32(0x62000 + 92);
      if (selected)
        ++prepared;
      ++executed;
      auto mode = m.ReadU32(0x62000 + 60);
      last = s.r[6] == 0 ? 0x82b00698 : mode == 2 ? 0x82afdcf0 : 0x82afdb90;
      return;
    }
    if (e == 0x82b1f1d0)
      return;
    throw std::runtime_error("unexpected item service");
  }
  void CallIndirect(GuestAddress e, GuestMemory &m,
                    manager_release_context61::Registers &s) override {
    if (ActionStorageIndirectFixture(e, s))
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
    check(!m.ReadU32(vars) && guest.selected == 3 &&
          m.ReadU32(state + 200 + 12) == 99);
    m.WriteU32(vars + 12, 1);
    op();
    check(guest.selected == 3);
    m.WriteU32(vars + 12, 2);
    op();
    check(guest.selected == 2 && guest.random == 1);
    m.WriteU32(vars + 12, 3);
    auto prepared = guest.prepared;
    op();
    check(guest.selected == 0 && guest.prepared == prepared &&
          !m.ReadU32(vars));
    m.WriteU32(vars + 8, 3);
    m.WriteU32(vars + 12, 77);
    m.WriteU32(actor + 60, 2);
    op();
    check(guest.selected == 77 && guest.last == 0x82afdcf0);
    m.WriteU32(actor + 60, 3);
    op();
    check(guest.last == 0x82afdb90);
    m.WriteU32(actor + 96, 1);
    auto executed = guest.executed;
    op(true);
    check(m.ReadU32(vars) == 1 && guest.executed == executed);
    m.WriteU32(actor + 64, 0x01000000);
    op(true);
    check(!m.ReadU32(vars));
    m.WriteU32(vars + 4, 2);
    executed = guest.executed;
    op();
    check(m.ReadU32(vars) == 1 && guest.executed == executed);
    m.WriteU32(actor + 96, 0);
    m.WriteU32(vars + 4, 1);
    m.WriteU32(vars + 8, 0);
    op();
    check(m.ReadU32(vars) == 1 && (m.ReadU32(actor + 64) & 0x2000));
    m.WriteU32(state + 8260 + 200 + 8, 1);
    m.WriteU32(vars + 12, 0);
    op();
    check(!m.ReadU32(vars) && guest.selected == 2);
    std::cout << "battle_script_item_action61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
