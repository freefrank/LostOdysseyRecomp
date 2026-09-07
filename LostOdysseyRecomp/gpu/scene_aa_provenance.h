#pragma once
#include <cstdint>

namespace gpu::scene_aa
{
// Describes scene treatment before ordinary UI overlays, not whether UI pixels
// themselves were filtered. Mixed means some retained content may already have
// AA; applying full-frame AA would risk processing those pixels twice.
enum class Coverage { None, Full, Mixed };

constexpr bool SkipFinalAA(Coverage coverage) { return coverage != Coverage::None; }

class Provenance
{
public:
    Coverage Get(uint64_t frame, uint64_t allocation) const
    {
        return valid_ && frame_ == frame && allocation_ == allocation ? coverage_ : Coverage::None;
    }

    // Call only after an actual, proven full scene copy has been recorded.
    void MarkFull(uint64_t frame, uint64_t allocation) { Set(frame, allocation, Coverage::Full); }

    // A confirmed clear or opaque replacement removes scene treatment in its
    // written region. Ordinary UI overlays deliberately do not call this.
    void Invalidate(uint64_t frame, uint64_t allocation, bool fullExtent)
    {
        Resolve(frame, allocation, Coverage::None, fullExtent);
    }

    // Source coverage must come from the actual source allocation and frame.
    // fullExtent describes destination coverage, not just the source dimensions.
    // A partial copy cannot erase the provenance of destination pixels it retains.
    void Resolve(uint64_t frame, uint64_t allocation, Coverage source, bool fullExtent)
    {
        Coverage result = source;
        if (!fullExtent)
        {
            Coverage retained = Coverage::None;
            if (valid_ && allocation_ == allocation)
            {
                // A partial write can retain AA-treated pixels from an older
                // frame. Keep their risk without calling them a current full scene.
                retained = frame_ == frame ? coverage_ :
                    (coverage_ == Coverage::None ? Coverage::None : Coverage::Mixed);
            }
            result = retained == source ? source : Coverage::Mixed;
        }
        Set(frame, allocation, result);
    }

private:
    void Set(uint64_t frame, uint64_t allocation, Coverage coverage)
    {
        valid_ = true; frame_ = frame; allocation_ = allocation; coverage_ = coverage;
    }
    uint64_t frame_ = 0, allocation_ = 0;
    Coverage coverage_ = Coverage::None;
    bool valid_ = false;
};
} // namespace gpu::scene_aa
