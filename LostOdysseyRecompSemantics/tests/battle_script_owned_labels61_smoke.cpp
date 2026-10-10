#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_owned_labels61.h"
#include <iostream>
struct LabelGuest final : manager_release_context61::GuestServices {
  unsigned commands = 0, allocations = 0, initializations = 0, flag = 0,
           expected = 0x41, predicate = 0;
  void Need(bool b) {
    if (!b)
      throw std::runtime_error("owned label ABI");
  }
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82296830) {
      unsigned n = 0;
      while (m.ReadU16(unsigned(s.r[3]) + 2 * n))
        ++n;
      s.r[3] = n;
      return;
    }
    if (e == 0x827c5f38) {
      ++initializations;
      m.WriteU32(0x8330b608, 0x70000);
      m.WriteU32(0x70000, 0x71000);
      m.WriteU32(0x71008, 0x2000);
      return;
    }
    if (e == 0x822954d8) {
      auto p = unsigned(s.r[3]), n = unsigned(s.r[4]);
      m.WriteU32(p, n ? 0x80000 : 0);
      m.WriteU32(p + 4, n);
      m.WriteU32(p + 8, n);
      return;
    }
    if (e == 0x82b7a0b0) {
      for (unsigned i = 0; i < unsigned(s.r[5]); ++i)
        m.WriteU8(unsigned(s.r[3]) + i, m.ReadU8(unsigned(s.r[4]) + i));
      return;
    }
    Need(s.r[3] == 0x832ca0e0 + 5232 && s.r[4] == 17);
    if (e == 0x82ab6850) {
      ++commands;
      return;
    }
    if (e == 0x82ab4758) {
      s.r[3] = predicate;
      return;
    }
    if (e == 0x82ab6b00) {
      Need(s.r[5] == 9 && s.fpr_bits[1] == 0);
      ++commands;
      return;
    }
    if (e == 0x82ab8e08 || e == 0x82abde20 || e == 0x82ab68c0 ||
        e == 0x82abdc68) {
      auto p = unsigned(e == 0x82ab8e08 ? s.r[6] : s.r[5]);
      Need(p == unsigned(s.r[1]) + 80 && m.ReadU32(p + 12) == p);
      if (expected)
        Need(m.ReadU32(p + 4) == 2 && m.ReadU32(p + 8) == 2 &&
             m.ReadU16(m.ReadU32(p)) == expected &&
             !m.ReadU16(m.ReadU32(p) + 2));
      else
        Need(!m.ReadU32(p) && !m.ReadU32(p + 4) && !m.ReadU32(p + 8));
      if (e == 0x82ab8e08)
        Need(s.r[5] == 9);
      if (e == 0x82ab68c0 || e == 0x82abdc68)
        Need(s.r[6] == flag);
      ++commands;
      return;
    }
    throw std::runtime_error("unexpected owned label service");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    Need(e == 0x2000 && s.r[3] == 0x70000 && s.r[4] == 0 && s.r[5] == 4 &&
         s.r[6] == 8);
    ++allocations;
    s.r[3] = 0x80000;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x8330b000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    LabelGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       code = 0x64000, vars = 0x65000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(vars, 17);
    m.WriteU32(vars + 4, 9);
    auto le = [&](unsigned p, unsigned v) {
      m.WriteU8(p, v);
      m.WriteU8(p + 1, v >> 8);
    };
    auto clear = [&]() {
      for (unsigned i = 0; i < 80; ++i)
        m.WriteU8(code + i, 0);
    };
    auto op = [&](unsigned e, unsigned pc) {
      s.r[3] = owner;
      m.WriteU32(actor + 52, 0);
      guest.Need(battle_script_owned_labels61::Apply(e, m, {guest, native}, s));
      guest.Need(s.r[1] == initial.r[1] && s.r[26] == initial.r[26] &&
                 s.r[31] == initial.r[31] && m.ReadU32(actor + 52) == pc);
    };
    le(code + 1, 0);
    le(code + 3, 1);
    m.WriteU16(code + 5, 0x41);
    op(0x82b016e8, 69);
    guest.Need(guest.initializations == 1 && guest.allocations == 1);
    clear();
    m.WriteU16(code + 3, 0x41);
    op(0x82b01d10, 67);
    guest.Need(guest.allocations == 2);
    clear();
    guest.expected = 0;
    op(0x82b01d10, 67);
    guest.Need(guest.allocations == 2);
    guest.expected = 0x41;
    for (unsigned e : {0x82b018c0u, 0x82b01ae8u})
      for (unsigned mode : {0u, 1u, 2u}) {
        clear();
        m.WriteU8(code + 1, mode);
        le(code + 2, 0);
        m.WriteU16(code + (e == 0x82b018c0 ? 3 : 4), 0x41);
        guest.flag = mode == 1;
        op(e, 68);
      }
    clear();
    le(code + 1, 0);
    le(code + 3, 1);
    op(0x82afb0c8, 3);
    op(0x82afb120, 5);
    le(code + 3, 0xfffe);
    guest.predicate = 0;
    op(0x82afb198, 0xfffffffe);
    guest.predicate = 2;
    op(0x82afb198, 5);
    std::cout << "battle_script_owned_labels61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
