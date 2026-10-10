#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_text61.h"
#include <iostream>
struct TextGuest final : manager_release_context61::GuestServices {
  unsigned displayed = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x8238e2f8) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x823a5058) {
      s.r[3] = 0x71000;
      return;
    }
    if (e == 0x82b079e8) {
      if (s.r[3] != 0x71000 || s.r[4] || s.r[5] != 30 || s.r[7] != 1 ||
          m.ReadU16(s.r[6]) != 0x4100)
        throw std::runtime_error("text presentation boundary");
      ++displayed;
      return;
    }
    throw std::runtime_error("text direct boundary");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("text indirect boundary");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83264000, 0x2000});
    regions.push_back({0x821a8000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    TextGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, state = 0x61000, actor = 0x62000,
                       code = 0x63000, vars = 0x64000, base = 0x65000,
                       out = 0x66000, scratch = 0x67000, names = 0x68000;
    m.WriteU32(owner + 44, state);
    m.WriteU32(owner + 24, actor);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(state + 52, scratch);
    m.WriteU32(scratch, 0xffffffd6);
    m.WriteU32(scratch + 4, 1);
    m.WriteU32(state + 60, 7);
    m.WriteU32(state + 64, 8);
    m.WriteU32(0x70000, 0x70100);
    m.WriteU32(0x70004, 2);
    m.WriteU32(0x70100, 0x70200);
    m.WriteU32(0x70104, 0x70300);
    auto string = [&](unsigned obj, unsigned text, unsigned c) {
      m.WriteU32(obj, text);
      m.WriteU32(obj + 4, 1);
      m.WriteU16(text, c);
      m.WriteU16(text + 2, 0);
    };
    m.WriteU32(0x70200 + 64, 7);
    m.WriteU32(0x70300 + 64, 8);
    string(0x70200 + 76, 0x70400, 'X');
    string(0x70300 + 76, 0x70410, 'Y');
    m.WriteU32(0x83264978 + 116, names);
    string(names + 84 + 60, 0x69000, 'I');
    string(names + 84, 0x69010, 'M');
    string(names + 84 + 48, 0x69020, 'S');
    m.WriteU16(0x82000b8c, ' ');
    auto le = [&](unsigned p, unsigned x, unsigned n) {
      for (unsigned i = 0; i < n; ++i)
        m.WriteU8(p + i, x >> (8 * i));
    };
    le(base, 16, 4);
    unsigned p = base + 16;
    for (unsigned x :
         {0x41u, 0xe1u, 0u, 0x1e1u, 0x2e1u, 0x3e1u, 1u, 0x4e1u, 10001u, 0x5e1u,
          1u, 0x6e1u, 1u, 0xbe1u, 2u, 0xf0u, 0xe0u}) {
      le(p, x, 2);
      p += 2;
    }
    auto format = [&](unsigned b, unsigned size) {
      s.r[3] = owner;
      s.r[4] = b;
      s.r[5] = 0;
      s.r[6] = size;
      s.r[7] = out;
      s.r[8] = 64;
      (void)battle_script_text61::Apply(0x82a9f1c0, m, {guest, native}, s);
    };
    format(base, 128);
    unsigned expected[]{0x4100, '-', '4', '2', 'X', 'Y',
                        'I',    'M', 'S', ' ', ' ', 0};
    for (unsigned i = 0; i < std::size(expected); ++i)
      if (m.ReadU16(out + 2 * i) != expected[i])
        throw std::runtime_error("text token expansion");
    if (s.r[3] != 1 || s.r[1] != initial.r[1] || s.r[21] != initial.r[21] ||
        s.r[31] != initial.r[31])
      throw std::runtime_error("formatter ABI");
    m.WriteU16(out, 123);
    format(0, 128);
    if (s.r[3] || m.ReadU16(out) != 123)
      throw std::runtime_error("null bank");
    format(base, 15);
    if (s.r[3] || m.ReadU16(out) != 123)
      throw std::runtime_error("offset rejection");
    m.WriteU32(state + 44, 128);
    m.WriteU32(state + 48, base);
    s.r[3] = owner;
    (void)battle_script_text61::Apply(0x82a9f718, m, {guest, native}, s);
    if (guest.displayed != 1 || m.ReadU32(actor + 52) != 3 ||
        s.r[1] != initial.r[1] || s.r[31] != initial.r[31])
      throw std::runtime_error("text opcode");
    s.r[4] = base;
    s.r[5] = out;
    s.r[6] = 4;
    (void)battle_script_text61::Apply(0x82834fe0, m, {guest, native}, s);
    if (m.ReadU32(out) != 16 || s.r[3] != base + 4)
      throw std::runtime_error("little-endian reader");
    std::cout << "battle_script_text61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
