#pragma once

#include <cstdint>

namespace gpu::render_arena
{
    inline constexpr uint32_t kGpuSlots = 2;
    inline constexpr uint64_t kVertexArenaSize = 1024ull << 20;
    inline constexpr uint64_t kSlotArenaSize = kVertexArenaSize / kGpuSlots;
    inline constexpr uint64_t kArenaHeadroom = 32ull << 20;

    inline uint64_t SlotBase(uint32_t slot)
    {
        return uint64_t(slot) * kSlotArenaSize;
    }

    inline bool SlotLow(uint64_t localOffset)
    {
        return localOffset + kArenaHeadroom > kSlotArenaSize;
    }

    enum class WrapAction : uint8_t { None, FlushAndRecycleIncoming };

    struct WrapDecision
    {
        WrapAction action = WrapAction::None;
        bool resetIncoming = false;
        uint32_t incomingSlot = 0;
    };

    // Prefer the current half, then the other half. kGpuSlots means neither
    // half can hold `needed` and DrawImpl must wrap/reset.
    inline uint32_t VertexAllocSlot(uint32_t currentSlot, uint64_t currentLocalOffset, uint64_t incomingLocalOffset, uint64_t needed)
    {
        if (currentLocalOffset + needed <= kSlotArenaSize)
            return currentSlot;
        const uint32_t incomingSlot = (currentSlot + 1u) % kGpuSlots;
        if (incomingLocalOffset + needed <= kSlotArenaSize)
            return incomingSlot;
        return kGpuSlots;
    }

    // Flush/recycle only when neither half has headroom. A single full half is
    // not a wrap: GetVertexBuffer appends to the other half without waiting.
    // Reset always accompanies that wrap; wait the just-submitted slot too
    // because the open list may hold cache-hit pointers into the incoming half.
    inline WrapDecision EvaluateWrap(uint32_t currentSlot, uint64_t currentLocalOffset, uint64_t incomingLocalOffset)
    {
        WrapDecision decision;
        decision.incomingSlot = (currentSlot + 1u) % kGpuSlots;
        if (!SlotLow(currentLocalOffset) || !SlotLow(incomingLocalOffset))
            return decision;
        decision.action = WrapAction::FlushAndRecycleIncoming;
        decision.resetIncoming = true;
        return decision;
    }

    struct WrapFenceWait
    {
        bool waitIncoming = false;
        bool waitSubmitted = false;
    };

    inline uint32_t SubmittedSlotAfterFlush(uint32_t incomingSlot)
    {
        return (incomingSlot + kGpuSlots - 1u) % kGpuSlots;
    }

    inline WrapFenceWait WrapFenceWaits(const WrapDecision& decision)
    {
        WrapFenceWait wait;
        if (decision.action == WrapAction::None)
            return wait;
        wait.waitIncoming = true;
        wait.waitSubmitted = decision.resetIncoming;
        return wait;
    }

    // Two command slots hide one in-flight list. A present-time Flush after
    // the swap Flush submits the incoming slot, so the next Begin waits the
    // list recorded this frame. Keep present barriers on the swap submit.
    inline bool ExtraSubmitConsumesIncomingSlot(uint32_t submitsThisFrame)
    {
        return submitsThisFrame >= kGpuSlots;
    }
}
