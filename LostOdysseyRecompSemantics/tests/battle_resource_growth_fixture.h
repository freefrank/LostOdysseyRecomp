#pragma once
namespace growth_fixture {
inline void Regions(std::vector<test::Region> &regions) {
  for (auto p :
       {0x820c0000u, 0x82218000u, 0x82021000u, 0x8201f000u, 0x8201d000u,
        0x82059000u, 0x821ba000u, 0x83264000u, 0x83213000u, 0x821a8000u})
    regions.push_back({p, 0x1000});
}
inline void Setup(GuestMemory &m) {
  auto f = [&](unsigned p, float v) {
    m.WriteU32(p, std::bit_cast<unsigned>(v));
  };
  auto q = [&](unsigned p, double v) {
    recovery_abi::WriteU64(m, p, std::bit_cast<std::uint64_t>(v));
  };
  q(0x820c00d0, 10);
  q(0x82000e90, 1);
  q(0x822183d8, .1);
  q(0x822181b8, 1);
  q(0x82021080, .1);
  q(0x82218280, .1);
  q(0x820c00c8, 1);
  q(0x82001010, 1);
  q(0x820c00c0, 1);
  f(0x8201f9f0, .5f);
  f(0x82007784, 1);
  f(0x820009fc, 10);
  f(0x8201dd2c, 100);
  f(0x82000d7c, .01f);
  for (auto p : {0x82059ad0u, 0x82000e44u, 0x82000dd4u, 0x82000e48u})
    f(p, 1);
  f(0x821baa74, 10);
  f(0x8200bca0, 10);
  f(0x822184dc, 9999);
  f(0x822181e4, 999);
  f(0x822182a0, 999);
  f(0x82000b3c, .25f);
  m.WriteU32(0x83264978, 0x180000);
  m.WriteU32(0x83264978 + 36, 0x100000);
  m.WriteU32(0x83264978 + 72, 0x120000);
  m.WriteU32(0x832ca0d0, 0x130000);
  m.WriteU32(0x130000 + 120, 0x140000);
  for (unsigned i = 0; i < 32; ++i) {
    m.WriteU32(0x83213438 + 8 * i, 0);
    m.WriteU32(0x8321343c + 8 * i, 1u << i);
  }
  for (unsigned group = 0; group < 3; ++group)
    for (unsigned off = 108; off <= 156; off += 4)
      f(0x100000 + 204 * group + off, 10);
  for (unsigned level = 0; level < 100; ++level) {
    auto row = 0x100000 + 204 * (level + 16);
    for (unsigned off = 108; off <= 156; off += 4)
      f(row + off, 20);
    f(row + 124, 100);
    f(row + 152, 50);
  }
  for (auto layer : {69300u, 71400u, 71540u, 69440u}) {
    m.WriteU32(0x140000 + layer + 4, 10);
    m.WriteU32(0x140000 + layer + 8, 10);
    for (unsigned off = 12; off <= 52; off += 4)
      f(0x140000 + layer + off, 10);
  }
}
} // namespace growth_fixture
