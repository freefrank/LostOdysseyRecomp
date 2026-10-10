#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_presentation61.h"
#include <iostream>
struct PresentationGuest final : manager_release_context61::GuestServices {
  unsigned expected = 0, mode = 0, calls = 0;
  bool numeric = false;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e != expected || s.r[3] != 0x832cc130)
      throw std::runtime_error("presentation target");
    if (numeric) {
      if (mode == 0 || mode == 144) {
        if (std::bit_cast<double>(s.fpr_bits[1]) != 10 || s.r[5] != 7)
          throw std::runtime_error("single numeric argument");
      }
      if (mode == 16 || mode == 32 || mode == 64) {
        if (std::bit_cast<double>(s.fpr_bits[1]) != 10 ||
            std::bit_cast<double>(s.fpr_bits[2]) != 20)
          throw std::runtime_error("numeric float pair");
      }
      if (mode == 48 && (s.r[4] != 10 || s.r[5] != 20 || s.r[6] != 7))
        throw std::runtime_error("float integer roundtrip");
      auto pair = (std::uint64_t(std::bit_cast<unsigned>(10.f)) << 32) |
                  std::bit_cast<unsigned>(20.f);
      if ((mode == 96 || mode == 128) && s.r[4] != pair)
        throw std::runtime_error("numeric packed float pair");
      if (mode == 112 && s.r[4] != 0x280a141e)
        throw std::runtime_error("packed color byte order");
    } else if (e == 0x82b1b7a0 || e == 0x82b1b850) {
      auto base = e == 0x82b1b7a0 ? 16u : 64u;
      auto sub = mode - base;
      if (s.r[4] != (sub <= 4 ? sub : 0) || s.r[5] != 10 || s.r[6] != 7)
        throw std::runtime_error("range default command");
    } else if (e == 0x82b1c050) {
      constexpr unsigned codes[]{0, 1, 2, 32, 33, 34, 75, 65, 66};
      auto sub = mode - 144;
      if (s.r[4] != 10 || s.r[5] != (sub < 9 ? codes[sub] : 32) || s.r[6] != 7)
        throw std::runtime_error("command selector map");
    }
    ++calls;
    s.r[3] = 91;
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("unexpected presentation indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x832cc000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    PresentationGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       code = 0x64000, vars = 0x65000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    for (unsigned i = 0; i < 5; ++i) {
      m.WriteU8(code + 2 + 2 * i, i);
      m.WriteU32(vars + 4 * i, i ? 10 * i : 7);
    }
    auto op = [&](unsigned e) {
      m.WriteU32(actor + 52, 0);
      m.WriteU8(code + 1, guest.mode);
      s.r[3] = owner;
      (void)battle_script_presentation61::Apply(e, m, {guest, native}, s);
      if (s.r[1] != initial.r[1] || s.r[28] != initial.r[28] ||
          s.r[31] != initial.r[31] || s.fpr_bits[29] != initial.fpr_bits[29] ||
          s.fpr_bits[31] != initial.fpr_bits[31])
        throw std::runtime_error("presentation ABI");
    };
    unsigned numeric[]{0x82b1bba8, 0x82b1ba30, 0x82b1baa8, 0x82b1b9b8,
                       0x82b1bb20, 0x82b1bc10, 0x82b1bc98, 0x82b1bd30,
                       0x82b1bd98, 0x82b1be20};
    guest.numeric = true;
    for (unsigned i = 0; i < 10; ++i) {
      guest.mode = i * 16;
      guest.expected = numeric[i];
      op(0x82afbea0);
      if (m.ReadU32(actor + 52) != 12)
        throw std::runtime_error("numeric cursor");
    }
    guest.numeric = false;
    struct C {
      unsigned mode, target;
    };
    for (auto c : {C{0, 0x82b1c788}, C{16, 0x82b1b7a0}, C{20, 0x82b1b7a0},
                   C{31, 0x82b1b7a0}, C{32, 0x82b1b7f8}, C{40, 0x82b1b850},
                   C{48, 0x82b1c7e0}, C{64, 0x82b1b850}, C{80, 0x82b1b8a8},
                   C{96, 0x82b1b900}, C{100, 0x82b1c050}, C{112, 0x82b1bef0},
                   C{128, 0x82b1bfd8}, C{150, 0x82b1c050}, C{160, 0x82b1c0c8},
                   C{177, 0x82b1c1c0}, C{192, 0x82b1c238}, C{208, 0x82b1c2a0},
                   C{224, 0x82b1be88}}) {
      guest.mode = c.mode;
      guest.expected = c.target;
      op(0x82afb880);
      if (m.ReadU32(actor + 52) != 10)
        throw std::runtime_error("command cursor");
    }
    guest.mode = 2;
    op(0x82afb788);
    if (m.ReadU32(0x832cc130) != 7)
      throw std::runtime_error("global presentation write");
    m.WriteU32(0x832cc130, 33);
    guest.mode = 3;
    op(0x82afb788);
    if (m.ReadU32(vars) != 33)
      throw std::runtime_error("global presentation read");
    guest.mode = 0;
    guest.expected = 0x82b1c5f8;
    op(0x82afb788);
    if (m.ReadU32(vars) != 91)
      throw std::runtime_error("presentation getter");
    guest.mode = 1;
    guest.expected = 0x82b1c6c0;
    op(0x82afb788);
    std::cout << "battle_script_presentation61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
