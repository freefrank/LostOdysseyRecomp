#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_scene_state61.h"
#include <iostream>
struct SceneStateGuest final : manager_release_context61::GuestServices {
  unsigned commands = 0, texts = 0, randoms = 0, virtuals = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x82380d30) {
      s.r[3] = 0x71000;
      return;
    }
    if (e == 0x82aad100 || e == 0x82ad9af0 || e == 0x82adc1c8 ||
        e == 0x82ae1498) {
      ++commands;
      return;
    }
    if (e == 0x82ab8870 || e == 0x82ad6ab0) {
      if (m.ReadU16(s.r[4]) != 0x3042 || m.ReadU16(s.r[4] + 64))
        throw std::runtime_error("inline scene text");
      ++texts;
      s.r[3] = 17;
      return;
    }
    if (e == 0x82ac1af0) {
      s.r[3] = s.r[5];
      return;
    }
    if (e == 0x82aa0740) {
      if (s.r[5] != 1 || s.r[6] != 89 || s.r[7] != 24)
        throw std::runtime_error("eligible action random contract");
      s.r[3] = 1;
      ++randoms;
      return;
    }
    if (e == 0x8229dfd8) {
      s.r[3] = 0x90000;
      return;
    }
    if (e == 0x82af7ff0 || e == 0x82b2c590) {
      ++commands;
      return;
    }
    throw std::runtime_error("scene state direct boundary");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e != 0x2000 && e != 0x2010 && e != 0x2020)
      throw std::runtime_error("scene state virtual boundary");
    ++virtuals;
    s.r[3] = e == 0x2000 ? 0x91000 : 1;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x832c9000, 0x4000});
    regions.push_back({0x83264000, 0x1000});
    regions.push_back({0x83291000, 0x1000});
    regions.push_back({0x83315000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    SceneStateGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       code = 0x64000, vars = 0x65000, resource = 0x80000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(actor + 4, resource);
    m.WriteU32(0x832c9c54 + 44, state);
    auto le = [&](unsigned p, unsigned x) {
      m.WriteU8(p, x);
      m.WriteU8(p + 1, x >> 8);
    };
    auto op = [&](unsigned e) {
      s.r[3] = owner;
      m.WriteU32(actor + 52, 0);
      (void)battle_script_scene_state61::Apply(e, m, {guest, native}, s);
      if (s.r[1] != initial.r[1] || s.r[21] != initial.r[21] ||
          s.r[31] != initial.r[31])
        throw std::runtime_error("scene state ABI");
    };
    auto text = [&](unsigned offset) {
      for (unsigned i = 0; i < 64; ++i)
        m.WriteU8(code + offset + i, 0);
      m.WriteU8(code + offset, 0x30);
      m.WriteU8(code + offset + 1, 0x42);
    };
    m.WriteU32(vars, 24);
    m.WriteU32(vars + 4, 7);
    le(code + 2, 0);
    le(code + 4, 1);
    for (unsigned mode : {1u, 2u, 4u}) {
      m.WriteU8(code + 1, mode);
      op(0x82b00508);
    }
    if (guest.commands != 3)
      throw std::runtime_error("scene command branches");
    m.WriteU8(code + 1, 1);
    text(3);
    op(0x82afce18);
    if (m.ReadU16(state + 17264) != 0x3042 || m.ReadU16(state + 17264 + 64) ||
        m.ReadU32(actor + 52) != 67)
      throw std::runtime_error("script text buffer");
    text(1);
    op(0x82afc6d8);
    if (m.ReadU16(actor + 340) != 0x3042 || guest.texts != 1)
      throw std::runtime_error("actor label update");
    le(code + 1, 0);
    le(code + 3, 1);
    le(code + 5, 2);
    le(code + 7, 3);
    op(0x82afb570);
    m.WriteU32(vars, 2);
    m.WriteU32(resource + 64, 24);
    m.WriteU32(resource + 2616, std::bit_cast<unsigned>(20.f));
    m.WriteU32(resource + 10744, 0x80000000);
    m.WriteU32(resource + 10748, 0x80000000);
    m.WriteU32(resource + 10752, 0x80000000);
    m.WriteU32(0x83264978 + 12, 0x100000);
    m.WriteU32(0x100000 + 50 * 96 + 16, 10);
    m.WriteU32(0x100000 + 51 * 96 + 16, 20);
    m.WriteU32(0x100000 + 52 * 96 + 16, 30);
    op(0x82afa5f0);
    if (m.ReadU32(vars + 8) != 7 || m.ReadU32(vars + 12) != 51 ||
        guest.randoms != 1)
      throw std::runtime_error("affordable action selection");
    m.WriteU32(vars, 10);
    op(0x82af9920);
    if ((m.ReadU32(0x70000 + 148) & 0x780) != (10 << 7))
      throw std::runtime_error("manager packed mode");
    m.WriteU8(code + 1, 1);
    op(0x82af9a30);
    if (!(m.ReadU32(actor + 64) & 0x200000))
      throw std::runtime_error("actor bit21");
    le(code + 2, 0);
    for (unsigned mode = 0; mode < 4; ++mode) {
      m.WriteU8(code + 1, mode);
      op(0x82afa850);
      auto p = state + 68 + 8 * mode;
      if (m.ReadU32(p) != 10 || m.ReadU8(p + 4) != (mode == 0 || mode == 1) ||
          m.ReadU8(p + 5) != (mode == 0 || mode == 2))
        throw std::runtime_error("queued flag command");
    }
    if (m.ReadU32(state + 196) != 4)
      throw std::runtime_error("flag queue count");
    m.WriteU32(0x83315fb4, 0x92000);
    m.WriteU32(0x92000, 0x93000);
    m.WriteU32(0x93000 + 352, 0x2000);
    le(code + 1, 0);
    m.WriteU32(vars, 2);
    m.WriteU32(0x90000 + 128, 5);
    op(0x82b00398);
    if (m.ReadU32(0x90000 + 128))
      throw std::runtime_error("scene request buffer reset");
    m.WriteU32(0x832cc05c + 112, 0x94000);
    m.WriteU32(0x94000, 0x95000);
    m.WriteU32(0x95000 + 12, 0x2010);
    m.WriteU32(0x95000 + 16, 0x2020);
    m.WriteU32(0x94000 + 4, 3);
    op(0x82afa980);
    if (m.ReadU32(actor + 52) != 3)
      throw std::runtime_error("scene object state branch");
    op(0x82afaa08);
    m.WriteU8(code + 1, 1);
    op(0x82afaa68);
    text(1);
    op(0x82b00428);
    if (m.ReadU32(0x94000 + 220) != 17 || guest.texts != 2)
      throw std::runtime_error("resolved text state");
    m.WriteU32(state + 28, 0x20000);
    op(0x82b00428);
    if (guest.texts != 2)
      throw std::runtime_error("text suppression gate");
    m.WriteU32(state + 28, 0);
    le(code + 1, 0);
    text(3);
    op(0x82afab10);
    if (m.ReadU32(actor + 52) != 67)
      throw std::runtime_error("unused inline payload cursor");
    le(code + 1, 0);
    op(0x82afabb0);
    if (m.ReadU32(vars) != 1)
      throw std::runtime_error("object predicate result");
    std::cout << "battle_script_scene_state61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
