#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_queues61.h"
#include <iostream>
struct QueueGuest final : manager_release_context61::GuestServices {
  unsigned allocated = 0, next = 0xa0000, numeric = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x8238e2f8) {
      s.r[3] = 0x71000;
      return;
    }
    if (e == 0x8238e308) {
      s.r[3] = s.r[4] == 24 ? 0x80000 : 0;
      return;
    }
    if (e == 0x82486c88) {
      if (s.r[3] != (allocated ? 4096 : 1024))
        throw std::runtime_error("queue allocation size");
      s.r[3] = next;
      next += 0x2000;
      ++allocated;
      return;
    }
    if (e == 0x82b7bc40) {
      for (unsigned i = 0; i < unsigned(s.r[5]); ++i)
        m.WriteU8(unsigned(s.r[3]) + i, unsigned(s.r[4]));
      return;
    }
    if (e == 0x8285fe68) {
      if (s.r[3] != 0x72000 || s.r[4] != 24 || s.r[5] != 7 ||
          std::bit_cast<double>(s.fpr_bits[1]) != -9.0 ||
          std::bit_cast<double>(s.fpr_bits[2]) != 12.0)
        throw std::runtime_error("signed numeric ABI");
      ++numeric;
      return;
    }
    throw std::runtime_error("unexpected queue service");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("unexpected queue indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83265000, 0x1000});
    regions.push_back({0x83245000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    QueueGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       code = 0x69000, vars = 0x6a000, resource = 0x80000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(actor, 44);
    m.WriteU32(actor + 8, 24);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(actor + 4, resource);
    m.WriteU32(state + 4, actor);
    m.WriteU32(state + 12, 1);
    m.WriteU32(resource + 64, 24);
    auto le = [&](unsigned p, unsigned v) {
      m.WriteU8(p, v);
      m.WriteU8(p + 1, v >> 8);
    };
    auto check = [](bool b) {
      if (!b)
        throw std::runtime_error("queue result");
    };
    auto op = [&](unsigned e, unsigned pc) {
      s.r[3] = owner;
      m.WriteU32(actor + 52, 0);
      check(battle_script_queues61::Apply(e, m, {guest, native}, s));
      check(m.ReadU32(actor + 52) == pc && s.r[1] == initial.r[1] &&
            s.r[22] == initial.r[22] && s.r[31] == initial.r[31]);
    };
    le(code + 1, 0);
    le(code + 3, 1);
    m.WriteU32(vars, 24);
    m.WriteU32(vars + 4, 7);
    op(0x82af7760, 5);
    op(0x82af7890, 5);
    check(m.ReadU32(resource + 4880) == 7 && m.ReadU32(resource + 76356) == 7);
    m.WriteU32(vars, 7);
    op(0x82af77c0, 3);
    check((m.ReadU32(state + 28) & 0x1800000) == 0x1800000);
    m.WriteU32(vars, 24);
    le(code + 5, 2);
    le(code + 7, 3);
    m.WriteU32(vars + 8, unsigned(-9));
    m.WriteU32(vars + 12, 12);
    m.WriteU32(0x832652f0, 0x72000);
    op(0x82afa2e8, 9);
    check(guest.numeric == 1);
    le(code + 1, 44);
    le(code + 3, 0);
    le(code + 5, 0);
    op(0x82af7810, 7);
    check(m.ReadU32(vars) == 24);
    le(code + 1, 45);
    op(0x82af7810, 7);
    check(m.ReadU32(vars) == 0xffffffff);
    m.WriteU32(state + 28, 0);
    for (unsigned mode = 0; mode < 4; ++mode) {
      m.WriteU8(code + 1, mode);
      op(0x82afa388, 2);
      check((m.ReadU32(state + 28) & 0x600000) == (mode == 0   ? 0x200000
                                                   : mode == 1 ? 0x600000
                                                   : mode == 2 ? 0x400000
                                                               : 0));
    }
    m.WriteU8(code + 1, 32);
    op(0x82af9870, 3);
    check(m.ReadU16(0x70000 + 148) == 32 && (m.ReadU32(actor + 64) & 0x4000));
    m.WriteU8(code + 1, 0);
    op(0x82af9870, 3);
    check(!m.ReadU16(0x70000 + 148) && (m.ReadU32(actor + 64) & 0x4000));
    le(code + 2, 0);
    m.WriteU32(vars, 24);
    m.WriteU32(0x82001270, 0x3f000000);
    m.WriteU8(code + 1, 0);
    op(0x82afa440, 4);
    check(m.ReadU32(resource + 76344) == 0x3f000000);
    m.WriteU8(code + 1, 1);
    op(0x82afa440, 4);
    check(!m.ReadU32(resource + 76344));
    le(code + 1, 0);
    m.WriteU32(vars, 77);
    op(0x82afa500, 3);
    m.WriteU32(vars, 88);
    op(0x82afa500, 3);
    check(guest.allocated == 3 && m.ReadU32(state + 16744) == 2 &&
          m.ReadU32(m.ReadU32(state + 16740)) == 77 &&
          m.ReadU32(m.ReadU32(state + 16740) + 4) == 88);
    le(code + 1, 0);
    le(code + 3, 1);
    le(code + 5, 2);
    m.WriteU32(vars, 31);
    m.WriteU32(vars + 4, 0);
    m.WriteU32(vars + 8, 77);
    op(0x82af78f8, 7);
    m.WriteU32(vars + 8, 88);
    op(0x82af78f8, 7);
    le(code + 1, 0xfffd);
    op(0x82af7ad0, 3);
    m.WriteU32(m.ReadU32(state + 16728) + 4, 5);
    op(0x82af7ad0, 0xfffffffd);
    le(code + 1, 0);
    le(code + 3, 1);
    le(code + 5, 2);
    m.WriteU32(vars, 3);
    m.WriteU32(vars + 4, 26);
    m.WriteU32(vars + 8, 99);
    op(0x82af78f8, 7);
    m.WriteU32(vars + 4, 27);
    op(0x82af78f8, 7);
    check(m.ReadU32(state + 16732) == 7 &&
          m.ReadU32(m.ReadU32(state + 16728) + 24) == 99 &&
          m.ReadU32(m.ReadU32(state + 16736) + 24) == 24);
    m.WriteU8(actor + 312, 5);
    op(0x82af7f18, 3);
    check(m.ReadU32(vars) == 1);
    m.WriteU32(state + 32, 123);
    op(0x82af7f70, 3);
    check(m.ReadU32(vars) == 123);
    m.WriteU8(code + 1, 0);
    op(0x82afd150, 2);
    check(m.ReadU32(resource + 76348) == 0x80000000);
    m.WriteU32(0x71000, 0x71100);
    m.WriteU32(0x71004, 1);
    m.WriteU32(0x71100, resource);
    m.WriteU32(resource + 60, 2);
    m.WriteU32(resource + 14660, 2);
    m.WriteU32(resource + 14656, 0xc0000);
    m.WriteU32(resource + 100, 0x40000007);
    m.WriteU32(0x8324570c, 0x73000);
    m.WriteU32(0x73000 + 16, 321);
    op(0x82aff4a8, 1);
    check(m.ReadU32(0xc0000) == 13 && m.ReadU32(0xc0000 + 124208) == 13 &&
          m.ReadU32(resource + 100) == 0x80000007 &&
          m.ReadU32(resource + 96) == 0xffffffff &&
          m.ReadU32(resource + 92) == 321);
    std::cout << "battle_script_queues61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
