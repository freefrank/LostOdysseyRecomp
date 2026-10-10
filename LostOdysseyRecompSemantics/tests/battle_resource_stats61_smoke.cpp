#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_resource_stats61.h"
#include <iostream>
struct StatsGuest final : manager_release_context61::GuestServices {
  unsigned step = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78 || e == 0x82ab0110) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x82ac0888 || e == 0x82ac2468) {
      if (s.r[3] != 0x73000 || s.r[4] != 0x80000)
        throw std::runtime_error("stats service arguments");
      if (e == 0x82ac0888) {
        if (step++ != 0 || m.ReadU32(0x80000 + 2532))
          throw std::runtime_error("base refresh order");
        m.WriteU32(0x80000 + 2472, 0x42c80000);
      } else {
        if (step++ != 1)
          throw std::runtime_error("gear refresh order");
        m.WriteU32(0x80000 + 2532, 0x42480000);
      }
      return;
    }
    throw std::runtime_error("stats direct");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("stats indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (auto p : {0x821a8000u, 0x8201f000u, 0x832c0000u, 0x83213000u,
                   0x83264000u, 0x83291000u, 0x82218000u})
      regions.push_back({p, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    StatsGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("stats state");
    };
    auto put = [&](unsigned off, float x) {
      m.WriteU32(0x80000 + off, std::bit_cast<unsigned>(x));
    };
    auto get = [&](unsigned off) {
      return std::bit_cast<float>(m.ReadU32(0x80000 + off));
    };
    for (unsigned i = 0; i < 32; ++i) {
      m.WriteU32(0x83213438 + 8 * i, 0);
      m.WriteU32(0x8321343c + 8 * i, 1u << i);
    }
    m.WriteU32(0x83264978, 0x150000);
    m.WriteU32(0x150000 + 17 * 196 + 124, 8);
    m.WriteU32(0x150000 + 17 * 196 + 128, 50);
    m.WriteU32(0x82000d7c, 0x3c23d70a);
    m.WriteU32(0x8201f9f0, 0x3f000000);
    m.WriteU32(0x83264978 + 72, 0x100000);
    m.WriteU32(0x83291dc0, 0x73000);
    m.WriteU32(0x82007784, 0x3f800000);
    m.WriteU32(0x82000b3c, 0x3e800000);
    m.WriteU32(0x822184dc, 0x43480000);
    m.WriteU32(0x822181e4, 0x42c80000);
    m.WriteU32(0x822182a0, 0x42c60000);
    auto run = [&](unsigned e, unsigned mode = 1) {
      s.r[3] = 0x73000;
      s.r[4] = 0x80000;
      s.r[5] = mode;
      check(battle_resource_stats61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[26] == initial.r[26] &&
            s.r[31] == initial.r[31]);
    };
    m.WriteU32(0x100000 + 44, 1);
    m.WriteU32(0x100000 + 48, 2);
    m.WriteU32(0x100000 + 52, 4);
    m.WriteU32(0x100000 + 56, 4);
    m.WriteU32(0x100000 + 60, 8);
    m.WriteU32(0x80000 + 2 * 272 + 232, 5);
    m.WriteU32(0x80000 + 4 * 272 + 232, 8);
    run(0x82ac1860);
    check(m.ReadU32(0x80000 + 2 * 272 + 232) == 1 &&
          !m.ReadU32(0x80000 + 4 * 272 + 232));
    put(4920, 100);
    put(2532, 100);
    put(2472, 100);
    put(2588, 500);
    put(4924, 70);
    put(2560, 70);
    put(2500, 70);
    put(2616, 300);
    put(2552, 120);
    put(2580, -5);
    put(2536, 10);
    put(2476, 20);
    run(0x82ac0620);
    check(get(2592) == 200 && get(2588) == 200 && get(2620) == 100 &&
          get(2616) == 100 && get(2596) == 30 && get(2612) == 99 &&
          get(2640) == 1);
    put(2588, 10);
    run(0x82ac0620);
    check(m.ReadU32(0x80000 + 232) & 2);
    run(0x82ac0620, 0);
    check(get(2592) == 300 && get(2588) == 300 && get(2620) == 210 &&
          get(2616) == 210 && !(m.ReadU32(0x80000 + 232) & 2));
    put(4920, 0);
    put(4924, 0);
    m.WriteU32(0x80000 + 5108, 17);
    for (unsigned i = 0; i < 5; ++i)
      m.WriteU32(0x80000 + 5116 + 4 * i, i + 1);
    run(0x82ac3058);
    check(g.step == 1 && get(2592) == 150 && get(2588) == 150 &&
          m.ReadU32(0x73000 + 4) == 17 && m.ReadU32(0x73000 + 28) == 5);
    auto effect = [&](unsigned kind, unsigned value, unsigned slot) {
      m.WriteU32(0x180000 + 124, kind);
      m.WriteU32(0x180000 + 128, value);
      s.r[3] = 0x73000;
      s.r[4] = 0x80000;
      s.r[5] = 0x180000;
      s.r[6] = slot;
      check(battle_resource_stats61::Apply(0x82ac0170, m, {g, native}, s));
    };
    effect(9, 15, 0);
    check(get(2532) == 35);
    effect(10, 7, 0);
    check(get(2544) == 7);
    effect(12, 0, 2);
    check(m.ReadU32(0x80000 + 4840) == 12);
    for (auto [input, expected] :
         {std::pair{15.9, 15.}, std::pair{16., 20.}, std::pair{14., 10.}}) {
      s.fpr_bits[1] = std::bit_cast<std::uint64_t>(input);
      check(battle_resource_stats61::Apply(0x82ac0100, m, {g, native}, s) &&
            std::bit_cast<double>(s.fpr_bits[1]) == expected &&
            s.r[1] == initial.r[1]);
    }
    m.WriteU32(0x80000 + 5112, 9);
    put(2536, 0);
    m.WriteU32(0x150000 + 9 * 196 + 56, 0x40400000);
    for (unsigned i = 0; i < 3; ++i) {
      m.WriteU32(0x150000 + 9 * 196 + 184 + 4 * i, i + 1);
      auto row = 0x150000 + 196 * (769 + i);
      m.WriteU32(row + 184, i + 1);
      m.WriteU32(row + 188, 2u << i);
      m.WriteU32(row + 192, 11 + i);
    }
    run(0x82ac0388);
    check(get(2536) == 3 && m.ReadU32(0x80000 + 4916) == 2 &&
          m.ReadU32(0x80000 + 4828) == 4 && m.ReadU32(0x80000 + 2652) == 8 &&
          m.ReadU32(0x80000 + 76252) == 1 && m.ReadU32(0x80000 + 76272) == 8 &&
          m.ReadU32(0x80000 + 76284) == 13);
    std::cout << "battle resource stats logic smoke passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
