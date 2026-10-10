#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_dispatch61.h"
struct DispatchGuest final : manager_release_context61::GuestServices {
  unsigned operations = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x8238e308) {
      if (s.r[4] != 9)
        throw std::runtime_error("completion ID");
      s.r[3] = 0x71000;
      m.WriteU32(0x71000 + 60, 5);
      return;
    }
    throw std::runtime_error("dispatch direct boundary");
  }
  void CallIndirect(GuestAddress e, GuestMemory &m,
                    manager_release_context61::Registers &s) override {
    if (s.lr != 0x8238bb44)
      throw std::runtime_error("opcode return address");
    auto actor = m.ReadU32(Address(s.r[3]) + 24);
    ++operations;
    if (e == 0x1000) {
      m.WriteU32(actor + 52, m.ReadU32(actor + 52) + 1);
      return;
    }
    if (e == 0x1004) {
      m.WriteU32(actor + 36, 0);
      return;
    }
    throw std::runtime_error("unknown synthetic opcode");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    test::GuestWindow w(cook_main_smoke::Regions);
    w.Fill(0);
    auto m = w.Memory();
    DispatchGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, script = 0x64000,
                       code = 0x65000, vars = 0x66000, events = 0x67000,
                       from = 0x68000, slots = 0x6a000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, script);
    m.WriteU32(actor + 16, 2);
    m.WriteU32(actor + 20, events);
    m.WriteU32(actor + 40, slots);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(vars, 2);
    m.WriteU32(from, 99);
    m.WriteU32(events + 4, 16);
    m.WriteU32(events + 8, 16);
    for (unsigned i = 0; i < 16; ++i) {
      m.WriteU32(slots + 24 * i, 255);
      m.WriteU32(slots + 24 * i + 8, 0xffffffff);
    }
    auto queue = [&](unsigned event) {
      s.r[3] = owner;
      s.r[4] = actor;
      s.r[5] = from;
      s.r[6] = event;
      s.r[7] = 7;
      (void)battle_script_dispatch61::Apply(0x82a9bdf8, m, {guest, native}, s);
      return s.r[3];
    };
    if (queue(1) != 1 || queue(1) != 0 || queue(2) != 1 ||
        m.ReadU32(slots + 15 * 24 + 12) != 99)
      throw std::runtime_error("last-free event insertion/duplicate");
    m.WriteU32(owner + 4 * (14 + 1), 0x1000);
    m.WriteU32(owner + 4 * (14 + 4), 0x82a9bf40);
    m.WriteU32(owner + 4 * (14 + 2), 0x1004);
    m.WriteU8(code + 16, 1);
    m.WriteU8(code + 17, 4);
    m.WriteU8(code + 20, 2);
    m.WriteU32(script + 16, 1);
    auto schedule = [&]() {
      s.r[3] = owner;
      (void)battle_script_dispatch61::Apply(0x8238b900, m, {guest, native}, s);
    };
    schedule();
    if (m.ReadU32(actor + 56) != 15 || m.ReadU32(slots + 15 * 24 + 4) != 17 ||
        m.ReadU32(slots + 15 * 24 + 8) != 2 || guest.operations != 1)
      throw std::runtime_error("priority tie / composed wait dispatch");
    schedule();
    schedule();
    schedule();
    if (m.ReadU32(actor + 36) || guest.operations != 2 ||
        m.ReadU32(slots + 15 * 24 + 4) != 20)
      throw std::runtime_error("wait resume and opcode exit");
    m.WriteU32(actor + 64, 8);
    m.WriteU32(actor + 468, 9);
    s.r[3] = owner;
    (void)battle_script_dispatch61::Apply(0x8238b850, m, {guest, native}, s);
    if (m.ReadU32(actor + 64) & 8 || m.ReadU32(actor + 468) != 0xffffffff ||
        s.r[1] != initial.r[1])
      throw std::runtime_error("completion event cleanup");
    std::puts("PASS event queue, priority scheduling, opcode loop and timed "
              "wait resume");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
