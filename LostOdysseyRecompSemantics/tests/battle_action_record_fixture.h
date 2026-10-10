#pragma once
// Synthetic replacement only for the still-external record-array reset/grow
// services. This is not an implementation of guest allocation or the full
// record initializer.
inline void InitializeActionRecordFixture(lo::semantic::gpu::GuestMemory &m,
                                          unsigned resource, bool reset) {
  auto base = m.ReadU32(resource + 14656);
  if (!base) {
    base = 0x100000;
    m.WriteU32(resource + 14656, base);
  }
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
