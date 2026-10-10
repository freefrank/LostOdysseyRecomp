#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_effect_mutation61.h"
#include "lo_semantics/battle_semantic_runtime61.h"
struct MutationGuest final : manager_release_context61::GuestServices {
  unsigned dispatches = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (battle_semantic_runtime61::Apply(e, m, {*this, cook_main_smoke::native},
                                         s))
      return;
    std::fprintf(stderr, "mutation direct %08x\n", e);
    throw std::runtime_error("mutation service");
  }
  void CallIndirect(GuestAddress e, GuestMemory &m,
                    manager_release_context61::Registers &s) override {
    if (e == 0x123450) {
      ++dispatches;
      m.WriteU8(unsigned(s.r[3]) + 208, 4);
      return;
    }
    if (e == 0x123454) {
      s.r[3] = 0;
      return;
    }
    throw std::runtime_error("mutation indirect service");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (unsigned p :
         {0x83213000u, 0x83245000u, 0x832cb000u, 0x832ae000u, 0x83264000u})
      regions.push_back({p, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    MutationGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, source = 0x80000, target = 0x90000,
                       result = 0x74000, record = 0x100000, gauge = 0x73000;
    auto put = [&](unsigned p, float f) {
      m.WriteU32(p, std::bit_cast<unsigned>(f));
    };
    auto get = [&](unsigned p) { return std::bit_cast<float>(m.ReadU32(p)); };
    auto check = [&](bool b, const char *msg) {
      if (!b)
        throw std::runtime_error(msg);
    };
    auto call = [&](unsigned entry) {
      s.r[3] = owner;
      check(battle_effect_mutation61::Apply(entry, m, {guest, native}, s),
            "mutation entry");
      check(s.r[1] == initial.r[1] && s.r[28] == initial.r[28] &&
                s.r[31] == initial.r[31],
            "mutation nonvolatile state");
    };
    for (unsigned i = 0; i < 32; ++i) {
      m.WriteU32(0x83213438 + 8 * i, 0);
      m.WriteU32(0x8321343c + 8 * i, 1u << i);
    }
    m.WriteU32(0x832cb790, result);
    m.WriteU32(result + 20, record);
    m.WriteU32(0x8324570c, 0x75000);
    m.WriteU32(0x832aeb00, gauge);
    m.WriteU32(owner + 4, source);
    m.WriteU32(owner + 8, target);
    m.WriteU32(source + 124, 0x40000000);
    m.WriteU32(target + 124, 0x50000000);
    m.WriteU32(target + 132, 1);
    m.WriteU32(target, 0x76000);
    for (unsigned off : {292u, 380u, 384u})
      m.WriteU32(0x76000 + off, 0x123454);
    put(0x82007784, 1);
    put(0x82000e50, 0);
    put(0x8201f9f0, .5f);
    put(0x82000b3c, .25f);
    put(0x82000d7c, .01f);
    m.WriteU32(owner + 108, 0);
    m.WriteU32(owner + 860, 0x123450);
    call(0x82b11660);
    check(guest.dispatches == 1 && m.ReadU8(owner + 208) == 4,
          "indexed tail effect dispatch");
    m.WriteU32(owner + 108, 7);
    call(0x82b11660);
    check(!m.ReadU8(owner + 208), "tail effect range gate");
    m.WriteU8(owner + 200, 1);
    m.WriteU32(owner + 112, 0);
    m.WriteU32(owner + 776, 0x123450);
    m.WriteU8(owner + 202, 1);
    call(0x82b0d378);
    check(guest.dispatches == 2 && !m.ReadU8(owner + 202),
          "eligible nested effect dispatch");
    m.WriteU32(owner + 112, 21);
    m.WriteU8(owner + 208, 1);
    call(0x82b0d378);
    check(!m.ReadU8(owner + 208), "nested effect range gate");
    m.WriteU8(owner + 200, 0);
    m.WriteU32(owner + 92, 0);
    m.WriteU32(owner + 116, 0);
    m.WriteU8(owner + 208, 1);
    call(0x82b0fcf0);
    check(!m.ReadU8(owner + 208), "side eligibility rejection");
    m.WriteU8(owner + 208, 1);
    call(0x82b0ea88);
    check(!m.ReadU8(owner + 208), "property clear rejection");
    m.WriteU8(owner + 200, 1);
    m.WriteU32(owner + 92, 5);
    m.WriteU32(owner + 100, 4);
    m.WriteU32(owner + 96, 6);
    m.WriteU32(owner + 104, 8);
    m.WriteU32(target + 6 * 272 + 232, 8);
    call(0x82b0fcf0);
    check((m.ReadU32(target + 5 * 272 + 232) & 4) &&
              !(m.ReadU32(target + 6 * 272 + 232) & 8) &&
              m.ReadU32(record + 14888) == 1,
          "property insertion and paired removal");
    call(0x82b0ea88);
    check(!(m.ReadU32(target + 5 * 272 + 232) & 4), "property clear callback");
    m.WriteU32(owner + 100, 32);
    put(owner + 80, -12.9f);
    call(0x82b10548);
    check(m.ReadU32(target + 4 * (68 * 5 + 5 + 59)) == unsigned(-12),
          "numeric property payload truncation");
    m.WriteU32(owner + 184, 1);
    call(0x82b104a8);
    check(m.ReadU32(source + 124) & 0x40000,
          "source flag before failed chance");
    m.WriteU32(owner + 184, 0);
    m.WriteU8(owner + 77, 1);
    put(owner + 32, 12);
    put(target + 2588, 100);
    put(target + 2592, 100);
    m.WriteU32(owner + 108, 40);
    call(0x82b0ddf8);
    check(get(target + 2588) == 40 && get(record + 14904) == 60 &&
              get(owner + 172) == 12,
          "fixed HP cap and recorded delta");
    m.WriteU32(target + 232, 2);
    put(target + 2588, 100);
    call(0x82b0dd20);
    check(get(target + 2588) == 25 && get(record + 14904) == 75,
          "quarter HP cap and recorded delta");
    m.WriteU8(gauge + 48, 1);
    m.WriteU32(target + 188, 30);
    m.WriteU32(owner + 120, 25);
    call(0x82b10dd8);
    check(m.ReadU32(target + 188) == 55, "percent gauge addition");
    m.WriteU32(owner + 120, 0);
    call(0x82b10dd8);
    check(!m.ReadU32(target + 188), "zero gauge payload resets current");
    std::puts("PASS effect dispatch, eligibility, property payloads, HP cap "
              "results and gauge changes");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
