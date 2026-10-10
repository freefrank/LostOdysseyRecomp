#include "lo_semantics/battle_script_events61.h"
#include "lo_semantics/battle_script_dispatch61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_script_events61 {
namespace {
using recovery_abi::Address;
struct Events {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Actor() { return W(owner + 24); }
  unsigned Slot(unsigned a) { return W(a + 40) + 24 * W(a + 56); }
  unsigned Code() { return W(Actor() + 36) + W(Actor() + 52); }
  unsigned Find(unsigned id) {
    if (id == 0x7ffffff8)
      return Actor();
    auto state = W(owner + 44);
    for (std::int32_t i = 0; i < std::int32_t(W(state + 12)); ++i) {
      auto p = W(state + 4) + 472u * unsigned(i);
      if (W(p) == id)
        return p;
    }
    return 0;
  }
  unsigned Query(unsigned actor, unsigned event) {
    if (W(Slot(actor) + 16) == event)
      return 0;
    for (unsigned i = 0; i < 16; ++i)
      if (W(W(actor + 40) + 24 * i + 16) == event)
        return 1;
    return 0xffffffff;
  }
  unsigned Target() {
    auto p = Code() + 2;
    unsigned id = 0;
    for (unsigned i = 0; i < 4; ++i)
      id |= unsigned(m.ReadU8(p + i)) << (8 * i);
    return Find(id);
  }
  void Stop() {
    auto state = W(owner + 44);
    m.WriteU32(state + 28, W(state + 28) | 0x80000000);
  }
  void Advance(unsigned n) { m.WriteU32(Actor() + 52, W(Actor() + 52) + n); }
  unsigned Enqueue() {
    auto target = Target(), from = Actor(), code = Code();
    s.r[3] = owner;
    s.r[4] = target;
    s.r[5] = from;
    s.r[6] = m.ReadU8(code + 6);
    s.r[7] = m.ReadU8(code + 1);
    (void)battle_script_dispatch61::Apply(0x82a9bdf8, m, d, s);
    return Address(s.r[3]);
  }
  void Run(unsigned entry) {
    if (entry == 0x82a9cb20) {
      if (Enqueue() == 2)
        Stop();
      else
        Advance(7);
      return;
    }
    if (entry == 0x82a9cfe8) {
      if (auto target = Target()) {
        auto priority = unsigned(m.ReadU8(Code() + 1));
        if (std::int32_t(W(Slot(target))) <= std::int32_t(priority)) {
          Stop();
          return;
        }
        for (unsigned i = 0; i < 16; ++i)
          if (std::int32_t(W(W(target + 40) + 24 * i)) <=
              std::int32_t(priority)) {
            Stop();
            return;
          }
      }
      Advance(6);
      return;
    }
    auto stage = W(Slot(Actor()) + 20);
    if (stage == 0) {
      auto result = Enqueue();
      if (result == 1) {
        m.WriteU32(Slot(Actor()) + 20, 1);
        Stop();
        return;
      }
      if (result == 2) {
        Stop();
        return;
      }
    } else if (stage == 1) {
      if (auto target = Target()) {
        auto status = Query(target, m.ReadU8(Code() + 6));
        if (entry == 0x82a9cbe8) {
          if (status != 0 && status != 0xffffffff) {
            Stop();
            return;
          }
        } else {
          if (status == 0 || status == 0xffffffff)
            m.WriteU32(Slot(Actor()) + 20, 2);
          Stop();
          return;
        }
      }
    } else if (stage == 2 && entry == 0x82a9cda8) {
      if (auto target = Target())
        if (Query(target, m.ReadU8(Code() + 6)) != 0xffffffff) {
          Stop();
          return;
        }
    }
    m.WriteU32(Slot(Actor()) + 20, 0);
    Advance(7);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x8238c118 && e != 0x82a9bee0 && e != 0x82a9cb20 &&
      e != 0x82a9cbe8 && e != 0x82a9cda8 && e != 0x82a9cfe8)
    return false;
  Events events{m, d, s, Address(s.r[3])};
  if (e == 0x8238c118) {
    s.r[3] = events.Find(Address(s.r[4]));
    return true;
  }
  if (e == 0x82a9bee0) {
    auto result = events.Query(Address(s.r[4]), Address(s.r[5]));
    s.r[3] = result == 0xffffffff ? ~std::uint64_t(0) : result;
    return true;
  }
  auto old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  if (e != 0x82a9cfe8)
    recovery_abi::WriteU64(m, old - 16, s.r[31]);
  s.r[1] -= 96;
  m.WriteU32(Address(s.r[1]), old);
  events.Run(e);
  s.r[1] += 96;
  if (e != 0x82a9cfe8)
    s.r[31] = recovery_abi::ReadU64(m, old - 16);
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_events61
