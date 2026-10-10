#pragma once
namespace requests_fixture {
inline void Setup(GuestMemory &m) {
  m.WriteU32(0x8330b608, 0x70000);
  m.WriteU32(0x70000, 0x70100);
  m.WriteU32(0x70004, 0xa0000);
  m.WriteU32(0x70104, 0x123404);
  m.WriteU32(0x820010a8, 0x123410);
  m.WriteU32(0x832cc0fc + 8, 0x80000);
  m.WriteU32(0x832cc0fc + 12, 1);
  m.WriteU32(0x80000, 0x81000);
  m.WriteU32(0x81008, 10);
  m.WriteU32(0x81000 + 28, 501);
  m.WriteU32(0x832cc0fc + 24, 0x90000);
  m.WriteU32(0x832cc0fc + 32, 8);
  m.WriteU32(0x83213d74, 10);
  for (unsigned i = 0; i < 3; ++i) {
    m.WriteU32(0x820c9ef8 + 16 * i, 0x1000 * (i + 1));
    m.WriteU32(0x820c9efc + 16 * i, 0x1000 * (i + 1));
  }
}
inline bool Direct(GuestAddress e, GuestMemory &m,
                   manager_release_context61::Registers &s) {
  if (e == 0x82380a18)
    return true;
  if (e == 0x82b5d230) {
    s.r[3] = 501;
    return true;
  }
  if (e == 0x82b5d1a0) {
    m.WriteU32(unsigned(s.r[5]), 0xb0000);
    return true;
  }
  return false;
}
inline bool Indirect(GuestAddress e, GuestMemory &m,
                     manager_release_context61::Registers &s) {
  if (e == 0x123404) {
    if (s.r[4] != 100 || s.r[5] != 8)
      throw std::runtime_error("scene request allocation ABI");
    s.r[3] = m.ReadU32(0x70004);
    m.WriteU32(0x70004, unsigned(s.r[3]) + 256);
    return true;
  }
  return false;
}
} // namespace requests_fixture
