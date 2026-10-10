#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_action_records61.h"
#include "battle_action_record_fixture.h"
#include <iostream>
struct RecordGuest final : manager_release_context61::GuestServices {
  unsigned configured = 0, marked = 0, advanced = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (ActionStorageDirectFixture(e, m, s))
      return;

    if (e == 0x82acd3b0 || e == 0x82b21340 || e == 0x82b11df0 ||
        e == 0x82b1f798)
      return;

    if (e == 0x82acde40) {
      if (s.r[4] != 0x80000)
        throw std::runtime_error("record configuration resource");
      ++configured;
      return;
    }
    if (e == 0x82af68d8) {
      if (s.r[3] != 0x832c9c54 || s.r[4] != 0x80000)
        throw std::runtime_error("record actor marker");
      ++marked;
      return;
    }
    if (e == 0x82b1f1d0) {
      ++advanced;
      return;
    }
    throw std::runtime_error("unexpected record service");
  }
  void CallIndirect(GuestAddress e, GuestMemory &m,
                    manager_release_context61::Registers &s) override {
    if (ActionStorageIndirectFixture(e, m, s))
      return;
    throw std::runtime_error("unexpected record indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x832c9000, 0x4000});
    regions.push_back({0x83245000, 0x1000});
    regions.push_back({0x8330b000, 0x1000});
    regions.push_back({0x821a8000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    SetupActionStorageFixture(m);
    RecordGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    m.WriteU32(0x832c9c54 + 44, 0x63000);
    m.WriteU32(0x63000 + 4, 0x62000);
    m.WriteU32(0x63000 + 12, 1);
    m.WriteU32(0x62000 + 8, 24);
    m.WriteU32(0x62000 + 60, 2);
    m.WriteU32(0x62000 + 336, 3);
    m.WriteU32(0x80000 + 64, 24);
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("record state");
    };
    auto run = [&](unsigned entry, unsigned a, unsigned b, unsigned c,
                   unsigned index) {
      s.r[3] = 0x80000;
      s.r[4] = a;
      s.r[5] = b;
      s.r[6] = c;
      s.r[7] = index;
      check(battle_action_records61::Apply(entry, m, {guest, native}, s));
      check(s.r[1] == initial.r[1] && s.r[25] == initial.r[25] &&
            s.r[31] == initial.r[31]);
    };
    run(0x82ab36c8, 7, 8, 25, 0);
    check(m.ReadU32(0x100000) == 7 && m.ReadU32(0x100004) == 8 &&
          m.ReadU32(0x100000 + 36) == 24 && m.ReadU32(0x100000 + 14884) == 25 &&
          m.ReadU32(0x100000 + 20) == 1 && m.ReadU32(0x100000 + 16) == 1 &&
          m.ReadU32(0x100000 + 24) == 2 && m.ReadU32(0x100000 + 28) == 3 &&
          (m.ReadU32(0x80000 + 124) & 512));
    run(0x82ab36c8, 0xffffffff, 0xffffffff, 26, 0);
    check(m.ReadU32(0x100000 + 14884 + 464) == 26 &&
          m.ReadU32(0x100000 + 16) == 2);
    run(0x82ab0b28, 30, 0, 1, 0);
    check(m.ReadU32(0x100000 + 36 + 464) == 30 &&
          (m.ReadU32(0x100000 + 248 + 464) & 0x80000000));
    run(0x82ab0b98, 27, 0, 1, 0);
    check(m.ReadU32(0x100000 + 14884 + 928) == 27 &&
          m.ReadU32(0x100000 + 20) == 3 && m.ReadU32(0x100000 + 16) == 2 &&
          m.ReadU32(0x100000 + 32) == 1 &&
          (m.ReadU32(0x100000 + 15096 + 928) & 0x80000000));
    run(0x82ab0b98, 27, 0, 1, 0);
    check(m.ReadU32(0x100000 + 20) == 3);
    m.WriteU32(0x80000 + 14680, 9);
    run(0x82ab38f0, 7, 8, 28, 1);
    auto next = 0x100000 + 124208;
    check(!m.ReadU32(0x80000 + 14680) && m.ReadU32(next) == 7 &&
          m.ReadU32(next + 14884) == 28 &&
          (m.ReadU32(0x62000 + 64) & 96) == 96 &&
          m.ReadU32(0x100000 + 296) == 1 && m.ReadU32(0x100000 + 15144) == 1);
    run(0x82ab38f0, 0xffffffff, 0xffffffff, 29, 1);
    check(m.ReadU32(next + 16) == 2);
    m.WriteU32(0x80000 + 112, 99);
    m.WriteU32(0x80000 + 116, 99);
    run(0x82ab36c8, 0xffffffff, 0xffffffff, 0xffffffff, 0);
    check((m.ReadU32(0x62000 + 64) & 0x20000000) &&
          m.ReadU32(0x80000 + 60) == 1 && !m.ReadU32(0x80000 + 112) &&
          !m.ReadU32(0x80000 + 116));
    m.WriteU32(0x80000 + 60, 0);
    run(0x82ab38f0, 0xffffffff, 0xffffffff, 0xffffffff, 1);
    check((m.ReadU32(0x62000 + 64) & 0x20000000) &&
          m.ReadU32(0x80000 + 60) == 1);
    m.WriteU32(0x80000 + 60, 2);
    run(0x82ab36c8, 0xffffffff, 0xffffffff, 0xffffffff, 0);
    check(m.ReadU32(0x80000 + 60) == 2 && guest.configured == 2);
    std::cout << "battle_action_records61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
