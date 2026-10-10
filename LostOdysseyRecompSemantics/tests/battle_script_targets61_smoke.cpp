#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_targets61.h"
#include <iostream>
struct TargetsGuest final : manager_release_context61::GuestServices {
  unsigned randomMax = 2;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78 || e == 0x8238e2f8) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x8238e308) {
      s.r[3] = 0x80000 + 0x4000 * (unsigned(s.r[4]) - 1);
      return;
    }
    if (e == 0x82a9b288) {
      s.r[3] = 0x72000;
      return;
    }
    if (e == 0x82ac85e8) {
      s.r[3] = randomMax ? 1 : 0;
      return;
    }
    throw std::runtime_error("target direct boundary");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e != 0x2000)
      throw std::runtime_error("target indirect boundary");
    s.r[3] = s.r[3] == 0x8c000 ? 1 : 0;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83213000, 0x1000});
    regions.push_back({0x83264000, 0x1000});
    regions.push_back({0x831f3000, 0x21000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    m.WriteU32(0x83264558, 0x75000);
    m.WriteU32(0x831f3300, 1);
    TargetsGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       code = 0x64000, vars = 0x65000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(state + 4, actor);
    m.WriteU32(state + 12, 1);
    m.WriteU32(actor, 7);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(actor + 4, 0x80000);
    m.WriteU32(actor + 72, 0x66000);
    m.WriteU32(actor + 64, 0x20000000);
    m.WriteU32(0x70000, 0x70100);
    m.WriteU32(0x70004, 4);
    m.WriteU32(0x83213438, 0);
    m.WriteU32(0x8321343c, 1);
    m.WriteU32(0x83213438 + 8 * 7, 0);
    m.WriteU32(0x8321343c + 8 * 7, 128);
    m.WriteU32(0x82000d7c, std::bit_cast<unsigned>(0.01f));
    unsigned flags[]{0x40000000, 0, 0x48000000, 0x08000000};
    float hp[]{20, 10, 30, 0};
    for (unsigned i = 0; i < 4; ++i) {
      auto resource = 0x80000 + 0x4000 * i;
      m.WriteU32(0x70100 + 4 * i, resource);
      m.WriteU32(resource + 64, i + 1);
      m.WriteU32(resource + 232, 128);
      m.WriteU32(resource + 68, i == 1 ? 8 : 9);
      m.WriteU32(resource + 124, flags[i]);
      m.WriteU32(resource + 132, 1);
      m.WriteU32(resource + 148, 7);
      m.WriteU32(resource + 2588, std::bit_cast<unsigned>(hp[i]));
      m.WriteU32(resource + 2592, std::bit_cast<unsigned>(100.f));
      m.WriteU32(resource + 2600, std::bit_cast<unsigned>(float(4 - i)));
      m.WriteU32(resource + 2616, std::bit_cast<unsigned>(10.f));
    }
    s.r[3] = owner;
    s.r[4] = 0x68000;
    s.r[5] = 0x69000;
    s.r[6] = 0x6a000;
    s.r[7] = 0x6b000;
    s.r[8] = 0;
    (void)battle_script_targets61::Apply(0x8238de58, m, {guest, native}, s);
    unsigned expectedCounts[]{1, 0, 1, 1, 1, 2};
    for (unsigned i = 0; i < 6; ++i)
      if (m.ReadU32(0x6b000 + 4 * i) != expectedCounts[i])
        throw std::runtime_error("target pool categories");
    for (unsigned i = 0; i < 5; ++i)
      m.WriteU8(code + 1 + 2 * i, i);
    unsigned entry = 0x8238d148;
    auto select = [&](unsigned filter, unsigned param, unsigned pool,
                      unsigned selection) {
      m.WriteU32(vars, filter);
      m.WriteU32(vars + 4, param);
      m.WriteU32(vars + 8, pool);
      m.WriteU32(vars + 12, selection);
      m.WriteU32(actor + 52, 0);
      s.r[3] = owner;
      (void)battle_script_targets61::Apply(entry, m, {guest, native}, s);
      if (s.r[1] != initial.r[1] || s.r[14] != initial.r[14] ||
          s.r[19] != initial.r[19] || s.r[31] != initial.r[31] ||
          s.fpr_bits[30] != initial.fpr_bits[30] ||
          s.fpr_bits[31] != initial.fpr_bits[31] ||
          m.ReadU32(actor + 52) != 11 ||
          m.ReadU32(vars + 16) != m.ReadU32(actor + 76))
        throw std::runtime_error("target selector ABI/cursor/result");
    };
    select(0, 0, 0, 0);
    if (m.ReadU32(actor + 76) != 3 || m.ReadU8(0x66000) != 1 ||
        m.ReadU8(0x66002) != 3)
      throw std::runtime_error("all target order");
    select(0, 0, 7, 0);
    if (m.ReadU32(actor + 76) != 2 || m.ReadU8(0x66000) != 2)
      throw std::runtime_error("self exclusion");
    select(1, 15, 0, 0);
    if (m.ReadU32(actor + 76) != 1 || m.ReadU8(0x66000) != 2)
      throw std::runtime_error("numeric threshold filter");
    select(6, 0, 0, 0);
    if (m.ReadU8(0x66000) != 3)
      throw std::runtime_error("maximum stat target");
    select(8, 0, 0, 0);
    if (m.ReadU8(0x66000) != 2)
      throw std::runtime_error("minimum stat target");
    select(2, 9, 0, 0);
    if (m.ReadU32(actor + 76) != 2)
      throw std::runtime_error("group filter");
    select(7, 1, 0, 0);
    if (m.ReadU32(actor + 76) != 2)
      throw std::runtime_error("same group target");
    m.WriteU32(0x80000 + 5116, 7);
    m.WriteU32(0x80000 + 5120, 7);
    select(11, 7, 0, 0);
    if (m.ReadU32(actor + 76) != 2 || m.ReadU8(0x66000) != 1 ||
        m.ReadU8(0x66001) != 1)
      throw std::runtime_error("duplicate field matches retained");
    m.WriteU32(0x88000 + 14660, 1);
    m.WriteU32(0x88000 + 14656, 0xb0000);
    m.WriteU32(0xb0000, 6);
    select(9, 0, 0, 0);
    if (m.ReadU32(actor + 76) != 1 || m.ReadU8(0x66000) != 3)
      throw std::runtime_error("pending action filter");
    m.WriteU32(0x72000 + 4 * 19, std::bit_cast<unsigned>(1.f));
    select(12, 1, 0, 0);
    if (m.ReadU32(actor + 76) != 3)
      throw std::runtime_error("inventory availability filter");
    select(0, 0, 0, 1);
    if (m.ReadU32(0x75000 + 4 * (128 * 9 + 84 + 3)) != 1 ||
        m.ReadU32(actor + 76) != 1 || m.ReadU8(0x66000) != 2)
      throw std::runtime_error("random target selection");
    select(5, 0, 0, 0);
    if (m.ReadU32(actor + 76))
      throw std::runtime_error("unimplemented source filter clears result");
    auto refine = [&](unsigned filter, unsigned parameter, unsigned pool,
                      unsigned selection) {
      entry = 0x8238d148;
      select(0, 0, 0, 0);
      entry = 0x82af86a8;
      select(filter, parameter, pool, selection);
    };
    m.WriteU32(0x88000 + 124, m.ReadU32(0x88000 + 124) | 0x10000000);
    refine(0, 0, 2, 0);
    if (m.ReadU32(actor + 76) != 1 || m.ReadU8(0x66000) != 3)
      throw std::runtime_error("refinement uses bit28 and team flag");
    refine(0, 0, 6, 0);
    if (m.ReadU32(actor + 76) != 1 || m.ReadU8(0x66000) != 2)
      throw std::runtime_error("refinement lower opposing group");
    for (unsigned i = 0; i < 3; ++i)
      m.WriteU32(0x80000 + 0x4000 * i + 2620, std::bit_cast<unsigned>(100.f));
    m.WriteU32(0x88000 + 2616, std::bit_cast<unsigned>(30.f));
    refine(5, 15, 0, 0);
    if (m.ReadU32(actor + 76) != 2 || m.ReadU8(0x66000) != 1 ||
        m.ReadU8(0x66001) != 2)
      throw std::runtime_error("refinement secondary stat threshold");
    refine(0, 0, 0, 1);
    if (m.ReadU32(0x75000 + 4 * (128 * 9 + 85 + 3)) != 1 ||
        m.ReadU32(actor + 76) != 1 || m.ReadU8(0x66000) != 2)
      throw std::runtime_error("refinement random service tag");
    m.WriteU32(actor + 76, 0);
    entry = 0x82af86a8;
    select(0, 0, 0, 0);
    if (m.ReadU32(actor + 76))
      throw std::runtime_error("empty refinement result");
    for (unsigned i = 0; i < 4; ++i)
      m.WriteU32(0x80000 + 0x4000 * i, 0x73000);
    m.WriteU32(0x73000 + 292, 0x2000);
    auto unavailable = [&](unsigned selection) {
      m.WriteU32(vars, 0);
      m.WriteU32(vars + 4, selection);
      m.WriteU32(actor + 52, 0);
      s.r[3] = owner;
      (void)battle_script_targets61::Apply(0x82afe6b8, m, {guest, native}, s);
      if (m.ReadU32(actor + 76) != 1 || m.ReadU32(vars + 8) != 1 ||
          m.ReadU8(0x66000) != 4 || m.ReadU32(actor + 52) != 7 ||
          s.r[1] != initial.r[1] || s.r[23] != initial.r[23])
        throw std::runtime_error("unavailable target pool and ABI");
    };
    unavailable(0);
    guest.randomMax = 0;
    unavailable(1);
    if (m.ReadU32(0x75000 + 4 * (128 * 9 + 86 + 3)) != 0)
      throw std::runtime_error("unavailable random service");
    std::cout << "battle_script_targets61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
