#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_effect_mutation61.h"
#include "lo_semantics/battle_semantic_runtime61.h"
#include "lo_semantics/recovery_abi.h"
#include "battle_resource_growth_fixture.h"
#include "battle_profile_fixture.h"
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
    if (profile_fixture::Indirect(e, s))
      return;
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
    regions.push_back({0x832ca000, 0x1000});
    regions.push_back({0x831f3000, 0x21000});
    growth_fixture::Regions(regions);
    regions.push_back({0x832c9000, 0x1000});
    regions.push_back({0x83291000, 0x1000});
    regions.push_back({0x83315000, 0x1000});
    regions.push_back({0x832c1000, 0x1000});
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
    m.WriteU8(owner + 200, 1);
    m.WriteU32(owner + 92, 5);
    m.WriteU32(owner + 100, 4);
    m.WriteU32(owner + 96, 6);
    m.WriteU32(owner + 104, 8);
    m.WriteU32(owner + 108, 7);
    m.WriteU32(owner + 112, 9);
    m.WriteU32(owner + 120, 1);
    call(0x82b0ec18);
    check((m.ReadU32(target + 5 * 272 + 232) & 4) &&
              (m.ReadU32(target + 6 * 272 + 232) & 8) &&
              m.ReadU32(target + 4 * (68 * 5 + 2 + 59)) == 7 &&
              m.ReadU32(target + 4 * (68 * 6 + 3 + 59)) == 9,
          "paired property payloads");
    call(0x82b0ee40);
    check(!(m.ReadU32(target + 5 * 272 + 232) & 4) &&
              !(m.ReadU32(target + 6 * 272 + 232) & 8),
          "paired property clear");
    m.WriteU32(target + 5 * 272 + 232, 8);
    call(0x82b0f7d0);
    check((m.ReadU32(target + 6 * 272 + 232) & 8) &&
              !(m.ReadU32(target + 5 * 272 + 232) & 8),
          "exclusive property category");
    m.WriteU32(source + 64, 24);
    m.WriteU32(owner + 108, 1);
    m.WriteU32(owner + 100, 16);
    call(0x82b11758);
    check(m.ReadU8(owner + 208) == 1 &&
              m.ReadU32(target + 4 * (68 * 5 + 4 + 59)) == 1 &&
              m.ReadU32(target + 4 * (68 * 5 + 4 + 91)) == 24,
          "random bounded payload and source attribution");
    for (unsigned bank : {1u, 5u, 7u})
      m.WriteU32(target + 272 * bank + 232, 0x7fffffff);
    call(0x82b0fdb0);
    check((m.ReadU32(target + 272 + 232) & 127) == 0 &&
              (m.ReadU32(target + 5 * 272 + 232) & 63) == 0 &&
              (m.ReadU32(target + 7 * 272 + 232) & 49) == 0,
          "selected property family clear");
    m.WriteU8(owner + 200, 0);
    m.WriteU32(owner + 92, 1);
    m.WriteU8(owner + 208, 1);
    call(0x82b0ec18);
    check(!m.ReadU8(owner + 208), "primary property side gate");
    m.WriteU32(owner + 92, 2);
    m.WriteU8(owner + 208, 1);
    call(0x82b0ee40);
    check(!m.ReadU8(owner + 208), "inverse property side gate");
    m.WriteU8(owner + 200, 1);
    m.WriteU8(owner + 77, 1);
    m.WriteU32(owner + 116, 0);
    m.WriteU32(owner + 92, 5);
    m.WriteU32(owner + 96, 6);
    m.WriteU32(owner + 100, 0);
    m.WriteU32(owner + 104, 0);
    for (unsigned bank : {1u, 3u, 4u, 5u, 7u})
      m.WriteU32(target + 272 * bank + 232, 0);
    m.WriteU32(source + 3 * 272 + 232, 0);
    put(0x82000e1c, 2);
    put(0x822184dc, 9999);
    put(owner + 80, 10);
    put(target + 2588, 40);
    put(target + 2592, 100);
    put(target + 2616, 20);
    put(target + 2620, 100);
    call(0x82b0a568);
    check(get(target + 2588) == 50 && get(record + 14920) == 10,
          "HP restoration pipeline");
    call(0x82b0a698);
    check(get(target + 2616) == 30 && get(record + 14952) == 10,
          "MP restoration pipeline");
    m.WriteU32(owner + 120, 7);
    call(0x82b0a7a0);
    check(get(target + 2588) == 60 && get(target + 2616) == 37 &&
              get(record + 14920) == 10 && get(record + 14952) == 7,
          "combined HP MP restoration");
    m.WriteU32(owner + 100, 16);
    m.WriteU32(owner + 108, 3);
    m.WriteU32(owner + 112, 4);
    m.WriteU32(owner + 120, 1);
    call(0x82b0a928);
    check(get(target + 2588) == 70 &&
              m.ReadU32(target + 4 * (68 * 5 + 4 + 59)) == 3,
          "HP restoration with paired property payload");
    put(owner + 80, 9999);
    call(0x82b0a568);
    check(get(target + 2588) == 100, "full HP sentinel shortcut");
    m.WriteU8(owner + 200, 0);
    m.WriteU32(owner + 116, 0);
    m.WriteU8(owner + 208, 1);
    call(0x82b0a698);
    check(!m.ReadU8(owner + 208), "restoration side gate");
    m.WriteU8(owner + 200, 1);
    m.WriteU32(owner + 184, 0);
    m.WriteU32(owner + 108, 2);
    m.WriteU32(target + 4876, 0);
    put(target + 2588, 100);
    put(target + 2592, 100);
    call(0x82b0c880);
    check(get(target + 2588) == 50 && get(owner + 32) == 50 &&
              get(owner + 172) == 50 && get(record + 14904) == 50,
          "fractional HP reduction and reporting");
    m.WriteU32(target + 4876, 1);
    call(0x82b0c880);
    check(get(target + 2588) == 50, "fractional HP passive immunity");
    m.WriteU32(target + 4876, 0);
    m.WriteU32(source + 124, 0x50000000);
    m.WriteU32(source + 132, 1);
    put(source + 2588, 100);
    put(source + 2592, 100);
    put(source + 2616, 10);
    put(source + 2620, 100);
    m.WriteU32(owner + 100, 0);
    m.WriteU32(owner + 92, 5);
    m.WriteU8(owner + 203, 0);
    call(0x82b0af88);
    check(get(source + 2588) == 50 && get(source + 2616) == 60 &&
              get(record + 104) == 50 && get(record + 56) == 50,
          "HP to MP conversion");
    m.WriteU32(0x832cb788, 0x70000);
    m.WriteU32(0x832ca0e8 + 20, 0x71000);
    m.WriteU32(0x71000, 0x72000);
    m.WriteU32(0x71004, 3);
    m.WriteU32(0x72000, source);
    m.WriteU32(0x72004, target);
    m.WriteU32(0x72008, 0xa0000);
    m.WriteU32(source + 64, 24);
    m.WriteU32(target + 64, 25);
    m.WriteU32(0xa0000 + 64, 0);
    m.WriteU32(0x83264558, 0xb0000);
    m.WriteU32(owner + 36, 0);
    m.WriteU32(owner + 40, record);
    m.WriteU32(record + 20, 2);
    m.WriteU32(record + 14884, 24);
    m.WriteU32(record + 14884 + 464, 25);
    call(0x82b110b8);
    check(!(m.ReadU32(source + 124) & 0x40000000) &&
              !(m.ReadU32(target + 124) & 0x40000000) &&
              (m.ReadU32(0xa0000 + 124) & 0x40000000),
          "random class flags and source ID fallback");
    growth_fixture::Setup(m);
    m.WriteU32(result + 20, 0x200000);
    m.WriteU32(0x832c9c54 + 44, 0xc0000);
    m.WriteU32(0xc0000 + 4, 0xc1000);
    m.WriteU32(0xc0000 + 12, 1);
    m.WriteU32(0xc1000 + 8, 24);
    m.WriteU32(0xc1000 + 316, 7);
    m.WriteU32(0x83291dc0, 0xd0000);
    m.WriteU32(source + 124, 0x40000000);
    m.WriteU32(source + 68, 2);
    m.WriteU32(source + 140, 9);
    m.WriteU32(source + 4952, 2);
    m.WriteU32(target + 4876, 1);
    m.WriteU32(owner + 100, 8);
    m.WriteU32(owner + 92, 0);
    m.WriteU32(owner + 168, 99);
    m.WriteU32(0x140000 + 280, 7);
    call(0x82b10368);
    check(m.ReadU32(source + 4952) == 3 && get(source + 2588) == 100 &&
              get(source + 2592) == 100 && m.ReadU32(owner + 168) == 3 &&
              !(m.ReadU32(target + 232) & 1),
          "growth effect refresh, heal and immune target");
    profile_fixture::Setup(m, 0x300000);
    m.WriteU32(0x93000 + 76, 30);
    m.WriteU32(0x93000 + 185200, 100);
    m.WriteU32(owner + 120, 20);
    m.WriteU8(owner + 77, 1);
    m.WriteU8(owner + 203, 0);
    put(owner + 80, 10);
    put(target + 2588, 40);
    put(target + 2592, 100);
    call(0x82b13220);
    check(get(target + 2588) == 50 && get(0x200000 + 72) == 10 &&
              m.ReadU32(0x93000 + 76) == 10 &&
              m.ReadU32(0x93000 + 185200) == 120,
          "priced healing and profile cost");
    call(0x82b13220);
    check(get(target + 2588) == 60 && m.ReadU32(0x93000 + 76) == 0 &&
              m.ReadU32(0x93000 + 185200) == 130,
          "priced healing cost capped by balance");
    m.WriteU32(0x832ca0e8 + 156, 2);
    m.WriteU32(target + 124, 0x10000000);
    m.WriteU8(gauge + 48, 1);
    put(gauge + 36, .25f);
    put(0x82000fb0, 1);
    m.WriteU32(target + 2136, 1);
    m.WriteU32(target + 2140, 5);
    m.WriteU32(owner + 184, 0);
    call(0x82b0db98);
    check(get(target + 2588) == 50 && get(owner + 172) == 10 &&
              get(0x200000 + 14968) == 5 && get(0x200000 + 14904) == 10,
          "manager-scaled damage, gauge reduction and shield absorption");
    m.WriteU32(owner + 184, 1);
    call(0x82b0db98);
    check(get(target + 2588) == 50 && get(owner + 32) == 0,
          "blocked damage mode");
    m.WriteU32(owner + 120, 7);
    m.WriteU32(owner + 92, 1);
    m.WriteU32(owner + 100, 2);
    m.WriteU32(owner + 96, 2);
    m.WriteU32(owner + 104, 4);
    m.WriteU32(target + 272 + 232, 2);
    m.WriteU32(target + 544 + 232, 4);
    put(target + 2616, 10);
    put(target + 2620, 100);
    call(0x82b0aa70);
    check(get(target + 2588) == 60 && get(target + 2616) == 17 &&
              !m.ReadU32(target + 272 + 232) && !m.ReadU32(target + 544 + 232),
          "combined restoration and dual property clearing");
    m.WriteU32(owner + 120, 0);
    call(0x82b0aa70);
    check(get(target + 2588) == 70 && get(target + 2616) == 17 &&
              s.fpr_bits[31] == initial.fpr_bits[31],
          "optional MP restoration and FPR save");
    m.WriteU32(owner + 92, 0);
    m.WriteU32(owner + 132, 3);
    m.WriteU32(owner + 136, 2);
    for (unsigned i = 1; i < 4; ++i)
      m.WriteU32(owner + 132 + 8 * i, 255);
    m.WriteU32(target + 3 * 272 + 232, 6);
    m.WriteU32(target + 4 * (68 * 3 + 1 + 59), 123);
    m.WriteU32(target + 4 * (68 * 3 + 1 + 91), 456);
    call(0x82b0f920);
    check(m.ReadU32(target + 3 * 272 + 232) == 4 &&
              !m.ReadU32(target + 4 * (68 * 3 + 1 + 59)) &&
              !m.ReadU32(target + 4 * (68 * 3 + 1 + 91)),
          "random ordinal property clearing");
    call(0x82b0f920);
    check(m.ReadU32(target + 3 * 272 + 232) == 4 &&
              !m.ReadU32(0x200000 + 14888),
          "empty eligible property selection cancels mark");
    m.WriteU32(0x832ca0e8 + 48, 0xde000);
    m.WriteU32(0xde000, 0xe0000);
    m.WriteU32(0xe0000, 0xe1000);
    m.WriteU32(0xe0004, 0xf1000);
    m.WriteU32(owner + 108, 0);
    m.WriteU32(owner + 124, 9);
    m.WriteU32(owner + 92, 8);
    m.WriteU32(owner + 100, 4);
    m.WriteU32(owner + 120, 7);
    auto shared = 0xe1000 + 72;
    m.WriteU32(shared + 12288, 0);
    m.WriteU32(shared + 4 * (3073 + 2), 99);
    call(0x82b10e98);
    check(m.ReadU32(source + 4880) == 9 && m.ReadU32(shared + 12288) == 4 &&
              m.ReadU32(shared + 4 * (3073 + 2)) == 7,
          "shared party property assignment");
    m.WriteU32(owner + 108, 1);
    m.WriteU32(target + 6 * 272 + 232, 0x3c0);
    for (unsigned i = 6; i < 10; ++i)
      m.WriteU32(target + 4 * (467 + i), 99);
    call(0x82b10e98);
    check(m.ReadU32(target + 4880) == 1 &&
              m.ReadU32(target + 6 * 272 + 232) == 64 &&
              m.ReadU32(target + 4 * 473) == 5 &&
              m.ReadU32(target + 4 * 474) == 0,
          "random target category and duration reset");
    put(owner + 80, 2);
    m.WriteU32(owner + 108, 0);
    put(source + 2616, 60);
    put(target + 2616, 10);
    call(0x82b0ad38);
    check(get(source + 2616) == 30 && get(target + 2616) == 40 &&
              get(0x200000 + 88) == 30 && get(0x200000 + 14952) == 30,
          "source to target MP transfer");
    m.WriteU32(owner + 108, 1);
    put(source + 2588, 100);
    put(target + 2588, 20);
    call(0x82b0ad38);
    check(get(source + 2588) == 50 && get(target + 2588) == 70 &&
              get(0x200000 + 56) == 50 && get(0x200000 + 14920) == 50,
          "source to target HP transfer");
    m.WriteU32(target + 64, m.ReadU32(source + 64));
    call(0x82b0ad38);
    check(get(source + 2588) == 50 && get(target + 2588) == 70,
          "same ID transfer skipped");
    m.WriteU32(owner + 184, 0);
    put(owner + 88, 100);
    m.WriteU32(owner + 20, 3);
    m.WriteU32(owner + 24, 18);
    m.WriteU32(owner + 92, 3);
    m.WriteU32(owner + 100, 6);
    m.WriteU32(owner + 96, 4);
    m.WriteU32(owner + 104, 16);
    m.WriteU32(target + 3 * 272 + 232, 0);
    m.WriteU32(target + 4 * 272 + 232, 0);
    call(0x82b0e798);
    check(m.ReadU32(target + 3 * 272 + 232) == 2 &&
              m.ReadU32(target + 4 * 272 + 232) == 16 &&
              (m.ReadU32(target + 124) & 4096),
          "random primary property and secondary application");
    m.WriteU32(target + 3 * 272 + 232, 0);
    call(0x82b0e798);
    check(!m.ReadU32(target + 3 * 272 + 232), "single-use effect target flag");
    m.WriteU32(owner + 92, 3);
    m.WriteU32(owner + 100, 6);
    m.WriteU32(owner + 96, 4);
    m.WriteU32(owner + 104, 16);
    m.WriteU32(target + 3 * 272 + 232, 2);
    m.WriteU32(target + 4 * 272 + 232, 16);
    m.WriteU32(target + 4876, 0);
    m.WriteU32(target + 5088, 0);
    call(0x82b0fbd0);
    check(m.ReadU32(target + 3 * 272 + 232) == 4 &&
              !m.ReadU32(target + 4 * 272 + 232),
          "dual property toggle");
    m.WriteU32(target + 4876, 2);
    m.WriteU32(owner + 104, 0);
    call(0x82b0fbd0);
    check(!m.ReadU32(target + 3 * 272 + 232),
          "toggle addition respects immunity");
    m.WriteU32(target + 4876, 128);
    call(0x82b0fbd0);
    check(!m.ReadU32(target + 3 * 272 + 232), "passive property seven gate");
    m.WriteU32(0x832cb778, 183);
    m.WriteU32(0xc1000 + 324, 0);
    m.WriteU32(0x93000 + 8260 + 181104, 3);
    m.WriteU32(0x93000 + 8260 + 181104 + 4, 2);
    m.WriteU32(owner + 120, 0);
    put(owner + 80, 2);
    m.WriteU32(target + 124, 0x50000000);
    m.WriteU32(target + 2136, 0);
    put(target + 2588, 100);
    call(0x82b12a98);
    check(m.ReadU32(0xc1000 + 324) == 1 && get(target + 2588) == 90 &&
              get(owner + 172) == 10,
          "profile aggregate damage and encounter override");
    m.WriteU32(owner + 120, 1);
    m.WriteU32(owner + 116, 5);
    put(owner + 80, 10);
    call(0x82b12a98);
    check(get(target + 2588) == 81 && get(owner + 172) == 9 &&
              s.fpr_bits[31] == initial.fpr_bits[31],
          "profile ratio damage and FPR preservation");
    m.WriteU32(owner + 20, 10);
    m.WriteU32(owner + 24, 166);
    m.WriteU32(owner + 92, 3);
    m.WriteU32(owner + 100, 2);
    m.WriteU32(owner + 108, 4);
    m.WriteU32(owner + 112, 5);
    m.WriteU32(owner + 120, 0);
    m.WriteU32(target + 60, 2);
    m.WriteU32(target + 14656, 0x400000);
    m.WriteU32(target + 14660, 2);
    m.WriteU32(target + 124, m.ReadU32(target + 124) | 512);
    m.WriteU32(target + 76368, 123);
    call(0x82b0f3e0);
    check(m.ReadU32(0x400000) == 13 && m.ReadU32(0x400000 + 124208) == 13 &&
              !m.ReadU32(target + 76368) && !(m.ReadU32(target + 124) & 512) &&
              m.ReadU32(target + 4 * (68 * 3 + 1 + 59)) == 4,
          "property effect rewrites pending action records");
    m.WriteU32(owner + 20, 3);
    m.WriteU32(owner + 24, 19);
    call(0x82b0f3e0);
    m.WriteU32(target + 3 * 272 + 232, 0);
    call(0x82b0f3e0);
    check((m.ReadU32(target + 124) & 2048) &&
              !m.ReadU32(target + 3 * 272 + 232),
          "property action single-use gate");
    m.WriteU32(owner + 20, 0);
    m.WriteU32(owner + 100, 0x2000000);
    m.WriteU32(target + 68, 0);
    m.WriteU32(target + 140, 7);
    call(0x82b0f3e0);
    check(m.ReadU32(target + 4952) == 2 && get(target + 2592) > 0,
          "property-triggered full stats rebuild");
    m.WriteU32(owner + 108, 0);
    m.WriteU32(owner + 120, 6);
    put(owner + 80, 10);
    put(source + 2588, 40);
    put(source + 2592, 100);
    put(source + 2616, 10);
    put(source + 2620, 100);
    put(target + 2588, 100);
    put(target + 2592, 100);
    put(target + 2616, 30);
    put(target + 2620, 100);
    m.WriteU32(target + 2136, 1);
    m.WriteU32(target + 2140, 5);
    call(0x82b10aa8);
    check(get(source + 2588) == 50 && get(target + 2588) == 95 &&
              get(source + 2616) == 16 && get(target + 2616) == 24 &&
              get(owner + 172) == 5,
          "HP MP drain preserves healing before shield absorption");
    m.WriteU32(owner + 108, 1);
    call(0x82b10aa8);
    check(get(source + 2616) == 16 && get(target + 2616) == 18 &&
              s.fpr_bits[30] == initial.fpr_bits[30] &&
              s.fpr_bits[31] == initial.fpr_bits[31],
          "MP-only damage mode and preserved floating registers");
    m.WriteU8(owner + 77, 0);
    m.WriteU32(owner + 184, 0);
    m.WriteU32(source + 5096, 0);
    m.WriteU32(0xb0000 + 4 * (128 * 15 + 112 + 3), 100);
    for (unsigned i = 100; i < 108; ++i)
      m.WriteU32(0x831f3300 + 4 * i, 1);
    m.WriteU32(source + 232, 0);
    m.WriteU32(source + 68, 0);
    m.WriteU32(target + 232, 0);
    m.WriteU32(target + 4880, 0);
    m.WriteU32(target + 4956, 0);
    m.WriteU32(target + 272 + 232, 0);
    m.WriteU32(target + 2136, 0);
    put(source + 2624, 100);
    put(target + 2628, 0);
    put(owner + 80, 20);
    put(owner + 84, 0);
    put(target + 2588, 100);
    call(0x82b0c4e8);
    check(get(target + 2588) == 80 && (m.ReadU32(target + 124) & 0x40000000),
          "physical damage pipeline and class marking");
    m.WriteU32(owner + 184, 3);
    put(target + 2616, 5);
    call(0x82b0c4e8);
    check(get(target + 2616) == 0 && get(target + 2588) == 65 &&
              get(owner + 32) == 15,
          "MP shield spills remaining damage into HP");
    m.WriteU32(owner + 184, 6);
    call(0x82b0c4e8);
    check(get(target + 2588) == 55, "half damage rounds down with minimum");
    m.WriteU32(owner + 184, 2);
    call(0x82b0c4e8);
    check(get(target + 2588) == 75 && get(owner + 32) == 0,
          "damage becomes healing");
    m.WriteU32(owner + 184, 0);
    m.WriteU32(owner + 120, 0);
    put(source + 2588, 50);
    put(target + 2588, 100);
    call(0x82b0c9e0);
    check(get(target + 2588) == 50 && get(owner + 172) == 50,
          "missing source HP damage");
    m.WriteU32(owner + 120, 1);
    m.WriteU32(owner + 116, 2);
    put(source + 2588, 75);
    put(target + 2588, 100);
    call(0x82b0c9e0);
    check(get(target + 2588) == 80,
          "healthy source uses physical damage fallback");
    put(source + 2588, 25);
    put(target + 2588, 100);
    call(0x82b0c9e0);
    check(get(target + 2588) == 25 && get(owner + 172) == 75,
          "low source HP threshold");
    m.WriteU32(source + 140, 10);
    m.WriteU32(source + 4952, 0);
    put(owner + 80, 2);
    m.WriteU32(0xb0000 + 4 * (128 * 15 + 10 + 3), 120);
    m.WriteU32(0x831f3300 + 4 * 120, 5);
    put(target + 2588, 100);
    call(0x82b0d7f0);
    check(get(target + 2588) == 94 && get(owner + 172) == 6,
          "level-scaled bounded random damage");
    m.WriteU8(owner + 200, 0);
    m.WriteU32(target + 124, m.ReadU32(source + 124));
    m.WriteU8(owner + 208, 1);
    call(0x82b0d7f0);
    check(!m.ReadU8(owner + 208) && get(target + 2588) == 94,
          "level damage side gate");
    m.WriteU8(owner + 200, 1);
    m.WriteU32(owner + 108, 2);
    m.WriteU32(source + 124, 0x50000000);
    m.WriteU32(target + 124, 0x40000000);
    m.WriteU32(0xb0000 + 4 * (128 * 15 + 112 + 3), 100);
    put(target + 2588, 100);
    call(0x82b0d418);
    check(get(target + 2588) == 90 && get(owner + 172) == 10,
          "level repeated attack sum");
    m.WriteU32(source + 124, 0x40000000);
    put(0x821baa74, .5f);
    call(0x82b0d418);
    check(get(target + 2588) == 85 && get(owner + 172) == 5,
          "creature level damage coefficient");
    m.WriteU32(owner + 120, 1);
    put(owner + 80, 20);
    put(0x82000da4, .1f);
    put(target + 2588, 100);
    put(target + 2616, 5);
    put(target + 2620, 100);
    put(source + 2616, 10);
    m.WriteU32(0xb0000 + 4 * (128 * 15 + 112 + 3), 100);
    call(0x82b0b178);
    check(get(target + 2588) == 80 && get(target + 2616) == 0 &&
              get(source + 2616) == 15 && get(owner + 172) == 0,
          "physical damage with capped MP siphon");
    m.WriteU32(owner + 120, 0);
    call(0x82b0b178);
    check(get(target + 2588) == 60 && get(owner + 172) == 20,
          "physical damage without MP siphon");
    m.WriteU32(owner + 92, 7);
    m.WriteU32(owner + 100, 4);
    m.WriteU32(owner + 108, 4);
    m.WriteU32(owner + 112, 5);
    m.WriteU32(owner + 120, 7);
    put(owner + 88, 100);
    put(target + 2588, 100);
    call(0x82b0b630);
    check(get(target + 2588) == 80 && get(owner + 172) == 20 &&
              m.ReadU32(target + 4 * (68 * 7 + 2 + 59)) == 4 &&
              m.ReadU32(target + 4 * (68 * 7 + 2 + 91)) == 5,
          "damage followed by paired property");
    m.WriteU32(owner + 92, 0);
    m.WriteU32(owner + 100, 16);
    m.WriteU32(owner + 96, 3);
    m.WriteU32(owner + 104, 8);
    call(0x82b0b630);
    check(get(target + 2588) == 60 && m.ReadU32(owner + 168) == 4 &&
              m.ReadU32(owner + 196) == 24 && (m.ReadU32(target + 232) & 16) &&
              (m.ReadU32(target + 3 * 272 + 232) & 8),
          "damage property mask bookkeeping");
    m.WriteU32(owner + 92, 3);
    m.WriteU32(owner + 104, 0);
    m.WriteU32(owner + 120, 6);
    put(target + 2588, 100);
    put(target + 2616, 30);
    call(0x82b0ba98);
    check(get(target + 2588) == 80 && get(target + 2616) == 24 &&
              get(owner + 172) == 20,
          "physical and randomized MP damage");
    m.WriteU32(owner + 184, 3);
    put(target + 2588, 100);
    put(target + 2616, 5);
    call(0x82b0ba98);
    check(get(target + 2588) == 79 && get(target + 2616) == 0 &&
              get(owner + 172) == 21,
          "combined MP hit spills into HP");
    m.WriteU32(owner + 184, 0);
    m.WriteU32(owner + 92, 3);
    m.WriteU32(owner + 100, 2);
    m.WriteU32(owner + 108, 4);
    m.WriteU32(owner + 96, 4);
    m.WriteU32(owner + 104, 8);
    m.WriteU32(owner + 112, 5);
    m.WriteU32(owner + 120, 0);
    put(owner + 80, 10);
    put(target + 2588, 100);
    call(0x82b0bfd0);
    check(get(target + 2588) == 90 && get(owner + 172) == 10 &&
              m.ReadU32(target + 4 * (68 * 3 + 1 + 59)) == 4 &&
              m.ReadU32(target + 4 * (68 * 4 + 3 + 59)) == 5,
          "bounded damage and dual property values");
    m.WriteU32(owner + 120, 1);
    call(0x82b0bfd0);
    check(get(target + 2588) == 90 && get(owner + 172) == 0,
          "property-only damage suppression");
    put(owner + 80, 9999);
    m.WriteU32(owner + 184, 1);
    put(target + 2588, 20000);
    put(target + 2592, 20000);
    call(0x82b0bfd0);
    check(get(target + 2588) == 10001 && get(owner + 172) == 9999,
          "sentinel direct damage precedes mode suppression");
    m.WriteU32(owner + 184, 0);
    m.WriteU32(owner + 92, 0);
    m.WriteU32(owner + 100, 16);
    m.WriteU32(owner + 96, 3);
    m.WriteU32(owner + 104, 8);
    m.WriteU32(owner + 196, 0);
    m.WriteU32(source + 188, 77);
    m.WriteU32(source + 124, 0x40000000);
    m.WriteU32(target + 4880, 1);
    m.WriteU32(owner + 124, 2);
    m.WriteU32(0xb0000 + 4 * (128 * 15 + 112 + 3), 100);
    put(owner + 80, 20);
    put(target + 2588, 100);
    call(0x82b0deb0);
    check(get(target + 2588) == 80 && (m.ReadU32(source + 124) & 0x40000) &&
              !m.ReadU32(source + 188) && m.ReadU32(owner + 196) == 16,
          "damage bypasses category multiplier and exhausts source gauge");
    m.WriteU32(owner + 120, 1);
    m.WriteU32(owner + 184, 0);
    m.WriteU32(owner + 40, record);
    m.WriteU32(record + 20, 2);
    put(source + 2588, 50);
    put(source + 2616, 10);
    put(target + 2588, 100);
    put(target + 2616, 7);
    call(0x82b0e300);
    check(get(target + 2588) == 75 && get(source + 2616) == 17 &&
              get(target + 2616) == 0 && get(owner + 172) == 25,
          "distributed source HP damage with MP siphon");
    m.WriteU32(owner + 184, 8);
    put(target + 2588, 40);
    put(target + 2616, 6);
    call(0x82b0e300);
    check(get(target + 2588) == 65 && get(source + 2616) == 23 &&
              get(target + 2616) == 0 && get(owner + 172) == 0,
          "mode eight heals before MP siphon");
    m.WriteU32(owner + 108, 0);
    m.WriteU32(owner + 112, 100);
    call(0x82b12d08);
    check((m.ReadU32(source + 124) & 128) && get(target + 2588) == 65,
          "source-only action flag shortcut");
    m.WriteU32(owner + 108, 1);
    m.WriteU32(owner + 112, 1);
    m.WriteU32(owner + 184, 0);
    m.WriteU32(target + 4880, 0);
    m.WriteU32(0xb0000 + 4 * (128 * 15 + 112 + 3), 100);
    m.WriteU32(0x93000 + 76, 30);
    m.WriteU32(0x93000 + 185200, 100);
    put(target + 2588, 100);
    put(target + 2616, 10);
    call(0x82b12d08);
    check(get(target + 2588) == 80 && get(target + 2616) == 0 &&
              get(owner + 172) == 20 && m.ReadU32(0x93000 + 76) == 0 &&
              m.ReadU32(0x93000 + 185200) == 130,
          "physical hit with MP siphon and profile spending");
    m.WriteU32(owner + 108, 1);
    m.WriteU32(owner + 112, 5);
    m.WriteU32(owner + 120, 7);
    call(0x82b106b8);
    check((m.ReadU32(target + 4956) & 5) == 5 &&
              m.ReadU32(target + 4960) == 7 && m.ReadU32(target + 4968) == 7,
          "selected duration mask assignment");
    m.WriteU32(owner + 108, 2);
    m.WriteU32(target + 5 * 272 + 232, 6);
    m.WriteU32(target + 272 + 232, 1);
    m.WriteU32(target + 508, 5);
    m.WriteU32(target + 780, 0);
    call(0x82b106b8);
    check(!(m.ReadU32(target + 5 * 272 + 232) & 6) &&
              (m.ReadU32(target + 6 * 272 + 232) & 6) == 6 &&
              (m.ReadU32(target + 2 * 272 + 232) & 1) &&
              m.ReadU32(target + 780) == 5,
          "property family conversion preserves effective value");
    m.WriteU32(owner + 108, 0);
    m.WriteU32(owner + 36, 0);
    m.WriteU32(owner + 92, 3);
    m.WriteU32(owner + 100, 2);
    m.WriteU32(owner + 96, 4);
    m.WriteU32(owner + 104, 8);
    m.WriteU32(target + 4 * 272 + 232, 8);
    call(0x82b106b8);
    check(m.ReadU32(owner + 188) == 0 &&
              (m.ReadU32(target + 3 * 272 + 232) & 2) &&
              !(m.ReadU32(target + 4 * 272 + 232) & 8),
          "one randomly selected target receives primary property");
    m.WriteU32(0x832cb798, 0xf9000);
    m.WriteU32(0xf9000 + 16, 0xfa000);
    m.WriteU32(target + 64, 25);
    m.WriteU32(owner + 108, 0);
    call(0x82b11a20);
    check(m.ReadU32(target + 5088) == 4 && m.ReadU32(0xf9000 + 24) == 54 &&
              m.ReadU32(0xfa000 + 268) == 54,
          "random temporary immunity and report");
    m.WriteU32(owner + 108, 1);
    m.WriteU32(target + 4880, 0);
    m.WriteU32(target + 4888, 0);
    m.WriteU32(0x200000 + 14888, 0);
    call(0x82b11a20);
    check(m.ReadU32(0xf9000 + 24) == 78 && !m.ReadU32(0x200000 + 14888) &&
              m.ReadU8(owner + 208) == 1,
          "empty category report omits result mark");
    m.WriteU32(target + 4880, 1);
    call(0x82b11a20);
    check(m.ReadU32(target + 5092) == 1 && m.ReadU32(0xf9000 + 24) == 67,
          "category vulnerability report");
    m.WriteU32(target + 4880, 0);
    m.WriteU32(target + 4888, 0x84);
    call(0x82b11a20);
    check(m.ReadU32(target + 5092) == 2 && m.ReadU32(0xf9000 + 24) == 77,
          "highest flagged class report");
    std::puts("PASS effect dispatch, eligibility, property payloads, HP cap "
              "results and gauge changes");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
