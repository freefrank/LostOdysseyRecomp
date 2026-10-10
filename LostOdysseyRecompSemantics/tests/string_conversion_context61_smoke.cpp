#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/string_conversion_context61.h"
#include <iostream>
struct ConversionGuest final : manager_release_context61::GuestServices {
  unsigned allocated = 0, error = 0, utf8 = 0;
  bool fail = false;
  void Need(bool b) {
    if (!b)
      throw std::runtime_error("conversion ABI");
  }
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x830d9cdc) {
      Need(s.r[5] == 0 && s.r[4] >= 2 * s.r[7]);
      if (fail) {
        s.r[3] = 0xffffffff;
        return;
      }
      for (unsigned i = 0; i < unsigned(s.r[7]); ++i)
        m.WriteU16(unsigned(s.r[3]) + 2 * i, m.ReadU8(unsigned(s.r[6]) + i));
      s.r[3] = 0;
      return;
    }
    if (e == 0x830d9efc) {
      s.r[3] = 87;
      return;
    }
    if (e == 0x822ca180) {
      error = unsigned(s.r[3]);
      return;
    }
    if (e == 0x827ca660) {
      Need(s.r[3] == 0x90000 && s.r[4] == 3 && s.r[5] == 0xa0000 &&
           s.r[6] == 3);
      ++utf8;
      s.r[3] = 2;
      return;
    }
    throw std::runtime_error("unexpected conversion direct");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    Need(e == 0x123400 && s.r[3] == 0x70000 && s.r[5] == 8);
    allocated = unsigned(s.r[4]);
    s.r[3] = 0xa0000;
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
    ConversionGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    m.WriteU32(0x8330b608, 0x70000);
    m.WriteU32(0x70000, 0x71000);
    m.WriteU32(0x71004, 0x123400);
    auto run = [&](unsigned e) {
      g.Need(string_conversion_context61::Apply(e, m, {g, native}, s));
      g.Need(s.r[1] == initial.r[1] && s.r[28] == initial.r[28] &&
             s.r[31] == initial.r[31]);
    };
    s.r[3] = 0x80000;
    s.r[4] = 0;
    run(0x823227c8);
    g.Need(!m.ReadU32(0x80100));
    m.WriteU8(0x90000, 'A');
    m.WriteU8(0x90001, 'Z');
    s.r[3] = 0x90000;
    run(0x8229c538);
    g.Need(s.r[3] == 2);
    s.r[3] = 0x80000;
    s.r[4] = 0x90000;
    run(0x823227c8);
    g.Need(m.ReadU32(0x80100) == 0x80000 && m.ReadU16(0x80000) == 'A' &&
           m.ReadU16(0x80002) == 'Z' && !g.allocated);
    for (unsigned i = 0; i < 128; ++i)
      m.WriteU8(0x90000 + i, 'x');
    s.r[3] = 0x80000;
    s.r[4] = 0x90000;
    run(0x823227c8);
    g.Need(g.allocated == 516 && m.ReadU32(0x80100) == 0xa0000 &&
           !m.ReadU16(0xa0100));
    auto convert = [&](unsigned cp, unsigned len, unsigned cap) {
      s.r[3] = cp;
      s.r[4] = 0;
      s.r[5] = 0x90000;
      s.r[6] = len;
      s.r[7] = 0xa0000;
      s.r[8] = cap;
      run(0x8229c560);
    };
    convert(0, 0xffffffff, 0);
    g.Need(s.r[3] == 129);
    convert(0, 3, 2);
    g.Need(!s.r[3] && g.error == 122);
    g.fail = true;
    convert(0, 3, 3);
    g.Need(!s.r[3] && g.error == 87);
    convert(65001, 3, 3);
    g.Need(s.r[3] == 2 && g.utf8 == 1);
    g.Need(!string_conversion_context61::Apply(0, m, {g, native}, s));
    std::cout << "string_conversion_context61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
