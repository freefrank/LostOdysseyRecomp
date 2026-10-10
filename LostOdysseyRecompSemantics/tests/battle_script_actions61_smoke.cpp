#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_actions61.h"
#include <iostream>
#include "battle_action_record_fixture.h"
struct ActionsGuest final : manager_release_context61::GuestServices {
  unsigned mode = 4, predicate = 0, reset = 0, effects = 0, ready = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (ActionStorageDirectFixture(e, m, s))
      return;

    if (e == 0x82af68d8)
      return;
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x8238e2f8) {
      s.r[3] = 0x71000;
      return;
    }
    if (e == 0x82389b48) {
      s.r[3] = mode;
      return;
    }
    if (e == 0x8238e308) {
      s.r[3] = 0x80000;
      return;
    }
    if (e == 0x82afde70) {
      s.r[3] = predicate;
      return;
    }
    if (e == 0x82acd530) {
      ++reset;
      return;
    }
    if (e == 0x82b1f1d0)
      return;
    if (e == 0x82ac3118) {
      ++reset;
      return;
    }
    if (e == 0x82aca1b8 || e == 0x82ac9000) {
      if (s.r[3] != 0x80000 || s.r[4] != 9)
        throw std::runtime_error("target effect arguments");
      ++effects;
      return;
    }
    throw std::runtime_error("action direct boundary");
  }
  void CallIndirect(GuestAddress e, GuestMemory &m,
                    manager_release_context61::Registers &s) override {
    if (e == 0x123458) {
      if (s.r[3] != 0x73000 || s.ctr != e)
        throw std::runtime_error("composed eligibility descriptor ABI");
      m.WriteU8(0x73000 + 208, predicate);
      m.WriteU8(0x73000 + 76, 0);
      return;
    }
    if (ActionStorageIndirectFixture(e, m, s))
      return;
    if (e != 0x2000)
      throw std::runtime_error("action virtual boundary");
    s.r[3] = ready;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83245000, 0x1000});
    regions.push_back({0x832c9000, 0x4000});
    regions.push_back({0x83291000, 0x1000});
    regions.push_back({0x83213000, 0x1000});
    regions.push_back({0x8330b000, 0x1000});
    regions.push_back({0x821a8000, 0x1000});
    regions.push_back({0x83264000, 0x1000});
    regions.push_back({0x831f3000, 0x21000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    SetupActionStorageFixture(m);
    m.WriteU32(0x832ca0d8, 0x73000);
    m.WriteU32(0x73000 + 4 * 124, 0x123458);
    m.WriteU32(0x80000 + 132, 1);
    ActionsGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       code = 0x64000, vars = 0x65000, resource = 0x80000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(0x832c9c54 + 44, state);
    m.WriteU32(state + 4, actor);
    m.WriteU32(state + 12, 1);
    m.WriteU32(actor + 8, 24);
    m.WriteU32(state + 4, actor);
    m.WriteU32(state + 12, 1);
    m.WriteU32(actor, 7);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(actor + 4, resource);
    m.WriteU32(actor + 80, 0x66000);
    m.WriteU32(resource, 0x81000);
    m.WriteU32(0x81000 + 292, 0x2000);
    auto le = [&](unsigned p, unsigned x, unsigned n = 2) {
      for (unsigned i = 0; i < n; ++i)
        m.WriteU8(p + i, x >> (8 * i));
    };
    auto op = [&](unsigned e) {
      s.r[3] = owner;
      m.WriteU32(actor + 52, 0);
      (void)battle_script_actions61::Apply(e, m, {guest, native}, s);
      if (s.r[1] != initial.r[1] || s.r[27] != initial.r[27] ||
          s.r[31] != initial.r[31])
        throw std::runtime_error("actions ABI");
    };
    le(code + 1, 100);
    m.WriteU32(actor + 64, 0x20000000);
    op(0x82af6e10);
    if (m.ReadU32(actor + 52) != 100)
      throw std::runtime_error("action flag branch");
    m.WriteU32(actor + 88, 1);
    op(0x82af7110);
    if (m.ReadU32(actor + 52) != 100)
      throw std::runtime_error("blocked action branch");
    m.WriteU32(actor + 88, 2);
    guest.predicate = 0;
    op(0x82af7110);
    if (m.ReadU32(actor + 52) != 3)
      throw std::runtime_error("available action branch");
    m.WriteU32(actor + 96, 5);
    m.WriteU32(actor + 64, 0x01000000);
    m.WriteU32(resource + 100, 0x80000000);
    op(0x8238c3e8);
    if (m.ReadU32(actor + 96) || (m.ReadU32(actor + 64) & 0x01000000) ||
        !(m.ReadU32(resource + 124) & 512) || guest.reset != 1)
      throw std::runtime_error("action preparation");
    guest.mode = 0;
    m.WriteU32(resource + 100, 0);
    m.WriteU32(actor + 64, 0x20000000);
    op(0x8238c3e8);
    if (m.ReadU32(actor + 52) != 100 || (m.ReadU32(actor + 64) & 0x20000000))
      throw std::runtime_error("prepare branch refresh");
    guest.mode = 4;
    le(code + 1, 0);
    le(code + 3, 1);
    m.WriteU32(vars, 0);
    m.WriteU32(vars + 4, 9);
    m.WriteU32(resource + 64, 24);
    m.WriteU32(actor + 84, 0);
    m.WriteU32(actor + 96, 0);
    for (unsigned event : {1u, 2u, 3u}) {
      m.WriteU32(actor + 60, event);
      m.WriteU32(actor + 96, 0);
      op(0x82b00888);
    }
    if (!(m.ReadU32(resource + 100) & 0x80000000u) || m.ReadU8(0x66000) != 24 ||
        m.ReadU32(actor + 84) != 1)
      throw std::runtime_error("action dispatch selection");
    guest.predicate = 1;
    op(0x82afe628);
    if (m.ReadU32(vars + 4) != 0 || m.ReadU8(0x66000) != 24)
      throw std::runtime_error("recovered self-target preparation result");
    guest.ready = 0;
    op(0x82af6e70);
    if (m.ReadU32(vars + 4) || m.ReadU32(actor + 336) != 8)
      throw std::runtime_error("target selection");
    m.WriteU8(code + 1, 2);
    m.WriteU16(0x70000 + 148, 1);
    m.WriteU32(0x70000 + 164, 5);
    op(0x82af9568);
    if (m.ReadU32(resource + 132) != 1 || m.ReadU32(0x70000 + 164) != 6)
      throw std::runtime_error("conditional resource count");
    m.WriteU8(code + 1, 1);
    op(0x82af99d8);
    if (!(m.ReadU32(actor + 64) & 0x10000000))
      throw std::runtime_error("actor low-bit flag");
    le(code + 1, 0);
    m.WriteU32(0x70000 + 56, 12);
    op(0x8238d0e8);
    if (m.ReadU32(vars) != 12)
      throw std::runtime_error("battle phase query");
    m.WriteU32(vars, 1);
    m.WriteU32(vars + 4, 23);
    op(0x82af9740);
    if (m.ReadU32(resource + 16 + 75932) != 23 ||
        m.ReadU32(resource + 16 * 4747) != 0xfffffffb)
      throw std::runtime_error("resource slot reset");
    le(code + 1, 7, 4);
    le(code + 5, 0);
    le(code + 7, 1);
    le(code + 9, 2);
    m.WriteU32(resource + 14660, 1);
    m.WriteU32(resource + 14656, 0x90000);
    m.WriteU32(0x90000, 11);
    m.WriteU32(0x90004, 22);
    m.WriteU32(0x90000 + 14884, 33);
    op(0x82af9c10);
    if (m.ReadU32(vars) != 11 || m.ReadU32(vars + 4) != 22 ||
        m.ReadU32(vars + 8) != 33)
      throw std::runtime_error("resource record query");
    m.WriteU32(vars, 9);
    m.WriteU32(0x83213438 + 8 * 9, 0);
    m.WriteU32(0x83213438 + 8 * 9 + 4, 512);
    m.WriteU32(resource + 232, 512);
    op(0x8238c018);
    if (m.ReadU32(vars + 4) != 512)
      throw std::runtime_error("property mask query");
    op(0x82af8458);
    op(0x82af8510);
    if (guest.effects != 2)
      throw std::runtime_error("resource effects");
    std::cout << "battle_script_actions61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
