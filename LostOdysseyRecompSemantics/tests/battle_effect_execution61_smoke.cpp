#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_effect_execution61.h"
#include <iostream>
#include "battle_progression_fixture.h"
struct ExecutionGuest final : manager_release_context61::GuestServices {
  unsigned listQueries = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (progression_fixture::Direct(e, s))
      return;
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x76000;
      return;
    }
    if (e == 0x8238e2f8) {
      ++listQueries;
      s.r[3] = 0x7b000;
      return;
    }
    if (e == 0x8238e308) {
      if (s.r[4] != 24 && s.r[4] != 25)
        throw std::runtime_error("resource lookup ID");
      s.r[3] = s.r[4] == 24 ? 0x80000 : 0x90000;
      return;
    }

    throw std::runtime_error("execution direct " + std::to_string(e));
  }
  void CallIndirect(GuestAddress e, GuestMemory &m,
                    manager_release_context61::Registers &s) override {
    if (progression_fixture::Indirect(e, s))
      return;
    throw std::runtime_error("execution indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (auto p :
         {0x8201d000u, 0x8201f000u, 0x8204f000u, 0x821ba000u, 0x82218000u,
          0x82089000u, 0x83291000u, 0x83213000u, 0x83264000u, 0x832ae000u})
      regions.push_back({p, 0x1000});
    regions.push_back({0x832c9000, 0x4000});
    regions.push_back({0x831f3000, 0x21000});
    regions.push_back({0x83315000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    progression_fixture::Setup(m);
    ExecutionGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("execution state");
    };
    for (unsigned i = 0; i < 32; ++i) {
      m.WriteU32(0x83213438 + 8 * i, 0);
      m.WriteU32(0x8321343c + 8 * i, 1u << i);
    }
    m.WriteU32(0x832ca0cc, 0x73000);
    m.WriteU32(0x832cb790, 0x74000);
    m.WriteU32(0x83264558, 0x75000);
    m.WriteU32(0x832cb784, 0x77000);
    m.WriteU32(0x832ca0d8, 0x78000);
    m.WriteU32(0x832aeb00, 0x79000);
    m.WriteU32(0x832c9c54 + 44, 0x7d000);
    m.WriteU32(0x7d004, 0x7e000);
    m.WriteU32(0x7d00c, 1);
    m.WriteU32(0x7e008, 25);
    m.WriteU32(0x73004, 0x80000);
    m.WriteU32(0x73008, 0x90000);
    m.WriteU32(0x73000 + 32, 0x7a000);
    m.WriteU32(0x74000 + 20, 0x100000);
    m.WriteU32(0x80000 + 64, 24);
    m.WriteU32(0x90000 + 64, 25);
    m.WriteU32(0x8201dd2c, 0x42c80000);
    m.WriteU32(0x82000d7c, 0x3c23d70a);
    m.WriteU32(0x8201f9f0, 0x3f000000);
    m.WriteU32(0x821baa74, 0x3f000000);
    m.WriteU32(0x82000fb0, 0x3f800000);
    m.WriteU32(0x82007784, 0x3f800000);
    m.WriteU32(0x82000e1c, 0x40000000);
    m.WriteU32(0x8204fc20, 0x447a0000);
    m.WriteU32(0x82000e40, 0xbf800000);
    auto reset = [&]() {
      m.WriteU32(0x7e000 + 64, 0);
      for (unsigned bank = 0; bank < 9; ++bank) {
        m.WriteU32(0x80000 + 272 * bank + 232, 0);
        m.WriteU32(0x90000 + 272 * bank + 232, 0);
      }
      for (unsigned p = 0; p < 20000; p += 4)
        m.WriteU32(0x100000 + p, 0);
      m.WriteU32(0x100000 + 36, 24);
      m.WriteU32(0x100000 + 14884, 25);
      for (auto off : {14u, 18u, 22u, 26u, 30u, 34u, 38u, 3726u, 3730u, 3734u,
                       3738u, 3742u})
        m.WriteU32(0x100000 + 4 * off, 0xbf800000);
      for (auto who : {0x80000u, 0x90000u}) {
        m.WriteU32(who + 2588, 0x43480000);
        m.WriteU32(who + 2592, 0x43480000);
        m.WriteU32(who + 2620, 0x42c80000);
      }
      for (auto offset : {36u, 45u, 64u, 65u, 108u, 109u, 120u})
        m.WriteU8(0x73000 + offset, 0);
      for (auto offset : {40u, 52u, 84u, 88u, 92u, 104u})
        m.WriteU32(0x73000 + offset, 0);
      m.WriteU32(0x73000 + 56, 1);
      m.WriteU32(0x80000 + 2604, 0x447a0000);
      m.WriteU32(0x90000 + 2608, 0);
      m.WriteU32(0x80000 + 2596, 0x42c80000);
      m.WriteU32(0x90000 + 2600, 0x41200000);
      m.WriteU32(0x90000 + 2616, 0x42480000);
      m.WriteU32(0x90000 + 4956, 0);
      m.WriteU32(0x80000 + 4916, 0);
      m.WriteU32(0x76000 + 192, 777);
    };
    auto run = [&]() {
      s.r[3] = 0x73000;
      check(battle_effect_execution61::Apply(0x82b22948, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[26] == initial.r[26] &&
            s.fpr_bits[30] == initial.fpr_bits[30] &&
            s.fpr_bits[31] == initial.fpr_bits[31]);
    };
    reset();
    run();
    check(m.ReadU32(0x76000 + 192) == 90 &&
          std::bit_cast<float>(m.ReadU32(0x100000 + 4 * 3726)) == 90 &&
          std::bit_cast<float>(m.ReadU32(0x90000 + 2588)) == 110);
    reset();
    m.WriteU8(0x73000 + 45, 1);
    m.WriteU32(0x78000 + 64, 9);
    run();
    check(m.ReadU32(0x76000 + 192) == 45 &&
          std::bit_cast<float>(m.ReadU32(0x90000 + 2588)) == 155);
    reset();
    m.WriteU32(0x80000 + 2604, 0);
    m.WriteU32(0x90000 + 2608, 0x447a0000);
    run();
    check(m.ReadU32(0x76000 + 192) == 777);
    reset();
    m.WriteU32(0x90000 + 4956, 4);
    run();
    check(m.ReadU32(0x7e000 + 64) == 0x88000 && m.ReadU32(0x73000 + 52) == 2 &&
          m.ReadU32(0x100000 + 4 * 3758) == 1 && m.ReadU32(0x76000 + 192) == 0);
    reset();
    m.WriteU32(0x90000 + 7 * 272 + 232, 1u << 21);
    run();
    check(m.ReadU32(0x73000 + 52) == 1 && m.ReadU32(0x76000 + 192) == 0);
    reset();
    m.WriteU32(0x80000 + 4916, 1);
    m.WriteU32(0x90000 + 272 + 232, 1u << 16);
    m.WriteU32(0x90000 + 4 * (127 + 16), 2);
    run();
    check(m.ReadU32(0x73000 + 52) == 8 &&
          std::bit_cast<float>(m.ReadU32(0x100000 + 4 * 3730)) == 90 &&
          std::bit_cast<float>(m.ReadU32(0x90000 + 2588)) == 200 &&
          m.ReadU32(0x76000 + 192) == 0);
    reset();
    m.WriteU32(0x90000 + 4 * 272 + 232, 1u << 13);
    m.WriteU32(0x90000 + 232, 2);
    run();
    check(m.ReadU32(0x73000 + 52) == 5 &&
          std::bit_cast<float>(m.ReadU32(0x100000 + 4 * 3734)) == 50 &&
          std::bit_cast<float>(m.ReadU32(0x100000 + 4 * 3726)) == 40 &&
          m.ReadU32(0x76000 + 192) == 40 && m.ReadU32(0x90000 + 2616) == 0 &&
          std::bit_cast<float>(m.ReadU32(0x90000 + 2588)) == 160);
    reset();
    m.WriteU32(0x80000 + 3 * 272 + 232, 1u << 2);
    m.WriteU32(0x80000 + 4940, 100);
    m.WriteU32(0x80000 + 2588, 0x43160000);
    m.WriteU32(0x82000dac, 0x3dcccccd);
    run();
    check(m.ReadU8(0x73000 + 36) == 1 &&
          std::bit_cast<float>(m.ReadU32(0x80000 + 2588)) == 170 &&
          std::bit_cast<float>(m.ReadU32(0x100000 + 4 * 18)) == 20 &&
          m.ReadU32(0x100000 + 4 * 10) == 1);
    m.WriteU32(0x73000 + 40, 1);
    m.WriteU32(0x90000 + 196, 7);
    m.WriteU32(0x90000 + 200, 0x80000000);
    m.WriteU32(0x7b000, 0x7b100);
    m.WriteU32(0x7b004, 2);
    m.WriteU32(0x7b100, 0x90000);
    m.WriteU32(0x7b104, 0xa0000);
    m.WriteU32(0xa0000 + 196, 8);
    s.r[3] = 0x73000;
    check(battle_effect_execution61::Apply(0x82b22870, m, {g, native}, s) &&
          g.listQueries == 4 && s.r[1] == initial.r[1]);
    check(!battle_effect_execution61::Apply(0, m, {g, native}, s));
    std::cout << "battle_effect_execution61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
