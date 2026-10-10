#include "lo_semantics/battle_semantic_runtime61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/recovery_abi.h"
#include "battle_semantic_routes.inc"
namespace lo::semantic::gpu::battle_semantic_runtime61 {
namespace {
bool Routed(GuestAddress entry, GuestMemory &memory, Dependencies dependencies,
            Registers &state) {
  if (entry == 0x82b7a0b0)
    return crt_copy_full_context::Apply(entry, memory, state);
  if (entry == 0x82b7bc40) {
    auto destination = recovery_abi::Address(state.r[3]),
         bytes = recovery_abi::Address(state.r[5]);
    auto padding = (0u - destination) & 3u,
         prefix = bytes < padding ? bytes : padding, remaining = bytes - prefix;
    (void)FillGuestMemory(memory, destination,
                          recovery_abi::Address(state.r[4]), bytes);
    state.r[6] = state.r[3] + prefix + (remaining & ~3u);
    state.r[5] -= prefix;
    state.r[4] = (state.r[4] & 0xffffffff00000000ull) |
                 (unsigned(std::uint8_t(state.r[4])) * 0x01010101u);
    auto tail = remaining & 3u;
    state.r[0] = tail;
    state.cr0 = {0, std::uint8_t(tail > 0), std::uint8_t(tail == 0),
                 state.xer_so};
    state.ctr = tail == 3 ? 1 : 0;
    return true;
  }
  return Route(entry, memory, dependencies, state);
}
class Bridge final : public manager_release_context61::GuestServices {
  Dependencies original;

public:
  explicit Bridge(Dependencies dependencies) : original(dependencies) {}
  void CallDirect(GuestAddress entry, GuestMemory &memory,
                  Registers &state) override {
    if (!Routed(entry, memory, {*this, original.fp}, state))
      original.guest.CallDirect(entry, memory, state);
  }
  void CallIndirect(GuestAddress entry, GuestMemory &memory,
                    Registers &state) override {
    if (!Routed(entry, memory, {*this, original.fp}, state))
      original.guest.CallIndirect(entry, memory, state);
  }
};
} // namespace
bool Apply(GuestAddress entry, GuestMemory &memory, Dependencies dependencies,
           Registers &state) {
  Bridge bridge(dependencies);
  return Routed(entry, memory, {bridge, dependencies.fp}, state);
}
} // namespace lo::semantic::gpu::battle_semantic_runtime61
