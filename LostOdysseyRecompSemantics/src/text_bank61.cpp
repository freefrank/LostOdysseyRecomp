#include "lo_semantics/text_bank61.h"
#include "lo_semantics/registered_metadata_string.h"
#include "lo_semantics/recovery_abi.h"
#include <cstdint>
namespace lo::semantic::gpu::text_bank61 {
namespace {
using recovery_abi::Address;
struct Bank : ArrayResizeServices {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  Bank(GuestMemory &memory, Dependencies deps, Registers &state)
      : m(memory), d(deps), s(state) {}
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  void InitializeManager() override {
    s.r[3] = 0;
    d.guest.CallDirect(0x827c5f38, m, s);
  }
  GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
                             GuestAddress old, std::uint32_t bytes,
                             std::uint32_t argument) override {
    s.r[3] = manager;
    s.r[4] = old;
    s.r[5] = bytes;
    s.r[6] = argument;
    s.ctr = method;
    d.guest.CallIndirect(method & ~3u, m, s);
    return Address(s.r[3]);
  }
  void Manager() {
    if (!Word(0x8330b608))
      InitializeManager();
  }
  unsigned Resize(unsigned old, unsigned bytes) {
    Manager();
    auto manager = Word(0x8330b608);
    return ResizeStorage(Word(Word(manager) + 8), manager, old, bytes, 8);
  }
  void ResetString(unsigned header) {
    auto capacity = Word(header + 8);
    m.WriteU32(header + 4, 0);
    if (capacity) {
      m.WriteU32(header + 8, 0);
      ResizeArray(m, *this, header, 2, 8);
    }
    if (auto p = Word(header)) {
      Manager();
      s.r[3] = Word(0x8330b608);
      s.r[4] = p;
      s.ctr = Word(Word(Address(s.r[3])) + 12);
      d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    m.WriteU32(header, 0);
    m.WriteU32(header + 8, 0);
    m.WriteU32(header + 4, 0);
  }
  void Assign(unsigned header, unsigned source) {
    s.r[3] = registered_metadata_string::AssignString(m, *this, header, source,
                                                      Address(s.r[1]));
  }
  void Release(unsigned p) {
    Manager();
    s.r[3] = Word(0x8330b608);
    s.r[4] = p;
    s.ctr = Word(Word(Address(s.r[3])) + 12);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  void ClearStrings(unsigned vector) {
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(Word(vector + 4)); ++i)
      ResetString(Word(vector) + 12 * i);
    m.WriteU32(vector + 4, 0);
    if (Word(vector + 8)) {
      m.WriteU32(vector + 8, 0);
      if (auto p = Word(vector))
        m.WriteU32(vector, Resize(p, 0));
    }
  }
  unsigned AppendRecord(unsigned vector, unsigned stride) {
    auto used = Word(vector + 4), needed = used + 1;
    m.WriteU32(vector + 4, needed);
    if (std::int32_t(needed) > std::int32_t(Word(vector + 8))) {
      auto cap = needed + unsigned(std::int32_t(3 * needed) / 8) + 32;
      m.WriteU32(vector + 8, cap);
      if (Word(vector) || cap)
        m.WriteU32(vector, Resize(Word(vector), cap * stride));
    }
    auto p = Word(vector) + stride * used;
    for (unsigned i = 0; i < stride; ++i)
      m.WriteU8(p + i, 0);
    return p;
  }
  void Consume(unsigned columns) {
    auto input = Address(s.r[3]), output = Address(s.r[4]),
         old = Address(s.r[1]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 20; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 208;
    auto sp = Address(s.r[1]), temporary = sp + 80;
    m.WriteU32(sp, old);
    for (unsigned j = 0; j < 3; ++j)
      m.WriteU32(temporary + 4 * j, 0);
    auto end = Word(input);
    s.r[3] = input;
    s.r[4] = temporary;
    Parse();
    auto stride = columns == 7 ? 16u : 4u;
    for (unsigned offset = 16; offset < end; offset += stride) {
      auto record = input + offset;
      if (m.ReadU8(record))
        continue;
      auto out = AppendRecord(output, 12 * columns);
      for (unsigned j = 0; j < columns; ++j) {
        auto index = std::int32_t(std::int16_t(m.ReadU16(record + 2 + 2 * j)));
        auto header = Word(temporary) + 12 * unsigned(index);
        Assign(out + 12 * j, Word(header + 4) ? Word(header) : 0x821a83d0);
      }
    }
    ClearStrings(temporary);
    Release(input);
    // The temporary array destructor follows the explicit element cleanup.
    if (auto p = Word(temporary))
      Release(p);
    for (unsigned j = 0; j < 3; ++j)
      m.WriteU32(temporary + 4 * j, 0);
    s.r[1] += 208;
    for (unsigned i = 20; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = Word(old - 8);
  }
  void Parse() {
    auto input = Address(s.r[3]), output = Address(s.r[4]),
         old = Address(s.r[1]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 20; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 704;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(Word(output + 4)); ++i)
      ResetString(Word(output) + 12 * i);
    m.WriteU32(output + 4, 0);
    if (Word(output + 8)) {
      m.WriteU32(output + 8, 0);
      if (auto p = Word(output))
        m.WriteU32(output, Resize(p, 0));
    }
    auto count = std::int32_t(Word(input + 8));
    auto table = input + Word(input + 4), wide = Word(input + 12);
    for (std::int32_t i = 0; i < count; ++i) {
      auto used = Word(output + 4), needed = used + 1,
           offset = Word(table + 8 * unsigned(i));
      m.WriteU32(output + 4, needed);
      if (std::int32_t(needed) > std::int32_t(Word(output + 8))) {
        auto capacity = needed + unsigned(std::int32_t(3 * needed) / 8) + 32;
        m.WriteU32(output + 8, capacity);
        if (Word(output) || capacity)
          m.WriteU32(output, Resize(Word(output), capacity * 12));
      }
      auto header = Word(output) + 12 * used;
      for (unsigned j = 0; j < 12; ++j)
        m.WriteU8(header + j, 0);
      Assign(header, 0x821a83d0);
      auto source = table + offset;
      if (!wide) {
        // This call site supplies a valid source and a fixed 256-code-unit
        // buffer to the byte-widening CRT path. Preserve its
        // truncation/termination rule.
        for (unsigned j = 0; j < 256; ++j) {
          auto ch = m.ReadU8(source + j);
          m.WriteU16(sp + 80 + 2 * j, ch);
          if (!ch)
            break;
        }
        source = sp + 80;
      }
      Assign(Word(output) + 12 * unsigned(i), source);
    }
    s.r[1] += 704;
    for (unsigned i = 20; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = Word(old - 8);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x8230ba20) {
    auto self = Address(s.r[3]), id = Address(s.r[4]);
    auto count = std::int32_t(m.ReadU32(self + 576));
    if (count > 0) {
      auto p = m.ReadU32(self + 572);
      for (std::int32_t i = 0; i < count; ++i, p += 60)
        if (m.ReadU32(p) == id) {
          s.r[3] = p;
          return true;
        }
    }
    s.r[3] = std::uint64_t(std::int64_t(std::int32_t(0x83265614)));
    return true;
  }
  if (e == 0x8283f220) {
    Bank(m, d, s).Consume(7);
    return true;
  }
  if (e == 0x8283fbe8) {
    Bank(m, d, s).Consume(1);
    return true;
  }
  if (e != 0x82aa1b50)
    return false;
  Bank(m, d, s).Parse();
  return true;
}
} // namespace lo::semantic::gpu::text_bank61
