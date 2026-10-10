#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_binding61.h"
#include <iostream>
struct BindingGuest final : manager_release_context61::GuestServices {
  unsigned callbacks = 0, transitions = 0;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x8238e2f8) {
      s.r[3] = 0x71000;
      return;
    }
    if (e == 0x82389b78) {
      s.r[3] = 0x72000;
      return;
    }
    if (e == 0x82aaa7c8) {
      if (s.r[3] != 0x72000 || s.r[4] != 13 || s.r[5] != 1)
        throw std::runtime_error("transition arguments");
      ++transitions;
      return;
    }
    unsigned calls[]{0x82ac7b08, 0x82ac7fc8, 0x82ac6e60, 0x82ac6f08};
    if (e != calls[callbacks % 4] || s.r[3] != 0x73000 || s.r[4] != 1)
      throw std::runtime_error("binding callback sequence");
    ++callbacks;
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("binding indirect boundary");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x832ca000, 0x2000});
    regions.push_back({0x832ae000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    BindingGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actors = 0x62000, state = 0x63000,
                       code = 0x64000, vars = 0x65000, r0 = 0x74000,
                       r1 = 0x76000;
    m.WriteU32(owner + 24, actors);
    m.WriteU32(owner + 44, state);
    m.WriteU32(state + 4, actors);
    m.WriteU32(state + 12, 2);
    m.WriteU32(actors, 3);
    m.WriteU32(actors + 36, code);
    m.WriteU32(actors + 12, vars);
    m.WriteU32(actors + 472 + 4, r0);
    m.WriteU32(actors + 472 + 8, 24);
    m.WriteU32(0x71000, 0x71100);
    m.WriteU32(0x71004, 2);
    m.WriteU32(0x71100, r0);
    m.WriteU32(0x71104, r1);
    m.WriteU32(r0 + 64, 24);
    m.WriteU32(r0 + 68, 7);
    m.WriteU32(r0 + 148, 4);
    m.WriteU32(r1 + 64, 25);
    m.WriteU32(r1 + 68, 7);
    m.WriteU32(r1 + 148, 0xffffffff);
    auto op = [&](unsigned e) {
      s.r[3] = owner;
      m.WriteU32(actors + 52, 0);
      (void)battle_script_binding61::Apply(e, m, {guest, native}, s);
      if (s.r[1] != initial.r[1] || s.r[27] != initial.r[27] ||
          s.r[31] != initial.r[31])
        throw std::runtime_error("binding ABI");
    };
    m.WriteU32(vars, 7);
    op(0x82afd7f8);
    if (m.ReadU32(actors + 4) != r0 || m.ReadU32(actors + 8) != 24 ||
        m.ReadU32(r0 + 148) != 3 || m.ReadU32(actors + 472 + 4) ||
        m.ReadU32(actors + 472 + 8) != 0xffffffff ||
        !(m.ReadU32(actors + 472 + 64) & 0x80000000))
      throw std::runtime_error("forced resource rebind");
    m.WriteU32(actors + 4, 0);
    op(0x82afeaf8);
    if (m.ReadU32(actors + 4) != r1 || m.ReadU32(r1 + 148) != 3)
      throw std::runtime_error("free resource binding");
    m.WriteU32(vars, 25);
    op(0x82afd7f8);
    if (m.ReadU32(actors + 4) != r1 || m.ReadU32(actors + 8) != 25)
      throw std::runtime_error("resource ID binding");
    m.WriteU32(actors + 4, 0);
    m.WriteU32(vars, 99);
    op(0x82afd7f8);
    if (!(m.ReadU32(state + 28) & 0x80000000) || m.ReadU32(actors + 52) != 3)
      throw std::runtime_error("missing binding stops");
    m.WriteU32(actors + 4, r0);
    m.WriteU32(r0 + 124, 0x10000000);
    m.WriteU32(0x832aeb00, 0x73000);
    m.WriteU8(code + 1, 1);
    m.WriteU8(0x73000 + 48, 99);
    op(0x82a9e988);
    if (!(m.ReadU32(actors + 64) & 0x40000000) ||
        !(m.ReadU32(r0 + 124) & 0x400000) || guest.callbacks != 4 ||
        m.ReadU8(0x73000 + 48))
      throw std::runtime_error("binding visibility state");
    m.WriteU32(0x832ca0e0 + 5784, 233);
    m.WriteU8(code + 1, 2);
    op(0x82a9e988);
    if ((m.ReadU32(actors + 64) & 0x40000000) ||
        (m.ReadU32(r0 + 124) & 0x400000) || guest.callbacks != 4)
      throw std::runtime_error("original exception and low bit");
    m.WriteU8(code + 1, 0);
    m.WriteU32(0x72000 + 56, 9);
    op(0x82af9ae0);
    if (m.ReadU32(actors + 52) || guest.transitions)
      throw std::runtime_error("transition wait");
    m.WriteU32(0x72000 + 56, 8);
    op(0x82af9ae0);
    if (m.ReadU32(actors + 52) != 2 || guest.transitions != 1)
      throw std::runtime_error("transition request");
    m.WriteU8(code + 1, 1);
    op(0x82af9b88);
    if (!(m.ReadU32(0x72000 + 148) & 0x800))
      throw std::runtime_error("battle flag");
    m.WriteU8(code + 1, 0);
    m.WriteU32(0x72000 + 52, 123);
    op(0x82af9a88);
    if (m.ReadU32(vars) != 123)
      throw std::runtime_error("battle state query");
    std::cout << "battle_script_binding61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
