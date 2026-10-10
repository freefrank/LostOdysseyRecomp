#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/string_storage_context61.h"
#include <iostream>
struct StringGuest final : manager_release_context61::GuestServices {
  unsigned allocations = 0, releases = 0, conversions = 0;
  void Need(bool b) {
    if (!b)
      throw std::runtime_error("string storage boundary");
  }
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x830d9cdc) {
      Need(s.r[5] == 0);
      for (unsigned i = 0; i < unsigned(s.r[7]); ++i)
        m.WriteU16(unsigned(s.r[3]) + 2 * i, m.ReadU8(unsigned(s.r[6]) + i));
      ++conversions;
      s.r[3] = 0;
      return;
    }
    if (e == 0x82b7a0b0) {
      for (unsigned i = 0; i < unsigned(s.r[5]); ++i)
        m.WriteU8(unsigned(s.r[3]) + i, m.ReadU8(unsigned(s.r[4]) + i));
      return;
    }
    throw std::runtime_error("unexpected string direct");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    Need(s.r[3] == 0x70000);
    if (e == 0x123408) {
      Need(s.r[4] == 516 && s.r[5] == 8);
      s.r[3] = 0xb0000;
      return;
    }
    if (e == 0x123400) {
      Need(s.r[6] == 8);
      ++allocations;
      s.r[3] = s.r[5] ? 0xa0000 : 0;
      return;
    }
    if (e == 0x123404) {
      Need(s.r[4] == 0xb0000 || s.r[4] == 0xa0000);
      ++releases;
      s.r[3] = 0;
      return;
    }
    throw std::runtime_error("unexpected string virtual");
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
    StringGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    m.WriteU32(0x8330b608, 0x70000);
    m.WriteU32(0x70000, 0x71000);
    m.WriteU32(0x71004, 0x123408);
    m.WriteU32(0x71008, 0x123400);
    m.WriteU32(0x7100c, 0x123404);
    auto run = [&](unsigned e) {
      s.r[3] = 0x80000;
      s.r[4] = 0x90000;
      g.Need(string_storage_context61::Apply(e, m, {g, native}, s));
      g.Need(s.r[1] == initial.r[1] && s.r[30] == initial.r[30] &&
             s.r[31] == initial.r[31]);
    };
    run(0x822d02f8);
    g.Need(!g.allocations && !m.ReadU32(0x80004));
    m.WriteU8(0x90000, 'A');
    m.WriteU8(0x90001, 'Z');
    run(0x822d02f8);
    g.Need(m.ReadU32(0x80004) == 3 && m.ReadU32(0x80008) == 3 &&
           m.ReadU16(0xa0000) == 'A' && m.ReadU16(0xa0002) == 'Z' &&
           !m.ReadU16(0xa0004) && !g.releases);
    run(0x82298938);
    g.Need(!m.ReadU32(0x80000) && !m.ReadU32(0x80004) && !m.ReadU32(0x80008));
    for (unsigned i = 0; i < 128; ++i)
      m.WriteU8(0x90000 + i, 'x');
    run(0x822d02f8);
    g.Need(g.releases == 1 && g.conversions == 2);
    run(0x82298a98);
    g.Need(!m.ReadU32(0x80000) && !m.ReadU32(0x80004) && !m.ReadU32(0x80008));
    // A zero-capacity header can still own storage; release must not skip it.
    m.WriteU32(0x80000, 0xa0000);
    run(0x82298938);
    g.Need(g.releases == 2 && !m.ReadU32(0x80000));
    m.WriteU16(0x90000, 'H');
    m.WriteU16(0x90002, 'i');
    m.WriteU16(0x90004, 0);
    run(0x8229f5e0);
    g.Need(m.ReadU32(0x80004) == 3 && m.ReadU16(0xa0000) == 'H' &&
           m.ReadU16(0xa0002) == 'i' && !m.ReadU16(0xa0004));
    auto allocations = g.allocations;
    s.r[3] = 0x80000;
    s.r[4] = 0xa0000;
    g.Need(string_storage_context61::Apply(0x8229f5e0, m, {g, native}, s) &&
           g.allocations == allocations && s.r[1] == initial.r[1]);
    s.r[3] = 0x90000;
    g.Need(string_storage_context61::Apply(0x82296830, m, {g, native}, s) &&
           s.r[3] == 2);
    m.WriteU16(0x90000, 0);
    run(0x8229f5e0);
    g.Need(!m.ReadU32(0x80000) && !m.ReadU32(0x80004) && !m.ReadU32(0x80008));
    g.Need(!string_storage_context61::Apply(0, m, {g, native}, s));
    m.WriteU32(0xc0000, 0);
    m.WriteU32(0xc0008, 4);
    s.r[3] = 0xc0000;
    s.r[4] = 8;
    s.r[5] = 8;
    if (!string_storage_context61::Apply(0x8229f678, m, {g, native}, s) ||
        m.ReadU32(0xc0000) != 0xa0000 || s.r[1] != initial.r[1])
      throw std::runtime_error("array resize adapter");
    std::cout << "string_storage_context61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
