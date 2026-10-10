#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_resource_modes61.h"
#include <iostream>
struct ResourceModesGuest final : manager_release_context61::GuestServices {
  unsigned forwarded = 0, refreshes = 0, randoms = 0;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x8238e308) {
      s.r[3] = s.r[4] <= 1 ? 0x80000 : 0;
      return;
    }
    if (e == 0x8238e2f8) {
      s.r[3] = 0x71000;
      return;
    }
    if (e == 0x8229dfd8) {
      s.r[3] = 0x100000;
      return;
    }
    if (e == 0x82ac0588 || e == 0x82ac25e8 || e == 0x82ac3058 ||
        e == 0x82ab76f8) {
      ++refreshes;
      return;
    }
    if (e == 0x8285fef8) {
      ++forwarded;
      return;
    }
    if (e == 0x82ab8128) {
      if (s.r[4] != 1 || s.r[5] ||
          s.r[6] != (std::uint64_t(std::bit_cast<unsigned>(-7.f)) << 32))
        throw std::runtime_error("packed scene Z argument");
      return;
    }
    if (e == 0x82aa0740) {
      if (s.r[5] || s.r[6] != 111 || s.r[7])
        throw std::runtime_error("same-side random selection");
      s.r[3] = 0;
      ++randoms;
      return;
    }
    throw std::runtime_error("resource modes direct boundary");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e == 0x2000) {
      s.r[3] = 0x90000;
      return;
    }
    if (e == 0x2010) {
      s.r[3] = 0;
      return;
    }
    throw std::runtime_error("resource modes virtual boundary");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83264000, 0x2000});
    regions.push_back({0x83291000, 0x1000});
    regions.push_back({0x83315000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    ResourceModesGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       code = 0x64000, vars = 0x65000, resource = 0x80000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(actor + 4, resource);
    m.WriteU32(resource + 64, 1);
    m.WriteU32(resource + 132, 1);
    m.WriteU32(resource, 0x72000);
    m.WriteU32(0x72000 + 292, 0x2010);
    m.WriteU32(0x72000 + 380, 0x2010);
    m.WriteU32(0x71000, 0x71100);
    m.WriteU32(0x71004, 1);
    m.WriteU32(0x71100, resource);
    m.WriteU32(0x83315fb4, 0x74000);
    m.WriteU32(0x74000, 0x74100);
    m.WriteU32(0x74100 + 352, 0x2000);
    m.WriteU32(0x83291dc0, 0x73000);
    m.WriteU8(code + 2, 0);
    m.WriteU8(code + 4, 1);
    auto op = [&](unsigned e, unsigned mode, unsigned a, unsigned b = 0) {
      m.WriteU32(vars, a);
      m.WriteU32(vars + 4, b);
      m.WriteU8(code + 1, mode);
      m.WriteU32(actor + 52, 0);
      s.r[3] = owner;
      (void)battle_script_resource_modes61::Apply(e, m, {guest, native}, s);
      if (s.r[1] != initial.r[1] || s.r[25] != initial.r[25] ||
          s.r[31] != initial.r[31])
        throw std::runtime_error("resource modes ABI");
    };
    op(0x82affb20, 0, 5);
    if (m.ReadU32(resource + 4956) != 1024)
      throw std::runtime_error("mapped resource flag");
    op(0x82affb20, 5, 1);
    if (!(m.ReadU32(resource + 76348) & 0x80000000))
      throw std::runtime_error("set resource high flag");
    op(0x82affb20, 6, 1);
    if (m.ReadU32(resource + 76348) & 0x80000000)
      throw std::runtime_error("clear resource high flag");
    op(0x82affb20, 7, 1);
    if (!(m.ReadU32(resource + 124) & 64))
      throw std::runtime_error("resource flag64");
    op(0x82affb20, 10, 3);
    if (m.ReadU32(resource + 4 * 19034) != 1)
      throw std::runtime_error("resource indexed toggle");
    op(0x82affb20, 11, 3);
    if (m.ReadU32(resource + 4 * 19034))
      throw std::runtime_error("resource indexed clear");
    m.WriteU32(0x100000 + 68, 123);
    op(0x82affb20, 14, 0);
    if (m.ReadU32(vars) != 123)
      throw std::runtime_error("buffer query");
    m.WriteU32(resource + 4948, 10);
    for (unsigned i = 0; i < 5; ++i)
      m.WriteU32(resource + 5116 + 4 * i, 20 + i);
    op(0x82affb20, 15, 77);
    if (m.ReadU32(resource + 5112) != 77 || m.ReadU32(0x100000 + 2728) != 77 ||
        m.ReadU32(0x73000 + 4) != 10 || m.ReadU32(0x73000 + 28) != 24 ||
        guest.refreshes != 3)
      throw std::runtime_error("resource refresh composition");
    op(0x82affb20, 16, 55);
    if (m.ReadU32(resource + 76376) != 55)
      throw std::runtime_error("resource field mode");
    m.WriteU32(actor + 296, 1);
    m.WriteU32(actor + 104, 1);
    m.WriteU32(resource + 14660, 3);
    op(0x82affb20, 17, 0);
    if (m.ReadU32(vars) != 3)
      throw std::runtime_error("latest resource action count");
    op(0x82affb20, 18, 1);
    if (m.ReadU32(vars) != 1 || guest.randoms != 1)
      throw std::runtime_error("same-side eligible resource");
    op(0x82b000a0, 2, 1, unsigned(-7));
    for (unsigned i = 0; i < 5; ++i)
      m.WriteU32(resource + 5116 + 4 * i, 0);
    op(0x82b000a0, 7, 1, 73);
    if (m.ReadU32(resource + 5116) != 73 || guest.forwarded != 1 ||
        guest.refreshes != 5)
      throw std::runtime_error("resource add slot and fallthrough");
    for (unsigned i = 0; i < 5; ++i)
      m.WriteU32(resource + 5116 + 4 * i, 80 + i);
    op(0x82b000a0, 7, 1, 90);
    if (guest.forwarded != 2 || guest.refreshes != 5)
      throw std::runtime_error("full slots retain external forwarding");
    op(0x82b000a0, 7, 1, 20);
    op(0x82b000a0, 9, 1, 0);
    if (m.ReadU32(vars + 4) != 20)
      throw std::runtime_error("primary resource state query");
    m.WriteU32(resource + 14652, 9);
    op(0x82b000a0, 6, 1, 0);
    if (m.ReadU32(vars + 4) != 9)
      throw std::runtime_error("resource detail query");
    m.WriteU32(0x100000 + 14308 + 136, 66);
    op(0x82b000a0, 10, 1, 0);
    if (m.ReadU32(vars + 4) != 66)
      throw std::runtime_error("banked buffer query");
    std::cout << "battle_script_resource_modes61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
