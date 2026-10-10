#include "lo_semantics/battle_action_storage61.h"
#include "lo_semantics/battle_action_records61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/battle_action_effects61.h"
#include "lo_semantics/battle_script_runtime61.h"
namespace lo::semantic::gpu::battle_action_records61 {
namespace {
using recovery_abi::Address;
struct Runtime {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Actor() { return W(owner + 24); }
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
  void Call(unsigned e) {
    if (!battle_action_storage61::Apply(e, m, d, s) &&
        !battle_action_effects61::Apply(e, m, d, s))
      d.guest.CallDirect(e, m, s);
  }
  unsigned Manager(unsigned method) {
    Call(0x82380a18);
    Call(method);
    return Address(s.r[3]);
  }
  void Append(unsigned resource, unsigned offset, unsigned target) {
    auto record = W(resource + 14656) + offset, index = W(record + 20);
    m.WriteU32(record + 14884 + 464 * index, target);
    record = W(resource + 14656) + offset;
    m.WriteU32(record + 20, W(record + 20) + 1);
    record = W(resource + 14656) + offset;
    m.WriteU32(record + 16, W(record + 20));
  }
  unsigned ScriptActor(unsigned resource) {
    s.r[3] = 0x832c9c54;
    s.r[4] = resource;
    (void)battle_script_runtime61::Apply(0x82a9bdb0, m, d, s);
    return Address(s.r[3]);
  }
  void Run(unsigned e) {
    auto resource = owner, kind = Address(s.r[4]), detail = Address(s.r[5]);
    if (e == 0x82ab0b28) {
      auto offset = 124208 * detail;
      for (unsigned i = 0; i < 32; ++i)
        if (W(W(resource + 14656) + offset + 36 + 464 * i) == 0xffffffff) {
          auto slot = W(resource + 14656) + offset + 464 * i;
          m.WriteU32(slot + 36, kind);
          slot = W(resource + 14656) + offset + 464 * i;
          m.WriteU32(slot + 248, W(slot + 248) | 0x80000000u);
          break;
        }
      return;
    }
    if (e == 0x82ab0b98) {
      auto offset = 124208 * detail;
      for (unsigned i = 0; i < 32; ++i)
        if (W(W(resource + 14656) + offset + 14884 + 464 * i) == kind)
          return;
      auto record = W(resource + 14656) + offset,
           slot = record + 464 * W(record + 20);
      m.WriteU32(slot + 14884, kind);
      record = W(resource + 14656) + offset;
      slot = record + 464 * W(record + 20);
      m.WriteU32(slot + 15096, W(slot + 15096) | 0x80000000u);
      record = W(resource + 14656) + offset;
      m.WriteU32(record + 20, W(record + 20) + 1);
      m.WriteU32(W(resource + 14656) + offset + 32, 1);
      return;
    }
    auto target = Address(s.r[6]), index = Address(s.r[7]),
         offset = 124208 * index;
    bool alternate = e == 0x82ab38f0;
    if (kind != 0xffffffff) {
      s.r[3] = resource;
      if (!index)
        Call(0x82ab31e0);
      else {
        s.r[4] = 0;
        Call(0x82ab2d88);
        for (unsigned i = 0; i < 32; ++i) {
          m.WriteU32(W(resource + 14656) + offset + 464 * i - 123912, 1);
          m.WriteU32(W(resource + 14656) + offset + 464 * i - 109064, 1);
        }
      }
      auto record = W(resource + 14656) + offset;
      if (alternate)
        m.WriteU32(resource + 14680, 0);
      m.WriteU32(record, kind);
      m.WriteU32(W(resource + 14656) + offset + 4, detail);
      s.r[3] = resource;
      s.r[4] = kind;
      s.r[5] = detail;
      s.r[6] = alternate;
      Call(0x82ab0d50);
      record = W(resource + 14656) + offset;
      if (!alternate)
        m.WriteU32(resource + 124, W(resource + 124) | 512);
      m.WriteU32(record + 20, 0);
      m.WriteU32(W(resource + 14656) + offset + 36, W(resource + 64));
      Append(resource, offset, target);
      auto actor = ScriptActor(resource), other = ScriptActor(resource);
      record = W(resource + 14656) + offset;
      m.WriteU32(record + 24, other ? W(other + 60) : 0xffffffff);
      m.WriteU32(W(resource + 14656) + offset + 28, W(actor + 336));
      if (alternate)
        m.WriteU32(actor + 64, W(actor + 64) | 96);
    } else if (target != 0xffffffff)
      Append(resource, offset, target);
    else if (!(W(resource + 60) & 255)) {
      if (!alternate) {
        s.r[3] = 0x832c9c54;
        s.r[4] = resource;
        Call(0x82af68d8);
      }
      m.WriteU32(resource + 112, W(0x82000e50));
      m.WriteU32(resource + 116, W(0x82000e50));
      s.r[3] = resource;
      Call(0x82b1f1d0);
    }
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82ab0b28 || e == 0x82ab0b98) {
    Runtime{m, d, s, Address(s.r[3])}.Run(e);
    return true;
  }
  if (e != 0x82ab36c8 && e != 0x82ab38f0)
    return false;
  unsigned first = e == 0x82ab36c8 ? 26 : 25, frame = 144;
  auto owner = Address(s.r[3]), old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  Runtime{m, d, s, owner}.Run(e);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_action_records61
