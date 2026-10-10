#include "lo_semantics/battle_script61.h"
#include "lo_semantics/battle_script_parameters61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
#include <limits>
namespace lo::semantic::gpu::battle_script61 {
namespace {
using recovery_abi::Address;
struct Script {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned State() { return W(owner + 44); }
  unsigned Call(unsigned e, unsigned a) {
    s.r[3] = a;
    if (!battle_script_parameters61::Apply(e, m, d, s))
      d.guest.CallDirect(e, m, s);
    return Address(s.r[3]);
  }
  unsigned Allocate(unsigned bytes) {
    if (!W(0x8330b608)) {
      s.r[3] = 0;
      d.guest.CallDirect(0x827c5f38, m, s);
    }
    s.r[3] = W(0x8330b608);
    s.r[4] = bytes;
    s.r[5] = 8;
    s.ctr = W(W(Address(s.r[3])) + 4);
    d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    return Address(s.r[3]);
  }
  void Zero(unsigned p, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i)
      m.WriteU8(p + i, 0);
  }
  void FreeField(unsigned object, unsigned off) {
    if (auto p = W(object + off)) {
      s.r[3] = p;
      (void)manager_release_context61::Apply(0x82388b58, m, d, s);
      m.WriteU32(object + off, 0);
    }
  }
  void Init(unsigned count) {
    m.WriteU32(owner + 44, Allocate(17780));
    m.WriteU32(State() + 40, 0);
    m.WriteU32(State() + 48, 0);
    auto scratch = Allocate(64);
    m.WriteU32(State() + 52, scratch);
    auto variables = Allocate(2048);
    m.WriteU32(State(), variables);
    m.WriteU32(State() + 24, count);
    auto bytes = count <= 0x8ad8f2u ? count * 472u : 0xffffffffu;
    auto records = Allocate(bytes);
    m.WriteU32(State() + 4, records);
    m.WriteU32(State() + 28, W(State() + 28) & 0x44001fff);
    m.WriteU32(State() + 20, W(0x82000e50));
    for (unsigned off : {16u, 60u, 32u, 196u, 16720u, 16724u})
      m.WriteU32(State() + off, 0);
    Zero(W(State()), 2048);
    Zero(W(State() + 52), 64);
    for (unsigned off : {16728u, 16732u, 16736u, 16740u, 16744u, 16748u})
      m.WriteU32(State() + off, 0);
    m.WriteU32(State() + 17776, 0xffffffff);
    s.r[3] = W(State() + 52);
  }
  void Release() {
    if (!State()) {
      s.r[3] = 1;
      return;
    }
    if (!(W(State() + 28) & 0x40000000)) {
      s.r[3] = 0;
      return;
    }
    for (unsigned off : {48u, 0u, 52u, 16728u, 16736u, 16740u})
      FreeField(State(), off);
    if (W(State() + 4)) {
      for (std::int32_t i = 0; i < std::int32_t(W(State() + 24)); ++i) {
        auto record = W(State() + 4) + 472u * unsigned(i);
        for (unsigned off : {12u, 20u, 40u, 48u, 28u, 36u, 80u, 72u})
          FreeField(record, off);
      }
      FreeField(State(), 4);
    }
    FreeField(State(), 40);
    FreeField(owner, 44);
    s.r[3] = 1;
  }
  unsigned WaitRecord(unsigned actor) {
    return W(actor + 40) + 24u * W(actor + 56);
  }
  void Wait() {
    s.r[6] = owner;
    auto actor = W(owner + 24), record = WaitRecord(actor);
    if (std::int32_t(W(record + 8)) < 0) {
      s.r[3] = owner;
      s.r[4] = 1;
      s.r[5] = 0;
      s.lr = 0x82a9bf8c;
      (void)battle_script_parameters61::Apply(0x8238be38, m, d, s);
      m.WriteU32(WaitRecord(actor) + 8, Address(s.r[3]));
      // The original helper contract keeps its r6 manager live across the call.
      auto script = W(Address(s.r[6]) + 44);
      m.WriteU32(script + 28, W(script + 28) | 0x80000000);
      return;
    }
    m.WriteU32(record + 8, W(record + 8) - W(State() + 16));
    if (std::int32_t(W(WaitRecord(W(owner + 24)) + 8)) >= 0)
      m.WriteU32(State() + 28, W(State() + 28) | 0x80000000);
    else {
      actor = W(owner + 24);
      m.WriteU32(actor + 52, W(actor + 52) + 3);
    }
  }
  void Update() {
    s.fpr_bits[31] = s.fpr_bits[1];
    if (!State() || !(W(State() + 28) & 0x40000000))
      return;
    if (W(State() + 28) & 0x08000000) {
      d.guest.CallDirect(0x823a5058, m, s);
      d.guest.CallDirect(0x82b07848, m, s);
    }
    if (m.ReadU8(owner + 4) == 1 && !W(owner + 8)) {
      m.WriteU32(owner + 8, 1);
      return;
    }
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto value = double(float(std::bit_cast<double>(s.fpr_bits[31]) *
                              double(std::bit_cast<float>(W(0x82000da8)))));
    std::int32_t ticks;
    if (value > double(std::numeric_limits<std::int32_t>::max()))
      ticks = std::numeric_limits<std::int32_t>::max();
    else if (std::isnan(value) ||
             value < double(std::numeric_limits<std::int32_t>::min()))
      ticks = std::numeric_limits<std::int32_t>::min();
    else
      ticks = std::int32_t(value);
    s.fpr_bits[0] = std::uint64_t(std::int64_t(ticks));
    m.WriteU32(State() + 16, unsigned(ticks));
    // Preserve the exact return address used by the existing fractional-tick
    // hook.
    s.lr = 0x8238ad64;
    Call(0x8238ae08, owner);
    Call(0x8238b4a8, owner);
    for (std::int32_t i = 0; i < std::int32_t(W(State() + 12)); ++i) {
      auto actor = W(State() + 4) + 472u * unsigned(i);
      m.WriteU32(owner + 24, actor);
      auto resource = W(actor + 4);
      if (resource) {
        m.WriteU32(State() + 60, W(resource + 64));
        s.r[4] = resource;
        Call(0x8238b5a0, owner);
      }
      if (!(W(W(owner + 24) + 64) & 0x80000000)) {
        Call(0x8238b850, owner);
        Call(0x8238b900, owner);
      }
      Call(0x8238c708, owner);
    }
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82a9dbb8 && e != 0x82a9e3b8 && e != 0x82a9bf40 && e != 0x8238acc8)
    return false;
  auto owner = Address(s.r[3]), arg = Address(s.r[4]), old = Address(s.r[1]);
  unsigned frame = e == 0x82a9bf40 ? 96 : 128, first = e == 0x82a9bf40   ? 31
                                                       : e == 0x82a9dbb8 ? 28
                                                       : e == 0x82a9e3b8 ? 27
                                                                         : 29;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  if (e == 0x8238acc8)
    recovery_abi::WriteU64(m, old - 40, s.fpr_bits[31]);
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  Script script{m, d, s, owner};
  if (e == 0x82a9dbb8)
    script.Init(arg);
  else if (e == 0x82a9e3b8)
    script.Release();
  else if (e == 0x82a9bf40)
    script.Wait();
  else
    script.Update();
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  if (e == 0x8238acc8)
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - 40);
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script61
