#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_group_gauge61.h"
#include <iostream>
struct GaugeGuest final : manager_release_context61::GuestServices {
  unsigned unavailable = 0;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x8238e2f8) {
      s.r[3] = 0x71000;
      return;
    }
    if (e == 0x82ab0110) {
      s.r[3] = 0x71200;
      return;
    }
    throw std::runtime_error("gauge direct boundary");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e != 0x123450)
      throw std::runtime_error("gauge virtual boundary");
    s.r[3] = s.r[3] == unavailable;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (auto p : {0x832cb000u, 0x832ae000u, 0x8201d000u, 0x8201f000u})
      regions.push_back({p, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    GaugeGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x73000, one = 0x80000, two = 0x90000,
                       three = 0xa0000;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("gauge state");
    };
    auto put = [&](unsigned p, float v) {
      m.WriteU32(p, std::bit_cast<unsigned>(v));
    };
    auto get = [&](unsigned p) { return std::bit_cast<float>(m.ReadU32(p)); };
    auto run = [&](unsigned e, unsigned mode = 0) {
      s.r[3] = owner;
      s.r[4] = mode;
      check(battle_group_gauge61::Apply(e, m, {guest, native}, s));
      check(s.r[1] == initial.r[1] && s.r[25] == initial.r[25] &&
            s.r[31] == initial.r[31]);
      return unsigned(s.r[3]);
    };
    m.WriteU32(0x832aeb00, owner);
    m.WriteU32(0x71000, 0x71100);
    m.WriteU32(0x71004, 3);
    m.WriteU32(0x71200, 0x71300);
    m.WriteU32(0x71300, one);
    m.WriteU32(0x71304, two);
    unsigned index = 0;
    for (auto resource : {one, two, three}) {
      m.WriteU32(0x71100 + 4 * index++, resource);
      m.WriteU32(resource, 0x74000);
      m.WriteU32(resource + 64, index);
      m.WriteU32(resource + 124,
                 0x40000000 | (resource == one ? 0x10000000 : 0));
      m.WriteU32(resource + 188, resource == one ? 70 : 30);
    }
    for (auto off : {292u, 380u, 384u})
      m.WriteU32(0x74000 + off, 0x123450);
    put(0x82007784, 1);
    put(0x8200104c, 0.75f);
    put(0x8201f9f0, 0.5f);
    put(0x82000da4, 0.25f);
    put(0x8201dd2c, 100);
    check(run(0x82ac7550, one) == 1);
    m.WriteU32(0x832cb778, 242);
    m.WriteU32(one + 64, 0);
    check(run(0x82ac7550, one) == 0);
    m.WriteU32(one + 64, 1);
    guest.unavailable = one;
    check(run(0x82ac7550, one) == 0);
    guest.unavailable = 0;
    m.WriteU8(owner + 24, 1);
    m.WriteU8(owner + 48, 1);
    put(owner + 16, 100);
    put(owner + 40, 60);
    run(0x82ac8448);
    check(get(owner + 4) == 60 && get(owner + 28) == 60);
    check(m.ReadU32(one + 12632) == 60 && m.ReadU32(one + 12636) == 2 &&
          m.ReadU32(one + 12640) == 60);
    check(m.ReadU32(two + 12632) == 60 && m.ReadU32(two + 12636) == 3 &&
          m.ReadU32(two + 12640) == 100);
    guest.unavailable = three;
    run(0x82ac7fc8);
    check(get(owner + 4) == 30);
    for (auto pair :
         {std::pair{10.f, 0u}, {25.f, 1u}, {50.f, 2u}, {75.f, 3u}}) {
      put(owner + 4, pair.first);
      run(0x82ac6e60);
      check(m.ReadU32(owner + 8) == pair.second);
    }
    m.WriteU32(one + 12644, 0x80000123);
    run(0x82afd218);
    check(!m.ReadU8(owner + 24) && m.ReadU32(one + 12644) == 0x123);
    run(0x82ac6e60);
    check(get(owner + 12) == 1 && m.ReadU32(owner + 8) == 3);
    guest.unavailable = 0;
    m.WriteU32(three + 124, 0);
    put(two + 2592, 100);
    put(two + 2588, 40);
    s.r[5] = 1;
    run(0x82ac7b08, 0);
    check(get(owner + 4) == 100 && get(owner + 16) == 100 &&
          m.ReadU32(owner + 20) == 1 && m.ReadU8(owner + 24) == 1 &&
          m.ReadU32(two + 188) == 40 && m.ReadU32(two + 192) == 100 &&
          (m.ReadU32(one + 12644) & 0x80000000));
    s.r[5] = 0;
    run(0x82ac7b08, 0);
    check(get(owner + 4) == 40 && get(owner + 16) == 100);
    m.WriteU32(three + 124, 0x10000000);
    s.r[5] = 0;
    run(0x82ac7b08, 0);
    check(!m.ReadU8(owner + 24) && !(m.ReadU32(one + 12644) & 0x80000000));
    check(!battle_group_gauge61::Apply(0, m, {guest, native}, s));
    std::cout << "battle group gauge logic smoke passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
