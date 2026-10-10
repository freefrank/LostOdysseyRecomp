#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_queries61.h"
#include <iostream>
struct QueriesGuest final : manager_release_context61::GuestServices {
  unsigned randoms = 0, effects = 0;
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
      s.r[3] = s.r[4] == 1 ? 0x80000 : 0xc0000;
      return;
    }
    if (e == 0x82aa0740) {
      if (s.r[5] || s.r[6] != 87 || s.r[7] != 1)
        throw std::runtime_error("reverse target random contract");
      s.r[3] = 0;
      ++randoms;
      return;
    }
    if (e == 0x82ac9be0 || e == 0x82ac9000) {
      if (s.r[3] != 0x80000 || s.r[4] != 9)
        throw std::runtime_error("resource effect query");
      ++effects;
      return;
    }
    if (e == 0x8229dfd8) {
      s.r[3] = 0x180000;
      return;
    }
    throw std::runtime_error("query direct boundary");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e == 0x2000) {
      s.r[3] = s.r[3] == 0x80000 ? 0 : 1;
      return;
    }
    if (e == 0x3000) {
      s.r[3] = 0x74000;
      return;
    }
    throw std::runtime_error("query virtual boundary");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83213000, 0x1000});
    regions.push_back({0x83264000, 0x1000});
    regions.push_back({0x83315000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    QueriesGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       code = 0x64000, vars = 0x65000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(state + 4, actor);
    m.WriteU32(state + 12, 2);
    m.WriteU32(actor, 7);
    m.WriteU32(actor + 472, 8);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(actor + 4, 0x80000);
    m.WriteU32(actor + 472 + 4, 0xc0000);
    m.WriteU32(actor + 472 + 64, 0x20000000);
    m.WriteU32(actor + 72, 0x66000);
    m.WriteU8(0x66000, 2);
    m.WriteU32(0x71000, 0x71100);
    m.WriteU32(0x71004, 2);
    for (unsigned i = 0; i < 2; ++i) {
      auto resource = 0x80000 + 0x40000 * i;
      m.WriteU32(0x71100 + 4 * i, resource);
      m.WriteU32(resource, 0x72000);
      m.WriteU32(resource + 64, i + 1);
      m.WriteU32(resource + 124, i ? 0x10000000 : 0);
      m.WriteU32(resource + 148, 7 + i);
    }
    m.WriteU32(0x72000 + 292, 0x2000);
    m.WriteU32(0x72000 + 308, 0x2000);
    auto le = [&](unsigned p, unsigned x, unsigned n = 2) {
      for (unsigned i = 0; i < n; ++i)
        m.WriteU8(p + i, x >> (8 * i));
    };
    auto op = [&](unsigned e) {
      s.r[3] = owner;
      m.WriteU32(actor + 52, 0);
      (void)battle_script_queries61::Apply(e, m, {guest, native}, s);
      if (s.r[1] != initial.r[1] || s.r[20] != initial.r[20] ||
          s.r[31] != initial.r[31])
        throw std::runtime_error("queries ABI");
    };
    le(code + 2, 0);
    for (unsigned mode = 0; mode < 4; ++mode) {
      m.WriteU8(code + 1, mode);
      op(0x82afecc8);
      if (m.ReadU32(vars) != (mode == 3 ? 0u : 1u))
        throw std::runtime_error("resource category count");
    }
    m.WriteU8(code + 1, 0);
    le(code + 2, 7, 4);
    le(code + 6, 0);
    le(code + 8, 1);
    le(code + 10, 2);
    m.WriteU32(vars, 3);
    m.WriteU32(vars + 4, 9);
    m.WriteU32(0xc0000 + 14660, 2);
    m.WriteU32(0xc0000 + 14656, 0x140000);
    for (unsigned i = 0; i < 2; ++i) {
      auto p = 0x140000 + 124208 * i;
      m.WriteU32(p, 3);
      m.WriteU32(p + 4, 9);
      m.WriteU32(p + 20, 1);
      m.WriteU32(p + 14884, 1);
    }
    op(0x82afef20);
    if (m.ReadU32(vars + 8) != 2 || guest.randoms != 1)
      throw std::runtime_error("deduplicated reverse action target");
    m.WriteU32(vars, 0xffffffff);
    op(0x82afef20);
    if (m.ReadU32(vars + 8) != 0xffffffff || guest.randoms != 1)
      throw std::runtime_error("detail without action type rejected");
    le(code + 1, 100);
    m.WriteU32(0x80000 + 60, 6);
    op(0x82af9f08);
    if (m.ReadU32(actor + 52) != 100)
      throw std::runtime_error("resource type branch");
    le(code + 1, 0);
    m.WriteU16(0x70000 + 148, 123);
    op(0x82af9980);
    if (m.ReadU32(vars) != 123)
      throw std::runtime_error("manager halfword query");
    le(code + 1, 0);
    le(code + 3, 1);
    le(code + 5, 2);
    m.WriteU32(vars, 1);
    m.WriteU32(vars + 4, 9);
    m.WriteU32(0x83213438 + 8 * 9 + 4, 512);
    m.WriteU32(0x80000 + 232, 512);
    op(0x82af9f80);
    if (m.ReadU32(vars + 8) != 512)
      throw std::runtime_error("resource-ID property query");
    op(0x82afa048);
    op(0x82afa100);
    if (guest.effects != 2)
      throw std::runtime_error("resource-ID effects");
    op(0x82af75a0);
    if (m.ReadU32(vars) != 2)
      throw std::runtime_error("first target query");
    m.WriteU32(vars, 42);
    op(0x82af75f0);
    if (m.ReadU32(0x80000 + 76352) != 42)
      throw std::runtime_error("resource field write");
    op(0x82af7658);
    if (m.ReadU32(actor + 320) != 42 || m.ReadU32(actor + 324) != 9)
      throw std::runtime_error("actor parameter pair");
    m.WriteU32(0x83315fb4, 0x73000);
    m.WriteU32(0x73000, 0x73100);
    m.WriteU32(0x73100 + 352, 0x3000);
    m.WriteU32(0x180000 + 4 * (2065 + 5 + 45276), 345);
    m.WriteU32(vars, 5);
    m.WriteU32(vars + 4, 1);
    op(0x82aff1b8);
    if (m.ReadU32(vars + 8) != 345)
      throw std::runtime_error("inventory indexed query");
    m.WriteU32(vars, 0);
    op(0x82aff1b8);
    if (m.ReadU32(vars + 8) != 345)
      throw std::runtime_error("first nonzero inventory query");
    m.WriteU32(0x180000 + 185200, 77);
    m.WriteU32(vars, 1024);
    op(0x82aff1b8);
    if (m.ReadU32(vars + 8) != 77)
      throw std::runtime_error("inventory special field");
    std::cout << "battle_script_queries61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
