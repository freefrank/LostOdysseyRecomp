#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_core61.h"
#include "lo_semantics/battle_script_registration61.h"
#include "lo_semantics/battle_script_calls61.h"
#include "lo_semantics/battle_script_dispatch61.h"
struct CoreGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e != 0x822c9fc0)
      throw std::runtime_error("core direct boundary");
    s.r[3] = 17;
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("unexpected core opcode boundary");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    test::GuestWindow w(cook_main_smoke::Regions);
    w.Fill(0);
    auto m = w.Memory();
    CoreGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, script = 0x64000,
                       code = 0x65000, vars = 0x66000, constants = 0x67000,
                       slots = 0x68000, global = 0x70000, resource = 0x72000,
                       scriptvars = 0x74000;
    m.WriteU32(owner + 8, 0x12345678);
    m.WriteU8(owner + 4, 0xa5);
    m.WriteU8(owner + 5, 0xa5);
    s.r[3] = owner;
    (void)battle_script_registration61::Apply(0x82a9fa40, m, s);
    if (m.ReadU32(owner) != 0x8204a1d8 || m.ReadU32(owner + 8) != 0x12345678 ||
        m.ReadU8(owner + 4) != 0xa5 || m.ReadU8(owner + 5) ||
        m.ReadU32(owner + 24) || m.ReadU32(owner + 44))
      throw std::runtime_error("manager registration header");
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, script);
    m.WriteU32(owner + 28, global);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(actor + 28, constants);
    m.WriteU32(actor + 40, slots);
    m.WriteU32(actor + 4, resource);
    m.WriteU32(script, scriptvars);
    auto half = [&](unsigned p, unsigned x) {
      m.WriteU8(p, x);
      m.WriteU8(p + 1, x >> 8);
    };
    half(code + 1, 0);
    half(code + 3, 0x8000);
    m.WriteU32(constants, 3);
    struct Case {
      unsigned entry, input, expected, length;
    };
    Case cases[]{
        {0x8238c5c0, 10, 3, 5},  {0x82a9d5f8, 10, 1, 3},
        {0x82a9c048, 10, 0, 3},  {0x82a9c088, 10, 13, 5},
        {0x82a9c0f0, 10, 7, 5},  {0x82a9c158, 2, 10, 5},
        {0x82a9c1d8, 10, 2, 5},  {0x82a9c258, 10, 11, 3},
        {0x82a9c2b0, 10, 9, 3},  {0x82a9c308, 10, 2, 5},
        {0x82a9c370, 10, 11, 5}, {0x82a9c3d8, 10, 9, 5},
        {0x82a9c440, 10, 80, 5}, {0x82a9c4b0, unsigned(-16), unsigned(-2), 5},
        {0x82a9c520, 0, 17, 3},  {0x82a9c570, 0, 1, 5}};
    for (auto c : cases) {
      m.WriteU32(actor + 52, 0);
      m.WriteU32(vars, c.input);
      s.r[3] = owner;
      (void)battle_script_core61::Apply(c.entry, m, {guest, native}, s);
      if (m.ReadU32(vars) != c.expected || m.ReadU32(actor + 52) != c.length)
        throw std::runtime_error("core arithmetic operation");
    }
    m.WriteU32(vars, 3);
    m.WriteU32(constants, 1);
    half(code + 6, 30);
    bool pass[]{false, true, true, false, true, false,
                true,  true, true, true,  false};
    for (unsigned mode = 0; mode < 11; ++mode) {
      m.WriteU32(actor + 52, 0);
      m.WriteU8(code + 5, mode);
      s.r[3] = owner;
      (void)battle_script_core61::Apply(0x8238bb90, m, {guest, native}, s);
      if (m.ReadU32(actor + 52) != (pass[mode] ? 8u : 30u))
        throw std::runtime_error("conditional branch modes");
    }
    auto set = [&](unsigned id, unsigned value) {
      m.WriteU32(actor + 52, 0);
      half(code + 1, id);
      s.r[3] = owner;
      s.r[4] = 1;
      s.r[5] = value;
      s.r[6] = 0;
      (void)battle_script_core61::Apply(0x8238c210, m, {guest, native}, s);
    };
    set(2048, 21);
    set(4096, 22);
    set(6144, 1);
    set(32514, unsigned(-7));
    set(0x8000, 99);
    set(256, 99);
    if (m.ReadU32(global + 4 * (2048 + 40668)) != 21 ||
        m.ReadU32(scriptvars) != 22 || m.ReadU32(global + 4 * 44764) != 1 ||
        std::bit_cast<float>(m.ReadU32(resource + 2592)) != -7 ||
        m.ReadU32(constants) != 1 || m.ReadU32(vars + 4 * 256))
      throw std::runtime_error("writable parameter classes");
    for (unsigned i = 0; i < 32; ++i)
      m.WriteU8(code + i, 0);
    for (unsigned i = 0; i < 16; ++i) {
      m.WriteU32(slots + 24 * i, 255);
      m.WriteU32(slots + 24 * i + 8, 0xffffffff);
    }
    m.WriteU32(slots, 0);
    m.WriteU32(slots + 16, 1);
    m.WriteU32(script + 16, 1);
    unsigned values[]{5, 2, 7, 1};
    for (unsigned i = 0; i < 4; ++i)
      m.WriteU32(constants + 4 * i, values[i]);
    unsigned opcodes[]{0, 2, 3, 4, 7},
        handlers[]{0x8238c648, 0x8238bb90, 0x8238c5c0, 0x82a9bf40, 0x82a9c088};
    for (unsigned i = 0; i < 5; ++i)
      if (m.ReadU32(owner + 4 * (14 + opcodes[i])) != handlers[i])
        throw std::runtime_error("registered core opcode");
    m.WriteU8(code, 3);
    half(code + 1, 0);
    half(code + 3, 0x8000);
    m.WriteU8(code + 5, 7);
    half(code + 6, 0);
    half(code + 8, 0x8001);
    m.WriteU8(code + 10, 2);
    half(code + 11, 0);
    half(code + 13, 0x8002);
    m.WriteU8(code + 15, 0);
    half(code + 16, 30);
    m.WriteU8(code + 18, 4);
    half(code + 19, 0x8003);
    m.WriteU8(code + 21, 0);
    for (unsigned i = 0; i < 3; ++i) {
      s.r[3] = owner;
      (void)battle_script_dispatch61::Apply(0x8238b900, m, {guest, native}, s);
    }
    if (m.ReadU32(vars) != 7 || m.ReadU32(slots) != 255 ||
        !(m.ReadU32(script + 28) & 0x80000000) || s.r[1] != initial.r[1])
      throw std::runtime_error(
          "composed arithmetic/condition/wait/end program");

    // Execute a subroutine through the constructor-installed call/return slots.
    for (unsigned i = 0; i < 32; ++i)
      m.WriteU8(code + i, 0);
    m.WriteU8(code, 26);
    half(code + 1, 10);
    m.WriteU8(code + 3, 0);
    m.WriteU8(code + 10, 20);
    half(code + 11, 0);
    half(code + 13, 0x8000);
    m.WriteU8(code + 15, 27);
    m.WriteU32(vars, 6);
    m.WriteU32(constants, 3);
    m.WriteU32(actor + 44, 0);
    m.WriteU32(actor + 48, 0x76000);
    m.WriteU32(slots, 0);
    m.WriteU32(slots + 4, 0);
    m.WriteU32(slots + 16, 1);
    s.r[3] = owner;
    (void)battle_script_dispatch61::Apply(0x8238b900, m, {guest, native}, s);
    if (m.ReadU32(vars) != 18 || m.ReadU32(actor + 44) ||
        m.ReadU32(0x76000) != 3 || m.ReadU32(slots) != 255)
      throw std::runtime_error("registered call/multiply/return/end program");
    m.WriteU32(actor + 52, 0);
    half(code + 1, 0);
    half(code + 3, 1);
    m.WriteU32(vars, 20);
    m.WriteU32(vars + 4, 6);
    s.r[3] = owner;
    (void)battle_script_calls61::Apply(0x82a9c650, m, {guest, native}, s);
    if (m.ReadU32(vars) != 3)
      throw std::runtime_error("integer division");
    m.WriteU32(actor + 52, 0);
    s.r[3] = owner;
    (void)battle_script_calls61::Apply(0x82a9c928, m, {guest, native}, s);
    if (m.ReadU32(vars) != 6 || m.ReadU32(vars + 4) != 3)
      throw std::runtime_error("operand swap");
    m.WriteU32(actor + 44, 32);
    m.WriteU32(script + 28, 0);
    s.r[3] = owner;
    (void)battle_script_calls61::Apply(0x82a9c9a8, m, {guest, native}, s);
    if (!(m.ReadU32(script + 28) & 0x80000000) || m.ReadU32(actor + 44) != 32)
      throw std::runtime_error("call stack full stop");
    std::puts(
        "PASS registered subroutine call/return, multiply/divide and swap");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
