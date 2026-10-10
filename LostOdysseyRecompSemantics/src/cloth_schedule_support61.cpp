#include "lo_semantics/cloth_schedule_support61.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
#include <cstdint>
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::cloth_schedule_support61 {
namespace {
using recovery_abi::Address;
struct Scheduling {
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
  float Float(unsigned p) { return std::bit_cast<float>(Word(p)); }
  void Score() {
    auto args = s.r;
    auto self = Address(args[3]), id = Address(args[4]),
         bucket = Address(args[5]);
    auto labels = Word(Address(args[6])), box = Address(args[7]),
         bestScore = Address(args[8]), bestMetric = Address(args[9]),
         bestID = Address(args[10]);
    auto constraint = Word(Word(self + 720)) + 68 * id;
    unsigned vertices[4];
    std::int32_t score = 0;
    for (unsigned j = 0; j < 4; ++j) {
      auto v = vertices[j] = Word(constraint + 4 * j);
      if (std::int32_t(v) < 0)
        ++score;
      else {
        auto label = Word(labels + 4 * v);
        if (label == bucket)
          ++score;
        else if (std::int32_t(label) >= 0)
          score = -10;
      }
    }
    float metric = Float(bestMetric);
    if (score >= 1) {
      float low[3], high[3];
      for (unsigned k = 0; k < 3; ++k) {
        low[k] = Float(box + 4 * k);
        high[k] = Float(box + 12 + 4 * k);
      }
      auto points = Word(Word(self + 716));
      for (unsigned j = 0; j < (std::int32_t(vertices[3]) < 0 ? 2u : 4u); ++j)
        for (unsigned k = 0; k < 3; ++k) {
          auto value = Float(points + 12 * vertices[j] + 4 * k);
          high[k] = float(high[k] - value) >= 0 ? high[k] : value;
          low[k] = float(low[k] - value) >= 0 ? value : low[k];
        }
      auto x = float(high[0] - low[0]), y = float(high[1] - low[1]),
           z = float(high[2] - low[2]);
      metric = float(float(std::abs(float(y - z)) + std::abs(float(x - y))) +
                     std::abs(float(z - x)));
    }
    auto best = std::int32_t(Word(bestScore));
    if (score > best || (score == best && metric < Float(bestMetric))) {
      m.WriteU32(bestMetric, std::bit_cast<unsigned>(metric));
      m.WriteU32(bestScore, unsigned(score));
      m.WriteU32(bestID, id);
    }
  }
  void Adjacency() {
    auto self = Address(s.r[3]), old = Address(s.r[1]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 22; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 176;
    m.WriteU32(Address(s.r[1]), old);
    auto points = Word(self + 716), constraints = Word(self + 720);
    auto count = (Word(points + 4) - Word(points)) / 12,
         items = (Word(constraints + 4) - Word(constraints)) / 68;
    Resize(self, count);
    for (unsigned i = 0; i < count; ++i)
      m.WriteU32(Word(self) + 4 * i, 0);
    for (unsigned i = 0; i < items; ++i)
      for (unsigned j = 0; j < 4; ++j) {
        auto v = Word(Word(constraints) + 68 * i + 4 * j);
        if (std::int32_t(v) >= 0) {
          auto p = Word(self) + 4 * v;
          m.WriteU32(p, Word(p) + 1);
        }
      }
    Resize(self + 20, count);
    unsigned total = 0;
    for (unsigned i = 0; i < count; ++i) {
      m.WriteU32(Word(self + 20) + 4 * i, total);
      auto p = Word(self) + 4 * i;
      total += Word(p);
      m.WriteU32(p, 0);
    }
    Resize(self + 40, total);
    for (unsigned i = 0; i < items; ++i)
      for (unsigned j = 0; j < 4; ++j) {
        auto v = Word(Word(constraints) + 68 * i + 4 * j);
        if (std::int32_t(v) >= 0) {
          auto p = Word(self) + 4 * v, used = Word(p),
               offset = Word(Word(self + 20) + 4 * v);
          m.WriteU32(p, used + 1);
          m.WriteU32(Word(self + 40) + 4 * (offset + used), i);
        }
      }
    s.r[1] += 176;
    for (unsigned i = 22; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = Word(old - 8);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  Scheduling t{m, d, s};
  if (e == 0x82bb56d0)
    t.Score();
  else if (e == 0x82bb59a8)
    t.Adjacency();
  else
    return false;
  return true;
}
} // namespace lo::semantic::gpu::cloth_schedule_support61
