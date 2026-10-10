#pragma once
namespace lo::semantic::gpu::profile_fixture {
inline void Setup(GuestMemory &m, unsigned profile) {
  m.WriteU32(0x83315fb4, 0x92000);
  m.WriteU32(0x92000, 0x92100);
  m.WriteU32(0x92100 + 352, 0x123420);
  m.WriteU32(0x832c1764, 0x94000);
  m.WriteU32(0x93000 + 52, 0x94000);
  m.WriteU32(0x93000 + 68, 0);
  m.WriteU32(0x83264978 + 128, profile);
}
inline bool Indirect(unsigned e, manager_release_context61::Registers &s) {
  if (e != 0x123420)
    return false;
  s.r[3] = 0x93000;
  return true;
}
} // namespace lo::semantic::gpu::profile_fixture
