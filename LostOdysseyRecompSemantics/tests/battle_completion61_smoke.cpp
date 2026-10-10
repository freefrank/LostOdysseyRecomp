#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_completion61.h"
#include <iostream>
#include "battle_profile_fixture.h"
struct CompletionGuest final : manager_release_context61::GuestServices {
  unsigned freed = 0, ready = 1, destroyed = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18) {
      s.r[3] = 0x70000;
      return;
    }
    throw std::runtime_error("completion direct");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (profile_fixture::Indirect(e, s))
      return;
    if (e == 0x123400) {
      if (s.r[3] != 0x86000 || s.r[4] != 1)
        throw std::runtime_error("destructor arguments");
      ++destroyed;
      return;
    }
    if (e == 0x123408) {
      if (s.r[4] != 0x82000 || s.r[5] != 0 || s.r[6] != 8)
        throw std::runtime_error("array release arguments");
      ++freed;
      s.r[3] = 0;
      return;
    }
    if (e != 0x123428)
      throw std::runtime_error("completion indirect");
    s.r[3] = ready;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x832c9000, 0x4000});
    regions.push_back({0x832c1000, 0x1000});
    regions.push_back({0x83315000, 0x1000});
    regions.push_back({0x83264000, 0x1000});
    regions.push_back({0x8330b000, 0x1000});
    regions.push_back({0x821a8000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    profile_fixture::Setup(m, 0x70000);
    m.WriteU32(0x8330b608, 0x85000);
    m.WriteU32(0x85000, 0x85100);
    m.WriteU32(0x85108, 0x123408);
    CompletionGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("completion state");
    };
    auto run = [&](unsigned e, unsigned owner = 0x832cc05c, unsigned id = 0) {
      s.r[3] = owner;
      s.r[4] = id;
      check(battle_completion61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[27] == initial.r[27]);
      return unsigned(s.r[3]);
    };
    m.WriteU32(0x832ca0d0, 0x71000);
    m.WriteU32(0x71000 + 452, 5);
    m.WriteU8(0x70000 + 133, 2);
    m.WriteU32(0x832cc05c + 116, 0xffffffff);
    m.WriteU32(0x832cc05c + 120, 0xffffffff);
    check(run(0x82b03428) == 1);
    m.WriteU8(0x70000 + 133, 5);
    check(run(0x82b03428) == 0);
    m.WriteU8(0x70000 + 133, 2);
    m.WriteU32(0x832cc05c, 0x72000);
    m.WriteU32(0x832cb670, 0x72000);
    m.WriteU32(0x72000 + 28, 0x123428);
    m.WriteU32(0x832cc05c + 116, 7);
    g.ready = 0;
    check(run(0x82b03428) == 0);
    g.ready = 1;
    m.WriteU32(0x832cc05c + 120, 8);
    m.WriteU32(0x832cb68c + 4, 0x80000);
    m.WriteU32(0x832cb68c + 8, 1);
    m.WriteU32(0x80004, 8);
    m.WriteU8(0x80000, 1);
    check(run(0x82b03428) == 0);
    m.WriteU8(0x80000, 4);
    m.WriteU32(0x832cc05c + 124, 0x81000);
    m.WriteU32(0x832cc05c + 128, 2);
    m.WriteU32(0x832cc05c + 132, 2);
    m.WriteU32(0x81004, 0xffffffff);
    m.WriteU32(0x8100c, 9);
    m.WriteU32(0x832cc05c + 136, 0x82000);
    m.WriteU32(0x832cc05c + 140, 1);
    m.WriteU32(0x832cc05c + 144, 1);
    m.WriteU32(0x82000, 123);
    m.WriteU32(0x82004, 10);
    m.WriteU32(0x832cc0fc + 8, 0x83000);
    m.WriteU32(0x832cc0fc + 12, 1);
    m.WriteU32(0x83000, 0x84000);
    m.WriteU32(0x84008, 10);
    m.WriteU8(0x84006, 3);
    check(run(0x82b03428) == 1 && m.ReadU32(0x81004) == 9 &&
          m.ReadU32(0x832cc05c + 128) == 1);
    check(run(0x82b02f08, 0x832cc05c, 123) == 0 &&
          run(0x82b02f08, 0x832cc05c, 99) == 0xffffffff);
    check(run(0x82aad200, 0, 123) == 0);
    m.WriteU8(0x84006, 6);
    check(run(0x82aad200, 0, 123) == 1);
    check(run(0x82b03428) == 0 && run(0x82aad200, 0, 99) == 0);
    m.WriteU32(0x82004, 0xffffffff);
    check(run(0x82aad200, 0, 123) == 0 && run(0x82b03428) == 1 &&
          g.freed == 1 && !m.ReadU32(0x832cc05c + 136));
    check(run(0x82b07e80, 0x832cb68c, 99) == 0 &&
          run(0x82b1a100, 0x832cc0fc, 99) == 0);
    m.WriteU32(0x832cc0fc + 24, 0x87000);
    m.WriteU32(0x832cc0fc + 28, 2);
    m.WriteU32(0x832cc0fc + 32, 2);
    m.WriteU32(0x87000, 0x86000);
    m.WriteU32(0x87004, 0x86100);
    m.WriteU32(0x86008, 31);
    m.WriteU8(0x86004, 2);
    m.WriteU32(0x86108, 32);
    m.WriteU8(0x86104, 3);
    m.WriteU32(0x86000, 0x88000);
    m.WriteU32(0x88000, 0x123400);
    check(run(0x82b19fd0, 0x832cc0fc, 31) == 1 &&
          run(0x82b19fd0, 0x832cc0fc, 32) == 0);
    run(0x82b1a518, 0x832cc0fc, 31);
    check(g.destroyed == 1 && m.ReadU32(0x832cc0fc + 28) == 1 &&
          m.ReadU32(0x87000) == 0x86100);
    run(0x82b1a518, 0x832cc0fc, 99);
    check(g.destroyed == 1);
    m.WriteU32(0x89000 + 48, 1);
    m.WriteU32(0x89000 + 40, 1);
    m.WriteU32(0x89000 + 36, 0x89100);
    m.WriteU16(0x89100, 'A');
    m.WriteU16(0x89200, 'A');
    check(run(0x82ab5768, 0x89000, 0x89200) == 1);
    m.WriteU16(0x89200, 'B');
    check(run(0x82ab5768, 0x89000, 0x89200) == 0);
    m.WriteU8(0x89000 + 52, 1);
    check(run(0x82ab5768, 0x89000, 0x89200) == 0);
    m.WriteU8(0x89000 + 52, 0);
    m.WriteU32(0x89000 + 40, 0);
    m.WriteU16(0x89200, 0);
    check(run(0x82ab5768, 0x89000, 0x89200) == 1);
    std::cout << "battle completion logic smoke passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
