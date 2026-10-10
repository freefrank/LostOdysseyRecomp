#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_marshaling61.h"
#include <iostream>
struct MarshalGuest final : manager_release_context61::GuestServices {
  unsigned prepared = 0, executed = 0, picker = 0, ok = 1, kind = 0, detail = 0,
           labels = 0, flags = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    auto fail = [e](bool good) {
      if (!good)
        throw std::runtime_error("marshaling service ABI " + std::to_string(e));
    };
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x8238e2f8) {
      s.r[3] = 0x71000;
      return;
    }
    if (e == 0x82acf108) {
      fail(s.r[3] == 0x70000);
      return;
    }
    if (e == 0x82afde70) {
      fail(s.r[3] == 0x60000 && s.r[5] == 0);
      prepared = unsigned(s.r[4]) + 1;
      return;
    }
    if (e == 0x82ac9aa8) {
      s.r[3] = ok;
      return;
    }
    if (e == 0x82ad0c10) {
      s.r[3] = ok;
      return;
    }
    if (e == 0x82aa0740) {
      picker = s.r[6] == 100   ? 0x82aff8e0
               : s.r[6] == 101 ? 0x82aff9e8
                               : 0x82aff4e8;
      s.r[3] = 0;
      return;
    }
    if (e == 0x82b08b80) {
      s.r[3] = 7;
      return;
    }
    if (e == 0x8238e308) {
      s.r[3] = 0x80000;
      return;
    }
    if (e == 0x82ac9a28) {
      ++executed;
      s.r[3] = 0;
      return;
    }
    if (e == 0x82ab36c8) {
      fail(s.r[3] == 0x80000 && !m.ReadU32(0x80000 + 14680));
      kind = m.ReadU32(0x62000 + 88);
      detail = m.ReadU32(0x62000 + 92);
      return;
    }
    if (e == 0x82b2a138) {
      fail(s.r[3] == 0x832ca0e0 + 5232 && s.r[4] == 17 && s.r[5] == 9 &&
           s.r[6] == 1 && s.r[7] == 0);
      ++flags;
      return;
    }
    if (e == 0x82b2a330) {
      fail(s.r[3] == 0x832ca0e0 + 5232 && s.r[4] == 17 && s.r[6] == 1 &&
           s.r[7] == 1 && m.ReadU16(unsigned(s.r[5])) == 0x41 + 31 &&
           m.ReadU16(unsigned(s.r[5]) + 2) == 0);
      ++labels;
      return;
    }
    if (e == 0x82aadcb8) {
      auto sp = unsigned(s.r[1]);
      fail(s.r[3] == 0x832ca0e0 && s.r[4] == 17 && s.r[5] == 9 && s.r[6] == 5 &&
           s.r[7] == sp + 160 && s.r[8] == 0x3f8000003f800000ull &&
           s.r[9] == 0x3f80000000000000ull && s.r[10] == 0);
      for (unsigned i = 0; i < 32; ++i)
        fail(m.ReadU16(sp + 160 + 2 * i) == 0x41 + i &&
             m.ReadU16(0x62000 + 340 + 2 * i) == 0x41 + i);
      fail(!m.ReadU16(sp + 224) && !m.ReadU32(sp + 88) &&
           !recovery_abi::ReadU64(m, sp + 96) && !m.ReadU32(sp + 104) &&
           !m.ReadU32(sp + 116));
      ++labels;
      return;
    }
    throw std::runtime_error("unexpected marshaling service");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("unexpected indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83213000, 0x1000});
    regions.push_back({0x83264000, 0x1000});
    regions.push_back({0x832c9000, 0x4000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    MarshalGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       code = 0x64000, vars = 0x65000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(actor + 4, 0x80000);
    m.WriteU32(actor + 80, 0x66000);
    m.WriteU32(actor + 84, 1);
    m.WriteU8(0x66000, 24);
    m.WriteU32(0x82007784, 0x3f800000);
    m.WriteU32(0x80000 + 232, 3);
    m.WriteU32(0x80000 + 15212, 19);
    m.WriteU32(0x83264984, 0x90000);
    m.WriteU32(0x90000 + 96 * 19 + 8, 8);
    for (unsigned i = 0; i < 25; ++i)
      m.WriteU32(0x832139e8 + 4 * i, 19);
    for (unsigned i = 0; i < 6; ++i)
      m.WriteU32(0x83213a4c + 4 * i, 19);
    for (unsigned i = 0; i < 53; ++i)
      m.WriteU32(0x83213a68 + 4 * i, 19);

    auto le = [&](unsigned p, unsigned v) {
      m.WriteU8(p, v);
      m.WriteU8(p + 1, v >> 8);
    };
    auto check = [](bool yes) {
      if (!yes)
        throw std::runtime_error("marshaling result");
    };
    auto op = [&](unsigned e) {
      s.r[3] = owner;
      m.WriteU32(actor + 52, 0);
      check(battle_script_marshaling61::Apply(e, m, {guest, native}, s));
      check(s.r[1] == initial.r[1] && s.r[26] == initial.r[26] &&
            s.r[31] == initial.r[31]);
    };
    le(code + 1, 0);
    le(code + 3, 1);
    for (unsigned mode = 0; mode < 6; ++mode) {
      m.WriteU32(vars, mode);
      m.WriteU32(0x80000 + 14680, 99);
      op(0x82b00d08);
      check(!m.ReadU32(vars + 4) && m.ReadU32(actor + 52) == 5);
      if (mode == 0)
        check(guest.kind == 0 && m.ReadU32(actor + 84) == 1);
      if (mode == 4)
        check(guest.kind == 30 && m.ReadU32(actor + 84) == 0);
      if (mode >= 1 && mode <= 3)
        check(guest.kind == 7 && guest.detail == 19);
      if (mode == 5)
        check(guest.kind == 0 && guest.detail == 0);
    }
    m.WriteU32(0x70000 + 24, 1);
    m.WriteU32(vars, 4);
    op(0x82b00d08);
    check(guest.picker == 0x82aff4e8 && guest.kind == 7);
    guest.ok = 0;
    auto executions = guest.executed;
    m.WriteU32(vars, 2);
    op(0x82b00d08);
    check(m.ReadU32(vars + 4) == 1 && guest.executed == executions);
    m.WriteU32(actor + 4, 0);
    op(0x82b00d08);
    check(!m.ReadU32(vars + 4) && guest.executed == executions);
    m.WriteU32(actor + 4, 0x80000);
    m.WriteU32(actor + 80, 0x66000);
    m.WriteU32(actor + 84, 1);
    m.WriteU8(0x66000, 24);
    le(code + 1, 0);
    le(code + 3, 1);
    le(code + 5, 2);
    le(code + 7, 3);
    m.WriteU32(vars, 17);
    m.WriteU32(vars + 4, 9);
    m.WriteU32(vars + 8, 5);
    m.WriteU32(vars + 12, 0);
    op(0x82afcec8);
    check(guest.flags == 1 && m.ReadU32(actor + 52) == 9);
    for (unsigned i = 0; i < 32; ++i)
      m.WriteU16(code + 7 + 2 * i, 0x41 + i);
    op(0x82afcb08);
    check(m.ReadU32(actor + 52) == 71);
    op(0x82afcf60);
    check(guest.labels == 2 && m.ReadU32(actor + 52) == 71);
    std::cout << "battle_script_marshaling61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
