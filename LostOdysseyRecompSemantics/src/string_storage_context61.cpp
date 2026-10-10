#include "lo_semantics/string_conversion_context61.h"
#include "lo_semantics/string_storage_context61.h"
#include "lo_semantics/manager_facade.h"
#include "lo_semantics/registered_metadata_string.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::string_storage_context61 {
namespace {
using recovery_abi::Address;
struct Bridge final : ManagerFacadeServices, ArrayResizeServices {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  Bridge(GuestMemory &memory, Dependencies deps, Registers &state)
      : m(memory), d(deps), s(state) {}
  std::uint64_t Direct(unsigned e) {
    if (!string_conversion_context61::Apply(e, m, d, s))
      d.guest.CallDirect(e, m, s);
    return s.r[3];
  }
  std::uint64_t Virtual(unsigned e) {
    s.ctr = e;
    d.guest.CallIndirect(e & ~3u, m, s);
    return s.r[3];
  }
  std::uint64_t AllocateRaw(unsigned n) override {
    s.r[3] = n;
    return Direct(0x823acbd0);
  }
  std::uint64_t ConstructPrimary(std::uint64_t p) override {
    s.r[3] = p;
    return Direct(0x827c5970);
  }
  std::uint64_t ConstructFallback(std::uint64_t p,
                                  GuestAddress manager) override {
    s.r[3] = p;
    s.r[4] = manager;
    return Direct(0x827c4ed0);
  }
  std::uint64_t CallMethod(GuestAddress e, std::uint64_t p) override {
    s.r[3] = p;
    return Virtual(e);
  }
  std::uint64_t ReleaseStorage(GuestAddress e, std::uint64_t manager,
                               std::uint64_t p) override {
    s.r[3] = manager;
    s.r[4] = p;
    return Virtual(e);
  }
  std::uint64_t AllocateStorage(GuestAddress e, std::uint64_t manager,
                                std::uint64_t bytes,
                                std::uint64_t alignment) override {
    s.r[3] = manager;
    s.r[4] = bytes;
    s.r[5] = alignment;
    return Virtual(e);
  }
  GuestAddress ResizeStorage(GuestAddress e, GuestAddress manager,
                             GuestAddress old, unsigned bytes,
                             unsigned alignment) override {
    s.r[3] = manager;
    s.r[4] = old;
    s.r[5] = bytes;
    s.r[6] = alignment;
    return Address(Virtual(e));
  }
  void InitializeManager() override { Direct(0x827c5f38); }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x823562a8) {
    auto header = Address(s.r[3]), capacity = Address(s.r[4]);
    auto previous = m.ReadU32(header + 8);
    m.WriteU32(header + 4, 0);
    if (previous != capacity) {
      m.WriteU32(header + 8, capacity);
      s.r[4] = 2;
      s.r[5] = 8;
      return string_storage_context61::Apply(0x8229f678, m, d, s);
    }
    return true;
  }
  if (e == 0x82320328 || e == 0x82474348) {
    auto old = Address(s.r[1]), header = Address(s.r[3]),
         index = Address(s.r[4]), count = Address(s.r[5]);
    unsigned frame = e == 0x82320328 ? 160 : 96,
             first = e == 0x82320328 ? 23 : 31;
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= frame;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    Bridge b(m, d, s);
    if (e == 0x82320328) {
      auto end = index + count;
      if (std::int32_t(index) < std::int32_t(end))
        for (unsigned i = 0; i < count; ++i) {
          auto row = m.ReadU32(header) + 12 * (index + i);
          // Reuse the existing inline reset/release model with its nested call
          // SP.
          s.r[3] = ResetTwoByteArray(m, b, row, sp + 96);
        }
      s.r[3] = header;
      s.r[4] = index;
      s.r[5] = count;
      s.r[6] = 12;
      s.r[7] = 8;
      (void)string_storage_context61::Apply(0x82298af8, m, d, s);
    } else {
      s.r[3] = header;
      s.r[4] = 0;
      s.r[5] = m.ReadU32(header + 4);
      (void)string_storage_context61::Apply(0x82320328, m, d, s);
      auto buffer = m.ReadU32(header);
      s.r[3] = buffer;
      if (buffer)
        s.r[3] = ReleaseManagerBuffer(m, b, buffer, sp);
      for (auto off : {0u, 4u, 8u})
        m.WriteU32(header + off, 0);
    }
    s.r[1] += frame;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x822d03d8) {
    while (true) {
      auto a = m.ReadU16(Address(s.r[3])), b = m.ReadU16(Address(s.r[4]));
      if (a != b || !b) {
        s.r[3] = a < b ? std::uint64_t(-1) : a > b ? 1 : 0;
        return true;
      }
      s.r[3] += 2;
      s.r[4] += 2;
    }
  }
  if (e == 0x8230bac0) {
    auto dest = Address(s.r[3]);
    do {
      auto word = m.ReadU16(Address(s.r[4]));
      s.r[4] += 2;
      m.WriteU16(dest, word);
      dest += 2;
      if (!word)
        break;
    } while (true);
    return true;
  }
  if (e == 0x82296830) {
    s.r[3] = registered_metadata_string::Utf16Length(m, s.r[3]);
    return true;
  }
  unsigned frame = 96, first = 31;
  switch (e) {
  case 0x82298af8:
    frame = 128;
    first = 28;
    break;
  case 0x8229f678:
    frame = 128;
    first = 27;
    break;
  case 0x82298938:
  case 0x82298a98:
    break;
  case 0x8229f5e0:
  case 0x82486c88:
    frame = 112;
    first = 30;
    break;
  case 0x822d02f8:
    frame = 368;
    first = 30;
    break;
  default:
    return false;
  }
  auto old = Address(s.r[1]), header = Address(s.r[3]),
       source = Address(s.r[4]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  s.r[31] = header;
  Bridge b(m, d, s);
  if (e == 0x82298af8)
    RemoveArrayRange(m, b, header, source, Address(s.r[5]), Address(s.r[6]),
                     Address(s.r[7]), sp);
  else if (e == 0x8229f678)
    ResizeArray(m, b, header, source, Address(s.r[5]));
  else if (e == 0x8229f5e0)
    s.r[3] =
        registered_metadata_string::AssignString(m, b, header, source, old);
  else if (e == 0x82486c88)
    s.r[3] = AllocateManagerBuffer(m, b, header, old);
  else if (e == 0x82298938)
    s.r[3] = ResetTwoByteArray(m, b, header, old);
  else if (e == 0x82298a98)
    s.r[3] = ReleaseTwoByteArray(m, b, header, old);
  else {
    s.r[30] = source;
    unsigned count = 0;
    if (m.ReadU8(source)) {
      while (m.ReadU8(source + count))
        ++count;
      ++count;
    }
    m.WriteU32(header + 4, count);
    m.WriteU32(header + 8, count);
    m.WriteU32(header, 0);
    ResizeArray(m, b, header, 2, 8);
    if (m.ReadU32(header + 4)) {
      s.r[3] = sp + 80;
      s.r[4] = source;
      b.Direct(0x823227c8);
      auto temp = Address(s.r[3]);
      s.r[3] = m.ReadU32(header);
      s.r[4] = m.ReadU32(temp + 256);
      s.r[5] = 2 * m.ReadU32(header + 4);
      b.Direct(0x82b7a0b0);
      auto storage = m.ReadU32(sp + 336);
      if (storage && storage != sp + 80) {
        s.r[3] = storage;
        (void)manager_release_context61::Apply(0x823f3340, m, d, s);
      }
    }
    s.r[3] = header;
  }
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::string_storage_context61
