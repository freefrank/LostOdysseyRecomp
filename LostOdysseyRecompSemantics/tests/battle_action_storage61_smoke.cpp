#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_action_storage61.h"
#include <iostream>
struct StorageGuest final : manager_release_context61::GuestServices {
  unsigned strings = 0, frees = 0, destroyed = 0, allocations = 0, bytes = 0;
  void Need(bool b) {
    if (!b)
      throw std::runtime_error("action storage ABI");
  }
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    auto p = unsigned(s.r[3]);
    if (e == 0x82b7bc40) {
      for (unsigned i = 0; i < unsigned(s.r[5]); ++i)
        m.WriteU8(p + i, unsigned(s.r[4]));
      return;
    }
    if (e == 0x822d02f8) {
      Need(s.r[4] == 0x821a8f04);
      for (unsigned i = 0; i < 12; i += 4)
        m.WriteU32(p + i, 0);
      ++strings;
      return;
    }
    if (e == 0x82298938) {
      ++frees;
      return;
    }
    throw std::runtime_error("unexpected storage boundary");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e == 0x82298938) {
      ++destroyed;
      return;
    }
    Need(e == 0x123400 && s.r[3] == 0x70000 && s.r[6] == 8);
    ++allocations;
    bytes = unsigned(s.r[5]);
    s.r[3] = bytes ? 0x100000 : 0;
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
    StorageGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    m.WriteU32(0x8330b608, 0x70000);
    m.WriteU32(0x70000, 0x71000);
    m.WriteU32(0x71008, 0x123400);
    m.WriteU32(0x82000e40, 0x3f800000);
    m.WriteU32(0x82000e50, 0);
    constexpr unsigned resource = 0x80000, header = resource + 14656,
                       record = 0x100000;
    m.WriteU32(resource + 64, 7);
    s.r[3] = resource;
    s.r[4] = 0;
    g.Need(battle_action_storage61::Apply(0x82ab2d88, m, {g, native}, s));
    g.Need(g.allocations == 1 && g.bytes == 33 * 124208 &&
           m.ReadU32(header) == record && m.ReadU32(header + 4) == 1 &&
           m.ReadU32(header + 8) == 33);
    g.Need(g.strings == 512 && g.frees == 512 && m.ReadU32(record + 36) == 7 &&
           m.ReadU32(record + 36 + 464) == 0xffffffff);
    g.Need(m.ReadU32(record + 56) == 0x3f800000 &&
           m.ReadU32(record + 14904) == 0x3f800000 &&
           m.ReadU32(record + 124036 + 42 * 4) == 0xffffffff);
    g.Need(s.r[1] == initial.r[1] && s.r[15] == initial.r[15] &&
           s.fpr_bits[30] == initial.fpr_bits[30] &&
           s.fpr_bits[31] == initial.fpr_bits[31]);
    s.r[3] = resource;
    s.r[4] = 0;
    g.Need(battle_action_storage61::Apply(0x82ab2d88, m, {g, native}, s));
    g.Need(g.allocations == 1 && m.ReadU32(header + 4) == 2 &&
           m.ReadU32(record + 124208 + 36) == 7);
    s.r[3] = header;
    s.r[4] = 33;
    g.Need(battle_action_storage61::Apply(0x82a9b698, m, {g, native}, s));
    g.Need(g.destroyed == 2048 && g.allocations == 1 && !m.ReadU32(header + 4));
    s.r[3] = header;
    s.r[4] = 0;
    g.Need(battle_action_storage61::Apply(0x82a9b698, m, {g, native}, s));
    g.Need(g.allocations == 2 && !g.bytes && !m.ReadU32(header));
    s.r[3] = resource;
    s.r[4] = 1;
    g.Need(battle_action_storage61::Apply(0x82ab2d88, m, {g, native}, s));
    g.Need(m.ReadU32(resource + 14668) == record &&
           m.ReadU32(resource + 14672) == 1);
    g.Need(!battle_action_storage61::Apply(0, m, {g, native}, s));
    std::cout << "battle_action_storage61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
