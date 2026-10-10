#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_events61.h"
#include "lo_semantics/battle_script_control61.h"
struct EventGuest final : manager_release_context61::GuestServices {
  unsigned enabled = 0, disabled = 0;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (s.r[3] != 0x70000 || s.r[4] != 20)
      throw std::runtime_error("actor status arguments");
    if (e == 0x82ac9be0) {
      ++enabled;
      return;
    }
    if (e == 0x82ac9000) {
      ++disabled;
      return;
    }
    throw std::runtime_error("unexpected event direct boundary");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("unexpected event indirect boundary");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    test::GuestWindow w(cook_main_smoke::Regions);
    w.Fill(0);
    auto m = w.Memory();
    EventGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, a = 0x62000, b = a + 472,
                       script = 0x65000, code = 0x66000, aslots = 0x68000,
                       bslots = 0x69000, eventtable = 0x6a000;
    m.WriteU32(owner + 24, a);
    m.WriteU32(owner + 44, script);
    m.WriteU32(script + 4, a);
    m.WriteU32(script + 12, 2);
    m.WriteU32(a, 7);
    m.WriteU32(b, 42);
    m.WriteU32(a + 36, code);
    m.WriteU32(a + 40, aslots);
    m.WriteU32(b + 40, bslots);
    m.WriteU32(b + 20, eventtable);
    m.WriteU32(b + 16, 20);
    for (unsigned i = 0; i < 16; ++i)
      m.WriteU32(bslots + 24 * i, 255);
    m.WriteU32(eventtable + 12, 10);
    m.WriteU32(eventtable + 16, 20);
    m.WriteU32(eventtable + 72, 30);
    m.WriteU8(code + 1, 7);
    m.WriteU8(code + 2, 42);
    m.WriteU8(code + 6, 3);
    s.r[3] = owner;
    s.r[4] = 0x7ffffff8;
    (void)battle_script_events61::Apply(0x8238c118, m, {guest, native}, s);
    if (s.r[3] != a)
      throw std::runtime_error("self actor sentinel");
    s.r[3] = owner;
    s.r[4] = 42;
    (void)battle_script_events61::Apply(0x8238c118, m, {guest, native}, s);
    if (s.r[3] != b)
      throw std::runtime_error("actor ID lookup");
    auto run = [&](unsigned e) {
      m.WriteU32(script + 28, 0);
      s.r[3] = owner;
      (void)battle_script_events61::Apply(e, m, {guest, native}, s);
    };
    run(0x82a9cbe8);
    if (m.ReadU32(aslots + 20) != 1 || m.ReadU32(a + 52) ||
        !(m.ReadU32(script + 28) & 0x80000000) ||
        m.ReadU32(bslots + 15 * 24 + 16) != 3)
      throw std::runtime_error("enqueue and wait for event start");
    run(0x82a9cbe8);
    if (m.ReadU32(a + 52))
      throw std::runtime_error("queued event still waits");
    m.WriteU32(b + 56, 15);
    run(0x82a9cbe8);
    if (m.ReadU32(a + 52) != 7 || m.ReadU32(aslots + 20))
      throw std::runtime_error("active event releases start wait");
    m.WriteU32(a + 52, 0);
    m.WriteU8(code + 6, 4);
    run(0x82a9cda8);
    m.WriteU32(b + 56, 14);
    run(0x82a9cda8);
    if (m.ReadU32(aslots + 20) != 2 || m.ReadU32(a + 52))
      throw std::runtime_error("completion wait stage");
    m.WriteU32(bslots + 14 * 24 + 16, 0);
    m.WriteU32(bslots + 14 * 24, 255);
    run(0x82a9cda8);
    if (m.ReadU32(a + 52) != 7 || m.ReadU32(aslots + 20))
      throw std::runtime_error("completed event releases wait");
    m.WriteU32(a + 52, 0);
    m.WriteU8(code + 1, 7);
    run(0x82a9cfe8);
    if (m.ReadU32(a + 52) || !(m.ReadU32(script + 28) & 0x80000000))
      throw std::runtime_error("priority barrier");
    m.WriteU8(code + 1, 6);
    run(0x82a9cfe8);
    if (m.ReadU32(a + 52) != 6)
      throw std::runtime_error("priority barrier release");
    m.WriteU32(a + 52, 0);
    m.WriteU8(code + 6, 18);
    for (unsigned i = 0; i < 16; ++i) {
      m.WriteU32(bslots + 24 * i, 1);
      m.WriteU32(bslots + 24 * i + 16, i);
    }
    run(0x82a9cb20);
    if (m.ReadU32(a + 52) || !(m.ReadU32(script + 28) & 0x80000000) ||
        s.r[1] != initial.r[1])
      throw std::runtime_error("full event queue retry");

    m.WriteU32(a + 52, 0);
    m.WriteU32(script + 28, 0);
    m.WriteU32(b + 4, 0x70000);
    m.WriteU32(0x70000 + 124, 0x10000000);
    m.WriteU8(code + 1, 1);
    s.r[3] = owner;
    (void)battle_script_control61::Apply(0x82a9e7d0, m, {guest, native}, s);
    if (!(m.ReadU32(b + 64) & 0x80000000) || m.ReadU32(0x70000 + 132) ||
        guest.enabled != 1)
      throw std::runtime_error("actor flag enable");
    m.WriteU32(a + 52, 0);
    m.WriteU8(code + 1, 2);
    s.r[3] = owner;
    (void)battle_script_control61::Apply(0x82a9e7d0, m, {guest, native}, s);
    if (m.ReadU32(b + 64) & 0x80000000 ||
        !(m.ReadU32(0x70000 + 124) & 0x10000) ||
        m.ReadU32(0x70000 + 132) != 1 || guest.disabled != 1)
      throw std::runtime_error("actor low-bit versus nonzero flag semantics");
    constexpr unsigned vars = 0x6c000, play = 0x6d000;
    m.WriteU32(a + 12, vars);
    m.WriteU32(owner + 28, play);
    auto half = [&](unsigned p, unsigned v) {
      m.WriteU8(p, v);
      m.WriteU8(p + 1, v >> 8);
    };
    auto op = [&](unsigned e) {
      m.WriteU32(a + 52, 0);
      s.r[3] = owner;
      (void)battle_script_control61::Apply(e, m, {guest, native}, s);
    };
    half(code + 1, 0);
    half(code + 3, 1);
    half(code + 5, 2);
    half(code + 8, 40);
    m.WriteU32(vars, 5);
    m.WriteU32(vars + 4, 0);
    m.WriteU32(vars + 8, 10);
    op(0x82a9d2b0);
    if (m.ReadU32(a + 52) != 10)
      throw std::runtime_error("inclusive range branch");
    m.WriteU32(vars, 11);
    op(0x82a9d2b0);
    if (m.ReadU32(a + 52) != 40)
      throw std::runtime_error("range branch target");
    half(code + 5, 30);
    m.WriteU32(vars, 4);
    m.WriteU32(vars + 4, 4);
    op(0x82a9d228);
    if (m.ReadU32(a + 52) != 7)
      throw std::runtime_error("mask branch");
    m.WriteU32(vars + 4, 2);
    op(0x82a9d228);
    if (m.ReadU32(a + 52) != 30)
      throw std::runtime_error("mask branch target");
    m.WriteU32(vars, 41);
    m.WriteU32(script + 16, 2);
    op(0x82a9d0d0);
    if (m.ReadU32(vars) != 43)
      throw std::runtime_error("tick accumulator");
    half(code + 5, 2);
    m.WriteU32(vars + 4, unsigned(-7));
    m.WriteU32(vars + 8, 3);
    op(0x82a9d130);
    if (m.ReadU32(vars) != unsigned(-1))
      throw std::runtime_error("signed remainder");
    m.WriteU32(vars, 20);
    m.WriteU32(play + 76, 9999998);
    op(0x82a9d360);
    if (m.ReadU32(play + 76) != 9999999)
      throw std::runtime_error("game counter upper cap");
    m.WriteU32(vars, unsigned(-10));
    m.WriteU32(play + 76, 5);
    op(0x82a9d360);
    if (m.ReadU32(play + 76) != unsigned(-5) || s.r[1] != initial.r[1])
      throw std::runtime_error("original counter lower behavior");
    std::puts(
        "PASS actor flags, range/mask branches, ticks, remainder and counter");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
