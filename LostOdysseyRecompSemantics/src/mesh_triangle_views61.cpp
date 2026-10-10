#include "lo_semantics/mesh_triangle_views61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::mesh_triangle_views61 {
bool Apply(GuestAddress entry, GuestMemory &m, Registers &s) {
  using recovery_abi::Address;
  auto self = Address(s.r[3]), part = Address(s.r[4]),
       channel = Address(s.r[5]);
  auto word = [&](unsigned p) { return m.ReadU32(p); };
  switch (entry) {
  case 0x82b9e048:
    s.r[3] = word(self + 184) ? word(word(self + 184) + 88) : 0;
    break;
  case 0x82b9e068:
    s.r[3] = part || channel >= 3 ? 0 : word(self + (channel ? 4 : 8));
    break;
  case 0x82b9e0a0:
    s.r[3] = part || channel >= 3 ? 0 : channel ? 1 : 4;
    break;
  case 0x82b9e180:
    s.r[3] = part || channel >= 3 ? 0 : 12;
    break;
  case 0x82b9e1b0: {
    auto source = word(part + 192) + 24 * channel;
    for (unsigned i = 0; i < 6; ++i)
      m.WriteU32(self + 4 * i, word(source + 4 * i));
    break;
  }
  case 0x82b9e1f8: {
    auto record = word(self + 196) + 8 * part;
    m.WriteU32(channel, word(record));
    s.r[3] = word(self + 200) + word(record + 4);
    break;
  }
  case 0x82b9e270: {
    auto source = part + 208;
    if (std::bit_cast<float>(word(source)) <
        std::bit_cast<float>(word(0x82000e50)))
      source = 0; // Preserve the original guest access on an absent mass cache.
    for (unsigned i = 0; i < 13; ++i)
      m.WriteU32(self + 4 * i, word(source + 4 * i));
    break;
  }
  default:
    return false;
  }
  return true;
}
} // namespace lo::semantic::gpu::mesh_triangle_views61
