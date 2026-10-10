#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_commands61.h"
#include <iostream>
struct CommandGuest final : manager_release_context61::GuestServices {
  unsigned calls = 0, last = 0, predicate = 1, mode = 0, expectedA = 0,
           expectedB = 0, levels = 0;
  void Need(bool x) {
    if (!x)
      throw std::runtime_error("battle command ABI");
  }
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    last = e;
    if (e == 0x82380a18) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x82ac0588 || e == 0x82ac3820) {
      Need(s.r[3] == 0x74000 && s.r[4] == 0x80000 && s.r[5] == 9);
      ++levels;
      return;
    }
    if (e == 0x82ac25e8 || e == 0x82ac2468 || e == 0x82ac0620) {
      Need(s.r[3] == 0x74000 && s.r[4] == 0x80000);
      if (e == 0x82ac0620)
        Need(s.r[5] == 1);
      return;
    }
    if (e == 0x82ab0110)
      return;
    if (e == 0x82a9f548 || e == 0x82a9f648 || e == 0x82a9f5c8 ||
        e == 0x82a9f6b0) {
      Need(s.r[3] == 0x60000 && s.r[4] == 17 && s.r[5] == 9 && s.r[6] == 5);
      ++calls;
      return;
    }
    if (e == 0x823a5058) {
      s.r[3] = 0x75000;
      return;
    }
    if (e == 0x82b079e8) {
      auto text = unsigned(s.r[6]);
      Need(s.r[3] == 0x75000 && s.r[4] == expectedA && s.r[5] == expectedB &&
           s.r[7] == mode && text == unsigned(s.r[1]) + 80);
      Need(m.ReadU16(text) == 0xe000 && m.ReadU16(text + 2) == 0xe001 &&
           m.ReadU16(text + 4) == 0xe000 && m.ReadU16(text + 6) == 0xe001 &&
           m.ReadU16(text + 8) == 0);
      ++calls;
      return;
    }
    Need(s.r[3] == 0x73000 && s.r[4] == 17);
    if (e == 0x82aa0d30) {
      s.r[3] = predicate;
      return;
    }
    Need(s.r[5] == 9);
    if (e == 0x82aa0ce0 || e == 0x82aa0cf0 || e == 0x82aa0d10)
      Need(s.r[6] == 5);
    else if (e == 0x82aa0d00) {
      Need(s.r[6] == unsigned(s.r[1]) + 80 &&
           m.ReadU16(unsigned(s.r[6])) == 0x41 &&
           !m.ReadU16(unsigned(s.r[6]) + 2));
    } else
      Need(e == 0x82aa0d20 || e == 0x82aa0d40);
    ++calls;
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("unexpected command indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x832c9000, 0x4000});
    regions.push_back({0x83291000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    CommandGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       code = 0x69000, vars = 0x6a000, resource = 0x80000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(actor + 4, resource);
    m.WriteU32(0x832cb798, 0x73000);
    m.WriteU32(0x83291dc0, 0x74000);
    m.WriteU32(0x832c9c54 + 44, state);
    auto le = [&](unsigned p, unsigned v) {
      m.WriteU8(p, v);
      m.WriteU8(p + 1, v >> 8);
    };
    auto clear = [&]() {
      for (unsigned i = 0; i < 80; ++i)
        m.WriteU8(code + i, 0);
    };
    auto op = [&](unsigned e, unsigned pc) {
      s.r[3] = owner;
      m.WriteU32(actor + 52, 0);
      guest.Need(battle_script_commands61::Apply(e, m, {guest, native}, s));
      guest.Need(m.ReadU32(actor + 52) == pc && s.r[1] == initial.r[1] &&
                 s.r[28] == initial.r[28] && s.r[31] == initial.r[31]);
    };
    le(code + 1, 0);
    le(code + 3, 1);
    le(code + 5, 2);
    m.WriteU32(vars, 17);
    m.WriteU32(vars + 4, 9);
    m.WriteU32(vars + 8, 5);
    op(0x82afb210, 7);
    op(0x82afb418, 5);
    op(0x82afb500, 5);
    // Source uses overlapping operand offsets 3 and 4 in two command handlers.
    le(code + 3, 1);
    le(code + 4, 0x100);
    m.WriteU32(vars + 4, 9);
    m.WriteU32(vars + 0x400, 5);
    op(0x82afb278, 7);
    op(0x82afb3b0, 7);
    clear();
    le(code + 1, 0);
    le(code + 3, 1);
    m.WriteU16(code + 5, 0x41);
    op(0x82afb2e0, 69);
    le(code + 3, 0xfffd);
    guest.predicate = 0;
    op(0x82afb488, 0xfffffffd);
    guest.predicate = 0x102;
    op(0x82afb488, 5);
    clear();
    le(code + 1, 0);
    m.WriteU32(vars, 9);
    m.WriteU32(resource + 64, 3);
    m.WriteU32(resource + 124, 0x10000000);
    m.WriteU32(resource + 2592, 0x42c80000);
    m.WriteU32(resource + 2620, 0x42480000);
    m.WriteU32(resource + 5108, 19);
    for (unsigned i = 0; i < 5; ++i)
      m.WriteU32(resource + 5116 + 4 * i, 20 + i);
    op(0x82afd038, 3);
    guest.Need(m.ReadU32(actor + 316) == 9 && m.ReadU32(resource + 140) == 9 &&
               m.ReadU32(resource + 2588) == 0x42c80000 &&
               m.ReadU32(resource + 2616) == 0x42480000 &&
               m.ReadU32(0x74004) == 19 && m.ReadU32(0x74000 + 28) == 24 &&
               (m.ReadU32(state + 28) & 0x10000000));
    m.WriteU32(resource + 64, 24);
    op(0x82afd038, 3);
    guest.Need(guest.levels == 2);
    m.WriteU32(vars, 100);
    op(0x82afd038, 3);
    guest.Need(guest.levels == 2 && m.ReadU32(actor + 316) == 100);
    clear();
    le(code + 2, 0);
    le(code + 4, 1);
    le(code + 6, 2);
    m.WriteU32(vars, 17);
    m.WriteU32(vars + 4, 9);
    for (unsigned mode = 0; mode < 4; ++mode) {
      m.WriteU8(code + 1, mode);
      op(0x82a9f790, 8);
      constexpr unsigned targets[]{0x82a9f548, 0x82a9f648, 0x82a9f5c8,
                                   0x82a9f6b0};
      guest.Need(guest.last == targets[mode]);
    }
    clear();
    le(code + 2, 0);
    le(code + 4, 1);
    for (unsigned i = 0; i < 4; ++i)
      m.WriteU16(code + 6 + 2 * i, i == 0   ? 9675
                                   : i == 1 ? 9633
                                   : i == 2 ? 9679
                                            : 9632);
    m.WriteU32(vars, 0);
    m.WriteU32(vars + 4, 0);
    guest.expectedA = guest.expectedB = 0xffffffff;
    guest.mode = 0;
    op(0x82a9f8d8, 22);
    m.WriteU8(code + 1, 1);
    guest.expectedA = guest.expectedB = 0;
    guest.mode = 1;
    op(0x82a9f8d8, 22);
    m.WriteU8(code + 1, 2);
    auto calls = guest.calls;
    op(0x82a9f8d8, 22);
    guest.Need(guest.calls == calls);
    std::cout << "battle_script_commands61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
