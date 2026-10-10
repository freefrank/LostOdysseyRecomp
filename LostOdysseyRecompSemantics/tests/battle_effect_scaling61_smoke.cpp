#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_effect_scaling61.h"
struct ScalingGuest final : manager_release_context61::GuestServices {
  unsigned unavailable = 0, peerQueries = 0;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x74000;
      return;
    }
    if (e == 0x8238e308) {
      s.r[3] = s.r[4] == 21 ? 0xc0000 : 0xd0000;
      return;
    }
    throw std::runtime_error("scaling direct service");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e != 0x123450)
      throw std::runtime_error("scaling virtual service");
    ++peerQueries;
    s.r[3] = s.r[3] == unavailable;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (unsigned p : {0x83213000u, 0x83264000u, 0x8201d000u})
      regions.push_back({p, 0x1000});
    regions.push_back({0x831f3000, 0x21000});
    regions.push_back({0x832ae000, 0x1000});
    regions.push_back({0x832cb000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    ScalingGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x50000, source = 0x80000, target = 0x90000;
    auto put = [&](unsigned p, float value) {
      m.WriteU32(p, std::bit_cast<unsigned>(value));
    };
    auto get = [&](unsigned p) { return std::bit_cast<float>(m.ReadU32(p)); };
    auto check = [&](bool b, const char *msg) {
      if (!b)
        throw std::runtime_error(msg);
    };
    auto call = [&](unsigned entry, unsigned option = 0, double input = 0) {
      s.r[3] = owner;
      s.r[5] = option;
      s.fpr_bits[1] = std::bit_cast<std::uint64_t>(input);
      check(battle_effect_scaling61::Apply(entry, m, {guest, native}, s),
            "scaling entry");
      check(s.r[1] == initial.r[1] && s.r[27] == initial.r[27] &&
                s.r[31] == initial.r[31] &&
                s.fpr_bits[31] == initial.fpr_bits[31],
            "scaling nonvolatile state");
      return std::bit_cast<double>(s.fpr_bits[1]);
    };
    for (unsigned i = 0; i < 32; ++i) {
      m.WriteU32(0x83213438 + 8 * i, 0);
      m.WriteU32(0x8321343c + 8 * i, 1u << i);
    }
    m.WriteU32(owner + 4, source);
    m.WriteU32(owner + 8, target);
    m.WriteU32(source + 64, 24);
    m.WriteU32(0x83264558, 0xa0000);
    for (unsigned i = 0; i < 64; ++i)
      m.WriteU32(0x831f3300 + 4 * i, 1);
    put(0x82000e50, 0);
    put(0x82007784, 1);
    put(0x82000e48, 2);
    put(0x8201dd2c, 100);
    put(0x82000d7c, .01f);
    put(0x82218674, .5f);
    put(0x8201f9f0, .5f);
    put(source + 2524, 3);
    check(call(0x82b09b30, 0, 50) == 52, "source-stat scaling");
    m.WriteU32(target + 4 * 272 + 232, 1u << 6);
    check(call(0x82b09b30, 0, 50) == 26, "target status multiplier");
    check(call(0x82b09b30, 1, 50) == 50, "explicit scaling bypass");
    m.WriteU8(owner + 77, 1);
    m.WriteU32(source + 3 * 272 + 232, 1u << 6);
    call(0x82b097a0);
    check(m.ReadU8(owner + 79) == 1, "guaranteed critical property");
    m.WriteU32(source + 3 * 272 + 232, 1u << 8);
    m.WriteU32(owner + 176, 23);
    call(0x82b097a0);
    check(m.ReadU8(owner + 79) == 1, "category-dependent critical property");
    m.WriteU32(owner + 176, 2);
    call(0x82b097a0);
    check(!m.ReadU8(owner + 79), "critical category rejection");
    m.WriteU8(owner + 77, 0);
    m.WriteU32(source + 5100, 100);
    call(0x82b097a0);
    check(m.ReadU8(owner + 79) == 1, "random critical threshold");
    put(owner + 28, 16);
    check(call(0x82b0a188) == 17, "bounded magnitude jitter");
    put(owner + 28, 0);
    m.WriteU8(owner + 68, 1);
    check(call(0x82b0a188) == 0, "explicit zero magnitude");
    m.WriteU8(owner + 77, 1);
    put(owner + 28, 7.9f);
    check(call(0x82b0a188) == 7, "unjittered truncation");
    m.WriteU8(owner + 77, 0);
    put(owner + 28, 10.2f);
    call(0x82b0a3b0);
    check(get(owner + 32) == 10, "final rounding");
    m.WriteU32(source + 232, 1u << 7);
    call(0x82b0a3b0);
    check(get(owner + 32) == 1, "source status minimum");
    m.WriteU32(source + 232, 0);
    m.WriteU8(owner + 203, 1);
    m.WriteU32(target + 4 * 272 + 232, 1u << 1);
    call(0x82b0a3b0);
    check(get(owner + 32) == 0, "target status zeroing chance");
    m.WriteU8(owner + 203, 0);
    m.WriteU32(source + 68, 20);
    for (unsigned p : {0xc0000u, 0xd0000u})
      m.WriteU32(p, 0x75000);
    m.WriteU32(0x75000 + 292, 0x123450);
    guest.unavailable = 0xc0000;
    put(owner + 28, 9);
    call(0x82b0a3b0);
    check(get(owner + 32) == 5 && guest.peerQueries == 1,
          "partner-dependent half and short circuit");
    guest.unavailable = 0;
    call(0x82b0a3b0);
    check(get(owner + 32) == 9 && guest.peerQueries == 3,
          "both partners available");
    m.WriteU32(0x832aeb00, 0xe0000);
    m.WriteU32(0x832cb790, 0xe1000);
    m.WriteU32(0xe1000 + 20, 0x100000);
    m.WriteU32(target + 124, 0x10000000);
    m.WriteU8(0xe0000 + 48, 1);
    put(0xe0000 + 36, .25f);
    put(0x82000fb0, 1);
    put(owner + 28, 20);
    check(call(0x82b0a0d0) == 15 && m.ReadU32(0x100000 + 15104) == 1,
          "opposite gauge attenuation and result marker");
    m.WriteU32(target + 124, 0x50000000);
    check(call(0x82b0a0d0) == 20, "class gauge bypass");
    m.WriteU32(target + 124, 0x10000000);
    m.WriteU8(0xe0000 + 48, 0);
    check(call(0x82b0a0d0) == 20, "disabled gauge");
    std::puts("PASS effect scaling, critical gates, jitter, status "
              "normalization and partner rounding");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
