#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_execution61.h"
#include "battle_action_record_fixture.h"
#include <iostream>
struct ExecutionGuest final : manager_release_context61::GuestServices {
  unsigned predicate = 0, configured = 0, finalized = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
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
      s.r[3] = s.r[4] == 24 ? 0x80000 : s.r[4] == 25 ? 0x90000 : 0xa0000;
      return;
    }
    if (e == 0x82ac9a28) {
      s.r[3] = predicate;
      return;
    }
    if (e == 0x82acee70)
      return;
    if (e == 0x82ab31e0 || e == 0x82ab2d88) {
      InitializeActionRecordFixture(m, unsigned(s.r[3]), e == 0x82ab31e0);
      return;
    }
    if (e == 0x82ab0d50) {
      if (s.r[3] != 0x80000)
        throw std::runtime_error("execution configuration resource");
      ++configured;
      return;
    }
    if (e == 0x82af68d8)
      return;
    if (e == 0x82b1f1d0) {
      ++finalized;
      return;
    }
    throw std::runtime_error("unexpected execution service");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("unexpected execution indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83245000, 0x1000});
    regions.push_back({0x83213000, 0x1000});
    regions.push_back({0x832c9000, 0x4000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    ExecutionGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       resource = 0x80000, record = 0x100000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(0x832c9c54 + 44, state);
    m.WriteU32(actor + 4, resource);
    m.WriteU32(actor + 8, 24);
    m.WriteU32(actor + 80, 0x64000);
    m.WriteU8(0x64000, 25);
    m.WriteU8(0x64001, 26);
    m.WriteU32(actor + 84, 2);
    m.WriteU32(state + 4, actor);
    m.WriteU32(state + 12, 1);
    m.WriteU32(0x71000, 0x71100);
    m.WriteU32(0x71004, 3);
    for (unsigned i = 0; i < 3; ++i) {
      m.WriteU32(0x71100 + 4 * i, resource + 0x10000 * i);
      m.WriteU32(resource + 0x10000 * i + 64, 24 + i);
    }
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("execution chain result");
    };
    auto run = [&](unsigned entry, unsigned a, unsigned b) {
      s.r[3] = owner;
      s.r[4] = a;
      s.r[5] = b;
      check(battle_script_execution61::Apply(entry, m, {guest, native}, s));
      check(s.r[1] == initial.r[1] && s.r[24] == initial.r[24] &&
            s.r[31] == initial.r[31]);
    };
    run(0x82b00698, 1, 9);
    check(m.ReadU32(record) == 1 && m.ReadU32(record + 4) == 0 &&
          m.ReadU32(record + 14884) == 25 &&
          m.ReadU32(record + 14884 + 464) == 26 &&
          m.ReadU32(record + 16) == 2 && m.ReadU32(actor + 100) == 10 &&
          m.ReadU32(actor + 96) == 1 && guest.finalized == 1);
    m.WriteU32(actor + 96, 0);
    m.WriteU32(actor + 300, 7);
    m.WriteU32(actor + 304, 8);
    m.WriteU32(actor + 308, 13);
    m.WriteU32(actor + 72, 0x65000);
    m.WriteU32(actor + 76, 2);
    m.WriteU8(0x65000, 25);
    m.WriteU8(0x65001, 26);
    run(0x82b00698, 2, 3);
    check(m.ReadU32(record) == 7 && m.ReadU32(record + 4) == 8 &&
          m.ReadU32(record + 16) == 2 && m.ReadU32(actor + 88) == 7);
    m.WriteU32(actor + 300, 0);
    m.WriteU32(actor + 96, 0);
    guest.predicate = 1;
    run(0x82b00698, 7, 8);
    check(m.ReadU32(record) == 0 && m.ReadU32(record + 14884) == 24 &&
          m.ReadU32(record + 16) == 1);
    auto busy = m.ReadU32(actor + 96), configured = guest.configured;
    run(0x82afdcf0, 7, 8);
    check(guest.configured == configured && m.ReadU32(actor + 96) == busy);
    guest.predicate = 0;
    m.WriteU32(actor + 96, 0);
    m.WriteU32(actor + 64, 0x01000000);
    auto finalized = guest.finalized;
    run(0x82afdcf0, 7, 8);
    check(m.ReadU32(record + 16) == 2 && m.ReadU32(actor + 96) == 1 &&
          guest.finalized == finalized && (m.ReadU32(actor + 64) & 96) == 96);
    m.WriteU32(actor + 96, 0);
    run(0x82afdb90, 7, 8);
    check(m.ReadU32(record + 16) == 2 && !m.ReadU32(actor + 96) &&
          guest.finalized == finalized + 1);
    // Ordered membership remains single-pass: order 2 preceding order 1 is not
    // revisited.
    InitializeActionRecordFixture(m, resource, true);
    m.WriteU32(record + 36, 24);
    m.WriteU32(resource + 212, 5);
    m.WriteU32(0x90000 + 212, 5);
    m.WriteU32(0x90000 + 216, 2);
    m.WriteU32(0xa0000 + 212, 5);
    m.WriteU32(0xa0000 + 216, 1);
    m.WriteU32(actor + 84, 0);
    run(0x82afd970, 0, 0);
    check(m.ReadU32(record + 36 + 464) == 26 &&
          m.ReadU32(record + 36 + 928) == 0xffffffff &&
          (m.ReadU32(record + 248 + 464) & 0x80000000));
    InitializeActionRecordFixture(m, resource, true);
    m.WriteU32(record + 36, 24);
    m.WriteU32(record + 14884, 25);
    m.WriteU32(record + 20, 1);
    m.WriteU32(record + 16, 1);
    m.WriteU32(resource + 212, 0);
    for (unsigned p : {resource, 0x90000u, 0xa0000u})
      m.WriteU32(p + 204, 4);
    m.WriteU32(actor + 84, 1);
    run(0x82afd970, 0, 0);
    check(m.ReadU32(record + 36 + 464) == 25 &&
          m.ReadU32(record + 36 + 928) == 26 &&
          m.ReadU32(record + 14884 + 464) == 24 &&
          m.ReadU32(record + 14884 + 928) == 26 &&
          m.ReadU32(record + 20) == 3 && m.ReadU32(record + 16) == 1);
    m.WriteU32(actor + 64, 0x2000);
    auto stock = state + 4 * (2065 + 77 + 50);
    m.WriteU32(stock, 1);
    run(0x82af6d60, 77, resource);
    check(!m.ReadU32(stock));
    run(0x82af6d60, 77, resource);
    check(!m.ReadU32(stock));
    std::cout << "battle_script_execution61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
