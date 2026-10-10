#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/archive_loader61.h"
#include "lo_semantics/archive_startup61.h"
#include "lo_semantics/archive_overlay61.h"
#include <map>
struct LoaderGuest final : archive_loader61::GuestServices {
  std::string opened;
  std::vector<unsigned char> file;
  unsigned cursor = 0, locks = 0, unlocks = 0, closes = 0, next = 0x90000;
  std::map<unsigned, unsigned> live;
  void ExchangeStatus(GuestMemory &m, GuestAddress p,
                      std::uint32_t v) override {
    m.WriteU32(p, v);
  }
  void CallDirect(GuestAddress e, GuestMemory &m,
                  archive_loader61::Registers &s) override {
    if (e == 0x827c5f38) {
      m.WriteU32(0x8330b608, 0x70000);
      m.WriteU32(0x70000, 0x71000);
      m.WriteU32(0x71004, 0x1004);
      return;
    }
    if (e == 0x82dd1b58) {
      auto out = Address(s.r[4]);
      std::string name = "overlay.fpi";
      for (unsigned i = 0; i <= name.size(); ++i)
        m.WriteU8(out + i, i == name.size() ? 0 : name[i]);
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
    if (e == 0x82be20a8) {
      if (s.r[3] != 0x240000)
        throw std::runtime_error("startup thread argument");
      s.r[3] = 11;
      return;
    }
    if (e == 0x82be1ae8) {
      s.r[3] = 22;
      return;
    }
    if (e == 0x82850970) {
      s.r[3] = 0x74000;
      return;
    }
    if (e == 0x82be2be0) {
      opened.clear();
      auto p = Address(s.r[3]);
      while (m.ReadU8(p))
        opened += char(m.ReadU8(p++));
      cursor = 0;
      s.r[3] = 7;
      return;
    }
    if (e == 0x82be1b80) {
      ++closes;
      return;
    }
    if (e == 0x82be2dd8) {
      auto bytes = Address(s.r[5]);
      auto n = std::min<unsigned>(bytes, file.size() - cursor);
      for (unsigned i = 0; i < n; ++i)
        m.WriteU8(Address(s.r[4]) + i, file[cursor + i]);
      cursor += n;
      m.WriteU32(Address(s.r[6]), n);
      s.r[3] = 1;
      return;
    }
    if (e == 0x823f3298 || e == 0x82486c88) {
      auto n = Address(s.r[3]);
      live[next] = n;
      s.r[3] = next;
      next += (n + 15) & ~15u;
      return;
    }
    if (e == 0x823f3340) {
      if (!live.erase(Address(s.r[3])))
        throw std::runtime_error("loader free");
      return;
    }
    throw std::runtime_error("loader guest boundary");
  }

  void CallIndirect(GuestAddress e, GuestMemory &,
                    archive_loader61::Registers &s) override {
    if (e != 0x1004 || s.r[5] != 8)
      throw std::runtime_error("overlay allocation");
    auto n = Address(s.r[4]);
    live[next] = n;
    s.r[3] = next;
    next += (n + 15) & ~15u;
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83264000, 0x2000});
    regions.push_back({0x8330b000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    LoaderGuest guest;
    auto s = sort_engine61_oracle::Initial(0);
    auto initial = s;
    constexpr unsigned root = 0x83264d90, owners = 0x60000, input = 0x63000;
    guest.file.assign(2048, 0);
    guest.file[13] = 1;
    guest.file[25] = 1;
    unsigned priorities[]{2, 9, 9, 12};
    auto mount = [&](unsigned i) {
      auto owner = owners + 512 * i;
      m.WriteU32(owner + 340, priorities[i]);
      s.r[3] = owner;
      (void)archive_overlay61::Apply(0x82854d98, m, {guest, native}, s);
      if (s.r[3] != 1 || !m.ReadU32(owner + 352) || s.r[1] != initial.r[1])
        throw std::runtime_error("overlay load");
    };
    for (unsigned i = 0; i < 3; ++i)
      mount(i);
    if (m.ReadU32(root + 212) != 3 || m.ReadU8(root + 224) != 1 ||
        m.ReadU8(root + 225) != 2 || m.ReadU8(root + 226) != 0)
      throw std::runtime_error("stable descending overlay order");
    auto table = m.ReadU32(root + 220), node = m.ReadU32(table);
    m.WriteU32(node + 12 + 4, 0);
    mount(3);
    if (m.ReadU32(table + 4) != 3 || m.ReadU32(node + 16) != owners + 1536 ||
        m.ReadU8(root + 224) != 1 || m.ReadU8(root + 225) != 2 ||
        m.ReadU8(root + 226) != 0)
      throw std::runtime_error("overlay hole reuse");
    s.r[3] = 4;
    (void)archive_overlay61::Apply(0x8285d418, m, {guest, native}, s);
    node = Address(s.r[3]);
    for (unsigned i = 0; i < 2; ++i) {
      m.WriteU32(input, 10 + i);
      s.r[3] = node;
      s.r[4] = input;
      s.r[5] = 4;
      (void)archive_overlay61::Apply(0x8285d920, m, {guest, native}, s);
      if (s.r[3] != i)
        throw std::runtime_error("segmented append index");
    }
    auto next = m.ReadU32(node);
    if (m.ReadU32(node + 12) != 10 || !next || m.ReadU32(next + 12) != 11 ||
        m.ReadU32(next + 8) != 4 || guest.locks != guest.unlocks)
      throw std::runtime_error("segmented append growth");
    std::puts("PASS DLC index insertion, hole reuse, priority order and "
              "segmented growth");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
