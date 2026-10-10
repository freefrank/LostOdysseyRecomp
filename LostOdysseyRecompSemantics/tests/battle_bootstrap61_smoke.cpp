#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_bootstrap61.h"
#include "battle_profile_fixture.h"
struct BootstrapGuest final : manager_release_context61::GuestServices {
  unsigned released = 0;
  void CallDirect(GuestAddress, GuestMemory &,
                  manager_release_context61::Registers &) override {
    throw std::runtime_error("bootstrap direct service");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (profile_fixture::Indirect(e, s))
      return;
    if (e != 0x123400 || s.r[3] != 0x70000 || s.r[5] || s.r[6] != 8)
      throw std::runtime_error("bootstrap resize ABI");
    ++released;
    s.r[3] = 0;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (unsigned p : {0x8330b000u, 0x83315000u, 0x832c1000u, 0x83264000u,
                       0x83263000u, 0x83245000u, 0x832ca000u})
      regions.push_back({p, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    BootstrapGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, stats = 0x64000;
    profile_fixture::Setup(m, 0x68000);
    m.WriteU32(0x83263198, 0x95000);
    m.WriteU32(0x92000 + 52, 0x95100);
    m.WriteU32(0x95100 + 60, 0x95000);
    m.WriteU32(0x8330b608, 0x70000);
    m.WriteU32(0x70000, 0x71000);
    m.WriteU32(0x71008, 0x123400);
    m.WriteU32(0x82000e50, 0);
    m.WriteU32(0x82007784, 0x3f800000);
    for (unsigned h : {owner + 8, owner + 132}) {
      m.WriteU32(h, 0x77000);
      m.WriteU32(h + 4, 2);
      m.WriteU32(h + 8, 2);
    }
    auto check = [&](bool b, const char *msg) {
      if (!b)
        throw std::runtime_error(msg);
    };
    auto call = [&](unsigned e, unsigned object) {
      s.r[3] = object;
      check(battle_bootstrap61::Apply(e, m, {guest, native}, s),
            "bootstrap entry");
      check(s.r[1] == initial.r[1] && s.r[27] == initial.r[27] &&
                s.r[31] == initial.r[31],
            "bootstrap nonvolatile state");
    };
    call(0x82aab200, owner);
    check(guest.released == 2 && m.ReadU32(owner + 20) == owner + 8 &&
              m.ReadU32(owner + 48) == owner + 36 && !m.ReadU32(owner + 8) &&
              !m.ReadU32(owner + 132),
          "list reset and internal headers");
    check(m.ReadU32(owner + 32) == 0x93000 && m.ReadU32(owner + 4) == 20 &&
              m.ReadU8(owner + 61) == 1 && m.ReadU8(owner + 65) == 1,
          "profile cast and manager defaults");
    for (unsigned p : {owner + 76, owner + 104})
      check(m.ReadU32(p + 4) == 0 && m.ReadU32(p + 20) == 0x3f800000 &&
                m.ReadU8(p + 24) == 1,
            "position defaults");
    m.WriteU32(0x92000 + 52, 0);
    call(0x82aab200, owner);
    check(!m.ReadU32(owner + 32) && guest.released == 2,
          "failed root type cast");
    call(0x82ad1c88, 0x69000);
    check(s.r[3] == 0x69000 && m.ReadU32(0x8324570c) == 0x69000 &&
              m.ReadU32(0x69000) == 0x820c0eb0 && !m.ReadU32(0x69010),
          "small manager construction");
    for (unsigned i = 0; i < 1216; ++i)
      m.WriteU8(stats + i, 0xa5);
    m.WriteU32(0x832ca0d0, 0x6a000);
    m.WriteU32(0x6a000 + 292, 0x6b000);
    for (unsigned i = 0; i < 13; ++i)
      m.WriteU32(0x6b000 + 4 * i, 100 + i);
    call(0x82ac1b90, stats);
    for (unsigned i = 0; i < 32; ++i) {
      auto row = stats + 100 + 12 * i;
      check(!m.ReadU32(row) && m.ReadU32(row + 4) == 0xffffffff &&
                !m.ReadU8(row + 8) && m.ReadU8(row + 9) == 0xa5,
            "stats roster reset and padding");
    }
    for (unsigned i = 0; i < 5; ++i) {
      auto row = stats + 564 + 128 * i;
      check(!m.ReadU32(row) && !m.ReadU32(row + 88) && !m.ReadU8(row + 96) &&
                m.ReadU8(row + 97) == 0xa5 && !m.ReadU32(row + 112) &&
                !m.ReadU32(row + 124),
            "stats working slots");
    }
    for (unsigned i = 0; i < 13; ++i)
      check(m.ReadU32(stats + 32 + 4 * i) == 100 + i,
            "descriptor defaults copied");
    check(m.ReadU32(stats) == 0xa5a5a5a5 && !m.ReadU32(stats + 1204) &&
              !m.ReadU8(stats + 1213) && m.ReadU8(stats + 1214) == 0xa5,
          "stats untouched fields");
    std::puts("PASS battle manager defaults, type cast, list reset and stats "
              "baseline");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
