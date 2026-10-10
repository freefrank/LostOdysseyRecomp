#pragma once
// Synthetic initial state for isolated link scenarios. Runtime record creation
// uses the recovered storage implementation and the service fixtures below.
inline void InitializeActionRecordFixture(lo::semantic::gpu::GuestMemory &m,
                                          unsigned resource, bool reset) {
  auto base = m.ReadU32(resource + 14656);
  if (!base) {
    base = 0x100000;
    m.WriteU32(resource + 14656, base);
  }
  if (base == 0x100000)
    m.WriteU32(0x78200, 1);
  auto index = reset ? 0u : m.ReadU32(resource + 14660);
  auto record = base + 124208 * index;
  for (unsigned i = 0; i < 124208; i += 4)
    m.WriteU32(record + i, 0);
  for (unsigned i = 0; i < 32; ++i) {
    m.WriteU32(record + 36 + 464 * i, 0xffffffff);
    m.WriteU32(record + 14884 + 464 * i, 0xffffffff);
  }
  m.WriteU32(resource + 14660, index + 1);
}

// Narrow synthetic boundaries only: heap service and memset. Growth and nested
// record defaults execute recovered code.
inline void SetupActionStorageFixture(lo::semantic::gpu::GuestMemory &m) {
  m.WriteU32(0x8330b608, 0x78000);
  m.WriteU32(0x78000, 0x78100);
  m.WriteU32(0x78108, 0x123400);
  m.WriteU32(0x82000e40, 0x3f800000);
  m.WriteU32(0x82000e50, 0);
  m.WriteU32(0x8324570c, 0x79000);
  m.WriteU32(0x83264558, 0x74000);
  m.WriteU32(0x83264984, 0x500000);
  m.WriteU32(0x832649c0, 0x600000);
  m.WriteU32(0x832ca0d0, 0x7f000);
  m.WriteU32(0x7f000 + 132, 0x700000);
}
inline bool ActionStorageDirectFixture(
    unsigned e, lo::semantic::gpu::GuestMemory &m,
    lo::semantic::gpu::manager_release_context61::Registers &s) {
  auto p = unsigned(s.r[3]);
  if (e == 0x82b7bc40) {
    for (unsigned i = 0; i < unsigned(s.r[5]); ++i)
      m.WriteU8(p + i, unsigned(s.r[4]));
    return true;
  }
  return false;
}
inline bool ActionStorageIndirectFixture(
    unsigned e, lo::semantic::gpu::GuestMemory &m,
    lo::semantic::gpu::manager_release_context61::Registers &s) {
  if (e != 0x123400)
    return false;
  auto old = unsigned(s.r[4]);
  if (!s.r[5]) {
    if (old)
      m.WriteU32(old == 0x100000 ? 0x78200 : 0x78204, 0);
    s.r[3] = 0;
  } else {
    auto storage = old ? old : (m.ReadU32(0x78200) ? 0x200000 : 0x100000);
    m.WriteU32(storage == 0x100000 ? 0x78200 : 0x78204, 1);
    s.r[3] = storage;
  }
  return true;
}
