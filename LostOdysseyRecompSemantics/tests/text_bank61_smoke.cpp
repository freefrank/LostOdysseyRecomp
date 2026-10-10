#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/cpx_decode61.h"
#include "lo_semantics/cpx_context61.h"
#include "lo_semantics/text_bank61.h"
#include <map>
struct BankGuest final : manager_release_context61::GuestServices {
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
      m.WriteU32(0x71008, 0x1008);
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
    if (e == 0x1008) {
      auto old = Address(s.r[4]), size = Address(s.r[5]);
      if (s.r[6] != 8)
        throw std::runtime_error("bank resize alignment");
      unsigned out = 0;
      if (size) {
        out = Allocate(size);
        if (old) {
          auto count = std::min(size, live.at(old));
          for (unsigned i = 0; i < count; ++i)
            m.WriteU8(out + i, m.ReadU8(old + i));
        }
      }
      if (old)
        Free(old);
      s.r[3] = out;
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
    regions.push_back({0x821a8000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    BankGuest guest;
    auto s = sort_engine61_oracle::Initial(0);
    auto initial = s;
    constexpr unsigned bank = 0x60000, output = 0x61000, owner = 0x62000,
                       rows = 0x63000;
    for (unsigned wide : {1u, 0u}) {
      m.WriteU32(bank + 4, 16);
      m.WriteU32(bank + 8, 3);
      m.WriteU32(bank + 12, wide);
      for (unsigned i = 0; i < 3; ++i) {
        m.WriteU32(bank + 16 + 8 * i, 24 + 16 * i);
        m.WriteU32(bank + 20 + 8 * i, 0xdead0000 + i);
      }
      unsigned short strings[3][4] = {
          {'A', 'B', 0, 0},
          {0, 0, 0, 0},
          {static_cast<unsigned short>(wide ? 0x732b : 0xe9), 'Z', 0, 0}};
      for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 4; ++j) {
          if (wide)
            m.WriteU16(bank + 40 + 16 * i + 2 * j, strings[i][j]);
          else
            m.WriteU8(bank + 40 + 16 * i + j, strings[i][j]);
        }
      s.r[3] = bank;
      s.r[4] = output;
      (void)text_bank61::Apply(0x82aa1b50, m, {guest, native}, s);
      if (m.ReadU32(output + 4) != 3 || m.ReadU32(output + 8) != 33)
        throw std::runtime_error("bank array count/growth");
      for (unsigned i = 0; i < 3; ++i) {
        auto header = m.ReadU32(output) + 12 * i;
        if (m.ReadU32(header + 4) != (i == 1 ? 0u : 3u))
          throw std::runtime_error("bank string length");
        if (i != 1)
          for (unsigned j = 0; j < 3; ++j)
            if (m.ReadU16(m.ReadU32(header) + 2 * j) != strings[i][j])
              throw std::runtime_error("bank offset string copy/byte widening");
      }
    }
    m.WriteU32(bank + 8, 0);
    s.r[3] = bank;
    s.r[4] = output;
    (void)text_bank61::Apply(0x82aa1b50, m, {guest, native}, s);
    if (!guest.live.empty() || m.ReadU32(output) || m.ReadU32(output + 4) ||
        m.ReadU32(output + 8))
      throw std::runtime_error("bank replacement cleanup");
    for (unsigned columns : {1u, 7u}) {
      auto blob = guest.Allocate(256);
      for (unsigned i = 0; i < 256; ++i)
        m.WriteU8(blob + i, 0);
      auto stride = columns == 7 ? 16u : 4u;
      m.WriteU32(blob, 16 + 2 * stride);
      m.WriteU32(blob + 4, 64);
      m.WriteU32(blob + 8, 3);
      m.WriteU32(blob + 12, 1);
      for (unsigned i = 0; i < 3; ++i) {
        m.WriteU32(blob + 64 + 8 * i, 24 + 16 * i);
        m.WriteU32(blob + 68 + 8 * i, 999);
        if (i != 1) {
          m.WriteU16(blob + 88 + 16 * i, 'X' + i);
          m.WriteU16(blob + 90 + 16 * i, 0);
        }
      }
      for (unsigned j = 0; j < columns; ++j)
        m.WriteU16(blob + 18 + 2 * j, j % 3);
      m.WriteU8(blob + 16 + stride, 1);
      s.r[3] = blob;
      s.r[4] = output;
      (void)text_bank61::Apply(columns == 7 ? 0x8283f220 : 0x8283fbe8, m,
                               {guest, native}, s);
      if (guest.live.contains(blob) || m.ReadU32(output + 4) != 1)
        throw std::runtime_error(
            "bank record consumer owns input/skips disabled records");
      auto data = m.ReadU32(output);
      for (unsigned j = 0; j < columns; ++j) {
        auto header = data + 12 * j, p = m.ReadU32(header),
             count = m.ReadU32(header + 4);
        if (j % 3 == 1) {
          if (count)
            throw std::runtime_error("bank empty column");
        } else if (count != 2 || m.ReadU16(p) != 'X' + j % 3)
          throw std::runtime_error("bank selected column");
        if (p)
          guest.Free(p);
      }
      guest.Free(data);
      for (unsigned j = 0; j < 3; ++j)
        m.WriteU32(output + 4 * j, 0);
      if (!guest.live.empty())
        throw std::runtime_error("bank consumer temporary ownership");
    }
    m.WriteU32(owner + 572, rows);
    m.WriteU32(owner + 576, 3);
    for (unsigned i = 0; i < 3; ++i)
      m.WriteU32(rows + 60 * i, 100 + i);
    s.r[3] = owner;
    s.r[4] = 101;
    (void)text_bank61::Apply(0x8230ba20, m, {guest, native}, s);
    if (s.r[3] != rows + 60)
      throw std::runtime_error("menu text row stride");
    s.r[3] = owner;
    s.r[4] = 999;
    (void)text_bank61::Apply(0x8230ba20, m, {guest, native}, s);
    if (s.r[3] != 0xffffffff83265614ull)
      throw std::runtime_error("menu fallback row");
    if (s.r[1] != initial.r[1] || guest.initializations != 1)
      throw std::runtime_error("bank ABI/manager reuse");
    std::puts("PASS offset-based wide/narrow banks, ignored lengths, "
              "replacement cleanup, 1/7-column consumers and menu lookup");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
