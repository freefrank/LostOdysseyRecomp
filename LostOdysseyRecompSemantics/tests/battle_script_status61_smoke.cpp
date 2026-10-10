#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_status61.h"
#include <iostream>
struct StatusGuest final : manager_release_context61::GuestServices {
  unsigned predicate = 0x107, calls = 0, busy = 0;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x8238e2f8) {
      s.r[3] = 0x71000;
      return;
    }
    if (e == 0x8238e308) {
      s.r[3] = s.r[4] == 24 ? 0x80000 : 0;
      return;
    }
    if (e == 0x82ad6c90 || e == 0x82ad6088 || e == 0x82ad6318 ||
        e == 0x82ad6710 || e == 0x82ad5f28 || e == 0x82ad6198 ||
        e == 0x82ad6448) {
      if (s.r[3] != 0x832cbfb0 || s.r[4] != 24)
        throw std::runtime_error("predicate arguments");
      ++calls;
      s.r[3] = predicate;
      return;
    }
    throw std::runtime_error("unexpected status service");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e != 0x2000 || s.r[3] != 0x80000)
      throw std::runtime_error("resource readiness ABI");
    s.r[3] = busy;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x832c9000, 0x4000});
    regions.push_back({0x832ae000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    StatusGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       code = 0x69000, vars = 0x6a000, resource = 0x80000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(state + 4, actor);
    m.WriteU32(state + 12, 1);
    m.WriteU32(actor + 8, 24);
    m.WriteU32(actor, 24);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(actor + 4, resource);
    m.WriteU32(actor + 80, 0x6b000);
    m.WriteU32(resource + 64, 24);
    m.WriteU32(resource, 0x72000);
    m.WriteU32(0x72000 + 292, 0x2000);
    m.WriteU32(0x832c9c54 + 44, state);
    auto le = [&](unsigned p, unsigned x) {
      m.WriteU8(p, x);
      m.WriteU8(p + 1, x >> 8);
    };
    auto id = [&](unsigned off) {
      le(code + off, 24);
      le(code + off + 2, 0);
    };
    auto check = [&](bool yes, const char *why) {
      if (!yes)
        throw std::runtime_error(why);
    };
    auto op = [&](unsigned e) {
      s.r[3] = owner;
      m.WriteU32(actor + 52, 0);
      check(battle_script_status61::Apply(e, m, {guest, native}, s),
            "entry routing");
      check(s.r[1] == initial.r[1] && s.r[28] == initial.r[28] &&
                s.r[31] == initial.r[31],
            "status ABI");
    };
    id(1);
    le(code + 5, 0xfffe);
    op(0x82af7e18);
    check(m.ReadU32(actor + 52) == 7, "actor ready branch");
    m.WriteU32(resource + 124, 64);
    op(0x82af7e18);
    check(m.ReadU32(actor + 52) == 0xfffffffe, "signed not-ready branch");
    le(code + 1, 0);
    m.WriteU32(vars, 5);
    op(0x82af7b68);
    check(m.ReadU32(vars) == 1 && m.ReadU32(state + 16724) == 60 &&
              (m.ReadU32(state + 28) & 0xc0000) == 0x40000,
          "phase start");
    m.WriteU32(state + 28, 0x80000);
    op(0x82af7b68);
    check(m.ReadU32(vars) == 0 && m.ReadU32(state + 28) == 0, "phase complete");
    m.WriteU8(code + 1, 0);
    le(code + 2, 0);
    m.WriteU32(vars, 44);
    m.WriteU32(resource + 75932 + 16 * 4, 44);
    op(0x82afac40);
    check(m.ReadU32(resource + 75944 + 16 * 4) == 1, "indexed enable");
    m.WriteU8(code + 1, 1);
    op(0x82afac40);
    check(m.ReadU32(resource + 75944 + 16 * 4) == 0, "indexed disable");
    le(code + 1, 0);
    le(code + 3, 1);
    m.WriteU32(actor + 64, 0x100000 | (5 << 15));
    m.WriteU32(actor + 328, 77);
    op(0x82af76c0);
    check(m.ReadU32(vars) == 5 && m.ReadU32(vars + 4) == 77 &&
              !(m.ReadU32(actor + 64) & 0x180000),
          "consume action result");
    op(0x82af76c0);
    check(!m.ReadU32(vars) && !m.ReadU32(vars + 4), "consumed action default");
    m.WriteU32(0x832aeb00, 0x73000);
    m.WriteU32(0x73000 + 56, 99);
    m.WriteU8(code + 1, 2);
    le(code + 2, 0);
    op(0x82afa258);
    check(m.ReadU32(vars) == 99, "global mode record");
    le(code + 1, 0);
    le(code + 3, 1);
    le(code + 5, 2);
    m.WriteU32(vars, 24);
    m.WriteU32(vars + 4, 7);
    m.WriteU32(vars + 8, 9);
    op(0x82aff360);
    check(m.ReadU32(resource + 212) == 7 && m.ReadU32(resource + 216) == 9,
          "set resource group");
    m.WriteU32(0x71000, 0x71100);
    m.WriteU32(0x71004, 3);
    m.WriteU32(0x71100, resource);
    m.WriteU32(0x71104, 0x85000);
    m.WriteU32(0x71108, 0x90000);
    m.WriteU32(0x85000 + 212, 7);
    m.WriteU32(0x85000 + 216, 5);
    m.WriteU32(0x90000 + 212, 8);
    m.WriteU32(vars + 4, 0);
    op(0x82aff360);
    check(!m.ReadU32(resource + 212) && !m.ReadU32(0x85000 + 216) &&
              m.ReadU32(0x90000 + 212) == 8,
          "clear matching group");
    id(1);
    le(code + 5, 0);
    op(0x82af7028);
    check(!m.ReadU32(vars) && m.ReadU8(0x6b000) == 24 &&
              m.ReadU32(actor + 336) == 8,
          "ready target select");
    guest.busy = 1;
    op(0x82af7028);
    check(m.ReadU32(vars) == 1, "busy target");
    m.WriteU8(code + 1, 2);
    id(2);
    m.WriteU16(0x70000 + 148, 1);
    op(0x82af9648);
    check(m.ReadU32(resource + 132) == 1 && m.ReadU32(0x70000 + 164) == 1,
          "other actor activation");
    le(code + 1, 0);
    le(code + 3, 1);
    m.WriteU32(vars + 4, 24);
    for (unsigned e : {0x82afacf8u, 0x82afae10u, 0x82afaf28u, 0x82afb040u}) {
      op(e);
      check(m.ReadU32(vars) == 7, "predicate low byte");
    }
    m.WriteU32(vars, 24);
    le(code + 3, 0xfffc);
    for (unsigned e : {0x82afad80u, 0x82afae98u, 0x82afafb0u}) {
      guest.predicate = 2;
      op(e);
      check(m.ReadU32(actor + 52) == 0xfffffffcu, "predicate exact-one branch");
      guest.predicate = 0x101;
      op(e);
      check(m.ReadU32(actor + 52) == 5, "predicate low-byte one");
    }
    auto count = guest.calls;
    m.WriteU32(state + 28, 0x20000);
    guest.predicate = 0;
    op(0x82afad80);
    check(guest.calls == count && m.ReadU32(actor + 52) == 5,
          "suppression bypass");
    std::cout << "battle_script_status61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
