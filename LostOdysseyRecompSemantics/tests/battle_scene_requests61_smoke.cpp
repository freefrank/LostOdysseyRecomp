#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_scene_requests61.h"
#include <iostream>
struct RequestGuest final : manager_release_context61::GuestServices {
  unsigned next = 0xa0000, deleted = 0, released = 0, metadata = 0xb0000;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18)
      return;
    if (e == 0x82b5d230) {
      s.r[3] = 501;
      return;
    }
    if (e == 0x82b5d1a0) {
      m.WriteU32(unsigned(s.r[5]), metadata);
      return;
    }
    if (e == 0x8236c7d8) {
      ++released;
      return;
    }
    if (e == 0x82377168) {
      for (unsigned i = 0; i < 3; ++i) {
        if (m.ReadU32(unsigned(s.r[4]) + 4 * i))
          throw std::runtime_error("zero packed origin");
        m.WriteU16(unsigned(s.r[3]) + 2 * i, 0);
      }
      return;
    }
    throw std::runtime_error("request direct boundary");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e == 0x123404) {
      if (s.r[4] != 100 || s.r[5] != 8)
        throw std::runtime_error("request allocation ABI");
      s.r[3] = next;
      next += 256;
      return;
    }
    if (e == 0x123410) {
      if (s.r[4] != 1)
        throw std::runtime_error("request destruction ABI");
      ++deleted;
      return;
    }
    throw std::runtime_error("request indirect boundary");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (auto p : {0x820c9000u, 0x832cc000u, 0x832cb000u, 0x83213000u,
                   0x8330b000u, 0x832d2000u})
      regions.push_back({p, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    RequestGuest g;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("request state");
    };
    constexpr unsigned owner = 0x832cc0fc, object = 0x74000;
    m.WriteU32(0x8330b608, 0x70000);
    m.WriteU32(0x70000, 0x70100);
    m.WriteU32(0x70104, 0x123404);
    m.WriteU32(0x820010a8, 0x123410);
    m.WriteU32(0x832d268c, 0x72000);
    m.WriteU32(0x832cb554, 0x73000);
    m.WriteU32(0x832cb558, 1);
    m.WriteU32(0x73000, object);
    m.WriteU32(object + 552, 77);
    m.WriteU32(object + 1312, 10);
    m.WriteU32(owner + 8, 0x80000);
    m.WriteU32(owner + 12, 1);
    m.WriteU32(0x80000, 0x81000);
    m.WriteU32(0x81008, 10);
    m.WriteU32(0x81000 + 28, 501);
    m.WriteU32(owner + 24, 0x90000);
    m.WriteU32(owner + 32, 8);
    m.WriteU32(0x83213d74, 10);
    m.WriteU32(0x82000dac, std::bit_cast<unsigned>(.5f));
    for (unsigned i = 0; i < 3; ++i) {
      m.WriteU32(0x820c9ef8 + 16 * i, 0x1000 * (i + 1));
      m.WriteU32(0x820c9efc + 16 * i, 0x1000 * (i + 1));
    }
    auto run = [&](unsigned e, unsigned a, unsigned b = 0, unsigned c = 0) {
      s.r[3] = owner;
      s.r[4] = a;
      s.r[5] = b;
      s.r[6] = c;
      check(battle_scene_requests61::Apply(e, m, {g, native}, s));
      check(s.r[1] == initial.r[1] && s.r[23] == initial.r[23] &&
            s.fpr_bits[31] == initial.fpr_bits[31]);
      return unsigned(s.r[3]);
    };
    check(run(0x82b1aa50, 0x10000001) == 0xffffffff);
    check(run(0x82b1aa50, 0x20000001) == 1);
    auto first = m.ReadU32(0x90000);
    check(m.ReadU32(first + 8) == 1 && m.ReadU32(first + 28) == 0x20000001 &&
          m.ReadU32(first + 20) == 10 && m.ReadU8(first + 4) == 1 &&
          m.ReadU16(first + 36) == 1000);
    check(run(0x82b1aa50, 0x20000001) == 0xffffffff);
    m.WriteU32(first + 96, std::bit_cast<unsigned>(1.f));
    m.WriteU32(owner + 20, 0);
    check(run(0x82b1aa50, 0x20000001) == 2);
    check(run(0x82b1aa50, 0x30000001) == 3);
    auto third = m.ReadU32(0x90008);
    check(m.ReadU32(third + 20) == 0);
    m.WriteU8(g.metadata + 9, 9);
    m.WriteU8(g.metadata + 13, 2);
    check(run(0x82b1aca8, 0x10000002, 77) == 4);
    auto attached = m.ReadU32(0x9000c);
    check(m.ReadU32(attached + 76) == object &&
          m.ReadU32(attached + 80) == 77 && m.ReadU32(attached + 56) == 8193 &&
          m.ReadU8(attached + 13) == 1);
    m.WriteU32(object + 604, 0x2000);
    check(run(0x82b1aca8, 0x10000003, 77) == 0xffffffff);
    m.WriteU32(object + 604, 0);
    g.metadata = 0;
    check(run(0x82b1aa50, 0x20000002) == 0xffffffff && g.deleted == 1 &&
          m.ReadU32(owner + 28) == 4);
    std::cout << "battle scene requests logic smoke passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
