#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_angles61.h"
#include "lo_semantics/mesh_hull_incremental61.h"
struct AngleGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress, GuestMemory &,
                  manager_release_context61::Registers &) override {
    throw std::runtime_error("unexpected angle direct boundary");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("unexpected angle indirect boundary");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x820bc000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    AngleGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto load = [&](const char *name, unsigned size) {
      auto path = std::getenv(name);
      if (!path)
        throw std::runtime_error(name);
      std::vector<unsigned char> data(size);
      std::ifstream f(path, std::ios::binary);
      f.read(reinterpret_cast<char *>(data.data()), size);
      if (f.gcount() != size)
        throw std::runtime_error("private angle fixture size");
      return data;
    };
    auto angle = load("LO_SCRIPT_ANGLE_CONSTANTS", 64),
         hull = load("LO_HULL_INCREMENTAL_CONSTANTS", 192),
         atan = load("LO_MESH_MATH_CONSTANTS", 184);
    for (unsigned i = 0; i < 24; ++i)
      m.WriteU8(0x820bc4f8 + i, angle[i]);
    for (unsigned i = 0; i < 40; ++i)
      m.WriteU8(0x82000fc8 + i, angle[24 + i]);
    for (unsigned i = 0; i < 120; ++i)
      m.WriteU8(0x83214d80 + i, hull[i]);
    for (unsigned i = 0; i < 8; ++i) {
      m.WriteU8(0x83215508 + i, hull[120 + i]);
      m.WriteU8(0x82000f28 + i, hull[128 + i]);
    }
    for (unsigned i = 0; i < 184; ++i)
      m.WriteU8(0x83214e88 + i, atan[i]);
    constexpr unsigned owner = 0x60000, actor = 0x61000, code = 0x62000,
                       vars = 0x63000, constants = 0x64000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(actor + 28, constants);
    m.WriteU8(code + 3, 0);
    m.WriteU8(code + 4, 0x80);
    m.WriteU8(code + 5, 1);
    m.WriteU8(code + 6, 0x80);
    auto call = [&](unsigned e, int a, int b) {
      m.WriteU32(actor + 52, 0);
      m.WriteU32(constants, unsigned(a));
      m.WriteU32(constants + 4, unsigned(b));
      s.r[3] = owner;
      (void)battle_script_angles61::Apply(e, m, {guest, native}, s);
      if (m.ReadU32(actor + 52) != 7 || s.r[1] != initial.r[1])
        throw std::runtime_error("angle cursor/stack");
      return std::int32_t(m.ReadU32(vars));
    };
    if (call(0x82a9c6d8, 0, 100) != 0 || call(0x82a9c790, 0, 100) != 100 ||
        std::abs(call(0x82a9c6d8, 512, 100) - 70) > 1 ||
        std::abs(call(0x82a9c790, 512, 100) - 70) > 1)
      throw std::runtime_error("scaled guest sine/cosine");
    if (call(0x82a9c848, 0, 0) != 0 || call(0x82a9c848, 0, 1) != 0 ||
        call(0x82a9c848, 1, 0) != -1024)
      throw std::runtime_error("script angle axes/zero");
    cook_main_smoke::Environment env(w);
    auto deps = env.Deps();
    for (bool cosine : {false, true}) {
      double expected =
          mesh_hull_incremental61::EvaluateGuestTrig(m, 0.25, cosine);
      s.fpr_bits[1] = std::bit_cast<std::uint64_t>(0.25);
      (void)mesh_hull_incremental61::Apply(cosine ? 0x822a2f08 : 0x822a2fe0, m,
                                           deps, s);
      if (s.fpr_bits[1] != std::bit_cast<std::uint64_t>(expected))
        throw std::runtime_error("shared trig reuse");
    }
    std::puts("PASS script sine/cosine/angle using recovered guest math");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
