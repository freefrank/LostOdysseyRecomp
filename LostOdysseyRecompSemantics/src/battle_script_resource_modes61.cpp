#include "lo_semantics/battle_script_resource_modes61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::battle_script_resource_modes61 {
namespace {
using recovery_abi::Address;
struct Modes {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner, sp;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Actor() { return W(owner + 24); }
  unsigned Resource() { return W(Actor() + 4); }
  unsigned Mode() {
    auto a = Actor();
    return m.ReadU8(W(a + 36) + W(a + 52) +
                    ((W(W(owner + 44) + 28) & 0x04000000) ? 2 : 1));
  }
  unsigned Get(unsigned off) {
    s.r[3] = owner;
    s.r[4] = off;
    (void)battle_script_extensions61::Apply(0x8238c198, m, d, s);
    return Address(s.r[3]);
  }
  void Set(unsigned off, unsigned value) {
    s.r[3] = owner;
    s.r[4] = off;
    s.r[5] = value;
    (void)battle_script_extensions61::Apply(0x8238c208, m, d, s);
  }
  void Next(unsigned n) {
    auto a = Actor();
    m.WriteU32(a + 52, W(a + 52) + n);
  }
  void Call(unsigned e) { d.guest.CallDirect(e, m, s); }
  unsigned Runtime(unsigned e) {
    Call(0x82380a18);
    Call(e);
    return Address(s.r[3]);
  }
  unsigned Find(unsigned id) {
    Runtime(0x82389b78);
    s.r[4] = id;
    Call(0x8238e308);
    return Address(s.r[3]);
  }
  void Virtual(unsigned object, unsigned slot) {
    s.r[3] = object;
    s.ctr = W(W(object) + slot);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
  }
  unsigned Buffer() {
    Virtual(W(0x83315fb4), 352);
    Call(0x8229dfd8);
    return Address(s.r[3]);
  }
  void Scene(unsigned e, unsigned a, unsigned b) {
    s.r[3] = 0x832ca0e0 + 5232;
    s.r[4] = a;
    s.r[5] = b;
    Call(e);
  }
  void ResourceService(unsigned e, unsigned resource) {
    s.r[3] = W(0x83291dc0);
    s.r[4] = resource;
    Call(e);
  }
  float Float(unsigned value) {
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    recovery_abi::WriteU64(m, sp + 80,
                           std::uint64_t(std::int64_t(std::int32_t(value))));
    return float(std::int32_t(value));
  }
  void Single(unsigned mode, unsigned value) {
    switch (mode) {
    case 0: {
      constexpr unsigned bits[]{32, 64, 128, 256, 1024, 512, 2048};
      unsigned bit = value - 1 < 7 ? bits[value - 1] : 0;
      auto resource = Resource();
      if (resource)
        m.WriteU32(resource + 4956, W(resource + 4956) | bit);
      break;
    }
    case 1:
    case 2:
      Scene(0x82ab79d8, value, mode == 1);
      break;
    case 3:
    case 4:
      Scene(0x82ab83b0, value, mode == 4);
      break;
    case 5:
    case 6: {
      auto resource = Find(value);
      if (resource) {
        auto flags = W(resource + 76348);
        m.WriteU32(resource + 76348,
                   mode == 5 ? flags | 0x80000000 : flags & 0x7fffffff);
      }
      break;
    }
    case 7: {
      auto resource = Find(value);
      if (resource)
        m.WriteU32(resource + 124, W(resource + 124) | 64);
      break;
    }
    case 8:
    case 9:
      s.r[3] = W(0x832652f0);
      s.r[4] = value;
      Call(mode == 8 ? 0x8285fe78 : 0x8285fe48);
      break;
    case 10:
    case 11: {
      auto resource = Resource();
      if (resource)
        m.WriteU32(resource + 4 * (value + 19031), mode == 10);
      break;
    }
    case 12:
      s.r[3] = 0x832ca0e0 + 5232;
      s.r[4] = value;
      Call(0x82ab5910);
      break;
    case 13:
      s.r[3] = W(0x83291dc0);
      s.r[4] = value;
      Call(0x82ac3498);
      break;
    case 14: {
      auto buffer = Buffer();
      Set(2, W(buffer + 68));
      break;
    }
    case 15: {
      auto buffer = Buffer(), resource = Find(0);
      m.WriteU32(resource + 5112, value);
      m.WriteU32(buffer + 2728, value);
      ResourceService(0x82ac0588, resource);
      ResourceService(0x82ac25e8, resource);
      m.WriteU32(W(0x83291dc0) + 4, W(resource + 4948));
      auto manager = W(0x83291dc0);
      for (unsigned i = 0; i < 5; ++i)
        m.WriteU32(manager + 12 + 4 * i, W(resource + 5116 + 4 * i));
      ResourceService(0x82ac3058, resource);
      break;
    }
    case 16: {
      auto resource = Resource();
      if (resource)
        m.WriteU32(resource + 76376, value);
      break;
    }
    case 17: {
      unsigned count = 0;
      auto actor = Actor(), n = W(actor + 296);
      if (std::int32_t(n) > 0) {
        auto resource = Find(W(actor + 80 + 24 * n));
        if (resource)
          count = W(resource + 14660);
      }
      Set(2, count);
      break;
    }
    case 18: {
      for (unsigned i = 0; i < 4; ++i)
        m.WriteU32(sp + 80 + 4 * i, 0xffffffff);
      auto id = Get(2), target = Find(id), list = Runtime(0x8238e2f8);
      unsigned count = 0;
      for (unsigned i = 0;
           std::int32_t(i) < std::int32_t(W(Runtime(0x8238e2f8) + 4)); ++i) {
        auto resource = W(W(list) + 4 * i);
        if ((W(target + 124) ^ W(resource + 124)) & 0x10000000)
          continue;
        Virtual(resource, 292);
        if (Address(s.r[3]) || (W(resource + 124) & 0x00200000) ||
            !W(resource + 132))
          continue;
        Virtual(resource, 380);
        if (Address(s.r[3]))
          continue;
        m.WriteU32(sp + 80 + 4 * count++, W(resource + 64));
        if (count >= 4)
          break;
      }
      // The source calls the random service even for an empty candidate range.
      s.r[3] = W(0x83264558);
      s.r[4] = 0;
      s.r[5] = count - 1;
      s.r[6] = 111;
      s.r[7] = 0;
      Call(0x82aa0740);
      Set(2, W(sp + 80 + 4 * Address(s.r[3])));
      break;
    }
    }
  }
  void Pair(unsigned mode, unsigned a, unsigned b) {
    switch (mode) {
    case 0:
    case 1:
      Scene(mode ? 0x82ab7ea0 : 0x82ab7c20, a, b);
      break;
    case 2: {
      auto number = Float(b);
      auto zero = W(0x82000e50);
      m.WriteU32(sp + 88, zero);
      m.WriteU32(sp + 92, zero);
      m.WriteU32(sp + 96, std::bit_cast<unsigned>(number));
      s.r[3] = 0x832ca0e0 + 5232;
      s.r[4] = a;
      s.r[5] = recovery_abi::ReadU64(m, sp + 88);
      s.r[6] = std::uint64_t(W(sp + 96)) << 32;
      Call(0x82ab8128);
      break;
    }
    case 3:
      s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(Float(b)));
      s.r[3] = W(0x832652f0);
      s.r[4] = a;
      Call(0x8285fe88);
      break;
    case 4:
      s.r[3] = 0x832cbfb0;
      s.r[4] = a;
      s.r[5] = b;
      Call(0x82ad6a48);
      break;
    case 5:
      Scene(0x82ab42f0, a, b & 255);
      break;
    case 6:
    case 9: {
      auto resource = Find(a);
      Set(4, resource ? W(resource + (mode == 6 ? 14652 : 5108)) : 0);
      break;
    }
    case 7: {
      auto resource = Find(a);
      if (resource) {
        bool changed = false;
        if (std::int32_t(b) < 73) {
          m.WriteU32(resource + 5108, b);
          changed = true;
        } else
          for (unsigned i = 0; i < 5; ++i)
            if (!W(resource + 5116 + 4 * i)) {
              m.WriteU32(resource + 5116 + 4 * i, b);
              changed = true;
              break;
            }
        if (changed) {
          ResourceService(0x82ac3058, resource);
          s.r[3] = 0x832ca0e0 + 5232;
          s.r[4] = W(resource + 64);
          Call(0x82ab76f8);
        }
      }
      [[fallthrough]];
    }
    case 8:
      s.r[3] = W(0x832652f0);
      s.r[4] = a;
      s.r[5] = b;
      Call(0x8285fef8);
      break;
    case 10: {
      auto buffer = Buffer();
      Set(4, W(buffer + 14308 * a + 136));
      break;
    }
    case 11:
      s.r[3] = W(0x832cb798);
      s.r[4] = a;
      s.r[5] = b;
      Call(0x82aa0d50);
      break;
    }
  }
  void Run(unsigned e) {
    auto a = Get(2);
    if (e == 0x82affb20) {
      auto mode = Mode();
      Single(mode, a);
      Next(4);
    } else {
      auto b = Get(4), mode = Mode();
      Pair(mode, a, b);
      Next(6);
    }
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82affb20 && e != 0x82b000a0)
    return false;
  auto owner = Address(s.r[3]), old = Address(s.r[1]);
  unsigned first = e == 0x82affb20 ? 25 : 28,
           frame = e == 0x82affb20 ? 160 : 144;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  auto sp = Address(s.r[1]);
  m.WriteU32(sp, old);
  Modes{m, d, s, owner, sp}.Run(e);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_resource_modes61
