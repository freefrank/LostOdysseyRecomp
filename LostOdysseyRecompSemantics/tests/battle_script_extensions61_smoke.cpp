#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_extensions61.h"
#include <iostream>
#include "lo_semantics/battle_script_dispatch61.h"
#include "lo_semantics/battle_script_registration61.h"
struct CoreGuest final : manager_release_context61::GuestServices {
 void CallDirect(GuestAddress,GuestMemory&,manager_release_context61::Registers&)override {throw std::runtime_error("unexpected direct boundary");}
 void CallIndirect(GuestAddress,GuestMemory&,manager_release_context61::Registers&)override {throw std::runtime_error("unexpected opcode boundary");}
};
int main() {
  try {
    using namespace cook_main_smoke;
    test::GuestWindow w(cook_main_smoke::Regions);
    w.Fill(0);
    auto m = w.Memory();
    CoreGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x64000,
                       code = 0x65000, vars = 0x66000, slots = 0x68000;
    s.r[3] = owner;
    (void)battle_script_registration61::Apply(0x82a9fa40, m, s);
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(actor + 40, slots);
    m.WriteU32(vars, 77);
    s.r[3] = owner;
    s.r[4] = 1;
    s.r[5] = 999;
    (void)battle_script_extensions61::Apply(0x8238c198, m, {guest, native}, s);
    if (s.r[3] != 77)
      throw std::runtime_error("getter tail alias");
    s.r[3] = owner;
    s.r[4] = 1;
    s.r[5] = 88;
    s.r[6] = 999;
    (void)battle_script_extensions61::Apply(0x8238c208, m, {guest, native}, s);
    if (m.ReadU32(vars) != 88)
      throw std::runtime_error("setter tail alias");
    m.WriteU8(code + 1, 0xfe);
    m.WriteU8(code + 2, 0xff);
    s.r[3] = owner;
    s.r[4] = 1;
    (void)battle_script_extensions61::Apply(0x8238c590, m, {guest, native}, s);
    if (s.r[3] != ~std::uint64_t(1))
      throw std::runtime_error("signed immediate");
    m.WriteU8(code, 127);
    m.WriteU8(code + 1, 0);
    m.WriteU8(code + 2, 0);
    s.r[3] = owner;
    s.lr = 0x1234;
    (void)battle_script_extensions61::Apply(0x82a9da28, m, {guest, native}, s);
    if (m.ReadU32(actor + 52) != 2 || !(m.ReadU32(state + 28) & 0x04000000) ||
        s.lr != 0x1234)
      throw std::runtime_error("extended tail skip");
    m.WriteU32(actor + 52, 0);
    s.r[3] = owner;
    (void)battle_script_dispatch61::Apply(0x8238bab0, m, {guest, native}, s);
    if (m.ReadU32(actor + 52) != 2 || !(m.ReadU32(state + 28) & 0x80000000) ||
        (m.ReadU32(state + 28) & 0x04000000) || s.r[1] != initial.r[1])
      throw std::runtime_error("composed extended opcode/end");
    s.r[3] = owner;
    (void)battle_script_extensions61::Apply(0x82af7fb8, m, {guest, native}, s);
    if (m.ReadU32(actor + 52) != 11)
      throw std::runtime_error("nine-byte skip");
    std::cout << "battle_script_extensions61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
