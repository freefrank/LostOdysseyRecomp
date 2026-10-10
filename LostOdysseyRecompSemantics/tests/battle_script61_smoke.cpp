#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/cpx_decode61.h"
#include "lo_semantics/cpx_context61.h"
#include "lo_semantics/battle_script61.h"
#include <map>
struct ContextGuest final : manager_release_context61::GuestServices {
  unsigned steps = 0, callbacks = 0, waitParameters = 0;
  unsigned next = 0x90000, initializations = 0, allocations = 0;
  std::map<unsigned, unsigned> live;
  unsigned Allocate(unsigned size) {
    auto p = next;
    next += (size + 15) & ~15u;
    live[p] = size;
    ++allocations;
    return p;
  }
  void Free(unsigned p) {
    if (!live.erase(p))
      throw std::runtime_error("CPX unowned release");
  }
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x8238be38) {
      if (s.r[4] != 1 || s.r[5] != 0)
        throw std::runtime_error("wait parameter args");
      ++waitParameters;
      s.r[3] = 2;
      return;
    }
    if (e == 0x8238ae08) {
      if (s.lr != 0x8238ad64 || std::bit_cast<double>(s.fpr_bits[31]) <= 0)
        throw std::runtime_error("timer hook LR/delta");
      ++steps;
      return;
    }
    if (e == 0x8238b4a8 || e == 0x8238b5a0 || e == 0x8238b850 ||
        e == 0x8238b900 || e == 0x8238c708) {
      ++callbacks;
      return;
    }
    if (e == 0x827c5f38) {
      ++initializations;
      m.WriteU32(0x8330b608, 0x70000);
      m.WriteU32(0x70000, 0x71000);
      m.WriteU32(0x71004, 0x1004);
      m.WriteU32(0x7100c, 0x100c);
      s.r[4] = 0xbad;
      s.r[5] = 0xbad;
      return;
    }
    if (e == 0x82486c88) {
      s.r[3] = Allocate(Address(s.r[3]));
      return;
    }
    if (e == 0x823f3340) {
      Free(Address(s.r[3]));
      return;
    }
    throw std::runtime_error("unexpected CPX direct call");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (e == 0x1004) {
      if (s.r[5] != 8)
        throw std::runtime_error("CPX alignment");
      s.r[3] = Allocate(Address(s.r[4]));
      return;
    }
    if (e == 0x100c) {
      Free(Address(s.r[4]));
      return;
    }
    throw std::runtime_error("unexpected CPX indirect call");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x8330b000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    ContextGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000;
    m.WriteU32(0x82000da8, std::bit_cast<unsigned>(60.0f));
    m.WriteU32(0x82000e50, 0);
    s.r[3] = owner;
    s.r[4] = 2;
    (void)battle_script61::Apply(0x82a9dbb8, m, {guest, native}, s);
    auto script = m.ReadU32(owner + 44), actors = m.ReadU32(script + 4);
    if (guest.live.size() != 4 || guest.live.at(script) != 17780 ||
        guest.live.at(actors) != 944 || m.ReadU32(script + 17776) != 0xffffffff)
      throw std::runtime_error("script initial allocations");
    s.r[3] = owner;
    (void)battle_script61::Apply(0x82a9e3b8, m, {guest, native}, s);
    if (s.r[3] != 0 || guest.live.size() != 4)
      throw std::runtime_error("inactive release refusal");
    m.WriteU32(script + 28, 0x40000000);
    m.WriteU8(owner + 4, 1);
    auto update = [&](double delta) {
      s.r[3] = owner;
      s.fpr_bits[1] = std::bit_cast<std::uint64_t>(delta);
      (void)battle_script61::Apply(0x8238acc8, m, {guest, native}, s);
    };
    update(1.0 / 120);
    if (!m.ReadU32(owner + 8) || guest.steps)
      throw std::runtime_error("first update gate");
    update(1.0 / 120);
    if (m.ReadU32(script + 16) != 0 || guest.steps != 1)
      throw std::runtime_error("original high-FPS integer tick truncation");
    update(1.0 / 30);
    if (m.ReadU32(script + 16) != 2 || guest.steps != 2)
      throw std::runtime_error("integer tick step");
    m.WriteU32(owner + 24, actors);
    auto records = guest.Allocate(24);
    m.WriteU32(actors + 40, records);
    m.WriteU32(records + 8, 0xffffffff);
    auto variables = guest.Allocate(4), bytecode = guest.Allocate(4);
    m.WriteU32(variables, 2);
    m.WriteU32(actors + 12, variables);
    m.WriteU32(actors + 36, bytecode);
    auto wait = [&]() {
      s.r[3] = owner;
      (void)battle_script61::Apply(0x82a9bf40, m, {guest, native}, s);
    };
    wait();
    if (m.ReadU32(records + 8) != 2 || !(m.ReadU32(script + 28) & 0x80000000) ||
        guest.waitParameters != 0)
      throw std::runtime_error("wait parameter initialization");
    m.WriteU32(script + 16, 1);
    wait();
    wait();
    if (m.ReadU32(records + 8) != 0 || m.ReadU32(actors + 52))
      throw std::runtime_error("zero wait still blocks");
    wait();
    if (m.ReadU32(actors + 52) != 3 || m.ReadU32(records + 8) != 0xffffffff)
      throw std::runtime_error("wait negative advances opcode");
    m.WriteU32(script + 12, 2);
    m.WriteU32(actors + 472 + 64, 0x80000000);
    auto calls = guest.callbacks;
    update(1.0 / 60);
    if (guest.callbacks - calls != 4 || m.ReadU32(owner + 24) != actors + 472)
      throw std::runtime_error("actor update scheduling");
    s.r[3] = owner;
    (void)battle_script61::Apply(0x82a9e3b8, m, {guest, native}, s);
    if (s.r[3] != 1 || m.ReadU32(owner + 44) || !guest.live.empty() ||
        s.r[1] != initial.r[1] || s.fpr_bits[31] != initial.fpr_bits[31])
      throw std::runtime_error("script release and ABI");
    std::puts(
        "PASS battle script lifetime, integer tick update and timed wait");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
