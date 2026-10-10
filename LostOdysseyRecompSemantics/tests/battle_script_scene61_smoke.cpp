#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_scene61.h"
#include <iostream>
struct SceneGuest final : manager_release_context61::GuestServices {
  unsigned expected = 0, result = 1, calls = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e != expected)
      throw std::runtime_error("scene service target");
    if (e == 0x82b04720 &&
        (s.r[3] != 0x832cc05c || s.r[4] != ~std::uint64_t(0) || s.r[5] != 20 ||
         s.r[6] != 30 || s.r[7] || s.r[8] != 11))
      throw std::runtime_error("signed scene argument");
    if (e == 0x82b1afe8 &&
        (s.r[3] != 0x832cc0fc || s.r[4] != 0x61000000 ||
         s.r[5] != ((std::uint64_t(std::bit_cast<unsigned>(10.f)) << 32) |
                    std::bit_cast<unsigned>(15.f)) ||
         s.r[6] != (std::uint64_t(std::bit_cast<unsigned>(20.f)) << 32) ||
         s.r[7] || s.r[8] != 60 || s.r[9] != 1))
      throw std::runtime_error("packed XYZ callback");
    if (e == 0x82b1a048 && std::bit_cast<double>(s.fpr_bits[1]) != -1.)
      throw std::runtime_error("float scene parameter");
    if (e == 0x82b19c00 &&
        (s.r[3] != 0xffffffff || std::bit_cast<double>(s.fpr_bits[1]) != 20.))
      throw std::runtime_error("two-argument scene parameter");
    if (e == 0x82b1b2e8 &&
        (m.ReadU16(s.r[3]) != 0x3042 || m.ReadU16(s.r[3] + 64) ||
         s.r[4] != 0xffffffff || s.r[5] != 255 || s.r[6] != 0x33301 ||
         std::bit_cast<double>(s.fpr_bits[1]) != 20.))
      throw std::runtime_error("inline text payload");
    ++calls;
    s.r[3] = result;
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("unexpected scene indirect call");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x832ca000, 0x2000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    SceneGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       code = 0x64000, vars = 0x65000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    auto le = [&](unsigned p, unsigned x) {
      m.WriteU8(p, x);
      m.WriteU8(p + 1, x >> 8);
    };
    auto op = [&](unsigned e) {
      m.WriteU32(actor + 52, 0);
      s.r[3] = owner;
      (void)battle_script_scene61::Apply(e, m, {guest, native}, s);
      if (s.r[1] != initial.r[1] || s.r[29] != initial.r[29] ||
          s.r[31] != initial.r[31] || s.fpr_bits[31] != initial.fpr_bits[31])
        throw std::runtime_error("scene ABI");
    };
    m.WriteU32(vars, 0xffffffff);
    m.WriteU32(vars + 4, 20);
    m.WriteU32(vars + 8, 30);
    m.WriteU32(vars + 12, 40);
    m.WriteU32(vars + 16, 50);
    m.WriteU32(vars + 20, 60);
    for (auto e : {0x82af7ea0u, 0x82af7c20u}) {
      le(code + 1, 0);
      le(code + 3, 0xfffa);
      guest.expected = e == 0x82af7ea0 ? 0x82b02880 : 0x82b1b958;
      guest.result = 1;
      op(e);
      if (m.ReadU32(actor + 52) != 5)
        throw std::runtime_error("scene condition pass");
      guest.result = 0;
      op(e);
      if (m.ReadU32(actor + 52) != 0xfffffffa)
        throw std::runtime_error("scene signed branch");
    }
    guest.result = 99;
    for (unsigned i = 0; i < 6; ++i)
      le(code + 2 + 2 * i, i);
    m.WriteU8(code + 1, 0);
    guest.expected = 0x82b04720;
    op(0x82afc9c8);
    m.WriteU32(0x82000d6c, std::bit_cast<unsigned>(0.5f));
    guest.expected = 0x82b1afe8;
    op(0x82afc8a0);
    if (m.ReadU32(actor + 52) != 14)
      throw std::runtime_error("packed command cursor");
    guest.expected = 0x82b1aa50;
    op(0x82afc790);
    if (m.ReadU32(vars) != 99)
      throw std::runtime_error("scene returned identifier");
    m.WriteU8(code + 1, 2);
    le(code + 4, 100);
    m.WriteU8(0x832ca0e0 + 5739, 1);
    op(0x82afc4f0);
    if (m.ReadU32(actor + 52) != 6)
      throw std::runtime_error("scene global predicate");
    m.WriteU8(0x832ca0e0 + 5739, 0);
    op(0x82afc4f0);
    if (m.ReadU32(actor + 52) != 100)
      throw std::runtime_error("scene global false");
    m.WriteU8(code + 1, 7);
    op(0x82afc4f0);
    if (m.ReadU32(actor + 52))
      throw std::runtime_error("invalid mode cursor preservation");
    m.WriteU32(vars, 0xffffffff);
    le(code + 4, 1);
    m.WriteU8(code + 1, 0);
    guest.expected = 0x82b1a048;
    op(0x82afc418);
    m.WriteU8(code + 1, 1);
    guest.expected = 0x82b19c00;
    op(0x82afc418);
    for (unsigned i = 1; i < 65; ++i)
      m.WriteU8(code + i, 0);
    m.WriteU8(code + 1, 0x30);
    m.WriteU8(code + 2, 0x42);
    le(code + 65, 0);
    le(code + 67, 1);
    le(code + 69, 2);
    m.WriteU32(vars + 8, 255);
    guest.expected = 0x82b1b2e8;
    op(0x82af7c98);
    if (m.ReadU32(actor + 52) != 71)
      throw std::runtime_error("inline text cursor");
    std::cout << "battle_script_scene61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
