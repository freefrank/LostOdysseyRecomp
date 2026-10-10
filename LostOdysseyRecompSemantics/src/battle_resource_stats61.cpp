#include "lo_semantics/battle_resource_stats61.h"
#include "lo_semantics/battle_action_effects61.h"
#include "lo_semantics/battle_property_mutation61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/string_storage_context61.h"
#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/battle_evaluation_chance61.h"
#include <limits>
#include <utility>
#include <bit>
namespace lo::semantic::gpu::battle_resource_stats61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_resource_stats61::Apply(e, m, d, s) &&
      !string_storage_context61::Apply(e, m, d, s) &&
      !battle_script_actions61::Apply(e, m, d, s) &&
      !battle_action_readiness61::Apply(e, m, d, s) &&
      !battle_evaluation_chance61::Apply(e, m, d, s) &&
      !battle_property_mutation61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82b1f1d0)
    return battle_action_effects61::Apply(e, m, d, s);
  if (e == 0x82ab0b10) {
    auto resource = Address(s.r[3]), value = m.ReadU32(0x82000e50);
    m.WriteU32(resource + 112, value);
    m.WriteU32(resource + 116, value);
    return battle_action_effects61::Apply(0x82b1f1d0, m, d, s);
  }
  if (e == 0x82acd530) {
    auto old = Address(s.r[1]), resource = Address(s.r[4]);
    m.WriteU32(old - 8, Address(s.lr));
    recovery_abi::WriteU64(m, old - 16, s.r[31]);
    s.r[1] -= 96;
    m.WriteU32(Address(s.r[1]), old);
    if (m.ReadU32(resource + 100) & 0x40000000) {
      s.r[3] = resource;
      (void)battle_resource_stats61::Apply(0x82ab0b10, m, d, s);
      m.WriteU32(resource + 100, m.ReadU32(resource + 100) & ~0x40000000u);
    }
    s.r[1] += 96;
    s.r[31] = recovery_abi::ReadU64(m, old - 16);
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82ac3118) {
    auto old = Address(s.r[1]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 28; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 128;
    m.WriteU32(Address(s.r[1]), old);
    Call(0x82380a18, m, d, s);
    Call(0x8238e2f8, m, d, s);
    auto list = Address(s.r[3]);
    for (unsigned i = 0;; ++i) {
      Call(0x82380a18, m, d, s);
      Call(0x8238e2f8, m, d, s);
      if (std::int32_t(i) >= std::int32_t(m.ReadU32(Address(s.r[3]) + 4)))
        break;
      auto resource = m.ReadU32(m.ReadU32(list) + 4 * i);
      for (unsigned off = 0; off < 192; off += 4)
        m.WriteU32(resource + 75932 + off, m.ReadU32(0x83213368 + off));
    }
    s.r[1] += 128;
    for (unsigned i = 28; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  auto trunc = [](double x) {
    return x > double(std::numeric_limits<std::int32_t>::max())
               ? std::numeric_limits<std::int32_t>::max()
           : !(x >= -2147483648.) ? std::numeric_limits<std::int32_t>::min()
                                  : std::int32_t(x);
  };
  if (e == 0x82acf108 || e == 0x82acf280) {
    auto owner = Address(s.r[3]), old = Address(s.r[1]);
    bool allied = e == 0x82acf108;
    unsigned frame = allied ? 160 : 144, first = allied ? 24 : 25;
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= frame;
    m.WriteU32(Address(s.r[1]), old);
    auto scene = [&]() { return m.ReadU32(m.ReadU32(0x832c9c54 + 44) + 28); };
    unsigned result = 1;
    if ((scene() & 0x1800000) != (allied ? 0x1000000u : 0x800000u)) {
      Call(0x82380a18, m, d, s);
      Call(0x82389b78, m, d, s);
      auto flags = m.ReadU16(Address(s.r[3]) + 148);
      unsigned total = 0, inactive = 0;
      bool allOrdinary = true;
      for (unsigned i = 0;
           std::int32_t(i) < std::int32_t(m.ReadU32(m.ReadU32(owner + 20) + 4));
           ++i) {
        auto resource = m.ReadU32(m.ReadU32(m.ReadU32(owner + 20)) + 4 * i);
        if (bool(m.ReadU32(resource + 124) & 0x8000000) != allied ||
            !m.ReadU32(resource + 132))
          continue;
        ++total;
        s.r[3] = resource;
        d.guest.CallIndirect(m.ReadU32(m.ReadU32(resource) + 308), m, s);
        if (Address(s.r[3])) {
          ++inactive;
          if (allied) {
            resource = m.ReadU32(m.ReadU32(m.ReadU32(owner + 20)) + 4 * i);
            s.r[3] = resource;
            d.guest.CallIndirect(m.ReadU32(m.ReadU32(resource) + 380), m, s);
            if (Address(s.r[3]))
              allOrdinary = false;
          }
        }
      }
      m.WriteU32(owner + (allied ? 24 : 28), total - inactive);
      result = total == inactive;
      if (allied) {
        if ((allOrdinary && (flags & 2)) || (flags & 8) || (scene() & 0x200000))
          result = 0;
      } else if ((flags & 4) || (scene() & 0x400000))
        result = 0;
    }
    s.r[3] = result;
    s.r[1] += frame;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82ac0888) {
    auto old = Address(s.r[1]), resource = Address(s.r[4]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 14; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    recovery_abi::WriteU64(m, old - 160, s.fpr_bits[31]);
    s.r[1] -= 816;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    m.WriteU32(sp + 132, 0x8204a1d8);
    m.WriteU32(sp + 136, 0x8204a1d8);
    m.WriteU32(sp + 128, 0x83264978);
    const unsigned elementOffsets[] = {112, 104, 116, 80, 108};
    for (auto off : elementOffsets)
      m.WriteU32(sp + off, 0xffffffff);
    for (auto off : {124u, 120u, 88u, 96u, 100u})
      m.WriteU32(sp + off, 0);
    for (unsigned i = 0; i < 128; ++i)
      m.WriteU32(sp + 144 + 4 * i, 0);
    unsigned count = 0;
    auto initialCount = std::int32_t(m.ReadU32(resource + 5156));
    for (std::int32_t i = 0; i < initialCount; ++i) {
      auto id = m.ReadU32(resource + 5160 + 4 * unsigned(i));
      if (id)
        m.WriteU32(sp + 144 + 4 * count++, id);
    }
    auto equipment = m.ReadU32(0x83264978);
    for (unsigned i = 0; i < 5; ++i) {
      auto id =
          m.ReadU32(equipment + 196 * m.ReadU32(resource + 5116 + 4 * i) + 24);
      if (!id)
        continue;
      bool found = false;
      for (unsigned j = 0; j < count; ++j)
        if (m.ReadU32(sp + 144 + 4 * j) == id) {
          found = true;
          break;
        }
      if (!found)
        m.WriteU32(sp + 144 + 4 * count++, id);
    }
    auto mask = [&](unsigned id) {
      s.r[3] = id;
      Call(0x8238aab0, m, d, s);
      return Address(s.r[3]);
    };
    auto present = [&](unsigned id) {
      s.r[3] = resource;
      s.r[4] = id;
      Call(0x8238e368, m, d, s);
      return Address(s.r[3]) & 255;
    };
    auto simple = [&](unsigned bank, unsigned bits, unsigned value) {
      s.r[3] = resource;
      s.r[4] = bank;
      s.r[5] = bits;
      s.r[6] = value;
      s.r[7] = 0;
      Call(0x82ac8968, m, d, s);
    };
    auto paired = [&](unsigned bank, unsigned bits, unsigned value,
                      unsigned aux) {
      s.r[3] = resource;
      s.r[4] = bank;
      s.r[5] = bits;
      s.r[6] = value;
      s.r[7] = aux;
      s.r[8] = 1;
      Call(0x82ac8ec8, m, d, s);
    };
    for (unsigned id = 48; id <= 52; ++id) {
      s.r[3] = resource;
      s.r[4] = id;
      Call(0x82ac9000, m, d, s);
    }
    unsigned cumulative[7]{}, maxima[8]{}, special[2]{};
    maxima[4] = 100;
    constexpr unsigned sumIds[] = {44, 45, 39, 40, 41, 42, 43};
    constexpr unsigned maxIds[] = {230, 239, 236, 237, 252, 240, 241, 238};
    constexpr unsigned maxOffsets[] = {124, 120, 88, 0, 0, 96, 100, 0};
    for (unsigned i = 0; i < count; ++i) {
      auto id = m.ReadU32(sp + 144 + 4 * i);
      if (!id)
        continue;
      auto row = m.ReadU32(0x83264978 + 72) + 104 * id;
      auto type = m.ReadU32(row + 44), bank = m.ReadU32(row + 48),
           bits = m.ReadU32(row + 52), value = m.ReadU32(row + 64);
      if (type == 2) {
        m.WriteU32(resource + 4956,
                   m.ReadU32(resource + 4956) | m.ReadU32(row + 76));
      } else if (type == 6) {
        m.WriteU32(resource + 4876,
                   m.ReadU32(resource + 4876) | bits | m.ReadU32(row + 60));
      } else if (type == 3 && bank == 1) {
        for (unsigned j = 0; j < 5; ++j)
          if (bits == mask(48 + j)) {
            auto p = sp + elementOffsets[j];
            if (std::int32_t(value) > std::int32_t(m.ReadU32(p)))
              m.WriteU32(p, value);
            break;
          }
      } else if (type == 1) {
        if (bank == 1) {
          unsigned total = value;
          for (unsigned j = 0; j < 7; ++j)
            if (bits == mask(sumIds[j])) {
              cumulative[j] += value;
              total = cumulative[j];
              break;
            }
          simple(bank, bits, total);
        } else if (bank == 7) {
          bool handled = false;
          for (unsigned j = 0; j < 8; ++j)
            if (bits == mask(maxIds[j])) {
              handled = true;
              bool write =
                  j == 6 ||
                  (j == 4 ? std::int32_t(value) < std::int32_t(maxima[j])
                          : std::int32_t(value) > std::int32_t(maxima[j]));
              if (write) {
                auto total = j == 6 ? maxima[j] + value : value;
                if (j == 6)
                  m.WriteU32(sp + 100, total);
                paired(bank, bits, total, m.ReadU32(row + 68));
                maxima[j] = j == 6 ? total : m.ReadU32(row + 64);
                if (maxOffsets[j])
                  m.WriteU32(sp + maxOffsets[j], maxima[j]);
              }
              break;
            }
          if (handled)
            continue;
          if (bits != mask(232) && bits != mask(233)) {
            paired(bank, bits, value, m.ReadU32(row + 68));
            continue;
          }
          bool insert = m.ReadU32(row + 72) == 0;
          for (unsigned part = 0; part < 2; ++part) {
            for (unsigned j = 0; j < 2; ++j) {
              if (m.ReadU32(row + (part ? 60 : 52)) != mask(232 + j))
                continue;
              auto v = m.ReadU32(row + (part ? 68 : 64));
              // The secondary value intentionally still uses the primary mask.
              if (insert)
                paired(bank, m.ReadU32(row + 52), v, m.ReadU32(row + 68));
              v = m.ReadU32(row + (part ? 68 : 64));
              if (std::int32_t(v) > std::int32_t(special[j]))
                special[j] = v;
            }
          }
        } else if (bank == 4 && (bits == mask(131) || bits == mask(132))) {
          paired(4, bits, value, 0);
        } else {
          s.r[3] = resource;
          s.r[4] = bank;
          s.r[5] = bits;
          s.r[6] = 0;
          Call(0x82aca6f0, m, d, s);
        }
      }
    }
    if (count) {
      for (unsigned j = 0; j < 2; ++j)
        if (special[j] && present(232 + j))
          paired(7, mask(232 + j), special[j], 0);
      for (auto j : {4u, 0u, 1u, 2u, 3u}) {
        auto value = m.ReadU32(sp + elementOffsets[j]);
        if (value != 0xffffffff)
          simple(1, mask(48 + j), value);
      }
    }
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto load = [&](unsigned p) { return std::bit_cast<float>(m.ReadU32(p)); };
    auto factor = load(0x82000d7c);
    s.fpr_bits[31] = std::bit_cast<std::uint64_t>(double(factor));
    struct Modifier {
      unsigned id, base, bonus, scale;
    };
    for (auto mod : {Modifier{44, 2472, 2532, 10},
                     {45, 2500, 2560, 10},
                     {39, 2476, 2536, 5},
                     {41, 2480, 2540, 5},
                     {43, 0, 2548, 5},
                     {40, 2504, 2564, 5},
                     {42, 0, 2568, 5}}) {
      if (present(mod.id) != 1)
        continue;
      auto base = mod.base ? trunc(load(resource + mod.base)) : 0;
      if (mod.base)
        m.WriteU32(sp + 80, unsigned(base));
      s.r[3] = resource;
      s.r[4] = mod.id;
      Call(0x82ac9858, m, d, s);
      auto coefficient = std::int32_t(Address(s.r[3]) * mod.scale);
      auto delta =
          mod.base
              ? trunc(float(float(float(coefficient) * float(base)) * factor))
              : coefficient;
      if (mod.base)
        m.WriteU32(sp + 80, unsigned(delta));
      recovery_abi::WriteU64(m, sp + 88, std::uint64_t(std::int64_t(delta)));
      auto result = float(float(delta) + load(resource + mod.bonus));
      s.fpr_bits[13] = std::bit_cast<std::uint64_t>(double(float(delta)));
      s.fpr_bits[0] = std::bit_cast<std::uint64_t>(double(result));
      m.WriteU32(resource + mod.bonus, std::bit_cast<unsigned>(result));
    }
    if (m.ReadU32(resource + 124) & 0x10000000) {
      auto row = m.ReadU32(0x83264978 + 36) + 204 * m.ReadU32(resource + 68);
      m.WriteU32(resource + 5156, m.ReadU32(row + 36));
      if (!m.ReadU32(resource + 152) && present(241) == 1)
        m.WriteU32(resource + 5156,
                   m.ReadU32(resource + 5156) + m.ReadU32(sp + 100));
      auto capacity = m.ReadU32(resource + 5156) + m.ReadU32(resource + 4936);
      m.WriteU32(resource + 5156, std::int32_t(capacity) > 30 ? 30 : capacity);
      m.WriteU32(resource + 5152, 1);
      if (present(240) == 1)
        m.WriteU32(resource + 5152, m.ReadU32(sp + 96));
      if (std::int32_t(m.ReadU32(resource + 5152)) > 3)
        m.WriteU32(resource + 5152, 3);
    }
    m.WriteU32(sp + 136, m.ReadU32(sp + 132));
    s.r[1] += 816;
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - 160);
    for (unsigned i = 14; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82ac0100) {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto value = trunc(std::bit_cast<double>(s.fpr_bits[1]));
    m.WriteU32(Address(s.r[1]) - 16, unsigned(value));
    if (value % 5)
      value =
          std::int32_t(unsigned(std::int32_t(unsigned(value) + 5) / 10) * 10);
    recovery_abi::WriteU64(m, Address(s.r[1]) - 16,
                           std::uint64_t(std::int64_t(value)));
    s.fpr_bits[0] = std::bit_cast<std::uint64_t>(double(value));
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(float(value)));
    return true;
  }
  if (e == 0x82ac0388) {
    auto resource = Address(s.r[4]),
         row = m.ReadU32(0x83264978) + 196 * m.ReadU32(resource + 5112);
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto bonus = float(std::bit_cast<float>(m.ReadU32(resource + 2536)) +
                       std::bit_cast<float>(m.ReadU32(row + 56)));
    m.WriteU32(resource + 2536, std::bit_cast<unsigned>(bonus));
    s.fpr_bits[0] = std::bit_cast<std::uint64_t>(double(bonus));
    unsigned traits[] = {m.ReadU32(row + 184), m.ReadU32(row + 188),
                         m.ReadU32(row + 192)};
    for (auto off : {4916u, 4828u, 2652u})
      m.WriteU32(resource + off, 0);
    for (unsigned i = 0; i < 3; ++i) {
      auto trait = m.ReadU32(0x83264978) + 196 * (traits[i] + 768),
           category = m.ReadU32(trait + 184), kind = m.ReadU32(trait + 188);
      m.WriteU32(resource + 76252 + 4 * i, category);
      m.WriteU32(resource + 76264 + 4 * i, kind);
      m.WriteU32(resource + 76276 + 4 * i, m.ReadU32(trait + 192));
      if (category >= 1 && category <= 3) {
        auto off = category == 1 ? 4916 : category == 2 ? 4828 : 2652;
        m.WriteU32(resource + off, m.ReadU32(resource + off) | kind);
      }
    }
    s.r[3] = 76276;
    return true;
  }
  if (e != 0x82ac1860 && e != 0x82ac0620 && e != 0x82ac3058 &&
      e != 0x82ac0170 && e != 0x82ac2468)
    return false;
  auto owner = Address(s.r[3]), resource = Address(s.r[4]),
       mode = Address(s.r[5]), old = Address(s.r[1]);
  unsigned frame = e == 0x82ac1860   ? 144
                   : e == 0x82ac2468 ? 688
                                     : 112,
           first = e == 0x82ac1860   ? 26
                   : e == 0x82ac2468 ? 23
                   : e == 0x82ac0170 ? 31
                                     : 30;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  if (e == 0x82ac0170) {
    auto row = mode, slot = Address(s.r[6]);
    if (m.ReadU32(row + 12) == 1)
      m.WriteU32(resource + 180, m.ReadU32(row + 52));
    auto type = m.ReadU32(row + 124);
    auto floating = [&]() {
      if (s.cached_fp_control & 0x8040) {
        s.cached_fp_control &= ~0x8040u;
        d.fp.SetHostFpControl(s.cached_fp_control);
      }
    };
    auto f = [&](unsigned reg, double value) {
      s.fpr_bits[reg] = std::bit_cast<std::uint64_t>(value);
    };
    auto load = [&](unsigned p) { return std::bit_cast<float>(m.ReadU32(p)); };
    if (type == 8 || type == 9) {
      floating();
      auto n = std::int32_t(m.ReadU32(row + 128));
      recovery_abi::WriteU64(m, sp + 80, std::uint64_t(std::int64_t(n)));
      auto product = float(float(n) * load(resource + 2472));
      auto value = trunc(float(double(product) * double(load(0x82000d7c)) +
                               double(load(0x8201f9f0))));
      m.WriteU32(sp + 80, unsigned(value));
      if (type == 8) {
        value =
            std::int32_t(unsigned(std::int32_t(unsigned(value) + 5) / 10) * 10);
        recovery_abi::WriteU64(m, sp + 80, std::uint64_t(std::int64_t(value)));
        auto total = float(float(value) + load(resource + 2532));
        f(0, total);
        m.WriteU32(resource + 2532, std::bit_cast<unsigned>(total));
      } else {
        recovery_abi::WriteU64(m, sp + 80, std::uint64_t(std::int64_t(value)));
        f(1, float(value));
        s.r[3] = m.ReadU32(0x832c09c4);
        Call(0x82ac0100, m, d, s);
        auto total = float(load(resource + 2532) -
                           float(std::bit_cast<double>(s.fpr_bits[1])));
        f(0, total);
        m.WriteU32(resource + 2532, std::bit_cast<unsigned>(total));
      }
    } else if (type == 10) {
      floating();
      auto n = std::int32_t(m.ReadU32(row + 128));
      recovery_abi::WriteU64(m, sp + 80, std::uint64_t(std::int64_t(n)));
      f(13, float(n));
      auto value = float(float(n) + load(resource + 2544));
      f(0, value);
      m.WriteU32(resource + 2544, std::bit_cast<unsigned>(value));
    } else if (slot && (type == 12 || type == 13 || type == 15))
      m.WriteU32(resource + 4 * (slot + 1208), type);
  } else if (e == 0x82ac2468) {
    m.WriteU32(sp + 80, 0x8204a1d8);
    auto main = m.ReadU32(owner + 4);
    m.WriteU32(resource + 4948, main);
    m.WriteU32(owner + 8, main);
    m.WriteU32(resource + 4828, 0);
    m.WriteU32(resource + 4832, 0);
    for (unsigned i = 0; i < 5; ++i) {
      m.WriteU32(resource + 4836 + 4 * i, 0);
      m.WriteU32(resource + 4856 + 4 * i, 0);
    }
    auto load = [&](unsigned p, unsigned reg) {
      if (s.cached_fp_control & 0x8040) {
        s.cached_fp_control &= ~0x8040u;
        d.fp.SetHostFpControl(s.cached_fp_control);
      }
      auto v = std::bit_cast<float>(m.ReadU32(p));
      s.fpr_bits[reg] = std::bit_cast<std::uint64_t>(double(v));
      return v;
    };
    for (unsigned i = 0; i < 6; ++i) {
      auto row = m.ReadU32(0x83264978) + 196 * m.ReadU32(owner + 8 + 4 * i);
      s.r[3] = sp + 96;
      s.r[4] = m.ReadU32(row + 4) ? m.ReadU32(row) : 0x821a83d0;
      Call(0x8230bac0, m, d, s);
      if (m.ReadU32(row + 12) == 1) {
        auto kind = m.ReadU32(row + 52);
        m.WriteU32(resource + 180, kind);
        m.WriteU32(resource + 2648, kind);
        for (auto pair : {std::pair{0x20u, 5100u}, std::pair{0x10u, 5096u}})
          if (m.ReadU32(row + 20) & pair.first) {
            auto n = trunc(load(row + 88, 0));
            s.fpr_bits[0] = std::bit_cast<std::uint64_t>(std::int64_t(n));
            m.WriteU32(resource + pair.second, unsigned(n));
          }
      }
      s.r[3] = owner;
      s.r[4] = resource;
      s.r[5] = row;
      s.r[6] = i;
      Call(0x82ac0170, m, d, s);
      auto a = float(load(resource + 2536, 13) + load(row + 56, 0));
      s.fpr_bits[0] = std::bit_cast<std::uint64_t>(double(a));
      m.WriteU32(resource + 2536, std::bit_cast<unsigned>(a));
      auto b = float(load(resource + 2540, 12) + load(row + 60, 0));
      s.fpr_bits[0] = std::bit_cast<std::uint64_t>(double(b));
      m.WriteU32(resource + 2540, std::bit_cast<unsigned>(b));
    }
    s.r[3] = owner;
    s.r[4] = resource;
    Call(0x82ac0388, m, d, s);
    m.WriteU32(sp + 80, 0x8204a1d8);
  } else if (e == 0x82ac1860) {
    m.WriteU32(sp + 80, 0x8204a1d8);
    for (unsigned offset = 0; offset < 26624; offset += 104) {
      auto row = m.ReadU32(0x83264978 + 72) + offset;
      if (m.ReadU32(row + 44) != 1)
        continue;
      s.r[3] = resource;
      s.r[4] = m.ReadU32(row + 48);
      s.r[5] = m.ReadU32(row + 52);
      Call(0x82ac91d8, m, d, s);
      if (m.ReadU32(row + 60)) {
        s.r[3] = resource;
        s.r[4] = m.ReadU32(row + 56);
        s.r[5] = m.ReadU32(row + 60);
        Call(0x82ac91d8, m, d, s);
      }
    }
    m.WriteU32(sp + 80, 0x8204a1d8);
  } else if (e == 0x82ac3058) {
    Call(0x82380a18, m, d, s);
    Call(0x82ab0110, m, d, s);
    for (unsigned i = 0; i < 60; i += 4)
      m.WriteU32(resource + 2528 + i, 0);
    s.r[3] = owner;
    s.r[4] = resource;
    Call(0x82ac1860, m, d, s);
    m.WriteU32(m.ReadU32(0x83291dc0) + 4, m.ReadU32(resource + 5108));
    for (unsigned i = 0; i < 5; ++i)
      m.WriteU32(m.ReadU32(0x83291dc0) + 12 + 4 * i,
                 m.ReadU32(resource + 5116 + 4 * i));
    for (auto method : {0x82ac0888u, 0x82ac2468u, 0x82ac0620u}) {
      s.r[3] = owner;
      s.r[4] = resource;
      if (method == 0x82ac0620)
        s.r[5] = (m.ReadU32(resource + 124) >> 28) & 1;
      Call(method, m, d, s);
    }
  } else {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto f = [&](unsigned reg, double value) {
      s.fpr_bits[reg] = std::bit_cast<std::uint64_t>(value);
    };
    auto load = [&](unsigned p, unsigned reg) {
      auto value = std::bit_cast<float>(m.ReadU32(p));
      f(reg, value);
      return value;
    };
    auto put = [&](unsigned off, float value) {
      m.WriteU32(resource + off, std::bit_cast<unsigned>(value));
    };
    s.r[3] = resource;
    auto one = load(0x82007784, 11);
    auto hp =
        float(float(load(resource + 4920, 0) + load(resource + 2532, 13)) +
              load(resource + 2472, 13));
    f(0, hp);
    put(2592, hp);
    if (hp < one)
      put(2592, one);
    if (mode) {
      auto limit = load(0x822184dc, 0);
      if (load(resource + 2592, 13) > limit)
        put(2592, limit);
    }
    auto maxHP = load(resource + 2592, 12);
    if (load(resource + 2588, 0) > maxHP)
      put(2588, maxHP);
    auto mp =
             float(float(load(resource + 4924, 0) + load(resource + 2560, 13)) +
                   load(resource + 2500, 13)),
         zero = load(0x82000e50, 13);
    f(0, mp);
    put(2620, mp);
    if (mp < zero)
      put(2620, zero);
    if (mode) {
      auto limit = load(0x822181e4, 0);
      if (load(resource + 2620, 13) > limit)
        put(2620, limit);
    }
    auto maxMP = load(resource + 2620, 0);
    if (load(resource + 2616, 13) > maxMP)
      put(2616, maxMP);
    if (!mode) {
      put(2588, maxHP);
      put(2616, maxMP);
    }
    for (unsigned i = 0; i < 4; ++i) {
      auto value = float(load(resource + 2536 + 4 * i, 13 - i) +
                         load(resource + 2476 + 4 * i, 12 - i));
      put(2596 + 4 * i, value);
    }
    auto speed = float(load(resource + 2552, 0) + load(resource + 2492, 13)),
         cap = load(0x822182a0, 13);
    f(0, speed);
    put(2612, speed);
    if (speed > cap)
      put(2612, cap);
    if (load(resource + 2612, 0) < one)
      put(2612, one);
    for (unsigned i = 0; i < 4; ++i) {
      auto value = float(load(resource + 2564 + 4 * i, 12 - i) +
                         load(resource + 2504 + 4 * i, 10 - i));
      put(2624 + 4 * i, value);
    }
    put(2644, float(load(resource + 2584, 6) + load(resource + 2524, 7)));
    auto other = float(load(resource + 2580, 0) + load(resource + 2520, 12));
    f(0, other);
    put(2640, other);
    if (other > cap)
      put(2640, cap);
    if (load(resource + 2640, 0) < one)
      put(2640, one);
    m.WriteU32(sp + 80, 0x8204a1d8);
    auto threshold = float(load(resource + 2592, 13) * load(0x82000b3c, 0));
    f(0, threshold);
    s.r[3] = resource;
    s.r[4] = 1;
    if (threshold < load(resource + 2588, 13))
      Call(0x82ac9000, m, d, s);
    else {
      s.r[5] = 1;
      s.r[6] = 0;
      Call(0x82ac9be0, m, d, s);
    }
    m.WriteU32(sp + 80, 0x8204a1d8);
  }
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_resource_stats61
