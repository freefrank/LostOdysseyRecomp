#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_loading61.h"
#include "battle_profile_fixture.h"
struct LoadingGuest final : manager_release_context61::GuestServices {
  unsigned next = 0x100000, allocations = 0, freed = 0, operations = 0;
  int assetIndex = 0;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &) override {
    std::fprintf(stderr, "unexpected load direct %08x\n", e);
    throw std::runtime_error("loading direct boundary");
  }
  void CallIndirect(GuestAddress e, GuestMemory &m,
                    manager_release_context61::Registers &s) override {
    if (profile_fixture::Indirect(e, s))
      return;
    if (e == 0x123400) {
      if (s.r[3] != 0x70000 || s.r[5] != 8)
        throw std::runtime_error("loading allocation ABI");
      auto bytes = unsigned(s.r[4]);
      s.r[3] = bytes ? next : 0;
      next += (bytes + 15) & ~15u;
      ++allocations;
      return;
    }
    if (e == 0x123404) {
      if (s.r[3] != 0x70000)
        throw std::runtime_error("loading free ABI");
      ++freed;
      return;
    }
    if (e == 0x123408) {
      s.r[3] = std::uint64_t(std::int64_t(assetIndex));
      return;
    }
    if (e == 0x12340c) {
      auto actor = m.ReadU32(unsigned(s.r[3]) + 24);
      m.WriteU32(actor + 52, m.ReadU32(actor + 52) + 1);
      m.WriteU32(actor + 36, 0);
      ++operations;
      return;
    }
    throw std::runtime_error("loading indirect boundary");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (unsigned p :
         {0x8330b000u, 0x83315000u, 0x832c1000u, 0x83264000u, 0x832cc000u})
      regions.push_back({p, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    LoadingGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, input = 0x65000;
    m.WriteU32(0x8330b608, 0x70000);
    m.WriteU32(0x70000, 0x71000);
    m.WriteU32(0x71004, 0x123400);
    m.WriteU32(0x7100c, 0x123404);
    m.WriteU32(0x82000e50, 0);
    auto check = [&](bool b, const char *msg) {
      if (!b)
        throw std::runtime_error(msg);
    };
    auto call = [&](unsigned e, unsigned object, unsigned count = 0) {
      s.r[3] = object;
      s.r[4] = count;
      check(battle_script_loading61::Apply(e, m, {guest, native}, s),
            "loading entry");
      check(s.r[1] == initial.r[1] && s.r[18] == initial.r[18] &&
                s.r[31] == initial.r[31] &&
                s.fpr_bits[31] == initial.fpr_bits[31],
            "loading nonvolatile state");
    };
    auto payload = [&](bool emptyCode) {
      unsigned p = input;
      auto write = [&](unsigned value, unsigned bytes) {
        for (unsigned j = 0; j < bytes; ++j)
          m.WriteU8(p++, value >> (8 * j));
      };
      write(42, 4);
      write(1, 4);
      write(17, 4);
      write(2, 4);
      write(0, 2);
      write(1, 2);
      write(2, 4);
      write(0x12345678, 4);
      write(0xfffffffe, 4);
      write(emptyCode ? 0 : 2, 4);
      if (!emptyCode) {
        write(7, 1);
        write(9, 1);
      }
    };
    payload(false);
    m.WriteU32(owner + 32, input);
    call(0x82a9e108, owner, 2);
    auto state = m.ReadU32(owner + 44), actor = m.ReadU32(state + 4),
         spare = actor + 472;
    check(s.r[3] == 1 && m.ReadU32(state + 8) == 42 &&
              m.ReadU32(state + 12) == 1 && m.ReadU32(state + 24) == 2 &&
              (m.ReadU32(state + 28) & 0x40000000),
          "script header and loaded flag");
    check(m.ReadU32(actor) == 17 && m.ReadU32(actor + 16) == 2 &&
              m.ReadU32(m.ReadU32(actor + 20) + 4) == 1 &&
              m.ReadU32(m.ReadU32(actor + 28)) == 0x12345678 &&
              m.ReadU32(m.ReadU32(actor + 28) + 4) == 0xfffffffe,
          "event and constant decoding");
    check(m.ReadU32(actor + 32) == 2 && m.ReadU8(m.ReadU32(actor + 36)) == 7 &&
              m.ReadU8(m.ReadU32(actor + 36) + 1) == 9,
          "bytecode copy");
    check(m.ReadU32(spare + 8) == 0xffffffff && m.ReadU32(spare + 64) == 256 &&
              m.ReadU32(spare + 468) == 0xffffffff &&
              m.ReadU32(m.ReadU32(spare + 40)) == 255 &&
              !m.ReadU32(m.ReadU32(spare + 12)),
          "spare actor runtime initialization");
    m.WriteU32(owner + 4 * (7 + 14), 0x12340c);
    call(0x82a9f028, owner);
    check(guest.operations == 1 && m.ReadU32(actor + 52) == 1 &&
              m.ReadU32(m.ReadU32(actor + 40)) == 128 &&
              m.ReadU32(m.ReadU32(actor + 40) + 4) == 1,
          "initial event dispatch and saved cursor");
    payload(true);
    m.WriteU32(owner + 32, input);
    call(0x82a9e108, owner, 1);
    state = m.ReadU32(owner + 44);
    check(!s.r[3] && guest.freed == 1 && !(m.ReadU32(state + 28) & 0x40000000),
          "zero bytecode failure and source release");
    payload(false);
    call(0x82a9e108, owner, 0);
    check(!s.r[3] && guest.freed == 2, "actor count exceeds capacity");
    m.WriteU32(0x832cc05c, 0x72000);
    m.WriteU32(0x72000 + 116, 0x123408);
    m.WriteU32(0x832cc05c + 8, 0x73000);
    m.WriteU32(0x73000, 0x74000);
    m.WriteU32(0x74000 + 84, input);
    m.WriteU32(0x74000 + 72, 99);
    call(0x82b024e0, 0x832cc05c);
    check(s.r[3] == input, "script data asset lookup");
    call(0x82b025b8, 0x832cc05c);
    check(s.r[3] == 99, "script metadata asset lookup");
    guest.assetIndex = -1;
    call(0x82b024e0, 0x832cc05c);
    check(!s.r[3], "missing script asset");
    profile_fixture::Setup(m, 0x68000);
    m.WriteU32(0x93000 + 68, 0xffffffff);
    auto before = guest.allocations;
    call(0x82a9f0a0, owner);
    check(s.r[3] == 1 && guest.allocations == before &&
              m.ReadU32(owner + 28) == 0x93000,
          "profile no-script shortcut");
    std::puts("PASS script actor allocation, data decode, failure paths, asset "
              "lookup and initial dispatch");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
