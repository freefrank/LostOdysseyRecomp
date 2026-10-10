#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_scene_tasks61.h"
#include "battle_profile_fixture.h"
#include <iostream>
struct TasksGuest final : manager_release_context61::GuestServices {
  unsigned removed = 0, freed = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18)
      return;
    if (e == 0x822971e0) {
      s.r[3] = m.ReadU16(unsigned(s.r[3])) != m.ReadU16(unsigned(s.r[4]));
      return;
    }
    if (e == 0x82b08318) {
      if (s.r[4] != 0x99000 || s.r[5])
        throw std::runtime_error("secondary creation");
      s.r[3] = 8;
      return;
    }
    if (e == 0x82b1a7f0) {
      if (s.r[5] != 12 || s.r[6] != 0x99000 || s.r[7] != 2 || s.r[8] != 11 ||
          s.r[9])
        throw std::runtime_error("tracked creation");
      s.r[3] = 21;
      return;
    }
    if (e == 0x82b35568) {
      if (s.r[3] != 0xa0000)
        throw std::runtime_error("row destructor");
      ++removed;
      return;
    }
    throw std::runtime_error("tasks direct");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (profile_fixture::Indirect(e, s))
      return;
    if (e == 0x123400) {
      if (s.r[3] != 0xa6000 || s.r[4] != 1)
        throw std::runtime_error("tracked destructor");
      ++removed;
      return;
    }
    if (e == 0x123410) {
      if (s.r[3] == 0x832cb670) {
        if (s.r[4] || s.r[5] != 999 || s.r[6] != 0x99000 || s.r[7] != 11)
          throw std::runtime_error("primary creation");
        s.r[3] = 31;
      } else {
        if (s.r[4] != 8 || s.r[5] || s.r[6] != 11 || s.r[7] != 0x99000 ||
            s.r[8] != 11)
          throw std::runtime_error("row creation");
        s.r[3] = 41;
      }
      return;
    }
    if (e == 0x123424 || e == 0x123430) {
      ++removed;
      return;
    }
    if (e == 0x123408) {
      if (s.r[5] == 0) {
        ++freed;
        s.r[3] = 0;
      } else
        s.r[3] = 0x90000;
      return;
    }
    throw std::runtime_error("tasks indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (auto p :
         {0x832c1000u, 0x83315000u, 0x83264000u, 0x8330b000u, 0x821a8000u})
      regions.push_back({p, 0x1000});
    regions.push_back({0x832c9000, 0x4000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    profile_fixture::Setup(m, 0x70000);
    TasksGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("scene tasks state");
    };
    auto run = [&](unsigned e, unsigned owner = 0x832cc05c, unsigned n = 0) {
      s.r[3] = owner;
      s.r[4] = n;
      check(battle_scene_tasks61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[20] == initial.r[20]);
      return unsigned(s.r[3]);
    };
    m.WriteU32(0x832ca0d0, 0x71000);
    m.WriteU32(0x71000 + 452, 3);
    m.WriteU32(0x71000 + 448, 0x72000);
    m.WriteU32(0x832cc05c + 116, 0xffffffff);
    m.WriteU32(0x832cc05c + 120, 0xffffffff);
    m.WriteU32(0x832cc05c, 0x73000);
    m.WriteU32(0x832cb670, 0x73000);
    m.WriteU32(0x73000 + 16, 0x123410);
    m.WriteU32(0x73000 + 36, 0x123424);
    m.WriteU32(0x73000 + 48, 0x123430);
    m.WriteU32(0x8330b608, 0x8e000);
    m.WriteU32(0x8e000, 0x8f000);
    m.WriteU32(0x8f008, 0x123408);
    m.WriteU32(0x832cc05c + 124, 0x85000);
    m.WriteU32(0x832cc05c + 132, 4);
    m.WriteU32(0x832cc05c + 136, 0x86000);
    m.WriteU32(0x832cc05c + 144, 4);
    m.WriteU8(0x70000 + 133, 1);
    auto row = 0x72000 + 60;
    m.WriteU16(0x99000, 'X');
    m.WriteU32(row + 12, 0x99000);
    m.WriteU32(row + 16, 2);
    // Missing preset secondary name falls back to the profile name.
    m.WriteU32(0x70000 + 136, 0x99000);
    m.WriteU32(0x70000 + 140, 2);
    m.WriteU32(row + 24, 0x74000);
    m.WriteU32(row + 28, 1);
    m.WriteU32(row + 36, 0x74100);
    m.WriteU32(row + 40, 1);
    for (auto p : {0x74000u, 0x74100u}) {
      m.WriteU32(p, p == 0x74000 ? 11 : 12);
      m.WriteU32(p + 4, 0x99000);
      m.WriteU32(p + 8, 2);
    }
    run(0x82b04c50);
    check(m.ReadU32(0x832cc05c + 116) == 31 &&
          m.ReadU32(0x832cc05c + 120) == 8 &&
          m.ReadU32(0x832cc05c + 148) == row + 48);
    check(m.ReadU32(0x85000) == 11 && m.ReadU32(0x85004) == 41 &&
          m.ReadU32(0x86000) == 12 && m.ReadU32(0x86004) == 21);
    m.WriteU32(0x832cb68c + 4, 0xa0000);
    m.WriteU32(0x832cb68c + 8, 1);
    m.WriteU32(0x832cb68c + 12, 1);
    m.WriteU32(0xa0004, 8);
    m.WriteU32(0x832cc0fc + 8, 0xa5000);
    m.WriteU32(0x832cc0fc + 12, 1);
    m.WriteU32(0xa5000, 0xa6000);
    m.WriteU32(0xa6008, 21);
    m.WriteU32(0x832cc0fc + 16, 1);
    m.WriteU32(0xa6000, 0xa9000);
    m.WriteU32(0xa9000, 0x123400);
    m.WriteU8(0xa6000 + 33, 11);
    m.WriteU32(0x832cc05c + 36, 0xb0000);
    m.WriteU32(0x832cc05c + 40, 3);
    m.WriteU32(0x832cc05c + 44, 3);
    for (unsigned i = 0; i < 3; ++i) {
      m.WriteU8(0xb0000 + 44 * i + 1, i == 1 ? 5 : 4);
      m.WriteU32(0xb0000 + 44 * i + 36, i == 2 ? 99 : 21);
    }
    m.WriteU32(0x832cc05c + 48, 0xb1000);
    m.WriteU32(0x832cc05c + 52, 1);
    m.WriteU32(0x832cc05c + 56, 1);
    m.WriteU8(0xb1001, 4);
    m.WriteU32(0xb1000 + 36, 21);
    m.WriteU32(0xa6000 + 36, 0xa7000);
    m.WriteU32(0xa6000 + 40, 1);
    m.WriteU32(0xa6000 + 44, 1);
    m.WriteU8(0xa7000, 0);
    m.WriteU32(0xa6000 + 48, 0xa8000);
    m.WriteU32(0xa6000 + 52, 1);
    m.WriteU32(0xa6000 + 56, 1);
    m.WriteU32(0xa8000, 12);
    m.WriteU32(0x832cb6f0, 9);
    run(0x82b035e0);
    check(g.removed == 4 && g.freed == 7 && m.ReadU32(0xb0000 + 36) == 99 &&
          !m.ReadU32(0x832cc05c + 128) && !m.ReadU32(0x832cc05c + 140) &&
          !m.ReadU32(0x832cc05c + 148));
    m.WriteU8(0x70000 + 133, 0);
    run(0x82b04c50);
    check(g.removed == 4);
    m.WriteU32(0x88004, 2);
    m.WriteU32(0x88008, 2);
    check(run(0x82b1a560, 0x88000, 1) == 2 && m.ReadU32(0x88004) == 3 &&
          m.ReadU32(0x88008) == 36 && m.ReadU32(0x88000) == 0x90000);
    // Clear-all uses the same destructor/array resize path.
    m.WriteU32(0x832cb68c + 4, 0xa0000);
    m.WriteU32(0x832cb68c + 8, 1);
    m.WriteU32(0x832cb68c + 12, 1);
    run(0x82b08410, 0x832cb68c, 0xffffffff);
    check(g.removed == 5 && g.freed == 8 && !m.ReadU32(0x832cb68c + 8));
    m.WriteU32(0x832cc05c + 32, 13);
    m.WriteU32(0x832cc05c + 48, 0xb1000);
    m.WriteU32(0x832cc05c + 52, 1);
    m.WriteU32(0x832cc05c + 56, 1);
    m.WriteU8(0xb1001, 1);
    m.WriteU32(0xb1008, 77);
    s.r[3] = 0x832cc05c;
    s.r[4] = 1;
    s.r[5] = 77;
    check(battle_scene_tasks61::Apply(0x823883f0, m, {g, native}, s) &&
          m.ReadU32(0x832cc05c + 52) == 1);
    m.WriteU32(0x832cc05c + 32, 0);
    s.r[3] = 0x832cc05c;
    s.r[4] = 1;
    s.r[5] = 77;
    check(battle_scene_tasks61::Apply(0x823883f0, m, {g, native}, s) &&
          !m.ReadU32(0x832cc05c + 52));
    // Real event payload cleanup releases the name and each nested string.
    m.WriteU32(0xc0000 + 12, 0xc1000);
    m.WriteU32(0xc0000 + 16, 2);
    m.WriteU32(0xc0000 + 20, 2);
    m.WriteU32(0xc0000 + 24, 0xc2000);
    m.WriteU32(0xc0000 + 28, 1);
    m.WriteU32(0xc0000 + 32, 1);
    m.WriteU32(0xc2000, 0xc3000);
    m.WriteU32(0xc2004, 2);
    m.WriteU32(0xc2008, 2);
    auto before = g.freed;
    run(0x82aaf850, 0xc0000);
    check(g.freed == before + 3 && !m.ReadU32(0xc0000 + 12) &&
          !m.ReadU32(0xc0000 + 24) && !m.ReadU32(0xc2000));
    std::cout << "battle scene tasks logic smoke passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
