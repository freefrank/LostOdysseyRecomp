#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_global_modes61.h"
#include <iostream>
struct ModeGuest final : manager_release_context61::GuestServices {
  unsigned copies = 0;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x82389aa0) {
      s.r[3] = 0x74000;
      return;
    }
    if (e == 0x8238e2f8) {
      s.r[3] = 0x71000;
      return;
    }
    if (e == 0x8229dfd8) {
      s.r[3] = 0xa0000;
      return;
    }
    if (e == 0x822b3f50) {
      ++copies;
      return;
    }
    if (e == 0x82abdfd0 || e == 0x82aab870 || e == 0x82af5ba8 ||
        e == 0x82afd2f0 || e == 0x8285ff08 || e == 0x8285f9b8 ||
        e == 0x8285fea8 || e == 0x82afd218 || e == 0x82ac8448 ||
        e == 0x82ac7b08 || e == 0x82ac7fc8 || e == 0x82ac6e60 ||
        e == 0x82ac6f08)
      return;
    throw std::runtime_error("unexpected global mode service");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e != 0x2000 || s.r[3] != 0x73000)
      throw std::runtime_error("buffer virtual ABI");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (auto r :
         {test::Region{0x832c9000, 0x4000}, test::Region{0x83265000, 0x1000},
          test::Region{0x832ae000, 0x1000}, test::Region{0x83315000, 0x1000}})
      regions.push_back(r);
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    ModeGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       code = 0x69000, resource = 0x80000, buffer = 0xa0000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 4, resource);
    m.WriteU32(actor + 8, 24);
    m.WriteU32(resource + 64, 24);
    m.WriteU32(resource + 68, 8);
    m.WriteU32(state + 4, actor);
    m.WriteU32(state + 12, 1);
    m.WriteU32(0x832c9c54 + 44, state);
    m.WriteU32(0x71000, 0x71100);
    m.WriteU32(0x71004, 1);
    m.WriteU32(0x71100, resource);
    m.WriteU32(0x83315fb4, 0x73000);
    m.WriteU32(0x73000, 0x73100);
    m.WriteU32(0x73100 + 352, 0x2000);
    m.WriteU32(0x832652f0, 0x75000);
    m.WriteU32(0x832aeb00, 0x76000);
    m.WriteU32(0x832cb784, 0x77000);
    m.WriteU32(0x832ca0d0, 0x78000);
    m.WriteU32(0x78000 + 72, 0x79000);
    m.WriteU32(0x78000 + 76, 2);
    m.WriteU32(0x79000, 1);
    m.WriteU32(0x79000 + 72, 12);
    m.WriteU32(resource + 5156, 3);
    m.WriteU32(resource + 5160, 212);
    m.WriteU32(buffer + 117236, 2);
    m.WriteU32(buffer + 131544, 2);
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("global mode result");
    };
    auto op = [&](unsigned mode) {
      s.r[3] = owner;
      m.WriteU32(actor + 52, 0);
      m.WriteU8(code + 1, mode);
      check(battle_script_global_modes61::Apply(0x82b00e98, m, {guest, native},
                                                s));
      check(m.ReadU32(actor + 52) == 2 && s.r[1] == initial.r[1] &&
            s.r[28] == initial.r[28] && s.r[31] == initial.r[31]);
    };
    m.WriteU32(actor + 64, 0x80);
    op(1);
    check(m.ReadU32(actor + 64) == 0x1000);
    op(24);
    check(m.ReadU32(actor + 64) == 0x80);
    op(25);
    check(!m.ReadU32(actor + 64));
    op(2);
    op(3);
    check((m.ReadU32(actor + 64) & 0xc00) == 0xc00);
    op(5);
    check(m.ReadU32(buffer + 117240) == 28 &&
          m.ReadU32(buffer + 131548) == 28 &&
          m.ReadU32(resource + 5160) == 28 &&
          (m.ReadU32(resource + 5728) & 0x80000000));
    op(6);
    check(guest.copies == 2 && m.ReadU32(resource + 5164) == 212 &&
          (m.ReadU32(resource + 8672) & 0x80000000));
    op(7);
    check(!m.ReadU32(resource + 5168));
    for (unsigned mode = 0; mode < 39; ++mode)
      op(mode);
    check(
        m.ReadU8(0x832ca0e0 + 5738) == 0 && m.ReadU8(0x832ca0e0 + 5792) == 1 &&
        m.ReadU8(0x832ca0e0 + 5793) == 1 && m.ReadU8(0x832ca0e0 + 5512) == 2 &&
        m.ReadU32(buffer + 170860) == 10 &&
        (m.ReadU32(state + 28) & 0x18000) == 0x10000 &&
        (m.ReadU32(resource + 124) & 0xe000) == 0xe000 && !m.ReadU32(0x77004));
    auto flags = m.ReadU32(actor + 64);
    op(99);
    check(m.ReadU32(actor + 64) == flags);
    std::cout << "battle_script_global_modes61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
