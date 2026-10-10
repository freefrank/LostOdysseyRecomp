#include "lo_semantics/battle_script_item_action61.h"
#include "lo_semantics/battle_script_extensions61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/battle_script_execution61.h"
namespace lo::semantic::gpu::battle_script_item_action61 {
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
    if (!battle_script_execution61::Apply(e, m, d, s))
      d.guest.CallDirect(e, m, s);
  }
  unsigned Manager(unsigned method) {
    Call(0x82380a18);
    Call(method);
    return Address(s.r[3]);
  }
  void Run(unsigned) {
    auto busy = W(Actor() + 96), bank = Get(3), category = Get(5),
         selection = Get(7), resource = W(Actor() + 4);
    unsigned result = 1;
    if (resource && bank <= 1) {
      auto actor = Actor();
      m.WriteU32(actor + 64, (W(actor + 64) & ~0x2000u) | ((bank & 1) << 13));
      if (std::int32_t(busy) <= 0 || (m.ReadU8(Actor() + 64) & 1)) {
        unsigned chosen = 0, count = 0;
        bool available = true;
        if (category == 3)
          chosen = selection;
        else {
          unsigned type = category == 0   ? 1
                          : category == 1 ? 4
                          : category == 2 ? 3
                                          : 0,
                   sp = Address(s.r[1]);
          for (unsigned i = 0; i < 1024; ++i)
            m.WriteU32(sp + 80 + 4 * i, 0);
          auto state = W(owner + 44), table = W(0x83264978);
          for (unsigned i = 0; i < 1024; ++i)
            if (W(state + 8260 * bank + 200 + 4 * i) &&
                W(table + 196 * i + 148) == type && W(table + 196 * i + 152)) {
              m.WriteU32(sp + 80 + 4 * count, i);
              ++count;
            }
          if (!count)
            available = false;
          else if (selection < 2) {
            std::int32_t best = selection ? 0 : 99;
            for (unsigned i = 0; i < count; ++i) {
              auto id = W(sp + 80 + 4 * i);
              auto value = std::int32_t(W(table + 196 * id + 152));
              if (selection ? value >= best : value <= best) {
                best = value;
                chosen = id;
              }
            }
          } else if (selection == 2) {
            s.r[3] = W(0x83264558);
            s.r[4] = 0;
            s.r[5] = count - 1;
            s.r[6] = 88;
            s.r[7] = W(resource + 64);
            Call(0x82aa0740);
            chosen = W(sp + 80 + 4 * Address(s.r[3]));
          }
        }
        if (available) {
          if (chosen) {
            s.r[3] = owner;
            s.r[4] = chosen;
            s.r[5] = resource;
            Call(0x82af6d60);
          }
          auto mode = W(Actor() + 60);
          s.r[3] = owner;
          s.r[4] = 11;
          s.r[5] = chosen;
          Call(mode == 2 ? 0x82afdcf0 : mode == 3 ? 0x82afdb90 : 0x82b00698);
          result = 0;
        }
      }
    }
    Set(1, result);
    Next(9);
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e != 0x82b009b0)
    return false;
  unsigned first = 26, frame = 4240;
  // Preserve the original stack-probe read before allocating the large
  // candidate frame.
  (void)recovery_abi::ReadU64(m, Address(s.r[1]) - 4096);
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
} // namespace lo::semantic::gpu::battle_script_item_action61
