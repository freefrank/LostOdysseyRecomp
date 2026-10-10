#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_party61.h"
struct PartyGuest final : manager_release_context61::GuestServices {
  unsigned notified = 0;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82ab0110) {
      s.r[3] = 0x70000;
      return;
    }
    throw std::runtime_error("party direct boundary");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e != 0x2000 || s.r[3] != 0x65000 || s.r[4] != 7 || s.r[5] != 1 ||
        s.r[6] != 1 || s.r[7] != 1)
      throw std::runtime_error("inventory notification");
    ++notified;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x8204b000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    PartyGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, script = 0x63000,
                       code = 0x64000, play = 0x65000, vars = 0x66000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, script);
    m.WriteU32(owner + 28, play);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    auto half = [&](unsigned p, unsigned x) {
      m.WriteU8(p, x);
      m.WriteU8(p + 1, x >> 8);
    };
    auto op = [&](unsigned e) {
      m.WriteU32(actor + 52, 0);
      s.r[3] = owner;
      (void)battle_script_party61::Apply(e, m, {guest, native}, s);
    };
    unsigned values[]{1, 0xffffffff, 3, 0xffffffff, 4};
    for (unsigned i = 0; i < 5; ++i)
      m.WriteU32(play + 104 + 4 * i, values[i]);
    m.WriteU32(vars, 2);
    m.WriteU8(code + 1, 2);
    half(code + 2, 0);
    op(0x82a9eae8);
    if (m.ReadU32(play + 108) != 2 || m.ReadU32(play + 116) != 2)
      throw std::runtime_error("original multiple-vacancy insertion");
    m.WriteU8(code + 1, 3);
    op(0x82a9eae8);
    unsigned compact[]{1, 3, 4, 0xffffffff, 0xffffffff};
    for (unsigned i = 0; i < 5; ++i)
      if (m.ReadU32(play + 104 + 4 * i) != compact[i])
        throw std::runtime_error("party removal compaction");
    for (unsigned i = 0; i < 5; ++i)
      half(code + 1 + 2 * i, i);
    op(0x82a9d3c8);
    for (unsigned i = 0; i < 5; ++i) {
      if (m.ReadU32(vars + 4 * i) != compact[i])
        throw std::runtime_error("party read");
      m.WriteU32(vars + 4 * i, 9 - i);
    }
    op(0x82a9d450);
    for (unsigned i = 0; i < 5; ++i)
      if (m.ReadU32(play + 104 + 4 * i) != 9 - i)
        throw std::runtime_error("party write");
    m.WriteU32(vars, 3);
    m.WriteU8(code + 1, 0);
    half(code + 2, 0);
    op(0x82a9eae8);
    half(code + 1, 0);
    half(code + 3, 20);
    op(0x82a9d4e0);
    if (m.ReadU32(actor + 52) != 5 || m.ReadU32(play + 96) != 8)
      throw std::runtime_error("party bit branch");
    m.WriteU32(vars, 4);
    op(0x82a9d4e0);
    if (m.ReadU32(actor + 52) != 20)
      throw std::runtime_error("party bit missing branch");
    m.WriteU32(0x70000, 0x71000);
    m.WriteU32(0x71004, 0x72000);
    m.WriteU32(play, 0x73000);
    m.WriteU32(0x73000 + 312, 0x2000);
    m.WriteU32(0x8204bc58, std::bit_cast<unsigned>(5.0f));
    m.WriteU32(0x82000e50, 0);
    half(code + 1, 0);
    half(code + 3, 1);
    m.WriteU32(vars, 7);
    m.WriteU32(vars + 4, 10);
    op(0x82a9ea80);
    if (std::bit_cast<float>(m.ReadU32(0x72000 + 72 + 28)) != 5 ||
        m.ReadU32(0x72000 + 72 + 4096) != 7 || guest.notified != 1)
      throw std::runtime_error("inventory quantity/cap/notification");
    m.WriteU32(script + 52, 0x6e000);
    m.WriteU32(vars, 16);
    m.WriteU32(vars + 4, 9);
    op(0x82a9d1b8);
    if (m.ReadU32(0x6e000 + 64) != 9)
      throw std::runtime_error("original scratch index16 boundary");
    auto pc = m.ReadU32(actor + 52);
    s.r[3] = owner;
    (void)battle_script_party61::Apply(0x82a9da20, m, {guest, native}, s);
    if (m.ReadU32(actor + 52) != pc || s.r[1] != initial.r[1] ||
        s.r[31] != initial.r[31])
      throw std::runtime_error("reserved no-op behavior");
    std::puts("PASS party list/flags, inventory update, scratch boundary and "
              "no-op alias");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
