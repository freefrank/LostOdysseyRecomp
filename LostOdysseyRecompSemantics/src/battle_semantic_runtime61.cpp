#include "lo_semantics/battle_semantic_runtime61.h"
#include "battle_semantic_routes.inc"
namespace lo::semantic::gpu::battle_semantic_runtime61 {
namespace {
class Bridge final : public manager_release_context61::GuestServices {
  Dependencies original;

public:
  explicit Bridge(Dependencies dependencies) : original(dependencies) {}
  void CallDirect(GuestAddress entry, GuestMemory &memory,
                  Registers &state) override {
    if (!Route(entry, memory, {*this, original.fp}, state))
      original.guest.CallDirect(entry, memory, state);
  }
  void CallIndirect(GuestAddress entry, GuestMemory &memory,
                    Registers &state) override {
    if (!Route(entry, memory, {*this, original.fp}, state))
      original.guest.CallIndirect(entry, memory, state);
  }
};
} // namespace
bool Apply(GuestAddress entry, GuestMemory &memory, Dependencies dependencies,
           Registers &state) {
  Bridge bridge(dependencies);
  return Route(entry, memory, {bridge, dependencies.fp}, state);
}
} // namespace lo::semantic::gpu::battle_semantic_runtime61
