#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_services61.h"
#include <iostream>
struct ScriptServicesGuest final : manager_release_context61::GuestServices {
  unsigned expected = 0, calls = 0;
  bool missing = false;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e != 0x82380a18 && e != 0x82389b78 && e != 0x8238e308)
      throw std::runtime_error("service lookup boundary");
    if (e == 0x8238e308 && s.r[4] != 7)
      throw std::runtime_error("resource identifier");
    s.r[3] = missing ? 0 : 0x70000;
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e == 0x3000) {
      s.r[3] = 0x74000;
      return;
    }
    if (e == 0x3010) {
      s.r[3] = 99;
      return;
    }
    if (e != 0x2000 + expected || s.r[3] != 0x65000 || s.r[4] != 7)
      throw std::runtime_error("play service arguments");
    if ((expected == 560 || expected == 572) && s.r[5] != 20)
      throw std::runtime_error("play integer argument");
    if ((expected == 568 || expected == 552) &&
        std::bit_cast<double>(s.fpr_bits[1]) != 10.0)
      throw std::runtime_error("play first float");
    if (expected == 552 &&
        (std::bit_cast<double>(s.fpr_bits[2]) != 15.0 || s.r[7] != 40))
      throw std::runtime_error("play second float");
    if (expected == 580)
      s.fpr_bits[1] = std::bit_cast<std::uint64_t>(-3.75);
    if (expected == 576)
      s.r[3] = 88;
    ++calls;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83315000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    ScriptServicesGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       code = 0x64000, play = 0x65000, vars = 0x66000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(owner + 28, play);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(play, 0x71000);
    auto le = [&](unsigned p, unsigned x) {
      m.WriteU8(p, x);
      m.WriteU8(p + 1, x >> 8);
    };
    auto op = [&](unsigned e) {
      m.WriteU32(actor + 52, 0);
      s.r[3] = owner;
      (void)battle_script_services61::Apply(e, m, {guest, native}, s);
      if (s.r[1] != initial.r[1] || s.r[29] != initial.r[29] ||
          s.r[31] != initial.r[31])
        throw std::runtime_error("service ABI");
    };
    m.WriteU32(vars, 7);
    m.WriteU32(vars + 4, 20);
    m.WriteU32(vars + 8, 30);
    m.WriteU32(vars + 12, 40);
    le(code + 2, 0);
    le(code + 4, 1);
    constexpr unsigned offsets[]{2588, 2592, 2616, 2620};
    for (unsigned mode = 0; mode < 4; ++mode) {
      m.WriteU8(code + 1, mode);
      m.WriteU32(0x70000 + offsets[mode], std::bit_cast<unsigned>(42.75f));
      op(0x82a9ecc0);
      if (m.ReadU32(vars + 4) != 42 || m.ReadU32(actor + 52) != 6)
        throw std::runtime_error("resource stat read");
    }
    guest.missing = true;
    op(0x82a9ecc0);
    if (m.ReadU32(vars + 4))
      throw std::runtime_error("missing stat default");
    guest.missing = false;
    for (unsigned mode = 0; mode < 4; ++mode) {
      m.WriteU8(code + 1, mode);
      m.WriteU32(vars + 4, 20);
      m.WriteU32(0x70000 + (mode < 2 ? 2588 : 2616),
                 std::bit_cast<unsigned>(1.5f));
      op(0x82a9edd0);
      if (std::bit_cast<float>(m.ReadU32(0x70000 + (mode < 2 ? 2588 : 2616))) !=
          ((mode & 1) ? 21.5f : 20.0f))
        throw std::runtime_error("resource stat write");
    }
    for (unsigned i = 0; i < 4; ++i)
      le(code + 1 + 2 * i, i);
    m.WriteU32(0x82000d6c, std::bit_cast<unsigned>(0.5f));
    struct Case {
      unsigned e, slot, size;
    };
    for (auto c : {Case{0x82a9d638, 552, 9}, Case{0x82a9d6f0, 556, 3},
                   Case{0x82a9d758, 564, 3}, Case{0x82a9d7c0, 560, 5},
                   Case{0x82a9d920, 568, 5}, Case{0x82a9d9b8, 572, 5},
                   Case{0x82a9d828, 580, 5}, Case{0x82a9d8a8, 576, 5}}) {
      m.WriteU32(vars + 4, 20);
      m.WriteU32(0x71000 + c.slot, 0x2000 + c.slot);
      guest.expected = c.slot;
      op(c.e);
      if (m.ReadU32(actor + 52) != c.size)
        throw std::runtime_error("service cursor");
      if (c.slot == 580 && m.ReadU32(vars + 4) != 0xfffffffd)
        throw std::runtime_error("float service result");
      if (c.slot == 576 && m.ReadU32(vars + 4) != 88)
        throw std::runtime_error("integer service result");
    }
    m.WriteU32(vars, 1);
    m.WriteU32(vars + 4, 2);
    m.WriteU32(vars + 8, 123);
    op(0x82a9d570);
    if (m.ReadU32(play + 4 * (3577 + 2 + 689)) != 123)
      throw std::runtime_error("indexed play table");
    m.WriteU32(0x83315fb4, 0x72000);
    m.WriteU32(0x72000, 0x73000);
    m.WriteU32(0x73000 + 352, 0x3000);
    m.WriteU32(0x74000, 0x75000);
    m.WriteU32(0x75000 + 300, 0x3010);
    op(0x82a9ef08);
    if (m.ReadU32(vars) != 99)
      throw std::runtime_error("global chained query");
    std::cout << "battle_script_services61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
