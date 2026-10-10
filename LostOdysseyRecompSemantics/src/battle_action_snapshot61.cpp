#include "lo_semantics/battle_action_snapshot61.h"
#include "lo_semantics/battle_action_storage61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::battle_action_snapshot61 {
namespace {
using recovery_abi::Address;
void Copy(GuestMemory &m, Dependencies d, Registers &s) {
  auto source = Address(s.r[4]), dest = Address(s.r[5]);
  for (unsigned off : {0u, 4u, 20u})
    m.WriteU32(dest + off, m.ReadU32(source + off));
  for (unsigned i = 0; i < 32; ++i) {
    auto from = source + 464 * i, to = dest + 464 * i;
    m.WriteU32(to + 248, (m.ReadU32(from + 248) & 0x80000000u) |
                             (m.ReadU32(to + 248) & 0x7fffffffu));
    m.WriteU32(to + 36, m.ReadU32(from + 36));
    m.WriteU32(to + 15096, (m.ReadU32(from + 15096) & 0x80000000u) |
                               (m.ReadU32(to + 15096) & 0x7fffffffu));
    m.WriteU32(to + 14884, m.ReadU32(from + 14884));
    for (unsigned k = 0; k < 4; ++k) {
      for (unsigned off : {200u, 184u, 168u, 40u, 216u, 280u})
        m.WriteU32(to + off + 4 * k, m.ReadU32(from + off + 4 * k));
      for (unsigned off : {72u, 56u, 104u, 88u, 136u, 152u}) {
        auto bits = m.ReadU32(from + off + 4 * k);
        if (s.cached_fp_control & 0x8040) {
          s.cached_fp_control &= ~0x8040u;
          d.fp.SetHostFpControl(s.cached_fp_control);
        }
        s.fpr_bits[0] =
            std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(bits)));
        m.WriteU32(to + off + 4 * k, bits);
      }
      for (unsigned off : {15048u, 15032u, 15016u, 14888u, 15064u, 15128u})
        m.WriteU32(to + off + 4 * k, m.ReadU32(from + off + 4 * k));
      for (unsigned off : {14920u, 14904u, 14952u, 14936u, 14984u, 15000u}) {
        auto bits = m.ReadU32(from + off + 4 * k);
        s.fpr_bits[0] =
            std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(bits)));
        m.WriteU32(to + off + 4 * k, bits);
      }
    }
  }
  s.r[3] = 0;
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82acd3b0) {
    auto p = Address(s.r[3]);
    m.WriteU32(p + 4, Address(s.r[4]));
    m.WriteU32(p + 8, Address(s.r[5]));
    m.WriteU32(p + 12, Address(s.r[6]));
    return true;
  }
  if (e == 0x82acd3e0) {
    Copy(m, d, s);
    return true;
  }
  if (e != 0x82acee70)
    return false;
  auto manager = Address(s.r[3]), resource = Address(s.r[4]),
       mode = Address(s.r[5]) & 255, old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = 26; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= 144;
  m.WriteU32(Address(s.r[1]), old);
  auto flags = (m.ReadU32(resource + 100) & 0xc0000000u) |
               (m.ReadU32(resource + 172) & 0x3fffffffu);
  for (unsigned off : {88u, 92u, 96u})
    m.WriteU32(resource + off + 72, m.ReadU32(resource + off));
  m.WriteU32(resource + 172, flags);
  auto state = mode ? 0xfffffffeu : m.ReadU32(resource + 60) & 255u;
  m.WriteU32(resource + 156, state);
  if (state != 2)
    m.WriteU32(resource + 60, 0);
  s.r[3] = resource + 14668;
  s.r[4] = 0;
  (void)battle_action_storage61::Apply(0x82a9b698, m, d, s);
  for (unsigned i = 0;
       std::int32_t(i) < std::int32_t(m.ReadU32(resource + 14660)); ++i) {
    s.r[3] = resource;
    s.r[4] = 1;
    (void)battle_action_storage61::Apply(0x82ab2d88, m, d, s);
    s.r[3] = manager;
    s.r[4] = m.ReadU32(resource + 14656) + 124208 * i;
    s.r[5] = m.ReadU32(resource + 14668) + 124208 * i;
    Copy(m, d, s);
  }
  s.r[1] += 144;
  for (unsigned i = 26; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_action_snapshot61
