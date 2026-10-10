#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_resource_creation61.h"
#include "lo_semantics/battle_semantic_runtime61.h"
#include "battle_resource_growth_fixture.h"
struct CreationGuest final : manager_release_context61::GuestServices {
  bool found = true, loadSucceeds = true;
  unsigned allocations = 0, loads = 0, created = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x8229c7b8) {
      s.r[3] = found ? 0x72000 : 0;
      return;
    }
    if (e == 0x823ffdd8) {
      ++loads;
      s.r[3] = loadSucceeds ? 0x72000 : 0;
      return;
    }
    if (e == 0x82300100) {
      s.r[3] = 17;
      return;
    }
    if (e == 0x82401a10) {
      ++created;
      s.r[3] = 0x80000;
      return;
    }
    if (e == 0x82400a08)
      return;
    if (e == 0x828080a8) {
      if (s.r[3] != 4)
        throw std::runtime_error("roster stride");
      auto p = unsigned(s.r[4]), count = m.ReadU32(p + 4);
      m.WriteU32(p + 4, count + 1);
      s.r[3] = m.ReadU32(p) + count * 4;
      return;
    }
    if (battle_semantic_runtime61::Apply(e, m, {*this, cook_main_smoke::native},
                                         s))
      return;
    std::fprintf(stderr, "unhandled creation direct %08x\n", e);
    throw std::runtime_error("creation direct service");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e != 0x123400 || s.r[3] != 0x70000 || s.r[6] != 8)
      throw std::runtime_error("creation allocator ABI");
    ++allocations;
    s.r[3] = s.r[5] ? 0x300000 : 0;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    growth_fixture::Regions(regions);
    for (unsigned p : {0x8330b000u, 0x83315000u, 0x832c9000u, 0x832ca000u,
                       0x820c4000u, 0x83291000u})
      regions.push_back({p, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    growth_fixture::Setup(m);
    CreationGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, resource = 0x80000, list = 0x61000,
                       roster = 0x62000, profile = 0x200000, stats = 0x74000;
    m.WriteU32(0x8330b608, 0x70000);
    m.WriteU32(0x70000, 0x71000);
    m.WriteU32(0x71008, 0x123400);
    m.WriteU32(0x83315f9c, 0x72000);
    m.WriteU32(0x832c99f0, 0x73000);
    m.WriteU32(0x83291dc0, stats);
    m.WriteU32(owner + 20, list);
    m.WriteU32(list, roster);
    m.WriteU32(owner + 32, profile);
    m.WriteU32(0x83264978 + 116, 0x75000);
    m.WriteU32(0x82000e40, 0x3f800000);
    m.WriteU32(0x82000bb8, 0x40000000);
    auto check = [&](bool b, const char *msg) {
      if (!b)
        throw std::runtime_error(msg);
    };
    auto call = [&](unsigned id) {
      s.r[3] = owner;
      s.r[4] = 2;
      s.r[5] = 1;
      s.r[6] = id;
      s.r[7] = 77;
      for (unsigned i = 1; i <= 4; ++i)
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(i));
      check(
          battle_resource_creation61::Apply(0x82af5d18, m, {guest, native}, s),
          "creation entry");
      check(s.r[1] == initial.r[1], "creation stack");
      for (unsigned i = 22; i < 32; ++i)
        check(s.r[i] == initial.r[i], "creation nonvolatile GPR");
      for (unsigned i = 28; i < 32; ++i)
        check(s.fpr_bits[i] == initial.fpr_bits[i], "creation nonvolatile FPR");
    };
    guest.found = false;
    guest.loadSucceeds = false;
    call(2);
    check(!s.r[3] && !guest.created && !m.ReadU32(list + 4),
          "class load failure cleanup");
    guest.loadSucceeds = true;
    auto row = profile + 124 + 14308 * 2;
    m.WriteU32(row + 12, 9);
    m.WriteU32(row + 248, 0x42480000); // 50 current HP.
    call(2);
    check(s.r[3] == resource && m.ReadU32(list + 4) == 1 &&
              m.ReadU32(roster) == resource,
          "party creation and roster append");
    check(m.ReadU32(resource + 64) == 2 && m.ReadU32(resource + 68) == 2 &&
              m.ReadU32(resource + 72) == 77 && m.ReadU32(resource + 140) == 9,
          "party identifiers and profile restore");
    check(m.ReadU32(resource + 14660) == 1 && m.ReadU32(0x300000 + 36) == 0 &&
              m.ReadU32(resource + 148) == 0xffffffff &&
              m.ReadU32(resource + 60) == 0,
          "actual resource reset and action record creation");
    check(m.ReadU32(resource + 76324) == 0x3f800000 &&
              m.ReadU32(resource + 76328) == 0x40000000 &&
              m.ReadU32(resource + 76332) == 0x40400000 &&
              m.ReadU32(resource + 76344) == 0x41000000,
          "spawn coordinates and scaled angle");
    check((m.ReadU32(resource + 124) & 0x58000000) == 0x58000000 &&
              m.ReadU32(resource + 132) == 1,
          "party flags and active baseline");
    // A fresh synthetic object exercises the creature initialization branch.
    for (unsigned i = 0; i < 77000; ++i)
      m.WriteU8(resource + i, 0);
    m.WriteU32(0x140000 + 280, 7);
    guest.found = true;
    call(20);
    check(s.r[3] == resource && m.ReadU32(list + 4) == 2 &&
              m.ReadU32(resource + 64) == 20 && m.ReadU32(resource + 140) == 7,
          "creature creation");
    check((m.ReadU32(resource + 124) & 0x18000000) == 0 &&
              m.ReadU32(resource + 2588) == m.ReadU32(resource + 2592),
          "creature full HP and side flags");
    check(guest.created == 2 && guest.loads == 2, "class service paths");
    std::puts("PASS resource factory failure, party/creature initialization, "
              "actual record setup and roster append");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
