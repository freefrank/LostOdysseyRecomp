#pragma once
#include "lo_semantics/manager_release_context61.h"
namespace lo::semantic::gpu::progression_fixture {
inline constexpr unsigned Play = 0x1d0000;
inline void Setup(GuestMemory &m) {
  m.WriteU32(0x83315fb4, 0x1c0000);
  m.WriteU32(0x1c0000, 0x1c1000);
  m.WriteU32(0x1c1000 + 352, 0x123420);
  m.WriteU32(Play, 0x1d1000);
  m.WriteU32(0x1d1000 + 404, 0x123424);
  m.WriteU32(0x83291dc0, 0x7c000);
}
inline bool Direct(GuestAddress e, manager_release_context61::Registers &s) {
  if (e == 0x8229dfd8) {
    s.r[3] = Play;
    return true;
  }
  return e == 0x828208f8;
}
inline bool Indirect(GuestAddress e, manager_release_context61::Registers &s) {
  if (e == 0x123420) {
    s.r[3] = Play;
    return true;
  }
  return e == 0x123424;
}
} // namespace lo::semantic::gpu::progression_fixture
