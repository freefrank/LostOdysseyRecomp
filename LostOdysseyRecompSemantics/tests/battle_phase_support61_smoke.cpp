#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_phase_support61.h"
#include "lo_semantics/recovery_abi.h"
struct PhaseGuest final : manager_release_context61::GuestServices {
  unsigned periodicCalls = 0, phaseCalls = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82acb120) {
      if (m.ReadU32(unsigned(s.r[3])) != 0x8204a1d8 || s.r[4] != 0)
        throw std::runtime_error("periodic effect ABI");
      ++periodicCalls;
      return;
    }
    throw std::runtime_error("unexpected phase direct boundary");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e == 0x123430) {
      ++phaseCalls;
      return;
    }
    if (e != 0x123420)
      throw std::runtime_error("unexpected phase virtual boundary");
    s.r[3] = 0x93000;
  }
};
struct RestartGuest final : manager_release_context61::GuestServices {
  unsigned resets = 0, refreshes = 0, gaugeCalls = 0, timingClears = 0;
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("restart indirect boundary");
  }
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82400a18 || e == 0x82b2c410 || e == 0x82aaa7c8) {
      (void)battle_phase_support61::Apply(e, m,
                                          {*this, cook_main_smoke::native}, s);
      return;
    }
    switch (e) {
    case 0x82380a18:
      s.r[3] = 0x70000;
      return;
    case 0x82389aa0:
      s.r[3] = 0x91000;
      return;
    case 0x82298af8: {
      auto h = unsigned(s.r[3]), i = unsigned(s.r[4]), n = m.ReadU32(h + 4),
           p = m.ReadU32(h);
      if (s.r[5] != 1 || s.r[6] != 4 || s.r[7] != 8)
        throw std::runtime_error("restart erase ABI");
      for (unsigned j = i; j + 1 < n; ++j)
        m.WriteU32(p + 4 * j, m.ReadU32(p + 4 * j + 4));
      m.WriteU32(h + 4, n - 1);
      return;
    }
    case 0x82af5ba8:
      if (s.r[4] != 1 || s.r[5] != 3)
        throw std::runtime_error("restart formation ABI");
      return;
    case 0x82ab31e0:
      ++resets;
      return;
    case 0x82ac3058:
      ++refreshes;
      return;
    case 0x82acd3c0:
      ++timingClears;
      return;
    case 0x82ac7b08:
    case 0x82ac7fc8:
    case 0x82ac6e60:
    case 0x82ac6f08:
      ++gaugeCalls;
      return;
    case 0x82af52f0:
    case 0x82af6448:
    case 0x82a9f160:
    case 0x82ac1b90:
    case 0x82ac3118:
    case 0x82a9f0a0:
    case 0x82a9f028:
      return;
    default:
      throw std::runtime_error("unexpected restart service");
    }
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (auto p :
         {0x832ca000u, 0x832cb000u, 0x83315000u, 0x832c1000u, 0x83263000u,
          0x832cc000u, 0x832ae000u, 0x83291000u, 0x83245000u})
      regions.push_back({p, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    PhaseGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool ok) {
      if (!ok)
        throw std::runtime_error("phase support state");
    };
    auto run = [&](unsigned e, unsigned owner) {
      s.r[3] = owner;
      check(battle_phase_support61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[27] == initial.r[27] &&
            s.r[31] == initial.r[31]);
    };
    recovery_abi::WriteU64(m, 0x80008, 0x123456789abcdef0ULL);
    run(0x82400a18, 0x80000);
    check(recovery_abi::ReadU64(m, 0x80008) ==
          (0x123456789abcdef0ULL & ~0x4000ULL));
    m.WriteU32(0x82000e50, 0);
    m.WriteU32(0x80008, 123);
    run(0x82b08a60, 0x80000);
    check(!m.ReadU32(0x80008) && !m.ReadU32(0x80010));
    m.WriteU32(0x80000 + 28, 0x81000);
    m.WriteU32(0x80000 + 32, 2);
    m.WriteU32(0x81000, 7);
    m.WriteU32(0x81014, 11);
    m.WriteU32(0x81018, 19);
    run(0x82b2c410, 0x80000);
    check(s.r[3] == 19);
    m.WriteU32(0x81014, 3);
    run(0x82b2c410, 0x80000);
    check(unsigned(s.r[3]) == 0xffffffffu);
    m.WriteU32(0x83315fb4, 0x92000);
    m.WriteU32(0x92000, 0x92100);
    m.WriteU32(0x92100 + 352, 0x123420);
    m.WriteU32(0x832c1764, 0x94000);
    m.WriteU32(0x93000 + 52, 0x94000);
    run(0x82389b10, 0);
    check(s.r[3] == 0x93000);
    m.WriteU32(0x83263198, 0x94100);
    m.WriteU32(0x92000 + 52, 0x94100);
    m.WriteU32(0x80000 + 28, 0);
    run(0x82a9f160, 0x80000);
    m.WriteU32(0x832cb788, 0x70000);
    m.WriteU32(0x832ca0e8 + 20, 0x71000);
    m.WriteU32(0x71000, 0x72000);
    m.WriteU32(0x71004, 3);
    for (unsigned i = 0; i < 3; ++i) {
      auto p = 0xa0000 + 0x1000 * i;
      m.WriteU32(0x72000 + 4 * i, p);
      m.WriteU32(p + 64, i == 0 ? 4 : 20 + i);
      m.WriteU32(p + 88, 2);
    }
    m.WriteU32(0xa2000 + 124, 0x10000);
    s.r[4] = 1;
    run(0x82ace978, 0x70000);
    check(m.ReadU32(0xa0000 + 88) == 1 && m.ReadU32(0xa1000 + 88) == 2);
    s.r[4] = 0;
    run(0x82ace978, 0x70000);
    check(m.ReadU32(0xa1000 + 88) == 1 && m.ReadU32(0xa2000 + 88) == 2);
    m.WriteU32(0x80000 + 28, 0x100000);
    m.WriteU32(0x80000 + 44, 0x200000);
    for (auto bank : {200u, 8460u})
      for (unsigned i = 0; i < 1024; ++i)
        for (unsigned second = 0; second < 2; ++second) {
          auto off = bank + 4 * i + 4100 * second;
          m.WriteU32(0x100000 + 180904 + off, off ^ 0xabcdef);
        }
    run(0x82af6b48, 0x80000);
    check(m.ReadU32(0x200000 + 200) == (200u ^ 0xabcdef) &&
          m.ReadU32(0x200000 + 8460 + 4100 + 4092) ==
              ((8460u + 4100 + 4092) ^ 0xabcdef));
    m.WriteU32(0x80000, 0x82000);
    m.WriteU32(0x82000 + 12, 0x123430);
    m.WriteU32(0x80000 + 20, 0x83000);
    m.WriteU32(0x83004, 0);
    auto phase = [&](unsigned from, unsigned to, unsigned force) {
      m.WriteU32(0x80000 + 56, from);
      s.r[4] = to;
      s.r[5] = force;
      run(0x82aaa7c8, 0x80000);
    };
    phase(99, 4, 0);
    check(m.ReadU32(0x80000 + 56) == 99);
    phase(99, 2, 1);
    check(m.ReadU32(0x80000 + 56) == 2 && m.ReadU32(0x80000 + 144) == 2);
    phase(0, 1, 0);
    check(m.ReadU32(0x80000 + 56) == 1);
    m.WriteU32(0x80000 + 52, 7);
    phase(2, 3, 0);
    check(m.ReadU32(0x80000 + 52) == 8);
    phase(3, 2, 0);
    check(g.phaseCalls == 1 && m.ReadU32(0x80000 + 56) == 2);
    phase(3, 4, 0);
    check(m.ReadU32(0x80000 + 56) == 4);
    phase(4, 5, 0);
    phase(5, 6, 0);
    check(g.periodicCalls == 1 && m.ReadU32(0x80000 + 56) == 6);
    phase(8, 9, 0);
    check(m.ReadU32(0x80000 + 56) == 9);
    phase(11, 13, 0);
    check(m.ReadU32(0x80000 + 56) == 13);
    phase(12, 13, 0);
    check(m.ReadU32(0x80000 + 56) == 13);
    phase(13, 0, 0);
    check(m.ReadU32(0x80000 + 56) == 13 && s.r[3] == 0x93000);
    RestartGuest restart;
    m.WriteU32(0x80000 + 20, 0x83000);
    m.WriteU32(0x83000, 0x84000);
    m.WriteU32(0x83004, 2);
    m.WriteU32(0x84000, 0xa0000);
    m.WriteU32(0x84004, 0xa1000);
    m.WriteU32(0xa0000 + 124, 0x18000000);
    m.WriteU32(0xa0000 + 72, 0);
    m.WriteU32(0xa0000 + 60, 9);
    m.WriteU32(0xa1000 + 124, 0);
    recovery_abi::WriteU64(m, 0xa1008, 0x4000);
    m.WriteU32(0x832cc0cc, 0x85000);
    m.WriteU32(0x85000 + 28, 0x86000);
    m.WriteU32(0x85000 + 32, 1);
    m.WriteU32(0x86000, 11);
    m.WriteU32(0x86004, 19);
    m.WriteU8(0x91000 + 56, 3);
    m.WriteU32(0x832aeb00, 0x87000);
    m.WriteU8(0x87000 + 48, 1);
    m.WriteU8(0x87000 + 24, 1);
    m.WriteU32(0x80000 + 148, 0xffffffff);
    m.WriteU32(0x80000 + 156, 9);
    s.r[3] = 0x80000;
    check(battle_phase_support61::Apply(0x82ad40d0, m, {restart, native}, s));
    check(m.ReadU32(0x83004) == 1 && m.ReadU32(0xa0000 + 72) == 19 &&
          !m.ReadU32(0xa0000 + 60) && !recovery_abi::ReadU64(m, 0xa1008));
    check(restart.resets == 2 && restart.refreshes == 1 &&
          restart.timingClears == 1 && restart.gaugeCalls == 8);
    check(m.ReadU32(0x80000 + 4) == 20 && !m.ReadU32(0x80000 + 52) &&
          !m.ReadU32(0x80000 + 56) && m.ReadU32(0x80000 + 148) == 7 &&
          !m.ReadU32(0x80000 + 156));
    check(m.ReadU8(0x80000 + 61) == 1 && m.ReadU8(0x80000 + 65) == 1 &&
          s.r[1] == initial.r[1] && s.r[23] == initial.r[23]);
    std::puts("phase support smoke passed");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
