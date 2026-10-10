#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/archive_loader61.h"
#include <map>
struct LoaderGuest final : archive_loader61::GuestServices {
  std::vector<unsigned char> file;
  unsigned cursor = 0, locks = 0, unlocks = 0, closes = 0, next = 0x90000;
  std::map<unsigned, unsigned> live;
  void ExchangeStatus(GuestMemory &m, GuestAddress p,
                      std::uint32_t v) override {
    m.WriteU32(p, v);
  }
  void CallDirect(GuestAddress e, GuestMemory &m,
                  archive_loader61::Registers &s) override {
    if (e == 0x830d9c6c) {
      ++locks;
      return;
    }
    if (e == 0x830d9c7c) {
      ++unlocks;
      return;
    }
    if (e == 0x82be2be0) {
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
    if (e == 0x823f3298) {
      auto n = Address(s.r[3]);
      live[next] = n;
      s.r[3] = next;
      next += n;
      return;
    }
    if (e == 0x823f3340) {
      if (!live.erase(Address(s.r[3])))
        throw std::runtime_error("loader free");
      return;
    }
    throw std::runtime_error("loader guest boundary");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    archive_loader61::Registers &) override {
    throw std::runtime_error("loader indirect");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x83264000, 0x2000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    LoaderGuest guest;
    auto s = sort_engine61_oracle::Initial(0);
    auto initial = s;
    constexpr unsigned output = 0x60000, root = 0x83264d90;
    for (unsigned native = 0; native < 2; ++native) {
      guest.file.assign(2048, 0);
      auto half = [&](unsigned p, unsigned v) {
        guest.file[p] = native ? v >> 8 : v;
        guest.file[p + 1] = native ? v : v >> 8;
      };
      auto word = [&](unsigned p, unsigned v) {
        for (unsigned i = 0; i < 4; ++i)
          guest.file[p + i] = v >> (8 * (native ? 3 - i : i));
      };
      half(12, 1);
      half(24, native);
      half(26, 1);
      word(32, 64);
      guest.file[20] = 3;
      half(64 + 2, 1);
      word(64 + 4, 48);
      word(64 + 8, 128);
      word(64 + 12, 160);
      word(112, 0x10000001);
      half(112 + 14, 1);
      word(112 + 20, 1);
      word(136, 2);
      word(136 + 16, 123);
      word(136 + 20, 456);
      s.r[3] = 0;
      s.r[4] = 0x61000;
      s.r[5] = 0;
      s.r[7] = output;
      (void)archive_loader61::Apply(0x82854400, m,
                                    {guest, cook_main_smoke::native}, s);
      auto index = m.ReadU32(output), archive = index + 64;
      if (s.r[3] != 1 || m.ReadU32(index + 32) != archive ||
          m.ReadU32(archive + 4) != index + 112 ||
          m.ReadU32(archive + 8) != archive + 128 ||
          m.ReadU32(index + 112 + 20) != index + 136 ||
          m.ReadU32(index + 136 + 16) != 123 ||
          m.ReadU32(index + 136 + 20) != 456 || m.ReadU16(root + 2) != 3 ||
          m.ReadU32(root + 248) || guest.live.size() != 1 ||
          s.r[1] != initial.r[1])
        throw std::runtime_error("resident index relocation");
    }
    guest.file.assign(64, 0);
    auto before = m.ReadU32(output);
    s.r[3] = 0;
    s.r[4] = 0x61000;
    s.r[5] = 0;
    s.r[7] = output;
    (void)archive_loader61::Apply(0x82854400, m,
                                  {guest, cook_main_smoke::native}, s);
    if (s.r[3] != ~std::uint64_t(0) || m.ReadU32(output) != before ||
        m.ReadU32(root + 248) || guest.closes != 3 ||
        guest.locks != guest.unlocks)
      throw std::runtime_error("invalid index sector rejection");
    guest.live.erase(before);
    std::puts(
        "PASS FPI load, both endian entry relocation and invalid sector exit");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
