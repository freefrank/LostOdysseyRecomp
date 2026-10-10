#include "lo_semantics/battle_periodic_effects61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
#include <limits>
namespace lo::semantic::gpu::battle_periodic_effects61 {
namespace {
using recovery_abi::Address;
struct Sweep {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned sp;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  float F(unsigned p) { return std::bit_cast<float>(W(p)); }
  void Float(unsigned reg, float value) {
    s.fpr_bits[reg] = std::bit_cast<std::uint64_t>(double(value));
  }
  std::int32_t Trunc(float value) {
    return value > double(std::numeric_limits<std::int32_t>::max())
               ? std::numeric_limits<std::int32_t>::max()
           : !(value >= -2147483648.) ? std::numeric_limits<std::int32_t>::min()
                                      : std::int32_t(value);
  }
  unsigned Call(unsigned e, unsigned receiver) {
    s.r[3] = receiver;
    d.guest.CallDirect(e, m, s);
    return Address(s.r[3]);
  }
  unsigned Manager() {
    d.guest.CallDirect(0x82380a18, m, s);
    d.guest.CallDirect(0x82389b78, m, s);
    return Address(s.r[3]);
  }
  unsigned List() {
    d.guest.CallDirect(0x82380a18, m, s);
    d.guest.CallDirect(0x8238e2f8, m, s);
    return Address(s.r[3]);
  }
  unsigned Table(unsigned id) { return 0x83213438 + 8 * (id % 32); }
  unsigned Mask(unsigned id) { return W(Table(id) + 4); }
  unsigned Bank(unsigned id) { return W(Table(id)) + id / 32; }
  bool Has(unsigned resource, unsigned id) {
    return (W(resource + 272 * Bank(id) + 232) & Mask(id)) != 0;
  }
  unsigned Index(unsigned id) { return Call(0x82ac84e8, id); }
  void Remove(unsigned resource, unsigned id) {
    s.r[4] = id;
    s.r[5] = 1;
    Call(0x82ac8ee8, resource);
  }
  bool Tick(unsigned resource, unsigned id, unsigned base) {
    if (!Has(resource, id))
      return false;
    auto p = resource + 4 * (Index(id) + base), value = W(p);
    if (!value)
      return false;
    m.WriteU32(p, value - 1);
    if (value != 1)
      return false;
    Remove(resource, id);
    return true;
  }
  unsigned Virtual(unsigned resource, unsigned offset) {
    s.r[3] = resource;
    s.ctr = W(W(resource) + offset);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    return Address(s.r[3]);
  }
  bool Chance(unsigned resource, unsigned chance, unsigned tag) {
    s.r[4] = chance;
    s.r[5] = tag;
    s.r[6] = W(resource + 64);
    return (Call(0x82aa0838, W(0x83264558)) & 255) != 0;
  }
  unsigned Random(unsigned resource, unsigned maximum, unsigned tag) {
    s.r[4] = 0;
    s.r[5] = maximum;
    s.r[6] = tag;
    s.r[7] = W(resource + 64);
    return Call(0x82aa0740, W(0x83264558));
  }
  unsigned RandomStatus(unsigned resource, unsigned tag) {
    Random(resource, 100, tag);
    auto value = std::int32_t(W(W(0x83264558) + 4));
    auto table = value < 15   ? 0x832139b8u
                 : value < 45 ? 0x832139c4u
                              : 0x832139d4u;
    auto choice = Random(resource, value < 15 ? 2 : 3,
                         tag + (value < 15   ? 1
                                : value < 45 ? 2
                                             : 3));
    auto id = W(table + 4 * choice);
    s.r[4] = id;
    s.r[5] = 1;
    s.r[6] = 0;
    Call(0x82ac9be0, resource);
    return Mask(id);
  }
  void Stats(unsigned resource) {
    for (auto entry : {0x82ac0588u, 0x82ac25e8u}) {
      s.r[4] = resource;
      Call(entry, W(0x83291dc0));
    }
    auto stats = W(0x83291dc0);
    m.WriteU32(stats + 4, W(resource + 5108));
    for (unsigned i = 0; i < 5; ++i)
      m.WriteU32(stats + 12 + 4 * i, W(resource + 5116 + 4 * i));
    Call(0x82ac0888, stats);
    s.r[4] = resource;
    Call(0x82ac2468, W(0x83291dc0));
    s.r[4] = resource;
    s.r[5] = (W(resource + 124) >> 28) & 1;
    Call(0x82ac0620, W(0x83291dc0));
  }
  void Snapshot(unsigned resource, unsigned record) {
    for (unsigned i = 0; i < 4; ++i)
      m.WriteU32(record + 2192 * W(resource + 64) + 29732 + 4 * i,
                 unsigned(Trunc(F(
                     resource + (i < 2 ? 2588 + 4 * i : 2616 + 4 * (i - 2))))));
  }
  void ApplyAmount(unsigned resource, unsigned record, unsigned mode,
                   unsigned field, std::int32_t value, unsigned scratch) {
    recovery_abi::WriteU64(m, sp + scratch, std::uint64_t(std::int64_t(value)));
    Float(1, float(value));
    m.WriteU32(record + 40, 1);
    m.WriteU32(record + field, std::bit_cast<unsigned>(float(value)));
    s.r[4] = resource;
    s.r[6] = mode;
    Call(0x82b2bba0, W(0x832cb790));
  }
  void Disable(unsigned resource) {
    auto manager = Manager();
    s.r[4] = resource;
    s.r[5] = 1;
    s.r[6] = 0;
    Call(0x82ad0ad0, manager);
  }
  void Resource(unsigned resource, float percent, float quarter, float poison) {
    for (auto id : {226u, 227u, 245u, 247u, 248u})
      Tick(resource, id, 535);
    if (Tick(resource, 249, 535)) {
      m.WriteU32(resource + 4952, 0);
      Stats(resource);
    }
    Tick(resource, 242, 535);
    if (Has(resource, 243)) {
      auto id = W(resource + 4 * (Index(243) + 567)), manager = Manager();
      s.r[4] = id;
      auto peer = Call(0x8238e308, manager);
      auto p = resource + 4 * (Index(243) + 535), value = W(p);
      if (value) {
        m.WriteU32(p, value - 1);
        if (value == 1 || Virtual(resource, 300) || Virtual(peer, 300)) {
          Remove(resource, 243);
          Remove(peer, 244);
        }
      }
    }
    Tick(resource, 228, 535);
    Tick(resource, 229, 535);
    Tick(resource, 160, 399);
    Tick(resource, 111, 263);
    auto before = F(resource + 2588);
    Float(31, before);
    if (Has(resource, 0)) {
      Remove(resource, 19);
      Virtual(resource, 268);
      m.WriteU32(resource + 496, W(resource + 496) & ~1u);
      if (W(resource + 4952)) {
        m.WriteU32(resource + 4952, 0);
        if (W(resource + 124) & 0x10000000u)
          Stats(resource);
      }
    } else if (!(W(resource + 124) & 0x00200020u)) {
      auto record = W(resource + 14656);
      m.WriteU32(record + 36, W(resource + 64));
      unsigned excluded = 0;
      bool active = true;
      Tick(resource, 16, 59);
      if (Has(resource, 28))
        Remove(resource, 28);
      for (auto pair :
           {std::pair{5u, 46u}, std::pair{9u, 47u}, std::pair{3u, 51u}})
        if (Has(resource, pair.first) && Chance(resource, 30, pair.second))
          Remove(resource, pair.first);
      if (Has(resource, 245))
        active = false;
      else {
        bool added = false;
        if (Has(resource, 14)) {
          if (Chance(resource, 30, 35))
            Remove(resource, 14);
          else if (Chance(resource, 70, 36)) {
            excluded = RandomStatus(resource, 37);
            added = true;
          }
        }
        if (!added && Has(resource, 17)) {
          excluded |= RandomStatus(resource, 42);
          Remove(resource, 17);
        }
        if (Has(resource, 225)) {
          auto p = resource + 4 * (68 * W(Table(225)) + Index(225) + 535);
          m.WriteU32(p, W(p) + 1);
          if (std::int32_t(W(p)) >= 5 && !(W(resource + 76348) & 0x80000000u)) {
            if (!Has(resource, 15)) {
              auto bank = Bank(15), mask = Mask(15);
              if (bank || !((W(resource + 4876) | W(resource + 5088)) & mask)) {
                m.WriteU32(resource + 272 * bank + 232,
                           W(resource + 272 * bank + 232) | mask);
                if (!(W(resource + 124) & 0x10000000u)) {
                  s.r[4] = resource;
                  auto actor = Call(0x82a9bdb0, 0x832c9c54);
                  if (actor) {
                    if (!(W(actor + 64) & 0x00800000u))
                      m.WriteU32(resource + 272 * W(Table(15)) + 232,
                                 W(resource + 272 * W(Table(15)) + 232) |
                                     Mask(0));
                    else
                      m.WriteU32(actor + 64, W(actor + 64) | 0x00400000u);
                  }
                  Disable(resource);
                }
              }
            }
            active = false;
          }
        }
      }
      if (Has(resource, 15)) {
        Remove(resource, 225);
        m.WriteU32(record + 40, 1);
        active = false;
        s.r[4] = resource;
        auto actor = Call(0x82a9bdb0, 0x832c9c54);
        if (!(W(resource + 124) & 0x10000000u)) {
          if (actor && (W(actor + 64) & 0x00800000u))
            m.WriteU32(actor + 64, W(actor + 64) | 0x00400000u);
          s.r[4] = 0;
          s.r[5] = 1;
          s.r[6] = 0;
          Call(0x82ac9be0, resource);
          Disable(resource);
          m.WriteU32(resource + 132, 0);
        }
      }
      if (active && !(W(resource + 76348) & 0x80000000u)) {
        std::int32_t damage = 0;
        bool damagePresent = false;
        m.WriteU32(sp + 80, 0);
        if (Has(resource, 8)) {
          if (Has(resource, 4))
            Remove(resource, 4);
          damage = Trunc(float(F(resource + 2592) * poison));
          damagePresent = true;
        } else if (Has(resource, 4)) {
          damage = Trunc(float(F(resource + 2592) * quarter));
          damagePresent = true;
        }
        m.WriteU32(sp + 80, unsigned(damage));
        if (Has(resource, 9)) {
          recovery_abi::WriteU64(m, sp + 104,
                                 std::uint64_t(std::int64_t(damage)));
          damage =
              Trunc(float(std::fma(double(F(resource + 2592)), double(quarter),
                                   double(float(damage)))));
          damagePresent = true;
          m.WriteU32(sp + 80, unsigned(damage));
        }
        if (damagePresent) {
          ApplyAmount(resource, record, 0, 56, damage, 112);
          s.r[4] = resource;
          s.r[5] = unsigned(damage);
          Call(0x82ac7000, W(0x832aeb00));
          Snapshot(resource, record);
        }
        Float(1, before);
        s.r[4] = resource;
        s.r[5] = 0;
        s.r[6] = excluded;
        d.guest.CallDirect(0x82acad40, m, s);
      }
      if (!Has(resource, 0) && !Has(resource, 15)) {
        for (unsigned mp = 0; mp < 2; ++mp) {
          if (!Call(mp ? 0x82ab0738 : 0x82ab06a0, resource))
            continue;
          auto primary = 228 + mp, secondary = 250 + mp;
          std::int32_t rate = 0;
          m.WriteU32(sp + 80, 0);
          if (Has(resource, primary)) {
            auto value = std::int32_t(W(resource + 4 * (Index(primary) + 567)));
            if (value > 0)
              rate = value;
          }
          if (Has(resource, secondary)) {
            auto value =
                std::int32_t(W(resource + 4 * (Index(secondary) + 567)));
            if (value > rate)
              rate = value;
          }
          m.WriteU32(sp + 80, unsigned(rate));
          if (rate) {
            recovery_abi::WriteU64(m, sp + (mp ? 136 : 120),
                                   std::uint64_t(std::int64_t(rate)));
            auto amount = Trunc(
                float(float(float(rate) * F(resource + (mp ? 2620 : 2592))) *
                      percent));
            m.WriteU32(sp + 80, unsigned(amount));
            ApplyAmount(resource, record, mp ? 3 : 1, mp ? 104 : 72,
                        amount ? amount : 1, mp ? 144 : 128);
            Snapshot(resource, record);
          }
        }
      }
      if (!(float(F(resource + 2592) * quarter) < F(resource + 2588))) {
        if (!Has(resource, 0) && !Has(resource, 15)) {
          auto manager = Manager();
          if (!(m.ReadU16(manager + 148) & 1) && !Has(resource, 1)) {
            auto bank = Bank(1), mask = Mask(1);
            if (bank || !((W(resource + 4876) | W(resource + 5088)) & mask))
              m.WriteU32(resource + 272 * bank + 232,
                         W(resource + 272 * bank + 232) | mask);
          }
        }
      } else
        Remove(resource, 1);
      Remove(resource, 19);
      Virtual(resource, 268);
    }
    for (unsigned bit = 0; bit < 32; ++bit)
      if (W(resource + 4956) & (1u << bit)) {
        auto p = resource + 4960 + 4 * bit, value = W(p);
        if (value == 99)
          continue;
        --value;
        m.WriteU32(p, value);
        if (std::int32_t(value) <= 0)
          m.WriteU32(resource + 4956, W(resource + 4956) & ~(1u << bit));
      }
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82acb120)
    return false;
  auto old = Address(s.r[1]), mode = Address(s.r[4]) & 255;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = 14; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  for (unsigned i = 28; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 160 - 8 * (31 - i), s.fpr_bits[i]);
  s.r[1] -= 336;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  m.WriteU32(sp + 84, 0x8204a1d8);
  m.WriteU32(sp + 88, 0x8204a1d8);
  if (s.cached_fp_control & 0x8040) {
    s.cached_fp_control &= ~0x8040u;
    d.fp.SetHostFpControl(s.cached_fp_control);
  }
  Sweep run{m, d, s, sp};
  if (mode == 1) {
    auto rows = run.List();
    m.WriteU32(sp + 96, rows);
    auto percent = run.F(0x82218384), quarter = run.F(0x82000b3c),
         poison = run.F(0x82000da4);
    run.Float(29, percent);
    run.Float(30, quarter);
    run.Float(28, poison);
    for (unsigned i = 0;; ++i) {
      m.WriteU32(sp + 100, i);
      auto current = run.List();
      if (!(std::int32_t(i) < std::int32_t(run.W(current + 4))))
        break;
      run.Resource(run.W(run.W(rows) + 4 * i), percent, quarter, poison);
    }
  }
  for (auto method : {0x82ac7fc8u, 0x82ac6e60u, 0x82ac6f08u})
    for (unsigned side : {0u, 1u}) {
      s.r[4] = side;
      run.Call(method, run.W(0x832aeb00));
    }
  m.WriteU32(sp + 88, m.ReadU32(sp + 84));
  s.r[1] += 336;
  for (unsigned i = 28; i < 32; ++i)
    s.fpr_bits[i] = recovery_abi::ReadU64(m, old - 160 - 8 * (31 - i));
  for (unsigned i = 14; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_periodic_effects61
