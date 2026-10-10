#include "lo_semantics/battle_formation61.h"
#include "lo_semantics/battle_manager_access61.h"
#include "lo_semantics/mesh_hull_incremental61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::battle_formation61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_formation61::Apply(e, m, d, s) &&
      !(e == 0x82389aa0 && battle_manager_access61::Apply(e, m, d, s)))
    d.guest.CallDirect(e, m, s);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  unsigned frame = 0, first = 32;
  switch (e) {
  case 0x82acf5d8:
  case 0x82af5a88:
  case 0x82af5498:
    break;
  case 0x82af5b20:
    frame = 144;
    first = 27;
    break;
  case 0x82af5320:
    frame = 176;
    first = 27;
    break;
  case 0x82af5ba8:
    frame = 224;
    first = 18;
    break;
  default:
    return false;
  }
  auto old = Address(s.r[1]), owner = Address(s.r[3]), arg4 = Address(s.r[4]),
       arg5 = Address(s.r[5]);
  if (frame) {
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= frame;
    m.WriteU32(Address(s.r[1]), old);
  }
  if (e == 0x82acf5d8)
    recovery_abi::WriteU64(m, old - 8, s.r[31]);
  if (e == 0x82af5320)
    for (unsigned i = 26; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 48 - 8 * (32 - i), s.fpr_bits[i]);
  if (e == 0x82af5ba8)
    recovery_abi::WriteU64(m, old - 128, s.fpr_bits[31]);
  auto sp = Address(s.r[1]);
  auto get = [&](unsigned p) { return std::bit_cast<float>(m.ReadU32(p)); };
  auto put = [&](unsigned p, float v) {
    m.WriteU32(p, std::bit_cast<unsigned>(v));
  };
  auto fp = [&]() {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
  };
  if (e == 0x82acf5d8) {
    auto outB = Address(s.r[6]);
    m.WriteU32(outB, 0);
    m.WriteU32(arg5, 0);
    auto list = m.ReadU32(owner + 20);
    for (unsigned i = 0;
         std::int32_t(i) < std::int32_t(m.ReadU32(m.ReadU32(owner + 20) + 4));
         ++i) {
      auto resource = m.ReadU32(m.ReadU32(list) + 4 * i),
           flags = m.ReadU32(resource + 124);
      if (((flags >> 27) & 1) != (arg4 & 255))
        continue;
      auto output = (flags & 0x40000000) ? arg5 : outB;
      m.WriteU32(output, m.ReadU32(output) + 1);
    }
    s.r[3] = 1;
  } else if (e == 0x82af5a88) {
    m.WriteU32(arg5, 0);
    m.WriteU32(arg4, 0);
    bool nonempty = m.ReadU32(owner + 20) != 0;
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(m.ReadU32(owner + 20));
         ++i) {
      if (m.ReadU32(m.ReadU32(owner + 16) + 20 * i) == 0)
        m.WriteU32(arg4, m.ReadU32(arg4) + 1);
      if (m.ReadU32(m.ReadU32(owner + 16) + 20 * i) == 2)
        m.WriteU32(arg5, m.ReadU32(arg5) + 1);
    }
    s.r[3] = nonempty;
  } else if (e == 0x82af5498) {
    auto count = m.ReadU32(owner + 20), data = m.ReadU32(owner + 16);
    unsigned ordinal = 0;
    s.r[3] = std::uint64_t(-1);
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(count); ++i) {
      auto type = m.ReadU32(data + 20 * i);
      if (!((type == 0 && (arg4 & 255) == 1) ||
            (type == 2 && (arg4 & 255) == 0)))
        continue;
      if (ordinal++ == arg5) {
        s.r[3] = i;
        break;
      }
    }
  } else if (e == 0x82af5b20) {
    std::uint64_t result = std::uint64_t(-1);
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(m.ReadU32(owner + 20));
         ++i) {
      s.r[3] = m.ReadU32(owner + 16) + 28 * i;
      s.r[4] = sp + 80;
      s.r[5] = sp + 84;
      Call(0x82af5a88, m, d, s);
      if (m.ReadU32(sp + 80) == arg4 && m.ReadU32(sp + 84) == arg5) {
        result = i;
        break;
      }
    }
    s.r[3] = result;
  } else if (e == 0x82af5320) {
    double x = std::bit_cast<double>(s.fpr_bits[1]),
           y = std::bit_cast<double>(s.fpr_bits[2]),
           z = std::bit_cast<double>(s.fpr_bits[3]),
           angle = std::bit_cast<double>(s.fpr_bits[4]);
    auto outX = Address(s.r[7]), outY = Address(s.r[8]), outZ = Address(s.r[9]),
         outAngle = Address(s.r[10]);
    fp();
    Call(0x82380a18, m, d, s);
    Call(0x82389aa0, m, d, s);
    auto profile = Address(s.r[3]);
    fp();
    float radians =
        float(double(get(profile + 40)) *
              std::bit_cast<double>(recovery_abi::ReadU64(m, 0x82001030)));
    auto trig = [&](bool cosine) {
      return float(
          mesh_hull_incremental61::EvaluateGuestTrig(m, radians, cosine));
    };
    float cosine = trig(true), sine = trig(false);
    float translatedX = float(double(cosine) * x + get(profile + 28));
    put(outX, float(-(double(sine) * y - translatedX)));
    sine = trig(false);
    cosine = trig(true);
    float translatedY = float(double(cosine) * y);
    translatedY = float(double(sine) * x + translatedY);
    put(outY, float(translatedY + get(profile + 32)));
    put(outZ, float(double(get(profile + 36)) + z));
    put(outAngle, float(angle + get(profile + 40)));
  } else {
    auto descriptor = m.ReadU32(m.ReadU32(0x832ca0d0) + 84) + 28 * arg5;
    s.r[5] = sp + 84;
    s.r[6] = sp + 80;
    Call(0x82acf5d8, m, d, s);
    s.r[3] = descriptor;
    s.r[4] = m.ReadU32(sp + 84);
    s.r[5] = m.ReadU32(sp + 80);
    Call(0x82af5b20, m, d, s);
    auto formation = Address(s.r[3]);
    unsigned ordinalA = 0, ordinalB = 0;
    fp();
    float scale = get(0x82000bb8);
    for (unsigned i = 0;
         std::int32_t(i) < std::int32_t(m.ReadU32(m.ReadU32(owner + 20) + 4));
         ++i) {
      auto resource = m.ReadU32(m.ReadU32(m.ReadU32(owner + 20)) + 4 * i),
           flags = m.ReadU32(resource + 124);
      if (((flags >> 27) & 1) != (arg4 & 255))
        continue;
      auto row = m.ReadU32(descriptor + 16) + 28 * formation;
      bool special = (flags & 0x40000000) != 0;
      s.r[3] = row;
      s.r[4] = special;
      s.r[5] = special ? ordinalA++ : ordinalB++;
      Call(0x82af5498, m, d, s);
      auto point = m.ReadU32(row + 16) + 20 * Address(s.r[3]);
      fp();
      for (unsigned j = 1; j < 5; ++j)
        s.fpr_bits[j] =
            std::bit_cast<std::uint64_t>(double(get(point + 4 * j)));
      s.r[7] = resource + 76324;
      s.r[8] = resource + 76328;
      s.r[9] = resource + 76332;
      s.r[10] = resource + 76344;
      Call(0x82af5320, m, d, s);
      fp();
      put(resource + 76344, float(get(resource + 76344) * scale));
    }
    s.r[3] = 1;
  }
  if (e == 0x82acf5d8)
    s.r[31] = recovery_abi::ReadU64(m, old - 8);
  if (e == 0x82af5320)
    for (unsigned i = 26; i < 32; ++i)
      s.fpr_bits[i] = recovery_abi::ReadU64(m, old - 48 - 8 * (32 - i));
  if (e == 0x82af5ba8)
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - 128);
  if (frame) {
    s.r[1] += frame;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
  }
  return true;
}
} // namespace lo::semantic::gpu::battle_formation61
