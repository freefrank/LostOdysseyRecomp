#include "lo_semantics/battle_scene_requests61.h"
#include "lo_semantics/battle_scene_tasks61.h"
#include "lo_semantics/battle_manager_access61.h"
#include "lo_semantics/string_storage_context61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::battle_scene_requests61 {
namespace {
using recovery_abi::Address;
void Call(unsigned e, GuestMemory &m, Dependencies d, Registers &s) {
  if (!battle_scene_requests61::Apply(e, m, d, s) &&
      !battle_scene_tasks61::Apply(e, m, d, s) &&
      !string_storage_context61::Apply(e, m, d, s))
    d.guest.CallDirect(e, m, s);
}
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  auto owner = Address(s.r[3]), arg4 = Address(s.r[4]), arg5 = Address(s.r[5]),
       arg6 = Address(s.r[6]), arg7 = Address(s.r[7]), arg8 = Address(s.r[8]);
  auto packed4 = s.r[4], packed5 = s.r[5], packed6 = s.r[6];
  if (e == 0x82377168) {
    auto sp = Address(s.r[1]);
    for (unsigned i = 0; i < 3; ++i) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
      auto bits = m.ReadU32(arg4 + 4 * i);
      s.fpr_bits[0] =
          std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(bits)));
      m.WriteU32(sp - (i ? 16 : 12), bits);
      s.cached_fp_control |= 0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
      auto magnitude = bits & 0x7fffffffu, exponent = (bits >> 23) & 255,
           mantissa = (bits >> 13) & 1023;
      unsigned packed;
      // Guest D3D packing truncates and uses 0x7fff for overflow/NaN.
      if (magnitude > 0x477fe000u)
        packed = 0x7fff;
      else if (exponent > 112)
        packed = ((exponent - 112) << 10) | mantissa;
      else {
        auto shift = 113 - exponent;
        packed = shift > 31 ? 0 : (1024 + mantissa) >> shift;
      }
      packed |= (bits >> 16) & 0x8000;
      m.WriteU16(sp - 16, packed);
      m.WriteU16(owner + 2 * i, packed);
    }
    return true;
  }
  if (e == 0x82b35a20) {
    auto high = owner >> 16;
    s.r[3] = std::uint64_t(-1);
    for (unsigned i = 0; i < 3; ++i) {
      auto p = 0x820c9ef8 + 16 * i;
      if (high >= m.ReadU32(p) && high <= m.ReadU32(p + 4)) {
        s.r[3] = i;
        break;
      }
    }
    return true;
  }
  if (e == 0x82b5c6a0) {
    for (unsigned off = 0; off < 8; off += 2)
      m.WriteU16(owner + off, 0);
    m.WriteU16(owner + 4, 1000);
    m.WriteU8(owner + 7, arg4);
    return true;
  }
  if (e == 0x82b64a18) {
    for (unsigned off = 0; off < 40; off += 4)
      m.WriteU32(owner + off, 0);
    m.WriteU8(owner + 8, arg4);
    m.WriteU32(owner + 16, arg5);
    for (auto off : {23u, 25u, 27u})
      m.WriteU8(owner + off, 128);
    return true;
  }
  if (e == 0x82b19e88) {
    s.r[3] = std::uint64_t(-1);
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(m.ReadU32(owner + 12));
         ++i) {
      auto item = m.ReadU32(m.ReadU32(owner + 8) + 4 * i);
      if (m.ReadU32(item + 28) == arg4) {
        s.r[3] = m.ReadU32(item + 8);
        break;
      }
    }
    return true;
  }
  unsigned frame, first;
  switch (e) {
  case 0x82b35918:
  case 0x82b35640:
    frame = 112;
    first = 30;
    break;
  case 0x82b356b0:
  case 0x82b35778:
    frame = 96;
    first = 31;
    break;
  case 0x82b357c8:
    frame = 160;
    first = 27;
    break;
  case 0x82b1aa50:
    frame = 144;
    first = 26;
    break;
  case 0x82b1afe8:
    frame = 160;
    first = 24;
    break;
  case 0x82b1aca8:
    frame = 176;
    first = 23;
    break;
  default:
    return false;
  }
  auto old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  if (e == 0x82b357c8)
    recovery_abi::WriteU64(m, old - 56, s.fpr_bits[31]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  if (e == 0x82b1afe8)
    recovery_abi::WriteU64(m, old + 40, packed6);
  auto call = [&](unsigned a) { Call(a, m, d, s); };
  auto reset = [&]() {
    m.WriteU8(owner + 14, 0);
    s.r[3] = owner + 32;
    s.r[4] = 0;
    call(0x82b5c6a0);
    s.r[3] = owner + 40;
    s.r[4] = 1;
    s.r[5] = 1;
    call(0x82b64a18);
  };
  auto findScene = [&](unsigned id) {
    call(0x82380a18);
    s.r[4] = id;
    (void)battle_manager_access61::Apply(0x82380d30, m, d, s);
    return Address(s.r[3]);
  };
  if (e == 0x82b356b0) {
    recovery_abi::WriteU64(m, old + 32, packed4);
    recovery_abi::WriteU64(m, old + 40, packed5);
    m.WriteU32(owner + 80, 0xffffffff);
    m.WriteU32(owner + 84, unsigned(packed4 >> 32));
    m.WriteU32(owner + 88, unsigned(packed4));
    m.WriteU32(owner + 92, unsigned(packed5 >> 32));
    s.r[4] = owner + 84;
    if (m.ReadU8(owner + 12)) {
      m.WriteU8(owner + 14, 0);
      s.r[3] = owner + 68;
      call(0x82377168);
      m.WriteU32(owner + 56, 1);
    }
  } else if (e == 0x82b35918) {
    m.WriteU32(owner, 0x820010a8);
    m.WriteU32(owner + 8, 0xffffffff);
    m.WriteU8(owner + 4, 0);
    m.WriteU32(owner + 24, 0);
    m.WriteU32(owner + 28, 0xffffffff);
    m.WriteU32(owner + 20, 0xffffffff);
    reset();
    m.WriteU8(owner + 15, 0);
    s.r[3] = owner;
  } else if (e == 0x82b35778)
    reset();
  else if (e == 0x82b35640) {
    auto object = findScene(arg4);
    if (object) {
      m.WriteU32(owner + 80, arg4);
      if (m.ReadU8(owner + 12)) {
        m.WriteU32(owner + 76, object);
        m.WriteU8(owner + 14, 0);
        m.WriteU32(owner + 56, 8193);
      }
      s.r[3] = 1;
    }
  } else if (e == 0x82b357c8) {
    auto initialize = [&]() {
      if (m.ReadU32(owner + 24)) {
        s.r[3] = m.ReadU32(0x832d268c);
        s.r[4] = m.ReadU32(owner + 24);
        call(0x8236c7d8);
        m.WriteU32(owner + 24, 0);
      }
      m.WriteU8(owner + 4, 0);
      auto index = arg5;
      if (std::int32_t(index) < 0) {
        s.r[3] = m.ReadU32(0x832d268c);
        s.r[4] = arg4;
        s.r[5] = sp + 84;
        call(0x82b5d230);
        auto handle = Address(s.r[3]);
        if (!handle)
          return false;
        s.r[3] = 0x832cc0fc;
        s.r[4] = handle;
        call(0x82b19e88);
        index = Address(s.r[3]);
      }
      m.WriteU32(owner + 20, index);
      m.WriteU32(owner + 28, arg4);
      s.r[3] = m.ReadU32(0x832d268c);
      s.r[4] = arg4;
      s.r[5] = sp + 80;
      s.r[6] = 0;
      call(0x82b5d1a0);
      auto metadata = m.ReadU32(sp + 80);
      if (!metadata)
        return false;
      m.WriteU8(owner + 13, (m.ReadU8(metadata + 13) >> 1) & 1);
      auto kind = m.ReadU8(metadata + 9);
      auto zero = m.ReadU32(0x82000e50);
      if (s.cached_fp_control & 0x8040) {
        s.cached_fp_control &= ~0x8040u;
        d.fp.SetHostFpControl(s.cached_fp_control);
      }
      s.fpr_bits[31] =
          std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(zero)));
      if (kind != 9 && kind != 10 && kind != 11) {
        m.WriteU8(owner + 12, 0);
        s.r[3] = owner;
        call(0x82b35778);
      } else {
        m.WriteU8(owner + 12, 1);
        reset();
        for (unsigned off = 88; off < 100; off += 4)
          m.WriteU32(sp + off, zero);
        m.WriteU32(owner + 44, arg4);
        m.WriteU32(owner + 56, 1);
        s.r[3] = owner + 68;
        s.r[4] = sp + 88;
        call(0x82377168);
      }
      m.WriteU32(owner + 96, zero);
      m.WriteU8(owner + 4, 1);
      return true;
    };
    s.r[3] = initialize();
  } else {
    auto create = [&]() -> std::uint64_t {
      bool attached = e == 0x82b1aca8, positioned = e == 0x82b1afe8;
      auto key = arg4, object = attached ? findScene(arg5) : 0;
      if (object &&
          (m.ReadU32(object + 556) == 139 || m.ReadU32(object + 556) == 140) &&
          key - 0x61000000u <= 0x00ffffffu)
        key = m.ReadU32(object + 552) + 0x61001fec;
      auto id = m.ReadU32(owner + 20) + 1;
      for (;;) {
        bool collision = false;
        for (unsigned i = 0;
             std::int32_t(i) < std::int32_t(m.ReadU32(owner + 28)); ++i)
          if (m.ReadU32(m.ReadU32(m.ReadU32(owner + 24) + 4 * i) + 8) == id) {
            collision = true;
            break;
          }
        if (!collision)
          break;
        id = (id + 1) & 65535;
      }
      s.r[3] = key;
      call(0x82b35a20);
      auto type = Address(s.r[3]);
      unsigned parent = 0xffffffff;
      if (type == 0 && positioned)
        object = findScene(arg8);
      if (type == 0 || type == 2) {
        if (type == 0 && ((!attached && !positioned) || !object ||
                          (m.ReadU32(object + 604) & 0x6000)))
          return std::uint64_t(-1);
        auto required =
            type == 2 ? m.ReadU32(0x83213d74) : m.ReadU32(object + 1312);
        for (unsigned i = 0;
             std::int32_t(i) < std::int32_t(m.ReadU32(owner + 12)); ++i)
          if (m.ReadU32(m.ReadU32(m.ReadU32(owner + 8) + 4 * i) + 8) ==
              required) {
            parent = i;
            break;
          }
        if (parent == 0xffffffff)
          return std::uint64_t(-1);
        auto item = m.ReadU32(m.ReadU32(owner + 8) + 4 * parent);
        key |=
            (unsigned(std::int32_t(std::int8_t(m.ReadU8(item + 32)))) << 20) &
            0xfff00000u;
      }
      auto count = m.ReadU32(owner + 28);
      for (unsigned i = 0; std::int32_t(i) < std::int32_t(count); ++i) {
        auto item = m.ReadU32(m.ReadU32(owner + 24) + 4 * i);
        auto state = std::int8_t(m.ReadU8(item + 4));
        if ((state == 1 || state == 2) && m.ReadU32(item + 28) == key) {
          if (s.cached_fp_control & 0x8040) {
            s.cached_fp_control &= ~0x8040u;
            d.fp.SetHostFpControl(s.cached_fp_control);
          }
          if (std::bit_cast<float>(m.ReadU32(item + 96)) <
              std::bit_cast<float>(m.ReadU32(0x82000dac)))
            return std::uint64_t(-1);
        }
      }
      s.r[3] = 100;
      call(0x82486c88);
      auto task = Address(s.r[3]);
      m.WriteU32(sp + 80, task);
      if (task)
        call(0x82b35918);
      m.WriteU32(task + 8, id);
      s.r[3] = task;
      s.r[4] = key;
      s.r[5] = parent;
      call(0x82b357c8);
      if (!(Address(s.r[3]) & 255)) {
        s.r[3] = task;
        s.r[4] = 1;
        s.ctr = m.ReadU32(m.ReadU32(task));
        d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        return std::uint64_t(-1);
      }
      auto index = m.ReadU32(owner + 28);
      s.r[3] = owner + 24;
      s.r[4] = 1;
      call(0x82b1a560);
      m.WriteU32(m.ReadU32(owner + 24) + 4 * index, task);
      if (positioned) {
        s.r[3] = task;
        s.r[4] = packed5;
        s.r[5] = packed6 & 0xffffffff00000000ull;
        s.r[6] = arg7;
        call(0x82b356b0);
      } else if (object) {
        s.r[3] = task;
        s.r[4] = arg5;
        s.r[5] = arg6;
        call(0x82b35640);
      }
      m.WriteU32(owner + 20, id);
      return id;
    };
    s.r[3] = create();
  }
  s.r[1] += frame;
  if (e == 0x82b357c8)
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - 56);
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_scene_requests61
