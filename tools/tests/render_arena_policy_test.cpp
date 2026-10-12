#include "../../LostOdysseyRecomp/gpu/render_arena_policy.h"

#include <cstdio>

namespace {
unsigned checks = 0;
unsigned failures = 0;
void Check(bool condition, const char* name) {
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", name); }
}
}

int main() {
    using gpu::render_arena::EvaluateWrap;
    using gpu::render_arena::ExtraSubmitConsumesIncomingSlot;
    using gpu::render_arena::SlotBase;
    using gpu::render_arena::SlotLow;
    using gpu::render_arena::SubmittedSlotAfterFlush;
    using gpu::render_arena::VertexAllocSlot;
    using gpu::render_arena::WrapAction;
    using gpu::render_arena::WrapFenceWaits;
    using gpu::render_arena::kArenaHeadroom;
    using gpu::render_arena::kGpuSlots;
    using gpu::render_arena::kSlotArenaSize;
    using gpu::render_arena::kVertexArenaSize;

    Check(kGpuSlots == 2, "two GPU slots");
    Check(kVertexArenaSize == (1024ull << 20), "1 GiB vertex arena covers a city hold without wrap");
    Check(kSlotArenaSize * kGpuSlots == kVertexArenaSize, "halves cover the arena");
    Check(SlotBase(0) == 0, "slot 0 base");
    Check(SlotBase(1) == kSlotArenaSize, "slot 1 base");
    Check(!SlotLow(0), "empty slot is not low");
    Check(!SlotLow(kSlotArenaSize - kArenaHeadroom), "exactly at headroom is not low");
    Check(SlotLow(kSlotArenaSize - kArenaHeadroom + 1), "one byte past headroom is low");

    const auto idle = EvaluateWrap(0, 0, 0);
    Check(idle.action == WrapAction::None, "room left does not wrap");
    Check(idle.resetIncoming == false, "idle wrap does not reset");
    Check(idle.incomingSlot == 1, "incoming is the other slot");

    const uint64_t low = kSlotArenaSize - kArenaHeadroom + 1;
    const auto wrapKeep = EvaluateWrap(0, low, 0);
    Check(wrapKeep.action == WrapAction::None, "current low with incoming room does not wrap");
    Check(wrapKeep.resetIncoming == false, "no wrap does not reset");
    Check(wrapKeep.incomingSlot == 1, "incoming is still the other slot");
    Check(VertexAllocSlot(0, low, 0, 64) == 0, "headroom-low still fits a small alloc in current");
    Check(VertexAllocSlot(0, kSlotArenaSize, 0, 64) == 1, "full current half allocates in the other half");
    Check(VertexAllocSlot(0, 0, 0, 64) == 0, "empty current half allocates in place");
    Check(VertexAllocSlot(0, kSlotArenaSize, kSlotArenaSize, 64) == kGpuSlots, "both halves full cannot allocate");

    const auto wrapReset = EvaluateWrap(1, low, low);
    Check(wrapReset.action == WrapAction::FlushAndRecycleIncoming, "both low still wraps one slot");
    Check(wrapReset.resetIncoming == true, "both-low wrap resets incoming");
    Check(wrapReset.incomingSlot == 0, "wrap from slot 1 recycles slot 0");
    Check(wrapReset.incomingSlot != 1, "never recycle the current slot as incoming");

    Check(SubmittedSlotAfterFlush(1) == 0, "flush to slot 1 submitted slot 0");
    Check(SubmittedSlotAfterFlush(0) == 1, "flush to slot 0 submitted slot 1");

    const auto idleWait = WrapFenceWaits(idle);
    Check(!idleWait.waitIncoming, "idle wrap does not wait incoming");
    Check(!idleWait.waitSubmitted, "idle wrap does not wait submitted");

    const auto keepWait = WrapFenceWaits(wrapKeep);
    Check(!keepWait.waitIncoming, "single-half-full wrap does not wait incoming");
    Check(!keepWait.waitSubmitted, "single-half-full wrap does not wait just-submitted");

    const auto resetWait = WrapFenceWaits(wrapReset);
    Check(resetWait.waitIncoming, "reset waits incoming");
    Check(resetWait.waitSubmitted, "reset waits just-submitted before overwriting incoming half");

    Check(!ExtraSubmitConsumesIncomingSlot(1), "one swap submit keeps the incoming slot free");
    Check(ExtraSubmitConsumesIncomingSlot(2), "present Flush after swap Flush consumes the incoming slot");

    std::printf("render_arena_policy: %u/%u checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
