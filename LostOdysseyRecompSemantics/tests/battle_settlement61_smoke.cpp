#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_settlement61.h"
#include "lo_semantics/battle_action_readiness61.h"
#include "battle_profile_fixture.h"
struct SettlementGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress, GuestMemory &,
                  manager_release_context61::Registers &) override {
    throw std::runtime_error("unexpected settlement direct boundary");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (!profile_fixture::Indirect(e, s))
      throw std::runtime_error("unexpected settlement virtual boundary");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (auto p : {0x83213000u, 0x832ca000u, 0x832cb000u, 0x83315000u,
                   0x832c1000u, 0x83264000u, 0x8201d000u})
      regions.push_back({p, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    SettlementGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool ok) {
      if (!ok)
        throw std::runtime_error("settlement state");
    };
    auto put = [&](unsigned p, float v) {
      m.WriteU32(p, std::bit_cast<unsigned>(v));
    };
    auto get = [&](unsigned p) { return std::bit_cast<float>(m.ReadU32(p)); };
    constexpr unsigned owner = 0x60000, source = 0x80000, target = 0x90000,
                       table = 0x120000;
    auto run = [&](unsigned e) {
      s.r[3] = owner;
      check(battle_settlement61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[23] == initial.r[23] &&
            s.fpr_bits[31] == initial.fpr_bits[31]);
      return unsigned(s.r[3]);
    };
    for (unsigned i = 0; i < 32; ++i) {
      m.WriteU32(0x83213438 + 8 * i, 0);
      m.WriteU32(0x8321343c + 8 * i, 1u << i);
    }
    profile_fixture::Setup(m, 0x140000);
    m.WriteU32(0x832ca0d0, 0x130000);
    m.WriteU32(0x130000 + 120, table);
    m.WriteU32(owner + 100, 1);
    m.WriteU32(owner + 104, 24);
    m.WriteU32(owner + 112, 2);
    m.WriteU32(owner + 116, 25);
    m.WriteU8(owner + 120, 1);
    m.WriteU32(table + 140 + 96, 50);
    m.WriteU32(table + 280 + 96, 75);
    m.WriteU32(table + 140 + 100, 100);
    m.WriteU32(table + 280 + 100, 50);
    m.WriteU32(table + 140 + 92, 15);
    check(run(0x82ac1ce0) == 50);
    m.WriteU32(0x832cb788, 0x70000);
    m.WriteU32(0x832ca0e8 + 20, 0x71000);
    m.WriteU32(0x71000, 0x72000);
    m.WriteU32(0x71004, 2);
    m.WriteU32(0x72000, source);
    m.WriteU32(0x72004, target);
    m.WriteU32(source + 64, 24);
    m.WriteU32(target + 64, 25);
    m.WriteU32(source + 124, 0x10000080);
    m.WriteU32(target + 124, 0x10000100);
    m.WriteU32(source + 4 * 272 + 232, (1u << 17) | (1u << 18) | (1u << 20));
    m.WriteU32(target + 232, 1);
    run(0x82ac22f8);
    check(m.ReadU32(owner + 564) == source &&
          m.ReadU32(owner + 692) == target && m.ReadU32(owner + 1208) == 1 &&
          m.ReadU8(owner + 1212) == 1 && m.ReadU8(owner + 659) == 1);
    m.WriteU32(0x93000 + 76, 9999800);
    check(run(0x82ac31b0) == 510 && m.ReadU32(0x93000 + 76) == 9999999 &&
          m.ReadU32(owner + 96) == 9999800);
    s.r[4] = 7;
    run(0x82ac0068);
    s.r[4] = 7;
    run(0x82ac0068);
    s.r[4] = 0;
    run(0x82ac0068);
    check(m.ReadU32(owner + 484) == 7 && m.ReadU32(owner + 488) == 2);
    m.WriteU32(source + 140, 10);
    m.WriteU32(owner + 52, 3);
    run(0x82ac1dc8);
    check(m.ReadU32(owner + 568) == 6 && !m.ReadU32(owner + 696));
    put(0x82000e50, 0);
    put(0x8201dd2c, 100);
    put(source + 136, 95);
    put(target + 136, 70);
    run(0x82ac1fa0);
    check(get(source + 136) == 0 && m.ReadU8(owner + 572) == 1 &&
          m.ReadU32(owner + 684) == 95 && get(target + 136) == 70 &&
          m.ReadU32(owner + 812) == 70);
    m.WriteU32(source + 140, 99);
    put(source + 136, 98);
    run(0x82ac1fa0);
    check(get(source + 136) == 100);
    std::puts("settlement rewards, participants and progression smoke passed");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
