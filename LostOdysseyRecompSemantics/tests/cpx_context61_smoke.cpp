#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/cpx_decode61.h"
#include "lo_semantics/cpx_context61.h"
#include <map>
struct ContextGuest final : manager_release_context61::GuestServices {
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
    auto s = sort_engine61_oracle::Initial(0);
    auto initial = s;
    constexpr unsigned context = 0x60000, input = 0x61000, output = 0x62000,
                       sizeOut = 0x63000;
    s.r[3] = context;
    (void)cpx_context61::Apply(0x828571c8, m, {guest, native}, s);
    m.WriteU8(input + 6, 2);
    m.WriteU8(input + 7, 0);
    m.WriteU32(input + 8, __builtin_bswap32(40));
    m.WriteU32(input + 12, __builtin_bswap32(8));
    m.WriteU32(input + 16, __builtin_bswap32(24));
    m.WriteU32(input + 20, __builtin_bswap32(32));
    for (unsigned block = 0; block < 2; ++block) {
      auto p = input + 24 + 8 * block;
      m.WriteU8(p, 255);
      m.WriteU8(p + 1, 0);
      m.WriteU8(p + 2, 3);
      m.WriteU8(p + 3, 0);
      for (unsigned i = 0; i < 4; ++i)
        m.WriteU8(p + 4 + i, 'A' + 4 * block + i);
    }
    s.r[3] = context;
    s.r[4] = input;
    s.r[5] = 0;
    (void)cpx_context61::Apply(0x82857308, m, {guest, native}, s);
    if (s.r[3] != 1 || m.ReadU32(context + 12) != 24 ||
        m.ReadU32(context + 24) != 8 || guest.initializations != 1 ||
        guest.live.at(m.ReadU32(context + 4)) != 24)
      throw std::runtime_error("CPX header/index ownership");
    s.r[3] = context;
    (void)cpx_context61::Apply(0x82857438, m, {guest, native}, s);
    auto scratch = Address(s.r[3]);
    auto allocations = guest.allocations;
    s.r[3] = context;
    (void)cpx_context61::Apply(0x82857438, m, {guest, native}, s);
    if (s.r[3] != scratch || guest.allocations != allocations ||
        guest.live.at(scratch) != 65552)
      throw std::runtime_error("CPX lazy scratch");
    for (unsigned i = 0; i < 2; ++i) {
      s.r[3] = context;
      (void)cpx_context61::Apply(0x82857480, m, {guest, native}, s);
      if (s.r[3])
        throw std::runtime_error("CPX premature completion");
      s.r[3] = context;
      s.r[4] = 0xffffffff;
      s.r[5] = sizeOut;
      (void)cpx_context61::Apply(0x828574a8, m, {guest, native}, s);
      if (s.r[3] != 24 + 8 * i || m.ReadU32(sizeOut) != 8)
        throw std::runtime_error("CPX indexed block range");
      auto offset = Address(s.r[3]);
      s.r[3] = context;
      s.r[4] = output + 4 * i;
      s.r[5] = input + offset;
      (void)cpx_decode61::Apply(0x82857b00, m, s);
    }
    s.r[3] = context;
    (void)cpx_context61::Apply(0x82857480, m, {guest, native}, s);
    if (s.r[3] != 1 || m.ReadU32(context + 32) != 8)
      throw std::runtime_error("CPX completion");
    for (unsigned i = 0; i < 8; ++i)
      if (m.ReadU8(output + i) != 'A' + i)
        throw std::runtime_error("CPX indexed decode output");
    m.WriteU32(sizeOut, 0x55);
    s.r[3] = context;
    s.r[4] = 0xffffffff;
    s.r[5] = sizeOut;
    (void)cpx_context61::Apply(0x828574a8, m, {guest, native}, s);
    if (s.r[3] || m.ReadU32(sizeOut) != 0x55)
      throw std::runtime_error("CPX end range untouched size");
    auto owned = guest.Allocate(32);
    m.WriteU8(context + 20, 1);
    m.WriteU32(context + 28, owned);
    s.r[3] = context;
    (void)cpx_context61::Apply(0x82857290, m, {guest, native}, s);
    s.r[3] = context;
    (void)cpx_context61::Apply(0x82857200, m, {guest, native}, s);
    if (!guest.live.empty() || m.ReadU32(context + 4) ||
        m.ReadU32(context + 8) || s.r[1] != initial.r[1])
      throw std::runtime_error("CPX context cleanup");
    std::puts("PASS CPX header/index ownership, lazy scratch, two-block decode "
              "and cleanup");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
