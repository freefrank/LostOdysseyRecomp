#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_evaluation_theft61.h"
#include "lo_semantics/battle_action_eligibility61.h"
#include <iostream>
struct TheftGuest final : manager_release_context61::GuestServices {
  unsigned equipment = 0, inventoryNotices = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x76000;
      return;
    }
    if (e == 0x8238e308) {
      s.r[3] = s.r[4] == 24 ? 0x80000 : s.r[4] == 25 ? 0x90000 : 0;
      return;
    }
    if (e == 0x8229dfd8) {
      s.r[3] = 0x180000;
      return;
    }
    if (e == 0x82ab0110) {
      s.r[3] = 0x150000;
      return;
    }
    if (e == 0x82ac0888) {
      ++equipment;
      m.WriteU32(unsigned(s.r[4]) + 2472, 0x42c80000);
      return;
    }
    throw std::runtime_error("theft direct " + std::to_string(e));
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e == 0x123400) {
      s.r[3] = 0x180000;
      return;
    }
    if (e == 0x123404) {
      if (s.r[3] != 0x140000 || s.r[4] != 7 || s.r[5] != 1)
        throw std::runtime_error("inventory notice args");
      ++inventoryNotices;
      return;
    }
    throw std::runtime_error("theft indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (auto p : {0x821a8000u, 0x82218000u, 0x83213000u, 0x83264000u,
                   0x83245000u, 0x83291000u, 0x83315000u, 0x8204b000u})
      regions.push_back({p, 0x1000});
    regions.push_back({0x832c9000, 0x4000});
    regions.push_back({0x831f3000, 0x21000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    TheftGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("theft state");
    };
    for (unsigned i = 0; i < 32; ++i) {
      m.WriteU32(0x83213438 + 8 * i, 0);
      m.WriteU32(0x8321343c + 8 * i, 1u << i);
    }
    m.WriteU32(0x73004, 0x80000);
    m.WriteU32(0x73008, 0x90000);
    m.WriteU32(0x80000 + 64, 24);
    m.WriteU32(0x90000 + 64, 25);
    m.WriteU32(0x80000 + 124, 0x10000000);
    m.WriteU32(0x832cb798, 0x75000);
    m.WriteU32(0x75000 + 16, 0x100000);
    m.WriteU32(0x832cb790, 0x74000);
    m.WriteU32(0x74000 + 20, 0x110000);
    m.WriteU32(0x832ca0d0, 0x7a000);
    m.WriteU32(0x7a000 + 120, 0x120000);
    m.WriteU32(0x83264558, 0x79000);
    m.WriteU32(0x73000 + 88, 0x42c80000);
    m.WriteU32(0x83315fb4, 0x170000);
    m.WriteU32(0x170000, 0x171000);
    m.WriteU32(0x171000 + 352, 0x123400);
    m.WriteU32(0x832c9c54 + 28, 0x140000);
    m.WriteU32(0x140000, 0x141000);
    m.WriteU32(0x141000 + 312, 0x123404);
    m.WriteU32(0x150000, 0x150100);
    m.WriteU32(0x150104, 0x150200);
    m.WriteU32(0x8204bc58, 0x4479c000);
    m.WriteU32(0x83291dc0, 0x7c000);
    m.WriteU32(0x83264978 + 72, 0x220000);
    m.WriteU32(0x82007784, 0x3f800000);
    m.WriteU32(0x822184dc, 0x447a0000);
    m.WriteU32(0x822181e4, 0x447a0000);
    m.WriteU32(0x822182a0, 0x42c60000);
    m.WriteU32(0x90000 + 2588, 0x42c80000);
    m.WriteU32(0x83264978, 0x200000);
    m.WriteU32(0x83264978 + 116, 0x160000);
    auto run = [&]() {
      s.r[3] = 0x73000;
      check(battle_evaluation_theft61::Apply(0x82b13380, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[22] == initial.r[22] &&
            s.r[31] == initial.r[31]);
    };
    run();
    check(m.ReadU32(0x75000 + 24) == 255);
    m.WriteU32(0x73000 + 88, 0xbf800000);
    run();
    check(m.ReadU32(0x75000 + 24) == 254);
    m.WriteU32(0x73000 + 88, 0x42c80000);
    m.WriteU32(0x120000 + 132, 7);
    run();
    check(m.ReadU32(0x75000 + 24) == 2 &&
          m.ReadU32(0x90000 + 124) == 0x01000000 && g.inventoryNotices == 1 &&
          std::bit_cast<float>(m.ReadU32(0x150248 + 28)) == 1);
    run();
    check(m.ReadU32(0x75000 + 24) == 255);
    m.WriteU32(0x80000 + 124, 0);
    m.WriteU32(0x90000 + 124, 0x10000000);
    m.WriteU32(0x832c9c54 + 44, 0x7d000);
    m.WriteU32(0x7d004, 0x7e000);
    m.WriteU32(0x7d00c, 1);
    m.WriteU32(0x7e008, 24);
    m.WriteU32(0x7e000 + 320, 7);
    m.WriteU32(0x90000 + 5116, 7);
    run();
    check(g.equipment == 1 && !m.ReadU32(0x90000 + 5116) &&
          m.ReadU32(0x75000 + 24) == 251 && m.ReadU32(0x7d000 + 4 * 57) == 1 &&
          m.ReadU32(0x180000 + 4 * (45276 + 7)) == 1 &&
          m.ReadU32(0x90000 + 124) == 0x11000000);
    m.WriteU32(0x140000 + 76 + 4 * (39359 + 7), 0x3f800000);
    m.WriteU32(0x140000 + 76 + 4 * 40383, 7);
    run();
    check(m.ReadU32(0x75000 + 24) == 251 && !m.ReadU32(0x150248 + 28) &&
          !m.ReadU32(0x140000 + 76 + 4 * (39359 + 7)) &&
          !m.ReadU32(0x140000 + 76 + 4 * 40383) && !m.ReadU32(0x150248 + 4096));
    m.WriteU32(0x7e000 + 320, 8);
    run();
    check(m.ReadU32(0x75000 + 24) == 252);
    m.WriteU32(0x7e000 + 320, 0);
    m.WriteU32(0x150248 + 28, 0);
    run();
    check(m.ReadU32(0x75000 + 24) == 253);
    m.WriteU32(0x90000 + 124, 0);
    m.WriteU8(0x73000 + 208, 1);
    run();
    check(!m.ReadU8(0x73000 + 208));
    // The descriptor dispatcher reaches the recovered callback without a
    // service stub.
    m.WriteU32(0x73000 + 20, 2);
    m.WriteU32(0x73000 + 44, 0x7f000);
    m.WriteU32(0x7f000 + 32, 0);
    m.WriteU32(0x73000 + 4 * 124, 0x82b13380);
    m.WriteU8(0x73000 + 208, 1);
    s.r[3] = 0x73000;
    check(battle_action_eligibility61::Apply(0x82b120e0, m, {g, native}, s) &&
          !m.ReadU8(0x73000 + 208));
    std::cout << "battle evaluation theft logic smoke passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
