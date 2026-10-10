#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_result_application61.h"
#include <iostream>
struct ApplicationGuest final : manager_release_context61::GuestServices {
  unsigned low = 0, dead = 0, counter = 0;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82ac6348) {
      if (s.r[4] != 0x80000)
        throw std::runtime_error("death notification args");
      ++dead;
      return;
    }
    if (e == 0x82ac34f8) {
      if (s.r[4] != 2)
        throw std::runtime_error("counter args");
      ++counter;
      return;
    }
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x76000;
      return;
    }
    if (e == 0x8238e308) {
      s.r[3] = s.r[4] == 24 ? 0x80000 : 0x90000;
      return;
    }
    throw std::runtime_error("application direct " + std::to_string(e));
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("application indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83213000, 0x1000});
    regions.push_back({0x832c9000, 0x4000});
    regions.push_back({0x83291000, 0x1000});
    regions.push_back({0x8201f000, 0x1000});
    regions.push_back({0x83264000, 0x1000});
    regions.push_back({0x831f3000, 0x21000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    ApplicationGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("application state");
    };
    auto put = [&](unsigned p, float v) {
      m.WriteU32(p, std::bit_cast<unsigned>(v));
    };
    auto get = [&](unsigned p) { return std::bit_cast<float>(m.ReadU32(p)); };
    auto amount = [&](float v) {
      s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(v));
    };
    auto run = [&](unsigned e) {
      check(battle_result_application61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[27] == initial.r[27] &&
            s.r[31] == initial.r[31] && s.fpr_bits[31] == initial.fpr_bits[31]);
    };
    for (unsigned i = 0; i < 32; ++i) {
      m.WriteU32(0x83213438 + 8 * i, 0);
      m.WriteU32(0x8321343c + 8 * i, 1u << i);
    }
    put(0x82000e50, 0);
    put(0x82000e40, -1);
    put(0x82007784, 1);
    put(0x82000b3c, .25f);
    auto resource = 0x80000u, owner = 0x73000u;
    m.WriteU32(0x832c9c54 + 44, 0x77000);
    m.WriteU32(0x77000 + 4, 0x78000);
    m.WriteU32(0x77000 + 12, 1);
    m.WriteU32(0x78000 + 8, 24);
    m.WriteU32(resource + 64, 24);
    auto shield = [&](int capacity, float value) {
      m.WriteU32(resource + 7 * 272 + 232, 1);
      m.WriteU32(resource + 4 * 535, unsigned(capacity));
      s.r[3] = resource;
      s.r[5] = 0x71000;
      s.r[6] = 0x71004;
      amount(value);
      run(0x82ac9618);
    };
    shield(50, 20);
    check(get(0x71000) == 0 && get(0x71004) == 20 &&
          m.ReadU32(resource + 4 * 535) == 30);
    shield(30, 50);
    check(get(0x71000) == 20 && get(0x71004) == 30 &&
          m.ReadU32(resource + 4 * 535) == 0 &&
          m.ReadU32(resource + 7 * 272 + 232) == 0);
    auto invoke = [&](unsigned e, float value) {
      s.r[3] = owner;
      s.r[4] = resource;
      s.r[5] = 1;
      s.r[6] = 1;
      amount(value);
      run(e);
    };
    put(resource + 2588, 80);
    put(resource + 2592, 100);
    invoke(0x82b2b5e0, 40);
    check(get(resource + 2588) == 100 && get(owner + 28) == 80 &&
          std::bit_cast<double>(s.fpr_bits[1]) == 40);
    invoke(0x82b2b640, 80);
    check(get(resource + 2588) == 20 && (m.ReadU32(resource + 232) & 2) &&
          g.counter == 1);
    m.WriteU32(resource + 76348, 0x80000000);
    invoke(0x82b2b640, 50);
    check(get(resource + 2588) == 20 && g.counter == 1);
    m.WriteU32(resource + 76348, 0);
    m.WriteU32(resource + 4 * 272 + 232, 1u << 7);
    invoke(0x82b2b640, 30);
    check(get(resource + 2588) == 1 &&
          m.ReadU32(resource + 4 * 272 + 232) == 0 && !g.dead);
    invoke(0x82b2b640, 30);
    check(get(resource + 2588) == 0 && g.dead == 1);
    m.WriteU32(resource + 68, 279);
    invoke(0x82b2b640, 30);
    check(get(resource + 2588) == 1 && g.dead == 1);
    m.WriteU32(resource + 68, 0);
    m.WriteU32(resource + 232, 3);
    invoke(0x82b2b5e0, 60);
    check(get(resource + 2588) == 61 && m.ReadU32(resource + 232) == 0);
    // The real result traversal applies both sides, including repeated shield
    // deductions.
    m.WriteU32(owner + 20, 0x100000);
    m.WriteU32(0x100000 + 36, 24);
    m.WriteU32(0x100000 + 14884, 25);
    for (auto off :
         {14u, 18u, 22u, 26u, 30u, 34u, 38u, 3726u, 3730u, 3734u, 3738u, 3742u})
      put(0x100000 + 4 * off, -1);
    put(resource + 2588, 100);
    put(resource + 2616, 40);
    put(resource + 2620, 50);
    put(0x90000 + 2588, 100);
    put(0x90000 + 2592, 100);
    put(0x90000 + 2616, 10);
    put(0x90000 + 2620, 30);
    m.WriteU32(resource + 7 * 272 + 232, 1);
    m.WriteU32(resource + 4 * 535, 30);
    for (auto [off, value] : {std::pair{14u, 20.f},
                              {34u, 25.f},
                              {18u, 5.f},
                              {22u, 50.f},
                              {26u, 60.f},
                              {3726u, 40.f},
                              {3734u, 15.f},
                              {3730u, 10.f},
                              {3738u, 35.f}})
      put(0x100000 + 4 * off, value);
    s.r[3] = owner;
    run(0x82b2bd50);
    check(get(0x100000 + 4 * 30) == 30 && get(0x100000 + 4 * 14) == 0 &&
          get(0x100000 + 4 * 34) == 15 && get(resource + 2588) == 85 &&
          get(resource + 2616) == 50);
    check(get(0x90000 + 2588) == 70 && get(0x90000 + 2616) == 30 &&
          std::bit_cast<double>(s.fpr_bits[1]) == 40 &&
          s.fpr_bits[29] == initial.fpr_bits[29] &&
          s.fpr_bits[30] == initial.fpr_bits[30]);
    m.WriteU8(owner + 16, 1);
    s.r[3] = owner;
    run(0x82b2bd50);
    check(get(0x90000 + 2588) == 70 &&
          std::bit_cast<double>(s.fpr_bits[1]) == 0);
    m.WriteU32(0x83264558, 0x79000);
    m.WriteU32(resource + 64, 24);
    auto cleanup = [&](unsigned element, unsigned excluded, float prior) {
      s.r[3] = owner;
      s.r[4] = resource;
      s.r[5] = element;
      s.r[6] = excluded;
      amount(prior);
      run(0x82acad40);
    };
    m.WriteU32(resource + 232, (1u << 11) | 8 | 4 | 0x10000);
    put(resource + 2588, 40);
    m.WriteU32(resource + 88, 12);
    m.WriteU32(resource + 92, 13);
    m.WriteU32(resource + 100, 0x80000055);
    cleanup(1, 0, 50);
    check(m.ReadU32(resource + 232) == 0 && m.ReadU32(resource + 60) == 6 &&
          !m.ReadU32(resource + 88) && !m.ReadU32(resource + 92) &&
          m.ReadU32(resource + 100) == 0x55);
    m.WriteU32(resource + 232, 8 | 4 | 0x10000);
    m.WriteU32(resource + 60, 9);
    cleanup(1, 8, 50);
    check(m.ReadU32(resource + 232) == 8 && m.ReadU32(resource + 60) == 9);
    cleanup(0, 0, 40);
    check(m.ReadU32(resource + 232) == 8);
    cleanup(0, 0, 41);
    check(!m.ReadU32(resource + 232) && m.ReadU32(resource + 60) == 6);
    put(0x8201f9f0, .5f);
    auto modeRun = [&](unsigned mode, float input) {
      s.r[3] = owner;
      s.r[4] = resource;
      s.r[6] = mode;
      amount(input);
      run(0x82b2b9e0);
      return std::bit_cast<double>(s.fpr_bits[1]);
    };
    m.WriteU32(resource + 232, 0);
    put(resource + 2588, 80);
    put(resource + 2592, 100);
    put(resource + 2616, 20);
    put(resource + 2620, 60);
    check(modeRun(1, 9.6f) == 10 && get(resource + 2588) == 90);
    check(modeRun(0, 5.4f) == 5 && get(resource + 2588) == 85);
    check(modeRun(2, 30) == 30 && get(resource + 2616) == 0);
    check(modeRun(3, 70) == 70 && get(resource + 2616) == 60);
    put(resource + 2588, 10);
    check(modeRun(4, 2) == 50 && get(resource + 2588) == 50);
    put(resource + 2616, 5);
    check(modeRun(5, 3) == 20 && get(resource + 2616) == 20);
    put(resource + 2588, 80);
    check(modeRun(6, 9) == 55 && get(resource + 2588) == 25);
    put(resource + 2588, 80);
    check(modeRun(7, 30) == 50 && get(resource + 2588) == 30);
    put(resource + 2588, 81);
    check(modeRun(8, 2) == 41 && get(resource + 2588) == 40);
    check(modeRun(99, 12) == 0 && get(resource + 2588) == 40);
    std::cout << "battle result application logic smoke passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
