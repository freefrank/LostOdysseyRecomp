#include "lo_semantics/battle_script_targets61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/battle_script_events61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
#include <limits>
namespace lo::semantic::gpu::battle_script_targets61 {
namespace {
using recovery_abi::Address;
struct Targets {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner, sp;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Actor() { return W(owner + 24); }
  unsigned Get(unsigned off) {
    s.r[3] = owner;
    s.r[4] = off;
    (void)battle_script_extensions61::Apply(0x8238c198, m, d, s);
    return Address(s.r[3]);
  }
  void Call(unsigned e) { d.guest.CallDirect(e, m, s); }
  unsigned List() {
    Call(0x82380a18);
    Call(0x8238e2f8);
    return Address(s.r[3]);
  }
  unsigned Find(unsigned id) {
    Call(0x82380a18);
    Call(0x82389b78);
    s.r[4] = id;
    Call(0x8238e308);
    return Address(s.r[3]);
  }
  float F(unsigned p) {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    return std::bit_cast<float>(W(p));
  }
  unsigned Int(double f) {
    std::int32_t v;
    if (f > double(std::numeric_limits<std::int32_t>::max()))
      v = std::numeric_limits<std::int32_t>::max();
    else if (std::isnan(f) ||
             f < double(std::numeric_limits<std::int32_t>::min()))
      v = std::numeric_limits<std::int32_t>::min();
    else
      v = std::int32_t(f);
    s.fpr_bits[0] = std::uint64_t(std::int64_t(v));
    m.WriteU32(sp + 80, unsigned(v));
    return unsigned(v);
  }
  void Pools(unsigned teamA, unsigned teamB, unsigned all, unsigned counts,
             unsigned mode) {
    m.WriteU32(sp + 308, counts);
    m.WriteU32(sp + 80, 0x8204a1d8);
    m.WriteU32(sp + 84, 0x8204a1d8);
    for (unsigned i = 0; i < 64; ++i) {
      m.WriteU32(teamA + 4 * i, 0);
      m.WriteU32(teamB + 4 * i, 0);
      m.WriteU32(all + 4 * i, 0);
    }
    unsigned count[6]{};
    auto list = List();
    float zero = 0;
    if (mode != 1)
      zero = F(0x82000e50);
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(W(List() + 4)); ++i) {
      auto resource = W(W(list) + 4 * i);
      if (mode != 1) {
        s.r[3] = resource;
        s.r[4] = 0;
        (void)battle_script_actions61::Apply(0x8238c1a0, m, d, s);
        if (Address(s.r[3]) || F(resource + 2588) <= zero)
          continue;
      }
      if (mode <= 1 && !W(resource + 132))
        continue;
      auto flags = W(resource + 124), id = W(resource + 64);
      bool upper = flags & 0x08000000, team = flags & 0x40000000;
      unsigned category = upper ? (team ? 0 : 1) : (team ? 3 : 4),
               total = upper ? 2 : 5;
      auto dest = (team ? teamA : teamB) + (upper ? 128 : 0);
      m.WriteU32(dest + 4 * count[category]++, id);
      m.WriteU32(all + (upper ? 128 : 0) + 4 * count[total]++,
                 W(resource + 64));
    }
    for (unsigned i = 0; i < 6; ++i)
      m.WriteU32(counts + 4 * i, count[i]);
    m.WriteU32(sp + 84, m.ReadU32(sp + 80));
  }
  bool HasAction(unsigned resource, unsigned filter) {
    if ((W(resource + 60) & 255) >= 4)
      return false;
    s.r[3] = owner;
    s.r[4] = W(resource + 148);
    (void)battle_script_events61::Apply(0x8238c118, m, d, s);
    auto actor = Address(s.r[3]);
    if (!actor || !(W(actor + 64) & 0x20000000))
      return false;
    auto count = W(resource + 14660), records = W(resource + 14656);
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(count); ++i) {
      auto type = W(records + 124208 * i);
      if (filter == 9 ? (type >= 6 && type <= 9)
                      : type == (filter == 13 ? 3u : 1u))
        return true;
    }
    return false;
  }
  void SelectUnavailable() {
    auto pool = Get(1), selection = Get(3);
    m.WriteU32(sp + 80, 0x8204a1d8);
    s.r[3] = owner;
    s.r[4] = sp + 896;
    s.r[5] = sp + 1152;
    s.r[6] = sp + 384;
    s.r[7] = sp + 96;
    s.r[8] = 1;
    (void)battle_script_targets61::Apply(0x8238de58, m, d, s);
    m.WriteU32(Actor() + 76, 0);
    for (unsigned i = 0; i < 64; ++i) {
      m.WriteU32(sp + 128 + 4 * i, 0);
      m.WriteU32(sp + 640 + 4 * i, 0);
    }
    unsigned candidates = 0, selected = 0;
    auto append = [&](unsigned source, unsigned count, bool omitSelf) {
      auto resource = W(Actor() + 4);
      for (unsigned i = 0; std::int32_t(i) < std::int32_t(count); ++i) {
        auto id = W(source + 4 * i);
        if (omitSelf && id == W(resource + 64))
          continue;
        m.WriteU32(sp + 128 + 4 * candidates++, id);
      }
    };
    switch (pool) {
    case 0:
      append(sp + 384, W(sp + 116), false);
      append(sp + 512, W(sp + 104), false);
      break;
    case 1:
      append(sp + 512, W(sp + 104), false);
      break;
    case 2:
      append(sp + 1024, W(sp + 96), false);
      break;
    case 3:
      append(sp + 1280, W(sp + 100), false);
      break;
    case 4:
      append(sp + 384, W(sp + 116), false);
      break;
    case 5:
      append(sp + 896, W(sp + 108), false);
      break;
    case 6:
      append(sp + 1152, W(sp + 112), false);
      break;
    case 7:
      if (W(Actor() + 4)) {
        append(sp + 384, W(sp + 116), true);
        append(sp + 512, W(sp + 104), true);
      }
      break;
    }
    for (unsigned i = 0; i < candidates; ++i) {
      auto id = W(sp + 128 + 4 * i), resource = Find(id);
      s.r[3] = resource;
      s.ctr = W(W(resource) + 292);
      d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
      if (Address(s.r[3]))
        m.WriteU32(sp + 640 + 4 * selected++, id);
    }
    unsigned result = 0;
    if (pool <= 7 && selection == 0) {
      for (unsigned i = 0; i < selected; ++i) {
        auto actor = Actor();
        m.WriteU8(W(actor + 72) + i, W(sp + 640 + 4 * i));
        actor = Actor();
        m.WriteU32(actor + 76, W(actor + 76) + 1);
      }
      result = W(Actor() + 76);
    } else if (pool <= 7 && selection == 1 && std::int32_t(selected) > 0) {
      auto resource = W(Actor() + 4);
      s.r[3] = W(0x83264558);
      s.r[4] = 0;
      s.r[5] = selected - 1;
      s.r[6] = 86;
      s.r[7] = resource ? W(resource + 64) : 31;
      Call(0x82aa0740);
      m.WriteU8(W(Actor() + 72), W(sp + 640 + 4 * Address(s.r[3])));
      m.WriteU32(Actor() + 76, 1);
      result = 1;
    }
    s.r[3] = owner;
    s.r[4] = 5;
    s.r[5] = result;
    (void)battle_script_extensions61::Apply(0x8238c208, m, d, s);
    auto actor = Actor();
    m.WriteU32(actor + 52, W(actor + 52) + 7);
    m.WriteU32(sp + 80, 0x8204a1d8);
  }
  void Select(bool refine) {
    auto filter = Get(1), parameter = Get(3), pool = Get(5), selection = Get(7);
    m.WriteU32(sp + 88, 0x8204a1d8);
    unsigned candidateBase = sp + (refine ? 112 : 144),
             resultBase = sp + (refine ? 368 : 400);
    auto initialCount = W(Actor() + 76);
    if (!refine) {
      s.r[3] = owner;
      s.r[4] = sp + 912;
      s.r[5] = sp + 1168;
      s.r[6] = sp + 656;
      s.r[7] = sp + 112;
      s.r[8] = 0;
      (void)battle_script_targets61::Apply(0x8238de58, m, d, s);
      m.WriteU32(Actor() + 76, 0);
    }
    for (unsigned i = 0; i < 64; ++i) {
      m.WriteU32(candidateBase + 4 * i, 0);
      m.WriteU32(resultBase + 4 * i, 0);
    }
    unsigned candidates = 0, selected = 0;
    auto appendPool = [&](unsigned source, unsigned count, bool omitSelf) {
      auto resource = W(Actor() + 4);
      for (unsigned i = 0; std::int32_t(i) < std::int32_t(count); ++i) {
        auto id = W(source + 4 * i);
        if (omitSelf && id == W(resource + 64))
          continue;
        m.WriteU32(candidateBase + 4 * candidates++, id);
      }
    };
    bool valid = pool <= 7 && filter <= 18 && (refine || filter != 5) &&
                 (!refine || initialCount != 0);
    if (refine) {
      if (pool <= 7 && initialCount != 0) {
        auto resource = W(Actor() + 4);
        for (unsigned i = 0;
             std::int32_t(i) < std::int32_t((pool >= 1 && pool <= 6)
                                                ? W(Actor() + 76)
                                                : initialCount);
             ++i) {
          auto id = unsigned(m.ReadU8(W(Actor() + 72) + i));
          bool include = pool == 0;
          if (pool == 7)
            include = resource && id != W(resource + 64);
          else if (pool >= 1 && pool <= 6) {
            auto flags = W(Find(id) + 124);
            bool upper = flags & 0x10000000, team = flags & 0x40000000;
            include = pool == 1   ? upper
                      : pool == 2 ? (upper && team)
                      : pool == 3 ? (upper && !team)
                      : pool == 4 ? !upper
                      : pool == 5 ? (!upper && team)
                                  : (!upper && !team);
            id = m.ReadU8(W(Actor() + 72) + i);
          }
          if (include)
            m.WriteU32(candidateBase + 4 * candidates++, id);
        }
      }
      m.WriteU32(Actor() + 76, 0);
    } else if (valid) {
      switch (pool) {
      case 0:
        appendPool(sp + 656, W(sp + 132), false);
        appendPool(sp + 784, W(sp + 120), false);
        break;
      case 1:
        appendPool(sp + 784, W(sp + 120), false);
        break;
      case 2:
        appendPool(sp + 1040, W(sp + 112), false);
        break;
      case 3:
        appendPool(sp + 1296, W(sp + 116), false);
        break;
      case 4:
        appendPool(sp + 656, W(sp + 132), false);
        break;
      case 5:
        appendPool(sp + 912, W(sp + 124), false);
        break;
      case 6:
        appendPool(sp + 1168, W(sp + 128), false);
        break;
      case 7:
        if (W(Actor() + 4)) {
          appendPool(sp + 656, W(sp + 132), true);
          appendPool(sp + 784, W(sp + 120), true);
        }
        break;
      }
    }
    auto append = [&](unsigned id) {
      m.WriteU32(resultBase + 4 * selected++, id);
    };
    unsigned group = 0xffffffff;
    std::int32_t best = filter == 8 ? 999999 : filter == 10 ? 9999 : 0;
    if (valid && filter == 7)
      for (unsigned i = 0; i < candidates; ++i) {
        auto resource = Find(W(candidateBase + 4 * i));
        if (W(resource + 64) == parameter) {
          group = W(resource + 68);
          break;
        }
      }
    if (valid && filter == 12) {
      Call(0x82380a18);
      s.r[4] = 1;
      Call(0x82a9b288);
      auto inventory = Address(s.r[3]);
      auto zero = F(0x82000e50);
      bool has = false;
      if (parameter)
        has = F(inventory + 4 * (parameter + 18)) != zero;
      else
        for (unsigned i = 0; i < 1024; ++i)
          if (F(inventory + 72 + 4 * i) != zero) {
            has = true;
            break;
          }
      if (has)
        for (unsigned i = 0; i < candidates; ++i)
          append(W(candidateBase + 4 * i));
    } else if (valid)
      for (unsigned i = 0; i < candidates; ++i) {
        auto id = W(candidateBase + 4 * i);
        if (filter == 0) {
          append(id);
          continue;
        }
        auto resource = Find(id);
        bool include = false;
        switch (filter) {
        case 1:
        case 5: {
          auto limit = float(float(F(resource + (filter == 5 ? 2620 : 2592)) *
                                   float(std::int32_t(parameter))) *
                             F(0x82000d7c));
          include = !(F(resource + (filter == 5 ? 2616 : 2588)) > limit);
          break;
        }
        case 2:
          include = W(resource + 68) == parameter;
          break;
        case 3:
        case 4:
          s.r[3] = resource;
          s.r[4] = parameter;
          Call(0x8238e368);
          include = (Address(s.r[3]) & 255) == (filter == 3 ? 1u : 0u);
          break;
        case 6:
        case 8: {
          auto value = F(resource + 2588);
          bool better =
              filter == 6 ? !(float(best) > value) : !(float(best) < value);
          if (better) {
            best = std::int32_t(Int(value));
            m.WriteU32(resultBase, id);
            selected = 1;
          }
          break;
        }
        case 7:
          include = W(resource + 68) == group;
          break;
        case 9:
        case 13:
        case 14:
          include = HasAction(resource, filter);
          break;
        case 10: {
          auto value = std::int32_t(Int(F(resource + 2600)));
          if (best >= value) {
            best = value;
            m.WriteU32(resultBase, id);
            selected = 1;
          }
          break;
        }
        case 11:
          for (unsigned j = 0; j < 5; ++j)
            if (W(resource + 5116 + 4 * j) == parameter)
              append(id);
          break;
        case 15:
          s.r[3] = sp + 88;
          s.r[4] = resource;
          Call(0x82ac85e8);
          include = (Address(s.r[3]) & 255) == 1;
          break;
        case 16:
          include = !(W(resource + 124) & 0x00100000);
          break;
        case 17:
          include = (W(resource + 124) & 0x00100000) != 0;
          break;
        case 18:
          include = !(F(resource + 2616) <= F(0x82000e50));
          break;
        }
        if (include)
          append(id);
      }
    unsigned result = 0;
    if (valid && selection == 0) {
      for (unsigned i = 0; i < selected; ++i) {
        auto actor = Actor();
        m.WriteU8(W(actor + 72) + i, W(resultBase + 4 * i));
        actor = Actor();
        m.WriteU32(actor + 76, W(actor + 76) + 1);
      }
      result = W(Actor() + 76);
    } else if (valid && selection == 1 && std::int32_t(selected) > 0) {
      auto resource = W(Actor() + 4);
      s.r[3] = W(0x83264558);
      s.r[4] = 0;
      s.r[5] = selected - 1;
      s.r[6] = refine ? 85 : 84;
      s.r[7] = resource ? W(resource + 64) : 31;
      Call(0x82aa0740);
      auto id = W(resultBase + 4 * Address(s.r[3]));
      m.WriteU8(W(Actor() + 72), id);
      m.WriteU32(Actor() + 76, 1);
      result = 1;
    }
    s.r[3] = owner;
    s.r[4] = 9;
    s.r[5] = result;
    (void)battle_script_extensions61::Apply(0x8238c208, m, d, s);
    auto actor = Actor();
    m.WriteU32(actor + 52, W(actor + 52) + 11);
    m.WriteU32(sp + 88, 0x8204a1d8);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x8238de58 && e != 0x8238d148 && e != 0x82af86a8 && e != 0x82afe6b8)
    return false;
  auto owner = Address(s.r[3]), old = Address(s.r[1]);
  unsigned first = e == 0x8238de58   ? 14
                   : e == 0x82afe6b8 ? 23
                                     : 19,
           frame = e == 0x8238de58   ? 256
                   : e == 0x82af86a8 ? 752
                                     : 1552;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  unsigned ffirst = e == 0x8238de58   ? 31
                    : e == 0x82afe6b8 ? 32
                                      : 30,
           foffset = e == 0x8238de58 ? 160 : 120;
  for (unsigned i = ffirst; i < 32; ++i)
    recovery_abi::WriteU64(m, old - foffset - 8 * (31 - i), s.fpr_bits[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  Targets t{m, d, s, owner, sp};
  if (e == 0x8238de58)
    t.Pools(Address(s.r[4]), Address(s.r[5]), Address(s.r[6]), Address(s.r[7]),
            Address(s.r[8]));
  else if (e == 0x82afe6b8)
    t.SelectUnavailable();
  else
    t.Select(e == 0x82af86a8);
  s.r[1] += frame;
  for (unsigned i = ffirst; i < 32; ++i)
    s.fpr_bits[i] = recovery_abi::ReadU64(m, old - foffset - 8 * (31 - i));
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_targets61
