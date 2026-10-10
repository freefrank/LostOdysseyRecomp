#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/cpx_decode61.h"
#include "lo_semantics/cpx_context61.h"
#include "lo_semantics/cpx_lifecycle61.h"
#include "lo_semantics/cpx_stream61.h"
#include <map>
struct ContextGuest final : cpx_stream61::GuestServices {
  std::vector<unsigned char> file;
  unsigned cursor = 0, reads = 0, exchanges = 0;
  std::uint64_t ticks = 0;
  std::uint64_t ReadTimeBase() override { return ++ticks; }
  void ExchangeStatus(GuestMemory &m, GuestAddress p,
                      std::uint32_t value) override {
    ++exchanges;
    m.WriteU32(p, value);
  }
  unsigned locks = 0, unlocks = 0;
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
    if (e == 0x8284cfa8)
      return;
    if (e == 0x823f3298) {
      s.r[3] = Allocate(Address(s.r[3]));
      return;
    }
    if (e == 0x82850970) {
      s.r[3] = 0x74000;
      return;
    }
    if (e == 0x830d9c6c) {
      ++locks;
      return;
    }
    if (e == 0x830d9c7c) {
      ++unlocks;
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
  void CallIndirect(GuestAddress e, GuestMemory &m,
                    manager_release_context61::Registers &s) override {
    if (e == 0x2000) {
      if (s.r[6] != 0 || s.r[7] != 2)
        throw std::runtime_error("stream read flags");
      ++reads;
      auto n = Address(s.r[5]);
      for (unsigned i = 0; i < n; ++i)
        m.WriteU8(Address(s.r[4]) + i,
                  cursor + i < file.size() ? file[cursor + i] : 0);
      cursor += n;
      return;
    }
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
    constexpr unsigned self = 0x60000, vt = 0x61000;
    m.WriteU32(self, vt);
    m.WriteU32(vt + 20, 0x2000);
    guest.file.resize(40);
    auto little = [&](unsigned off, unsigned v) {
      for (unsigned i = 0; i < 4; ++i)
        guest.file[off + i] = v >> (8 * i);
    };
    guest.file[6] = 2;
    little(8, 40);
    little(12, 8);
    little(16, 24);
    little(20, 32);
    for (unsigned b = 0; b < 2; ++b) {
      auto p = 24 + 8 * b;
      guest.file[p] = 255;
      guest.file[p + 2] = 3;
      for (unsigned i = 0; i < 4; ++i)
        guest.file[p + 4 + i] = 'A' + 4 * b + i;
    }
    for (unsigned i = 0; i < 16; ++i)
      m.WriteU8(self + 52 + i, guest.file[i]);
    m.WriteU32(self + 16, 40);
    m.WriteU8(self + 31, 1);
    auto step = [&]() {
      s.r[3] = self;
      s.r[4] = 0;
      (void)cpx_stream61::Apply(0x8284e6b8, m, {guest, native}, s);
      if (s.r[1] != initial.r[1])
        throw std::runtime_error("stream stack");
      return s.r[3] != 0;
    };
    if (step() || m.ReadU8(self + 30) != 16 || !m.ReadU8(self + 88) ||
        m.ReadU8(self + 55) != 1)
      throw std::runtime_error("split reserve setup");
    if (step() || m.ReadU16(m.ReadU32(self + 100) + 16) != 1)
      throw std::runtime_error("decompression budget yield");
    if (!step() || m.ReadU32(self + 40) != 4 || m.ReadU32(self + 20) != 8 ||
        m.ReadU32(self + 96) || m.ReadU32(self + 100) ||
        m.ReadU8(self + 30) != 255)
      throw std::runtime_error("decompression finish cleanup");
    auto output = m.ReadU32(self + 48);
    for (unsigned i = 0; i < 8; ++i)
      if (m.ReadU8(output + i) != 'A' + i)
        throw std::runtime_error("stream decoded bytes");
    if (guest.live.size() != 1 || guest.live.at(output) != 8 ||
        guest.exchanges != 1)
      throw std::runtime_error("stream ownership");
    guest.Free(output);
    for (unsigned i = 0; i < 104; ++i)
      m.WriteU8(self + i, 0);
    m.WriteU32(self, vt);
    m.WriteU32(self + 16, 4);
    guest.file = {'T', 'E', 'S', 'T'};
    guest.cursor = 0;
    if (!step() || m.ReadU32(self + 40) != 4)
      throw std::runtime_error("plain stream");
    output = m.ReadU32(self + 48);
    for (unsigned i = 0; i < 4; ++i)
      if (m.ReadU8(output + i) != guest.file[i])
        throw std::runtime_error("plain bytes");
    guest.Free(output);
    if (!guest.live.empty() || guest.locks != guest.unlocks)
      throw std::runtime_error("stream final cleanup");
    std::puts("PASS CPX split reserve, staged decode yield/finish, plain read");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
