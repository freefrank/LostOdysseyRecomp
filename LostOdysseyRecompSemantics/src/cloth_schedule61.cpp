#include "lo_semantics/cloth_schedule61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/cloth_schedule_support61.h"
#include "lo_semantics/cloth_storage61.h"
#include <array>
#include <cstdint>
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::cloth_schedule61 {
namespace {
using recovery_abi::Address;
struct Schedule {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned Word(unsigned p) { return m.ReadU32(p); }
  void Invoke(unsigned slot) {
    s.r[3] = Word(0x832df548);
    s.ctr = Word(Word(Address(s.r[3])) + slot);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  unsigned Allocate(unsigned bytes) {
    s.r[4] = bytes;
    s.r[5] = 282;
    Invoke(8);
    return Address(s.r[3]);
  }
  void Free(unsigned p) {
    s.r[4] = p;
    Invoke(20);
  }
  void Copy(unsigned out, unsigned in, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i)
      m.WriteU8(out + i, m.ReadU8(in + i));
  }
  void Resize(unsigned v, unsigned count) {
    auto start = Word(v), end = Word(v + 4), used = (end - start) / 4;
    if (count > used) {
      if ((Word(v + 8) - start) / 4 < count) {
        auto p = Allocate(2 * count * 4);
        Copy(p, start, end - start);
        if (start)
          Free(start);
        start = p;
        m.WriteU32(v, p);
        m.WriteU32(v + 8, p + 2 * count * 4);
      }
      for (unsigned i = 4 * used; i < 4 * count; ++i)
        m.WriteU8(start + i, 0);
    }
    end = start + 4 * count;
    m.WriteU32(v + 4, end);
    if (end == start) {
      if (start)
        Free(start);
      m.WriteU32(v, 0);
      m.WriteU32(v + 4, 0);
      m.WriteU32(v + 8, 0);
    } else if (Word(v + 8) > end) {
      s.r[4] = start;
      s.r[5] = count * 4;
      Invoke(16);
      start = Address(s.r[3]);
      m.WriteU32(v, start);
      m.WriteU32(v + 4, start + count * 4);
      m.WriteU32(v + 8, start + count * 4);
    }
  }
  unsigned Append(unsigned vector, unsigned stride) {
    auto start = Word(vector), end = Word(vector + 4),
         used = (end - start) / stride;
    if (Word(vector + 8) <= end) {
      auto bytes = 2 * (used + 1) * stride, p = Allocate(bytes);
      Copy(p, start, end - start);
      if (start)
        Free(start);
      m.WriteU32(vector, p);
      m.WriteU32(vector + 8, p + bytes);
      end = p + used * stride;
    }
    m.WriteU32(vector + 4, end + stride);
    return end;
  }
  void Push(unsigned vector, unsigned value) {
    m.WriteU32(Append(vector, 4), value);
  }
  unsigned Child() {
    s.r[4] = 44;
    s.r[5] = 285;
    Invoke(8);
    auto p = Address(s.r[3]);
    if (p)
      for (unsigned offset : {0, 4, 8, 12, 24, 28, 32})
        m.WriteU32(p + offset, 0);
    return p;
  }
  float Float(unsigned p) { return std::bit_cast<float>(Word(p)); }
  void Float(unsigned p, float v) { m.WriteU32(p, std::bit_cast<unsigned>(v)); }
  void Run() {
    auto self = Address(s.r[3]), old = Address(s.r[1]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 464;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    auto points = Word(self + 716), constraints = Word(self + 720),
         output = Word(self + 732), ranges = Word(self + 728);
    auto count = (Word(points + 4) - Word(points)) / 12,
         items = (Word(constraints + 4) - Word(constraints)) / 68,
         kind = Word(self + 60);
    unsigned limit = 960;
    if (m.ReadU8(self + 64)) {
      auto cost = kind == 1 ? 25u : kind == 2 ? 45u : Word(sp + 108);
      limit = (60 - cost) * 16;
    }
    auto child = Child();
    unsigned tier = 0, childCount = 0, used = 0, stageUsed = 0;
    auto labels = [&]() { return self + 68 + 20 * tier; };
    auto resetBox = [&]() {
      for (unsigned j = 0; j < 3; ++j) {
        m.WriteU32(sp + 176 + 4 * j, Word(0x82000e0c));
        m.WriteU32(sp + 188 + 4 * j, Word(0x82000d64));
      }
    };
    resetBox();
    // Local owned vectors for processed constraints and the adjacency frontier.
    auto processed = sp + 112, frontier = sp + 140;
    for (unsigned j = 0; j < 3; ++j) {
      m.WriteU32(processed + 4 * j, 0);
      m.WriteU32(frontier + 4 * j, 0);
    }
    Resize(processed, items);
    for (unsigned i = 0; i < items; ++i)
      m.WriteU32(Word(processed) + 4 * i, 0xffffffff);
    auto resetLabels = [&]() {
      Resize(labels(), count);
      for (unsigned i = 0; i < count; ++i)
        m.WriteU32(Word(labels()) + 4 * i, 0xffffffff);
    };
    resetLabels();
    s.r[3] = self;
    (void)cloth_schedule_support61::Apply(0x82bb59a8, m, d, s);
    m.WriteU32(ranges + 4, Word(ranges));
    Push(ranges, 0);
    auto flush = [&]() {
      Push(output, child);
      ++childCount;
      child = Child();
      used = 0;
      m.WriteU32(frontier + 4, Word(frontier));
      resetBox();
    };
    for (;;) {
      m.WriteU32(sp + 84, 0xffffffff);
      m.WriteU32(sp + 100, 0xffffffff);
      m.WriteU32(sp + 96, Word(0x82000e0c));
      bool attempted = false;
      auto consider = [&](unsigned id) {
        if (Word(Word(processed) + 4 * id) != 0xffffffff)
          return;
        s.r[3] = self;
        s.r[4] = id;
        s.r[5] = childCount;
        s.r[6] = labels();
        s.r[7] = sp + 176;
        s.r[8] = sp + 100;
        s.r[9] = sp + 96;
        s.r[10] = sp + 84;
        (void)cloth_schedule_support61::Apply(0x82bb56d0, m, d, s);
        attempted = true;
      };
      for (unsigned p = Word(frontier); p < Word(frontier + 4); p += 4)
        consider(Word(p));
      if (std::int32_t(Word(sp + 84)) < 0)
        for (unsigned i = 0; i < items; ++i)
          consider(i);
      auto selected = Word(sp + 84);
      if (!attempted) {
        if (stageUsed)
          ++tier;
        break;
      }
      if (std::int32_t(selected) >= 0) {
        auto record = Word(constraints) + 68 * selected;
        unsigned cost = 8;
        for (unsigned j = 0; j < 4; ++j) {
          auto v = Word(record + 4 * j);
          if (v != 0xffffffff && Word(Word(labels()) + 4 * v) == 0xffffffff)
            cost += 16;
        }
        if (std::int32_t(used + cost) > std::int32_t(limit) && used) {
          flush();
          continue;
        }
        auto packed = sp + 208;
        if (kind == 1) {
          for (unsigned j = 0; j < 6; ++j)
            m.WriteU16(packed + 2 * j, 0xffff);
          for (unsigned j = 0; j < 3; ++j)
            m.WriteU32(packed + 12 + 4 * j, Word(record + 28 + 4 * j));
          m.WriteU16(packed + 28, m.ReadU16(record + 24));
        } else if (kind == 2) {
          for (unsigned j = 0; j < 4; ++j)
            m.WriteU16(packed + 2 * j, 0xffff);
          m.WriteU32(packed + 8, Word(record + 40));
          for (unsigned j = 0; j < 6; ++j)
            m.WriteU16(packed + 12 + 2 * j, Word(record + 44 + 4 * j) >> 16);
        }
        for (unsigned j = 0; j < 4; ++j) {
          auto v = Word(record + 4 * j);
          if (v == 0xffffffff)
            continue;
          auto assigned = Word(labels()) + 4 * v;
          if (Word(assigned) == 0xffffffff) {
            m.WriteU32(assigned, childCount);
            Push(child + 4, v);
            used += 16;
            stageUsed += 16;
            auto point = Word(points) + 12 * v;
            for (unsigned k = 0; k < 3; ++k) {
              auto value = Float(point + 4 * k), lo = Float(sp + 176 + 4 * k),
                   hi = Float(sp + 188 + 4 * k);
              Float(sp + 176 + 4 * k, float(lo - value) >= 0 ? value : lo);
              Float(sp + 188 + 4 * k, float(hi - value) >= 0 ? hi : value);
            }
            auto start = Word(Word(self + 20) + 4 * v),
                 n = Word(Word(self) + 4 * v);
            for (unsigned k = 0; k < n; ++k) {
              auto id = Word(Word(self + 40) + 4 * (start + k));
              if (Word(Word(processed) + 4 * id) == 0xffffffff)
                Push(frontier, id);
            }
          }
          auto n = (Word(child + 8) - Word(child + 4)) / 4;
          for (unsigned k = 0; k < n; ++k)
            if (Word(Word(child + 4) + 4 * k) == v) {
              m.WriteU16(packed + 2 * j, k);
              break;
            }
        }
        m.WriteU32(Word(processed) + 4 * selected, childCount);
        Copy(Append(child + 24, 32), packed, 32);
        used += 8;
        stageUsed += 8;
        continue;
      }
      if (used)
        flush();
      if (tier == 0) {
        unsigned target = 0xffffffff, bytes = limit;
        for (unsigned v = 0; v < count; ++v) {
          if (Word(Word(self + 68) + 4 * v) != 0xffffffff)
            continue;
          if (std::int32_t(bytes + 16) > std::int32_t(limit)) {
            for (;;) {
              ++target;
              if (target >= childCount) {
                target = childCount;
                bytes = 0;
                flush();
                break;
              }
              auto p = Word(Word(output) + 4 * target);
              bytes = 16 * ((Word(p + 8) - Word(p + 4)) / 4) +
                      8 * ((Word(p + 28) - Word(p + 24)) / 32);
              if (std::int32_t(bytes + 16) <= std::int32_t(limit))
                break;
            }
          }
          m.WriteU32(Word(self + 68) + 4 * v, target);
          Push(Word(Word(output) + 4 * target) + 4, v);
          bytes += 16;
          if (target == childCount)
            used += 16;
        }
      }
      stageUsed = 0;
      ++tier;
      if (tier == 32)
        break;
      resetLabels();
      Push(ranges, childCount);
    }
    if (used) {
      Push(output, child);
      ++childCount;
    } else if (child) {
      s.r[3] = child;
      (void)cloth_storage61::Apply(0x82ba7f10, m, d, s);
      Free(child);
    }
    m.WriteU32(self + 708, childCount);
    m.WriteU32(self + 712, tier);
    if (Word(processed))
      Free(Word(processed));
    if (Word(frontier))
      Free(Word(frontier));
    s.r[1] += 464;
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = Word(old - 8);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82bb6290)
    return false;
  Schedule{m, d, s}.Run();
  return true;
}
} // namespace lo::semantic::gpu::cloth_schedule61
