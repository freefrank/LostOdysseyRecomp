#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_effect_followups61.h"
#include <iostream>
struct FollowupGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x76000;
      return;
    }
    if (e == 0x8238e2f8) {
      s.r[3] = 0x77000;
      return;
    }
    if (e == 0x8238e308) {
      if (s.r[4] != 24 && s.r[4] != 26)
        throw std::runtime_error("followup lookup");
      s.r[3] = s.r[4] == 24 ? 0x80000 : 0xa0000;
      return;
    }
    throw std::runtime_error("followup direct " + std::to_string(e));
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e == 0x123408) {
      if (s.r[3] != 0x7f000 || s.r[6] != 8)
        throw std::runtime_error("label allocation");
      s.r[3] = s.r[5] ? 0x1a0000 : 0;
      return;
    }
    if (e != 0x123400)
      throw std::runtime_error("followup virtual");
    s.r[3] = 0;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (auto p : {0x821a8000u, 0x8330b000u, 0x83213000u, 0x83264000u,
                   0x8201d000u, 0x8201f000u})
      regions.push_back({p, 0x1000});
    regions.push_back({0x832c9000, 0x4000});
    regions.push_back({0x831f3000, 0x21000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    FollowupGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("followup state");
    };
    auto put = [&](unsigned p, float v) {
      m.WriteU32(p, std::bit_cast<unsigned>(v));
    };
    auto get = [&](unsigned p) { return std::bit_cast<float>(m.ReadU32(p)); };
    for (unsigned i = 0; i < 32; ++i) {
      m.WriteU32(0x83213438 + 8 * i, 0);
      m.WriteU32(0x8321343c + 8 * i, 1u << i);
    }
    m.WriteU32(0x73004, 0x80000);
    m.WriteU32(0x73008, 0x90000);
    m.WriteU32(0x832cb790, 0x74000);
    m.WriteU32(0x74000 + 20, 0x100000);
    m.WriteU32(0x83264558, 0x75000);
    m.WriteU32(0x77000, 0x77100);
    m.WriteU32(0x77004, 2);
    m.WriteU32(0x77100, 0x80000);
    m.WriteU32(0x77104, 0xa0000);
    m.WriteU32(0x78000 + 292, 0x123400);
    for (auto who : {0x80000u, 0xa0000u}) {
      m.WriteU32(who, 0x78000);
      put(who + 2592, 200);
      put(who + 2620, 100);
    }
    m.WriteU32(0x80000 + 64, 24);
    m.WriteU32(0xa0000 + 64, 26);
    put(0x82000d7c, .01f);
    put(0x8201f9f0, .5f);
    put(0x82007784, 1);
    auto run = [&](unsigned mask, unsigned forced = 0) {
      s.r[3] = 0x73000;
      s.r[4] = mask;
      s.r[5] = forced;
      check(battle_effect_followups61::Apply(0x82b21970, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[16] == initial.r[16] &&
            s.fpr_bits[30] == initial.fpr_bits[30] &&
            s.fpr_bits[31] == initial.fpr_bits[31] &&
            m.ReadU32(0x73000 + 88) == mask);
    };
    m.WriteU32(0x73000 + 104, 99);
    run(0);
    check(!m.ReadU32(0x73000 + 104));
    run(1u << 11);
    check((m.ReadU32(0x90000 + 232) & (1u << 11)) &&
          m.ReadU32(0x73000 + 104) == (1u << 11));
    run(1u << 11);
    check(!m.ReadU32(0x73000 + 104));
    put(0x73000 + 24, 100);
    put(0x73000 + 112, 20);
    put(0x73000 + 116, 10);
    put(0x80000 + 2588, 100);
    put(0x80000 + 2616, 10);
    m.WriteU32(0x90000 + 272 + 232, 1u << 14);
    run((1u << 21) | (1u << 22));
    check(get(0x80000 + 2588) == 120 && get(0x80000 + 2616) == 20 &&
          m.ReadU8(0x73000 + 120) == 1 && !m.ReadU32(0x90000 + 272 + 232) &&
          get(0x100000 + 4 * 18) == 20 && get(0x100000 + 4 * 26) == 10);
    put(0x80000 + 2588, 100);
    put(0xa0000 + 2588, 50);
    m.WriteU32(0x80000 + 3 * 272 + 232, 1u << 7);
    run(1u << 21);
    check(get(0x80000 + 2588) == 110 && get(0xa0000 + 2588) == 60);
    m.WriteU32(0x80000 + 3 * 272 + 232, 0);
    m.WriteU32(0x80000 + 7 * 272 + 232, 1u << 30);
    m.WriteU32(0x80000 + 124, 0x40000000);
    put(0x80000 + 2588, 100);
    put(0xa0000 + 2588, 50);
    run(1u << 21);
    check(get(0x80000 + 2588) == 100 && get(0xa0000 + 2588) == 70 &&
          get(0x100000 + 4 * 18) == 0);
    m.WriteU32(0x80000 + 7 * 272 + 232, 1u << 15);
    m.WriteU32(0x80000 + 124, 0);
    run(1u << 21, 1);
    check(get(0x80000 + 2588) == 140);
    m.WriteU32(0x73000 + 52, 2);
    run(1u << 21);
    check(get(0x80000 + 2588) == 140);

    m.WriteU32(0x832ca0cc, 0x73000);
    m.WriteU32(0x832cb784, 0x7a000);
    m.WriteU32(0x832ca0d0, 0x7b000);
    m.WriteU32(0x7b000 + 120, 0x110000);
    m.WriteU32(0x832c9c54 + 44, 0x7c000);
    m.WriteU32(0x7c000 + 4, 0x7d000);
    m.WriteU32(0x7c000 + 12, 1);
    m.WriteU32(0x7d000 + 8, 24);
    m.WriteU32(0x7d000 + 100, 12);
    m.WriteU32(0x90000 + 232, 0);
    m.WriteU32(0x73000 + 52, 0);
    m.WriteU32(0x80000 + 7 * 272 + 232, 0);
    auto mainRun = [&]() {
      s.r[3] = 0x73000;
      check(battle_effect_followups61::Apply(0x82b22100, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[23] == initial.r[23] &&
            s.fpr_bits[28] == initial.fpr_bits[28]);
    };
    mainRun();
    check(m.ReadU32(0x7d000 + 100) == 0 &&
          (m.ReadU32(0x90000 + 232) & (1u << 11)));
    m.WriteU32(0x73000 + 100, 2);
    m.WriteU32(0x80000 + 76252, 4);
    m.WriteU32(0x80000 + 76264, 0x200000);
    m.WriteU32(0x80000 + 76276, 13);
    put(0x82000dd0, 100);
    put(0x80000 + 2588, 100);
    mainRun();
    check(get(0x73000 + 112) == 100 && get(0x80000 + 2588) == 200 &&
          m.ReadU8(0x73000 + 120) == 1);
    m.WriteU32(0x80000 + 76252, 6);
    m.WriteU32(0x80000 + 76264, 1);
    put(0x8201dd2c, 100);
    mainRun();
    check(m.ReadU32(unsigned(initial.r[1]) - 208 + 80) == 100);
    m.WriteU32(0x8330b608, 0x7f000);
    m.WriteU32(0x7f000, 0x7f100);
    m.WriteU32(0x7f100 + 8, 0x123408);
    m.WriteU16(0x140000, 'K');
    m.WriteU16(0x140002, 0);
    m.WriteU32(0x83264978 + 116, 0x120000);
    m.WriteU32(0x7e000 + 16, 0x130000);
    m.WriteU32(0x120000 + 84 * 7 + 52, 1);
    m.WriteU32(0x120000 + 84 * 7 + 48, 0x140000);
    s.r[3] = 0x7e000;
    s.r[4] = 7;
    s.r[5] = 2;
    check(battle_effect_followups61::Apply(0x82aa12e0, m, {g, native}, s) &&
          m.ReadU32(0x130000 + 300) == 2 &&
          m.ReadU16(m.ReadU32(0x130000 + 296)) == 'K' &&
          s.r[1] == initial.r[1]);
    m.WriteU32(0x120000 + 84 * 7 + 52, 0);
    s.r[3] = 0x7e000;
    s.r[4] = 7;
    s.r[5] = 2;
    check(battle_effect_followups61::Apply(0x82aa12e0, m, {g, native}, s) &&
          !m.ReadU32(0x130000 + 296) && !m.ReadU32(0x130000 + 296 + 4));
    std::cout << "battle effect followups logic smoke passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
