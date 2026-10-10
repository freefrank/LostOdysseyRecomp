#include "lo_semantics/battle_manager_access61.h"
#include "lo_semantics/battle_scene_requests61.h"
#include "lo_semantics/battle_script_parameters61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
#include <limits>
namespace lo::semantic::gpu::battle_script_parameters61 {
namespace {
using recovery_abi::Address;
unsigned Parameter(GuestMemory &m, Dependencies d, Registers &s) {
  auto owner = Address(s.r[3]), actor = m.ReadU32(owner + 24),
       at = m.ReadU32(actor + 36) + m.ReadU32(actor + 52) + Address(s.r[4]);
  auto raw = unsigned(m.ReadU8(at)) | (unsigned(m.ReadU8(at + 1)) << 8);
  auto index = unsigned(std::int32_t(std::int16_t(raw))) + Address(s.r[5]);
  auto word = [&](unsigned base, unsigned i) {
    return m.ReadU32(base + 4 * i);
  };
  if (index & 0x8000)
    return word(m.ReadU32(actor + 28), index & 0x7fff);
  if (std::int32_t(index) < 2048)
    return word(m.ReadU32(actor + 12), index);
  if (std::int32_t(index) < 4096)
    return word(m.ReadU32(owner + 28), index + 40668);
  if (std::int32_t(index) < 6144)
    return word(m.ReadU32(m.ReadU32(owner + 44)), index - 4096);
  if (std::int32_t(index) < 22528) {
    auto bit = index - 6144;
    return (word(m.ReadU32(owner + 28), 44764 + bit / 32) >> (bit % 32)) & 1;
  }
  auto resource = m.ReadU32(actor + 4);
  if (index >= 32640 || !resource || index - 32514 > 8)
    return 0;
  switch (index - 32514) {
  case 0:
  case 1:
  case 2:
  case 3: {
    constexpr unsigned offsets[]{2592, 2620, 2588, 2616};
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    double value =
        std::bit_cast<float>(m.ReadU32(resource + offsets[index - 32514]));
    std::int32_t n;
    if (value > double(std::numeric_limits<std::int32_t>::max()))
      n = std::numeric_limits<std::int32_t>::max();
    else if (std::isnan(value) ||
             value < double(std::numeric_limits<std::int32_t>::min()))
      n = std::numeric_limits<std::int32_t>::min();
    else
      n = std::int32_t(value);
    s.fpr_bits[0] = std::uint64_t(std::int64_t(n));
    m.WriteU32(Address(s.r[1]) - 16, unsigned(n));
    return unsigned(n);
  }
  case 4:
    return m.ReadU32(resource + 4880);
  case 5:
    return m.ReadU32(resource + 152);
  case 8:
    return m.ReadU32(resource + 64);
  default:
    return 0;
  }
}
void Periodic(GuestMemory &m, Dependencies d, Registers &s, unsigned owner) {
  auto state = [&]() { return m.ReadU32(owner + 44); };
  if ((m.ReadU32(state() + 28) & 0xc0000) != 0x40000)
    return;
  m.WriteU32(state() + 16720,
             m.ReadU32(state() + 16720) + m.ReadU32(state() + 16));
  if (std::int32_t(m.ReadU32(state() + 16720)) <
      std::int32_t(m.ReadU32(state() + 16724)))
    return;
  m.WriteU32(state() + 16720,
             m.ReadU32(state() + 16720) - m.ReadU32(state() + 16724));
  auto index = m.ReadU32(state() + 16748);
  if (!m.ReadU32(m.ReadU32(state() + 16728) + 4 * index)) {
    m.WriteU32(state() + 28, (m.ReadU32(state() + 28) & ~0xc0000u) | 0x80000);
    return;
  }
  s.r[3] = 0x832cc0fc;
  s.r[4] = 0x61000000;
  s.r[5] = m.ReadU32(m.ReadU32(state() + 16736) + 4 * index);
  s.r[6] = 0;
  (void)battle_scene_requests61::Apply(0x82b1aca8, m, d, s);
  m.WriteU32(state() + 16748, m.ReadU32(state() + 16748) + 1);
}
void Flags(GuestMemory &m, Dependencies d, Registers &s, unsigned owner,
           unsigned resource) {
  s.r[3] = 0x832cb550;
  s.r[4] = m.ReadU32(resource + 64);
  (void)battle_manager_access61::Apply(0x82380d40, m, d, s);
  auto object = Address(s.r[3]);
  if (!object)
    return;
  auto state = [&]() { return m.ReadU32(owner + 44); };
  constexpr unsigned bits[]{28, 21, 31, 25, 24, 23, 22, 20, 19, 18,
                            17, 16, 15, 14, 13, 12, 11, 10, 9};
  for (std::int32_t i = 0; i < std::int32_t(m.ReadU32(state() + 196)); ++i) {
    auto record = state() + 68 + 8 * unsigned(i);
    bool required = m.ReadU8(record + 4) == 1,
         actual = (m.ReadU32(resource + 124) & 0x10000000) != 0;
    if (required != actual)
      continue;
    auto type = m.ReadU32(record);
    if (type > 18)
      continue;
    auto p = m.ReadU32(object + 516) + 444, mask = 1u << bits[type];
    m.WriteU32(p, (m.ReadU32(p) & ~mask) |
                      ((unsigned(m.ReadU8(record + 5)) & 1) << bits[type]));
  }
  m.WriteU32(state() + 196, 0);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x8238be38) {
    s.r[3] = Parameter(m, d, s);
    return true;
  }
  if (e != 0x8238b4a8 && e != 0x8238b5a0)
    return false;
  auto owner = Address(s.r[3]), resource = Address(s.r[4]),
       old = Address(s.r[1]);
  unsigned frame = e == 0x8238b4a8 ? 96 : 112,
           first = e == 0x8238b4a8 ? 31 : 30;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  if (e == 0x8238b4a8)
    Periodic(m, d, s, owner);
  else
    Flags(m, d, s, owner, resource);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_parameters61
