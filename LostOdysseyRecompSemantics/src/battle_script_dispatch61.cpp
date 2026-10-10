#include "lo_semantics/battle_script_dispatch61.h"
#include "lo_semantics/battle_script61.h"
#include "lo_semantics/battle_script_core61.h"
#include "lo_semantics/battle_script_calls61.h"
#include "lo_semantics/battle_script_angles61.h"
#include "lo_semantics/battle_script_events61.h"
#include "lo_semantics/battle_script_control61.h"
#include "lo_semantics/battle_script_party61.h"
#include "lo_semantics/battle_script_text61.h"
#include "lo_semantics/battle_script_services61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/battle_script_scene61.h"
#include "lo_semantics/battle_script_binding61.h"
#include "lo_semantics/battle_script_presentation61.h"
#include "lo_semantics/battle_script_actions61.h"
#include "lo_semantics/battle_script_history61.h"
#include "lo_semantics/battle_script_targets61.h"
#include "lo_semantics/battle_script_queries61.h"
#include "lo_semantics/battle_script_runtime61.h"
#include "lo_semantics/battle_script_scene_state61.h"
#include "lo_semantics/battle_script_resource_modes61.h"
#include "lo_semantics/battle_script_status61.h"
#include "lo_semantics/battle_script_marshaling61.h"
#include "lo_semantics/battle_script_owned_labels61.h"
#include "lo_semantics/battle_script_queues61.h"
#include "lo_semantics/battle_script_commands61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::battle_script_dispatch61 {
namespace {
using recovery_abi::Address;
struct Engine {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  unsigned Actor() { return W(owner + 24); }
  unsigned State() { return W(owner + 44); }
  unsigned Slot(unsigned actor) { return W(actor + 40) + 24 * W(actor + 56); }
  unsigned Queue(unsigned actor, unsigned from, unsigned event,
                 unsigned priority) {
    if (!actor || !from || std::int32_t(W(actor + 16)) < std::int32_t(event))
      return 0xffffffff;
    unsigned free = 0xffffffff;
    for (unsigned i = 0; i < 16; ++i) {
      auto p = W(actor + 40) + 24 * i;
      if (W(p) == 255)
        free = i;
      else if (W(p + 16) == event)
        return 0;
    }
    if (free == 0xffffffff)
      return 2;
    if (event && !W(W(actor + 20) + 4 * event))
      return 0xffffffff;
    auto p = W(actor + 40) + 24 * free;
    m.WriteU32(p, priority);
    m.WriteU32(p + 4, W(W(actor + 20) + 4 * event));
    m.WriteU32(p + 16, event);
    m.WriteU32(p + 12, W(from));
    return 1;
  }
  void Completion() {
    if (!(W(Actor() + 64) & 8) || W(Actor() + 468) == 0xffffffff)
      return;
    auto id = W(Actor() + 468);
    d.guest.CallDirect(0x82380a18, m, s);
    d.guest.CallDirect(0x82389b78, m, s);
    s.r[4] = id;
    d.guest.CallDirect(0x8238e308, m, s);
    auto result = Address(s.r[3]);
    if (!result || (W(result + 60) & 255) < 5)
      return;
    auto actor = Actor();
    auto queued = Queue(actor, actor, 2, 16);
    s.r[3] = queued == 0xffffffff ? ~std::uint64_t(0) : queued;
    m.WriteU32(Actor() + 64, W(Actor() + 64) & ~8u);
    m.WriteU32(Actor() + 468, 0xffffffff);
  }
  void Dispatch() {
    m.WriteU32(State() + 28, W(State() + 28) & 0x7fffffff);
    if ((W(Actor() + 64) & 0x400000) && W(Actor() + 60) == 1) {
      s.r[3] = 1;
      return;
    }
    while (W(Actor() + 36)) {
      auto code = m.ReadU8(W(Actor() + 36) + W(Actor() + 52));
      m.WriteU32(State() + 28, W(State() + 28) & ~0x04000000u);
      auto target = W(owner + 4 * (unsigned(code) + 14));
      s.r[3] = owner;
      s.ctr = target;
      s.lr = 0x8238bb44;
      DispatchOpcode(target, m, d, s);
      if (W(State() + 28) & 0x80000000) {
        s.r[3] = 1;
        return;
      }
    }
    s.r[3] = 0;
  }
  void Schedule() {
    unsigned minimum = 255;
    for (unsigned i = 0; i < 16; ++i) {
      auto value = W(W(Actor() + 40) + 24 * i);
      if (std::int32_t(value) <= std::int32_t(minimum)) {
        minimum = value;
        m.WriteU32(Actor() + 56, i);
      }
    }
    if (minimum == 255) {
      auto actor = Actor(), record = W(actor + 40);
      m.WriteU32(record + 4, W(W(actor + 20) + 4));
      m.WriteU32(record + 16, 1);
      m.WriteU32(record, 128);
      m.WriteU32(actor + 56, 0);
    }
    auto actor = Actor(), slot = Slot(actor);
    m.WriteU32(actor + 52, W(slot + 4));
    m.WriteU32(actor + 60, W(slot + 16));
    s.r[3] = owner;
    (void)battle_script_dispatch61::Apply(0x8238bab0, m, d, s);
    m.WriteU32(Slot(Actor()) + 4, W(Actor() + 52));
  }
};
} // namespace
void DispatchOpcode(GuestAddress target, GuestMemory &m, Dependencies d,
                    Registers &s) {
  if ((target & ~3u) == 0x82a9bf40)
    (void)battle_script61::Apply(0x82a9bf40, m, d, s);
  else if (!battle_script_core61::Apply(target & ~3u, m, d, s) &&
           !battle_script_calls61::Apply(target & ~3u, m, d, s) &&
           !battle_script_angles61::Apply(target & ~3u, m, d, s) &&
           !battle_script_events61::Apply(target & ~3u, m, d, s) &&
           !battle_script_control61::Apply(target & ~3u, m, d, s) &&
           !battle_script_party61::Apply(target & ~3u, m, d, s) &&
           !battle_script_text61::Apply(target & ~3u, m, d, s) &&
           !battle_script_services61::Apply(target & ~3u, m, d, s) &&
           !battle_script_extensions61::Apply(target & ~3u, m, d, s) &&
           !battle_script_scene61::Apply(target & ~3u, m, d, s) &&
           !battle_script_binding61::Apply(target & ~3u, m, d, s) &&
           !battle_script_presentation61::Apply(target & ~3u, m, d, s) &&
           !battle_script_actions61::Apply(target & ~3u, m, d, s) &&
           !battle_script_history61::Apply(target & ~3u, m, d, s) &&
           !battle_script_targets61::Apply(target & ~3u, m, d, s) &&
           !battle_script_queries61::Apply(target & ~3u, m, d, s) &&
           !battle_script_runtime61::Apply(target & ~3u, m, d, s) &&
           !battle_script_scene_state61::Apply(target & ~3u, m, d, s) &&
           !battle_script_resource_modes61::Apply(target & ~3u, m, d, s) &&
           !battle_script_status61::Apply(target & ~3u, m, d, s) &&
           !battle_script_marshaling61::Apply(target & ~3u, m, d, s) &&
           !battle_script_owned_labels61::Apply(target & ~3u, m, d, s) &&
           !battle_script_queues61::Apply(target & ~3u, m, d, s) &&
           !battle_script_commands61::Apply(target & ~3u, m, d, s))
    d.guest.CallIndirect(target & ~3u, m, s);
}
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82a9bdf8) {
    Engine engine{m, d, s, Address(s.r[3])};
    auto r = engine.Queue(Address(s.r[4]), Address(s.r[5]), Address(s.r[6]),
                          Address(s.r[7]));
    s.r[3] = r == 0xffffffff ? ~std::uint64_t(0) : r;
    return true;
  }
  if (e != 0x8238b850 && e != 0x8238b900 && e != 0x8238bab0)
    return false;
  auto owner = Address(s.r[3]), old = Address(s.r[1]);
  unsigned frame = e == 0x8238b900 ? 96 : 112,
           first = e == 0x8238b900 ? 31 : 30;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  Engine engine{m, d, s, owner};
  if (e == 0x8238b850)
    engine.Completion();
  else if (e == 0x8238b900)
    engine.Schedule();
  else
    engine.Dispatch();
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_script_dispatch61
