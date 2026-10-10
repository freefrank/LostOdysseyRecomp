#include "lo_semantics/battle_effect_execution61.h"
#include "lo_semantics/battle_result_application61.h"
#include "lo_semantics/battle_effect_calculation61.h"
#include "lo_semantics/battle_action_results61.h"
#include "lo_semantics/battle_action_records61.h"
#include "lo_semantics/battle_action_readiness61.h"
#include "lo_semantics/battle_property_mutation61.h"
#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <limits>
namespace lo::semantic::gpu::battle_effect_execution61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_effect_execution61::Apply(e, m, d, s) &&
      !battle_result_application61::Apply(e, m, d, s) &&
      !battle_effect_calculation61::Apply(e, m, d, s) &&
      !battle_action_results61::Apply(e, m, d, s) &&
      !battle_action_records61::Apply(e, m, d, s) &&
      !battle_action_readiness61::Apply(e, m, d, s) &&
      !battle_property_mutation61::Apply(e, m, d, s) &&
      !battle_script_actions61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
std::int32_t Trunc(double x) {
  return x > double(std::numeric_limits<std::int32_t>::max())
             ? std::numeric_limits<std::int32_t>::max()
         : !(x >= -2147483648.) ? std::numeric_limits<std::int32_t>::min()
                                : std::int32_t(x);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82b22870 && e != 0x82b22948)
    return false;
  auto owner = Address(s.r[3]), old = Address(s.r[1]);
  unsigned frame = e == 0x82b22870 ? 112 : 176,
           first = e == 0x82b22870 ? 29 : 26;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  if (e == 0x82b22948) {
    recovery_abi::WriteU64(m, old - 72, s.fpr_bits[30]);
    recovery_abi::WriteU64(m, old - 64, s.fpr_bits[31]);
  }
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  auto manager = [&](unsigned method) {
    Call(0x82380a18, m, d, s);
    Call(method, m, d, s);
    return Address(s.r[3]);
  };
  if (e == 0x82b22870) {
    if ((m.ReadU32(owner + 40) & 1) && m.ReadU32(m.ReadU32(owner + 8) + 196) &&
        (m.ReadU32(m.ReadU32(owner + 8) + 200) & 0x80000000u)) {
      auto list = manager(0x8238e2f8);
      unsigned i = 0;
      while (std::int32_t(i) <
             std::int32_t(m.ReadU32(manager(0x8238e2f8) + 4))) {
        auto resource = m.ReadU32(m.ReadU32(list) + 4 * i),
             target = m.ReadU32(owner + 8);
        if (m.ReadU32(resource + 196) == m.ReadU32(target + 196) &&
            !(m.ReadU32(resource + 200) & 0x80000000u)) {
          s.r[3] = m.ReadU32(owner + 4);
          s.r[4] = s.r[5] = 0xffffffffffffffffull;
          s.r[6] = m.ReadU32(resource + 64);
          s.r[7] = 0;
          Call(0x82ab36c8, m, d, s);
        }
        ++i;
      }
    }
  } else {
    m.WriteU32(sp + 80, 0x8204a1d8);
    auto f = [&](unsigned reg, double value) {
      s.fpr_bits[reg] = std::bit_cast<std::uint64_t>(value);
    };
    auto load = [&](unsigned address, unsigned reg) {
      if (s.cached_fp_control & 0x8040) {
        s.cached_fp_control &= ~0x8040u;
        d.fp.SetHostFpControl(s.cached_fp_control);
      }
      auto x = std::bit_cast<float>(m.ReadU32(address));
      f(reg, double(x));
      return x;
    };
    auto store = [&](unsigned address, unsigned reg) {
      m.WriteU32(address, std::bit_cast<unsigned>(
                              float(std::bit_cast<double>(s.fpr_bits[reg]))));
    };
    auto own = [&](unsigned address) {
      s.r[3] = owner;
      Call(address, m, d, s);
    };
    auto result = [&](unsigned address) {
      s.r[3] = m.ReadU32(0x832cb790);
      Call(address, m, d, s);
    };
    auto recordAt = [&](unsigned report, unsigned offset) {
      return m.ReadU32(report + 20) + 4 * (116 * m.ReadU32(report + 12) +
                                           m.ReadU32(report + 24) + offset);
    };
    auto record = [&](unsigned offset, float value) {
      m.WriteU32(recordAt(m.ReadU32(0x832cb790), offset),
                 std::bit_cast<unsigned>(value));
    };
    auto property = [&](unsigned resource, unsigned id) {
      s.r[3] = resource;
      s.r[4] = id;
      Call(0x8238e368, m, d, s);
      return (Address(s.r[3]) & 255) != 0;
    };
    auto rounded = [&](float x) {
      auto n = Trunc(x);
      s.fpr_bits[0] = std::bit_cast<std::uint64_t>(std::int64_t(n));
      m.WriteU32(sp + 88, unsigned(n));
      recovery_abi::WriteU64(m, sp + 88, std::uint64_t(std::int64_t(n)));
      f(0, double(float(n)));
      return float(n);
    };
    auto run = [&]() {
      own(0x82b22870);
      auto target = m.ReadU32(owner + 8);
      for (unsigned i = 0; i < 5; ++i)
        if (m.ReadU32(target + 4836 + 4 * i) == 15)
          m.WriteU8(owner + 65, 1);
      own(0x82b1fcc8);
      if (!(Address(s.r[3]) & 255))
        return;
      own(0x82b1ff20);
      own(0x82b204c8);
      if (m.ReadU32(owner + 52) == 2) {
        result(0x82b2b310);
        result(0x82b2b298);
        s.r[3] = 0x832c9c54;
        s.r[4] = m.ReadU32(owner + 8);
        Call(0x82af6a48, m, d, s);
      } else {
        own(0x82b20270);
        own(0x82b20650);
        store(owner + 20, 1);
        own(0x82b20ef0);
        store(owner + 20, 1);
        if (m.ReadU8(owner + 45) == 1) {
          s.r[3] = m.ReadU32(0x832ca0d8);
          Call(0x82b09d98, m, d, s);
          store(owner + 20, 1);
        }
        auto amount = float(std::bit_cast<double>(s.fpr_bits[1]));
        f(0, double(amount));
        if (m.ReadU8(owner + 36) == 1) {
          auto scale = load(0x82000e1c, 0);
          amount = float(amount * scale);
          f(0, double(amount));
        }
        m.WriteU32(owner + 68, std::bit_cast<unsigned>(amount));
        m.WriteU32(owner + 20, std::bit_cast<unsigned>(amount));
        own(0x82b207a0);
        store(owner + 72, 1);
        own(0x82b20b30);
        store(owner + 76, 1);
        own(0x82b20d38);
        store(owner + 80, 1);
        own(0x82b21068);
        store(owner + 20, 1);
        own(0x82b21148);
        auto bias = load(0x8201f9f0, 30);
        if (m.ReadU32(0x832ca0e0 + 5784) == 2) {
          auto targetId = m.ReadU32(m.ReadU32(owner + 8) + 64);
          if (targetId == 21 || targetId == 22) {
            auto find = [&](unsigned id) {
              manager(0x82389b78);
              s.r[4] = id;
              Call(0x8238e308, m, d, s);
              return Address(s.r[3]);
            };
            auto firstTarget = find(21), secondTarget = find(22);
            for (auto resource : {firstTarget, secondTarget}) {
              s.r[3] = resource;
              s.r[4] = 131;
              Call(0x82ac9000, m, d, s);
            }
            auto unavailable = [&](unsigned resource) {
              s.r[3] = resource;
              s.ctr = m.ReadU32(m.ReadU32(resource) + 292);
              d.guest.CallIndirect(Address(s.ctr), m, s);
              return Address(s.r[3]) != 0;
            };
            if (!unavailable(firstTarget) && !unavailable(secondTarget)) {
              s.r[3] = 131;
              Call(0x8238aab0, m, d, s);
              s.r[5] = s.r[3];
              s.r[3] = m.ReadU32(m.ReadU32(owner + 8) + 64) == 21
                           ? firstTarget
                           : secondTarget;
              s.r[4] = 4;
              s.r[6] = 8;
              s.r[7] = 0;
              s.r[8] = 1;
              Call(0x82ac8ec8, m, d, s);
            }
          }
          if (m.ReadU32(m.ReadU32(owner + 4) + 64) == 20 &&
              property(m.ReadU32(owner + 8), 19)) {
            auto value = load(owner + 24, 0);
            f(0, double(float(value * bias)));
            store(owner + 24, 0);
          }
        }
        if (m.ReadU8(owner + 36) == 1)
          result(0x82b2b2c0);
        auto zero = load(0x82000e50, 31);
        auto response = m.ReadU32(owner + 52);
        bool saveResult = true;
        if (response == 1) {
          m.WriteU32(owner + 24, std::bit_cast<unsigned>(zero));
          result(0x82b2b438);
        }
        if (response == 4) {
          m.WriteU32(owner + 24, std::bit_cast<unsigned>(zero));
          result(0x82b2b2e8);
          result(0x82b2b438);
        }
        response = m.ReadU32(owner + 52);
        if (response == 3 || response == 8) {
          record(3730, load(owner + 24, 0));
          if (response == 3)
            saveResult = false;
        } else {
          if (response == 7) {
            m.WriteU32(owner + 24, std::bit_cast<unsigned>(zero));
            result(0x82b2b438);
          } else if (response == 6) {
            auto value = load(owner + 24, 13), one = load(0x82007784, 0);
            auto adjusted = rounded(float(float(value + one) * bias));
            m.WriteU32(owner + 24, std::bit_cast<unsigned>(adjusted));
          }
          if (m.ReadU32(owner + 52) == 5) {
            auto value = load(owner + 24, 13),
                 mp = rounded(load(m.ReadU32(owner + 8) + 2616, 0));
            auto rest = float(mp - value);
            f(12, double(rest));
            if (!(rest < zero)) {
              record(3734, value);
              m.WriteU32(owner + 24, std::bit_cast<unsigned>(zero));
            } else {
              record(3734, mp);
              auto current = load(owner + 24, 13);
              auto deficit = float(current - mp);
              f(0, double(deficit));
              record(3726, deficit);
            }
          } else
            record(3726, load(owner + 24, 0));
        }
        if (m.ReadU32(owner + 60) &&
            (m.ReadU8(owner + 45) != 1 ||
             (m.ReadU32(m.ReadU32(owner + 4) + 124) & 0x10000000u)))
          own(0x82b21480);
        m.WriteU8(m.ReadU32(0x832cb790) + 16, m.ReadU32(owner + 52) == 2);
        result(0x82b2b248);
        result(0x82b2bd50);
        store(owner + 24, 1);
        if (saveResult)
          store(owner + 92, 1);
        own(0x82b22100);
        if (m.ReadU8(owner + 36) == 1 && property(m.ReadU32(owner + 4), 98) &&
            !m.ReadU8(owner + 120)) {
          auto report = m.ReadU32(0x832cb790);
          s.r[3] = report;
          s.r[4] = m.ReadU32(owner + 4);
          s.r[6] = 1;
          auto hp = load(Address(s.r[4]) + 2592, 13),
               factor = load(0x82000dac, 0);
          f(1, double(float(hp * factor)));
          Call(0x82b2b9e0, m, d, s);
          store(recordAt(report, 18), 1);
          result(0x82b2b270);
        }
        load(owner + 96, 1);
        s.r[4] = m.ReadU32(owner + 8);
        s.r[5] = m.ReadU32(m.ReadU32(owner + 4) + 4916);
        s.r[6] = m.ReadU32(owner + 104);
        Call(0x82acad40, m, d, s);
      }
      auto value = m.ReadU32(owner + 84);
      m.WriteU32(manager(0x82389b78) + 196, value);
      value = m.ReadU32(owner + 88);
      m.WriteU32(manager(0x82389b78) + 200, value);
      auto total = load(owner + 92, 31);
      auto address = manager(0x82389b78) + 192;
      auto integer = Trunc(total);
      s.fpr_bits[0] = std::bit_cast<std::uint64_t>(std::int64_t(integer));
      m.WriteU32(address, unsigned(integer));
      s.r[3] = m.ReadU32(owner + 4);
      s.r[4] = 47;
      Call(0x82ac9000, m, d, s);
    };
    run();
    m.WriteU32(sp + 80, 0x8204a1d8);
  }
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  if (e == 0x82b22948) {
    s.fpr_bits[30] = recovery_abi::ReadU64(m, old - 72);
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - 64);
  }
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_effect_execution61
