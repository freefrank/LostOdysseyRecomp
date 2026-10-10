#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_parameters61.h"
#include "battle_scene_request_fixture.h"
struct ParametersGuest final : manager_release_context61::GuestServices {
  unsigned events = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82b5d1a0)
      ++events;
    if (requests_fixture::Direct(e, m, s))
      return;
    throw std::runtime_error("parameter direct boundary");
  }
  void CallIndirect(GuestAddress e, GuestMemory &m,
                    manager_release_context61::Registers &s) override {
    if (requests_fixture::Indirect(e, m, s))
      return;
    throw std::runtime_error("parameter indirect boundary");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x832cb000, 0x1000});
    for (auto p :
         {0x832cc000u, 0x83213000u, 0x832d2000u, 0x8330b000u, 0x820c9000u})
      regions.push_back({p, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    requests_fixture::Setup(m);
    ParametersGuest guest;
    auto s = sort_engine61_oracle::Initial(0);
    constexpr unsigned owner = 0x60000, actor = 0x61000, script = 0x62000,
                       code = 0x63000, locals = 0x64000, constants = 0x65000,
                       variables = 0x66000, resource = 0x67000,
                       globals = 0x68000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 28, globals);
    m.WriteU32(owner + 44, script);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, locals);
    m.WriteU32(actor + 28, constants);
    m.WriteU32(actor + 4, resource);
    m.WriteU32(script, variables);
    auto parameter = [&](unsigned id) {
      m.WriteU8(code, id);
      m.WriteU8(code + 1, id >> 8);
      s.r[3] = owner;
      s.r[4] = 0;
      s.r[5] = 0;
      (void)battle_script_parameters61::Apply(0x8238be38, m, {guest, native},
                                              s);
      return Address(s.r[3]);
    };
    m.WriteU32(locals + 4, 11);
    m.WriteU32(globals + 4 * (2050 + 40668), 22);
    m.WriteU32(variables + 4, 33);
    m.WriteU32(globals + 4 * (44764 + 1), 8);
    m.WriteU32(constants + 4, 44);
    if (parameter(1) != 11 || parameter(2050) != 22 || parameter(4097) != 33 ||
        parameter(6179) != 1 || parameter(0x8001) != 44)
      throw std::runtime_error("script parameter storage classes");
    m.WriteU32(resource + 2592, std::bit_cast<unsigned>(-3.75f));
    m.WriteU32(resource + 4880, 55);
    m.WriteU32(resource + 64, 42);
    if (parameter(32514) != unsigned(-3) || parameter(32518) != 55 ||
        parameter(32520) != 0)
      throw std::runtime_error("actor parameter fields");
    constexpr unsigned flags = 0x6b000, actions = 0x6c000;
    m.WriteU32(script + 28, 0x40000);
    m.WriteU32(script + 16, 2);
    m.WriteU32(script + 16724, 3);
    m.WriteU32(script + 16728, flags);
    m.WriteU32(script + 16736, actions);
    m.WriteU32(flags, 1);
    m.WriteU32(actions, 77);
    for (unsigned i = 0; i < 3; ++i) {
      s.r[3] = owner;
      (void)battle_script_parameters61::Apply(0x8238b4a8, m, {guest, native},
                                              s);
    }
    if (guest.events != 1 || m.ReadU32(script + 16748) != 1 ||
        (m.ReadU32(script + 28) & 0xc0000) != 0x80000)
      throw std::runtime_error("periodic threshold and end marker");
    m.WriteU32(0x832cb550 + 4, 0x6d000);
    m.WriteU32(0x832cb550 + 8, 1);
    m.WriteU32(0x6d000, 0x6e000);
    m.WriteU32(0x6e000 + 552, 42);
    m.WriteU32(0x6e000 + 516, 0x6f000);
    m.WriteU32(0x6f000 + 444, 0x80000000);
    m.WriteU32(script + 196, 2);
    m.WriteU32(script + 68, 0);
    m.WriteU8(script + 73, 1);
    m.WriteU32(script + 76, 2);
    s.r[3] = owner;
    s.r[4] = resource;
    (void)battle_script_parameters61::Apply(0x8238b5a0, m, {guest, native}, s);
    if (m.ReadU32(0x6f000 + 444) != 0x10000000 || m.ReadU32(script + 196))
      throw std::runtime_error("queued actor flag application");
    std::puts("PASS parameter classes, periodic actions and actor flag queue");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
