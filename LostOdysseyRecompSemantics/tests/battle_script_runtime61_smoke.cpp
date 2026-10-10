#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_runtime61.h"
#include <iostream>
struct RuntimeGuest final : manager_release_context61::GuestServices {
  unsigned commands = 0, virtuals = 0, toggles = 0, predicate = 1;
  void CallDirect(GuestAddress e, GuestMemory &m,
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
    if (e == 0x82380d30) {
      s.r[3] = 0x72000;
      return;
    }
    if (e == 0x82b1e4e0 || e == 0x82b1e540) {
      if (s.r[3] != 0x832ca0e0 + 224 || s.r[4] != 24 || s.r[5] != 9 ||
          s.r[6] != 8)
        throw std::runtime_error("runtime command args");
      ++commands;
      return;
    }
    if (e == 0x82b035e0 || e == 0x82b04c50) {
      if (s.r[3] != 0x832cc05c)
        throw std::runtime_error("toggle object");
      ++toggles;
      return;
    }
    if (e == 0x82389aa0) {
      m.WriteU8(0x74000 + 133, predicate);
      s.r[3] = 0x74000;
      return;
    }
    throw std::runtime_error("runtime direct boundary");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if ((e != 0x2000 && e != 0x2010) || s.r[3] != 0x832ca0e0 + 232 ||
        s.r[4] != 24 || (e == 0x2010 && s.r[5] != 75))
      throw std::runtime_error("runtime virtual command");
    ++virtuals;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x832c9000, 0x4000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    RuntimeGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       code = 0x64000, vars = 0x65000;
    m.WriteU32(0x832ca0d0, 0x75000);
    m.WriteU32(0x75000 + 452, 2);
    m.WriteU32(0x832cc05c + 116, 0xffffffff);
    m.WriteU32(0x832cc05c + 120, 0xffffffff);
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(state + 4, actor);
    m.WriteU32(state + 12, 2);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(actor + 4, 0x80000);
    m.WriteU32(actor + 8, 24);
    m.WriteU32(actor + 64, 0x00400000);
    m.WriteU32(actor + 472 + 8, 25);
    m.WriteU32(actor + 80, 0x66000);
    m.WriteU32(0x832c9c54 + 44, state);
    m.WriteU32(0x71000, 0x71100);
    m.WriteU32(0x71004, 2);
    m.WriteU32(0x71100, 0x80000);
    m.WriteU32(0x71104, 0x84000);
    m.WriteU32(0x80000 + 64, 24);
    m.WriteU32(0x84000 + 64, 25);
    m.WriteU32(0x80000 + 68, 7);
    auto le = [&](unsigned p, unsigned x) {
      m.WriteU8(p, x);
      m.WriteU8(p + 1, x >> 8);
    };
    auto op = [&](unsigned e) {
      s.r[3] = owner;
      m.WriteU32(actor + 52, 0);
      (void)battle_script_runtime61::Apply(e, m, {guest, native}, s);
      if (s.r[1] != initial.r[1] || s.r[26] != initial.r[26] ||
          s.r[31] != initial.r[31])
        throw std::runtime_error("runtime ABI");
    };
    m.WriteU8(code + 1, 1);
    op(0x82afa1a8);
    if (!(m.ReadU32(actor + 64) & 0x00800000))
      throw std::runtime_error("actor flag23");
    le(code + 1, 0);
    m.WriteU32(vars, 17);
    op(0x82afa200);
    if (m.ReadU32(0x70000 + 156) != 17)
      throw std::runtime_error("manager field");
    op(0x82aff2a8);
    if (m.ReadU32(vars) != 1)
      throw std::runtime_error("linked actor flags count");
    le(code + 1, 0);
    le(code + 3, 1);
    le(code + 5, 2);
    m.WriteU32(vars, 24);
    m.WriteU32(vars + 4, 9);
    m.WriteU32(vars + 8, 8);
    op(0x82afb5e0);
    op(0x82afb718);
    m.WriteU32(0x832ca0e0 + 232, 0x73000);
    m.WriteU32(0x73000 + 24, 0x2000);
    m.WriteU32(0x73000 + 28, 0x2010);
    op(0x82afb650);
    op(0x82afb6b0);
    if (guest.commands != 2 || guest.virtuals != 2)
      throw std::runtime_error("runtime command dispatch");
    m.WriteU32(vars, 7);
    op(0x82aff3f0);
    if (m.ReadU32(vars + 4) != 24)
      throw std::runtime_error("group resource resolution");
    m.WriteU32(vars, 25);
    op(0x82aff3f0);
    if (m.ReadU32(vars + 4) != 25)
      throw std::runtime_error("resource ID passthrough");
    m.WriteU32(vars, 24);
    op(0x82af6f58);
    if (m.ReadU32(vars + 4) || m.ReadU8(0x66000) != 24 ||
        m.ReadU32(actor + 336) != 8)
      throw std::runtime_error("existing target selection");
    m.WriteU32(vars, 99);
    op(0x82af6f58);
    if (m.ReadU32(vars + 4) != 1)
      throw std::runtime_error("missing target selection");
    for (unsigned mode = 0; mode < 3; ++mode) {
      m.WriteU8(code + 1, mode);
      op(0x82afcc48);
    }
    if (guest.toggles != 2 || m.ReadU8(0x832ca0e0 + 5764) != 1)
      throw std::runtime_error("mode toggles");
    m.WriteU8(code + 1, 0);
    le(code + 2, 0);
    le(code + 4, 100);
    op(0x82afccf8);
    if (m.ReadU32(actor + 52) != 6)
      throw std::runtime_error("runtime predicate true");
    guest.predicate = 0;
    op(0x82afccf8);
    if (m.ReadU32(actor + 52) != 100)
      throw std::runtime_error("runtime predicate false");
    m.WriteU8(code + 1, 1);
    m.WriteU32(0x72000 + 604, 16);
    op(0x82afccf8);
    if (m.ReadU32(actor + 52) != 6)
      throw std::runtime_error("scene object predicate");
    m.WriteU8(code + 1, 3);
    op(0x82afccf8);
    if (m.ReadU32(actor + 52))
      throw std::runtime_error("invalid predicate mode preserves cursor");
    auto complete = [&](unsigned kind, unsigned value) {
      s.r[3] = owner;
      s.r[4] = 0x80000;
      s.r[5] = kind;
      s.r[6] = value;
      if (!battle_script_runtime61::Apply(0x82af69d0, m, {guest, native}, s))
        throw std::runtime_error("result dispatch");
    };
    m.WriteU32(actor + 64, 0x38004);
    complete(5, 123);
    if (s.r[3] != 1 || m.ReadU32(actor + 64) != 0xa8004 ||
        m.ReadU32(actor + 328) != 123)
      throw std::runtime_error("actor result publication");
    complete(1, 99);
    if (s.r[3] || m.ReadU32(actor + 328) != 123)
      throw std::runtime_error("pending result preserved");
    m.WriteU32(actor + 64, 0x100000);
    complete(2, 99);
    if (s.r[3] || m.ReadU32(actor + 64) != 0x100000)
      throw std::runtime_error("blocked result preserved");
    std::cout << "battle_script_runtime61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
