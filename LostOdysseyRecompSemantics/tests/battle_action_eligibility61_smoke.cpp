#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_action_eligibility61.h"
#include <iostream>
struct EligibilityGuest final : manager_release_context61::GuestServices {
  unsigned resultByte = 1, otherByte = 0, evaluated = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x70000;
      return;
    }
    throw std::runtime_error("eligibility direct");
  }
  void CallIndirect(GuestAddress e, GuestMemory &m,
                    manager_release_context61::Registers &s) override {
    if (e != 0x123456 || s.ctr != e || s.r[3] != 0x73000)
      throw std::runtime_error("eligibility indirect");
    ++evaluated;
    m.WriteU8(0x73000 + 208, resultByte);
    m.WriteU8(0x73000 + 76, otherByte);
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x831f3000, 0x21000});
    regions.push_back({0x83291000, 0x1000});
    regions.push_back({0x83264000, 0x1000});
    regions.push_back({0x83213000, 0x1000});
    regions.push_back({0x832c9000, 0x4000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    EligibilityGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("eligibility state");
    };
    m.WriteU32(0x83264984, 0x100000);
    m.WriteU32(0x832649c0, 0x110000);
    m.WriteU32(0x83264978, 0x120000);
    m.WriteU32(0x832ca0d0, 0x74000);
    m.WriteU32(0x74000 + 132, 0x130000);
    m.WriteU32(0x73000 + 4 * 124, 0x123456);
    m.WriteU32(0x90000 + 132, 1);
    m.WriteU32(0x832ca0d8, 0x73000);
    m.WriteU32(0x832c9c54 + 44, 0x63000);
    m.WriteU32(0x63004, 0x62000);
    m.WriteU32(0x6300c, 1);
    m.WriteU32(0x62008, 25);
    m.WriteU32(0x90000 + 64, 25);
    m.WriteU32(0x83213438, 0);
    m.WriteU32(0x8321343c, 4);
    auto run = [&](unsigned kind) {
      s.r[3] = 0x70000;
      s.r[4] = 0x80000;
      s.r[5] = 0x90000;
      s.r[6] = kind;
      s.r[7] = 9;
      check(battle_action_eligibility61::Apply(0x82ad0c10, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[27] == initial.r[27] &&
            s.r[31] == initial.r[31]);
      return unsigned(s.r[3]);
    };
    for (unsigned kind = 0; kind < 33; ++kind)
      check(run(kind) == unsigned(kind < 32 && kind != 17));
    m.WriteU32(0x90000 + 232, 4);
    check(!run(1) && !run(12) && run(0) == 1);
    s.r[3] = 0x90000;
    check(battle_action_eligibility61::Apply(0x8238abe0, m, {g, native}, s) &&
          s.r[3] == 4);
    m.WriteU32(0x90000 + 232, 0);
    m.WriteU16(0x70000 + 148, 32);
    m.WriteU32(0x62000 + 64, 0x4000);
    check(!run(1) && !run(3) && run(2) == 1);
    m.WriteU32(0x80000 + 68, 7);
    check(run(1) == 1 && !run(3));
    m.WriteU16(0x70000 + 148, 0);
    m.WriteU32(0x90000 + 132, 0);
    auto evaluated = g.evaluated;
    check(!run(2) && g.evaluated == evaluated);
    m.WriteU32(0x90000 + 132, 1);
    g.resultByte = 0;
    check(!run(2));
    g.otherByte = 1;
    check(run(2) == 1);
    // Exercise the actual category setup and leaf evaluator dispatch
    // separately.
    auto setup = [&](unsigned kind, unsigned detail) {
      s.r[3] = 0x73000;
      s.r[4] = 0x80000;
      s.r[5] = 0x90000;
      s.r[6] = kind;
      s.r[7] = detail;
      check(battle_action_eligibility61::Apply(0x82b14168, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[24] == initial.r[24]);
      return unsigned(s.r[3]);
    };
    m.WriteU32(0x120000 + 196 * 2 + 48, 5);
    for (auto kind : {2u, 3u, 6u, 7u, 8u, 9u, 10u, 11u, 0u}) {
      check(setup(kind, 2) == 1);
      check(m.ReadU32(0x73004) == 0x80000 && m.ReadU32(0x73010) == 0x90000);
      if (kind == 2 || (kind >= 6 && kind <= 9))
        check(m.ReadU32(0x73000 + 44) == 0x100000 + 192);
      if (kind == 3)
        check(m.ReadU32(0x73000 + 48) == 0x110000 + 208);
      if (kind == 10)
        check(m.ReadU32(0x73000 + 52) == 0x130000 + 136);
      if (kind == 11)
        check(m.ReadU32(0x73000 + 44) == 0x100000 + 480);
      auto prior = g.evaluated;
      s.r[3] = 0x73000;
      check(battle_action_eligibility61::Apply(0x82b120e0, m, {g, native}, s));
      check(g.evaluated == prior + unsigned(kind != 0));
      if (kind == 0)
        check(m.ReadU8(0x73000 + 208) == 0);
    }
    m.WriteU32(0x90000 + 232, 4);
    check(!setup(2, 2));
    for (auto descriptor : {7u, 18u}) {
      m.WriteU32(0x100000 + 192 + 32, descriptor);
      check(setup(2, 2) == 1);
    }
    m.WriteU32(0x90000 + 132, 0);
    check(!setup(2, 2));
    // Real row initialization, including category-specific untouched fields.
    m.WriteU32(0x90000 + 132, 1);
    m.WriteU32(0x90000 + 232, 0);
    for (unsigned kind = 0; kind <= 32; ++kind) {
      bool skill = kind == 2 || (kind >= 6 && kind <= 9) || kind == 15;
      bool item = kind == 3 || kind == 5 || (kind >= 25 && kind <= 29);
      bool special = kind == 10, inventory = kind == 11;
      unsigned row = item ? 0x110000 : special ? 0x130000 : 0x100000;
      unsigned start = item ? 48 : special ? 20 : 36;
      m.WriteU32(0x73000 + 44, 0x100000);
      m.WriteU32(0x73000 + 48, 0x110000);
      m.WriteU32(0x73000 + 52, 0x130000);
      m.WriteU32(0x73004, 0x80000);
      for (unsigned i = 0; i < 8; ++i)
        m.WriteU32(row + start + 4 * i, 100 + i);
      m.WriteU32(row + (item ? 24 : 16), 12);
      m.WriteU32(row + (special ? 4 : 12), 8);
      m.WriteU32(row + (item ? 92 : 88), 123);
      m.WriteU32(0x73000 + 84, 0x3f800000);
      m.WriteU32(0x73000 + 164, 0xdeadbeef);
      s.r[3] = 0x73000;
      s.r[4] = kind;
      check(battle_action_eligibility61::Apply(0x82b121b0, m, {g, native}, s));
      check(s.r[1] == initial.r[1]);
      check(m.ReadU32(0x73000 + 132) ==
            (skill || item || special || inventory ? 100u : 0u));
      check(m.ReadU32(0x73000 + 100) ==
            (skill || item || special || inventory ? 101u : 0u));
      check(m.ReadU32(0x73000 + 212) == (skill || item ? 12u : 0u));
      if (inventory || !(skill || item || special))
        check(m.ReadU32(0x73000 + 84) == 0x3f800000);
      if (!(skill || item || special || inventory))
        check(m.ReadU32(0x73000 + 164) == 0xdeadbeef);
    }
    m.WriteU32(0x83264558, 0x76000);
    m.WriteU32(0x80000 + 64, 25);
    m.WriteU32(0x831f3300, 2);
    m.WriteU32(0x100000 + 12, 64);
    s.r[3] = 0x73000;
    s.r[4] = 2;
    check(battle_action_eligibility61::Apply(0x82b121b0, m, {g, native}, s));
    check(m.ReadU32(0x73000 + 124) == 4 && m.ReadU32(0x73000 + 164) == 64);
    auto relation = [&](unsigned kind, unsigned deadCheck,
                        unsigned overrideFlag) {
      s.r[3] = 0x70000;
      s.r[4] = 0x80000;
      s.r[5] = 0x90000;
      s.r[6] = kind;
      s.r[7] = deadCheck;
      s.r[8] = overrideFlag;
      check(battle_action_eligibility61::Apply(0x82ace408, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[24] == initial.r[24]);
      return unsigned(s.r[3]);
    };
    m.WriteU32(0x90000 + 232, 0);
    m.WriteU32(0x80000 + 232, 0);
    for (unsigned left = 0; left < 2; ++left)
      for (unsigned right = 0; right < 2; ++right) {
        m.WriteU32(0x80000 + 124, left << 28);
        m.WriteU32(0x90000 + 124, right << 28);
        for (unsigned kind = 0; kind < 5; ++kind)
          check(relation(kind, 1, 0) ==
                unsigned(kind == 0 ||
                         ((kind == 1 || kind == 3) && left != right) ||
                         (kind == 2 && left == right)));
      }
    m.WriteU32(0x90000 + 232, 4);
    check(!relation(0, 1, 0) && relation(0, 0, 0));
    check(relation(4, 1, 1));
    m.WriteU32(0x8321343c + 8 * 18, 8);
    m.WriteU32(0x80000 + 7 * 272 + 232, 8);
    check(relation(4, 1, 0));
    check(!battle_action_eligibility61::Apply(0, m, {g, native}, s));
    std::cout << "battle_action_eligibility61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
