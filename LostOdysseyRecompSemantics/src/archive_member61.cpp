#include "lo_semantics/archive_member61.h"
#include "lo_semantics/archive_names61.h"
#include "lo_semantics/archive_metadata61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::archive_member61 {
namespace {
using recovery_abi::Address;
struct Lookup {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  unsigned Length(unsigned p) {
    unsigned n = 0;
    while (m.ReadU8(p + n))
      ++n;
    return n;
  }
  unsigned Enter(unsigned size) {
    auto old = Address(s.r[1]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 24; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= size;
    m.WriteU32(Address(s.r[1]), old);
    return old;
  }
  void Leave(unsigned old, unsigned size) {
    s.r[1] += size;
    for (unsigned i = 24; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = Word(old - 8);
  }
  unsigned Name(unsigned archive, unsigned record, unsigned out) {
    s.r[3] = archive;
    s.r[4] = record;
    s.r[5] = out;
    (void)archive_names61::Apply(0x82853030, m, s);
    return Address(s.r[3]);
  }
  unsigned ArchivePath(unsigned archive, unsigned out) {
    auto end = out + 48 + Length(out + 48);
    Name(archive, archive + 24, end);
    if ((Word(archive + 36) & 1) && Word(0x83264d94) == 1) {
      s.r[3] = end;
      (void)archive_metadata61::Apply(0x82852db8, m, d, s);
    }
    return end;
  }
  void Stamp(unsigned packed, unsigned out, unsigned sp) {
    s.r[3] = sp + 96;
    s.r[4] = packed;
    (void)archive_metadata61::Apply(0x828527a0, m, d, s);
    recovery_abi::WriteU64(m, sp + 80, 0);
    s.r[3] = sp + 96;
    s.r[4] = sp + 80;
    (void)archive_metadata61::Apply(0x82be2f60, m, d, s);
    recovery_abi::WriteU64(m, out + 24, recovery_abi::ReadU64(m, sp + 80));
  }
  bool Member(unsigned archive, unsigned entries, std::int32_t count,
              unsigned path, unsigned prefix, unsigned out) {
    auto old = Enter(288), sp = Address(s.r[1]);
    s.r[3] = sp + 160;
    s.r[4] = path + prefix;
    (void)archive_names61::Apply(0x82852d68, m, s);
    auto segment = Address(s.r[3]);
    unsigned record = 0, index = 0;
    bool found = false;
    for (std::int32_t i = 0; i < count; ++i) {
      auto p = entries + 24 * unsigned(i), size = Name(archive, p, sp + 112);
      if (!size || size != segment)
        continue;
      bool equal = true;
      for (unsigned j = 0; j <= size; ++j)
        if (m.ReadU8(sp + 112 + j) != m.ReadU8(sp + 160 + j)) {
          equal = false;
          break;
        }
      if (equal) {
        record = p;
        index = unsigned(i);
        found = true;
        break;
      }
    }
    bool success = false;
    if (found) {
      auto next = prefix + segment;
      if (m.ReadU8(path + next)) {
        if (Word(record) & 0x10000000)
          success = Member(archive, Word(record + 20), m.ReadU16(record + 14),
                           path, next + 1, out);
      } else {
        auto end = ArchivePath(archive, out);
        m.WriteU8(out, m.ReadU8(archive));
        m.WriteU8(out + 1, m.ReadU8(archive + 1));
        unsigned type = 0;
        if (Word(record) & 0x10000000)
          type = 16;
        else {
          auto packedOffset = Word(record + 8);
          if ((packedOffset & 0xc0000000) != 0xc0000000) {
            type = 1;
            recovery_abi::WriteU64(m, out + 16, unsigned(packedOffset << 11));
          } else {
            m.WriteU8(out + 4, m.ReadU8(out + 4) | 4);
            unsigned i = 0;
            do {
              auto ch = m.ReadU8(path + i);
              m.WriteU8(end + i, ch);
              ++i;
              if (!ch)
                break;
            } while (true);
          }
          auto flags = m.ReadU8(record + 14);
          m.WriteU8(out + 5, flags & 3);
          if (flags & 16)
            m.WriteU8(out + 4, m.ReadU8(out + 4) | 2);
          if (Word(record) & 0x80000000)
            m.WriteU8(out + 4, m.ReadU8(out + 4) | 1);
          m.WriteU8(out + 2, m.ReadU8(record + 15));
          m.WriteU32(out + 8, Word(record + 16));
          m.WriteU32(out + 12, Word(record + 20));
        }
        m.WriteU8(out + 3, type);
        Stamp(record + 4, out, sp);
        m.WriteU32(out + 36, archive);
        m.WriteU32(out + 40, record);
        m.WriteU32(out + 44, index);
        success = true;
      }
    }
    Leave(old, 288);
    s.r[3] = success;
    return success;
  }
  void Find() {
    auto header = Address(s.r[3]), original = Address(s.r[4]),
         path = Address(s.r[5]), length = Address(s.r[6]),
         out = Address(s.r[7]), archiveOut = Address(s.r[8]);
    auto old = Enter(704), sp = Address(s.r[1]);
    s.r[3] = header;
    s.r[4] = sp + 112;
    s.r[5] = 32;
    s.r[6] = path;
    s.r[7] = archiveOut;
    (void)archive_names61::Apply(0x82853da8, m, s);
    auto count = std::int32_t(s.r[3]);
    bool result = false;
    if (count < 0) {
      auto archive = Word(archiveOut);
      ArchivePath(archive, out);
      m.WriteU8(out, m.ReadU8(archive));
      m.WriteU8(out + 1, m.ReadU8(archive + 1));
      m.WriteU8(out + 3, 17);
      Stamp(archive + 28, out, sp);
      m.WriteU32(out + 36, archive);
      result = true;
    } else {
      for (std::int32_t i = 0; i < count; ++i) {
        auto candidate = sp + 112 + 16 * unsigned(i), archive = Word(candidate),
             prefix = Word(candidate + 8);
        m.WriteU32(archiveOut, archive);
        if (Member(archive, Word(archive + 4), m.ReadU16(archive + 2), path,
                   prefix ? prefix + 1 : 0, out)) {
          result = true;
          break;
        }
      }
      if (result && (m.ReadU8(out + 4) & 4)) {
        auto end = out + 48 + Length(out + 48);
        for (unsigned i = 0; i < length; ++i)
          m.WriteU8(end + i, m.ReadU8(original + i));
      }
    }
    Leave(old, 704);
    s.r[3] = result;
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  Lookup l{m, d, s};
  if (e == 0x828548a0) {
    l.Find();
    return true;
  }
  if (e == 0x82853b28) {
    (void)l.Member(Address(s.r[3]), Address(s.r[4]), std::int32_t(s.r[5]),
                   Address(s.r[6]), Address(s.r[7]), Address(s.r[8]));
    return true;
  }
  return false;
}
} // namespace lo::semantic::gpu::archive_member61
