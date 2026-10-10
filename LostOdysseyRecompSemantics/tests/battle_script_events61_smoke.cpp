#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_events61.h"
struct EventGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress, GuestMemory &,
                  manager_release_context61::Registers &) override {
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
    std::puts(
        "PASS cross-actor event start/completion waits and priority barrier");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
