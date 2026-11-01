#include "gpu/texture_budget.h"

#include <cstdio>
#include <stdexcept>
#include <vector>

using namespace gpu::texture_budget;

static size_t checks = 0;
static void Check(bool condition, const char* message)
{
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

static constexpr uint64_t kMib = 1ull << 20;
using Entries = std::vector<Candidate<int>>;

static void TestLimits()
{
    const uint64_t vram = 8192 * kMib;
    Check(LimitsFor(vram, nullptr).hardBytes == 4096 * kMib, "the default is half of the video memory");
    Check(LimitsFor(vram, nullptr).softBytes == 3072 * kMib, "the soft limit is 3/4 of the hard limit");
    Check(LimitsFor(vram, "").hardBytes == 4096 * kMib, "an empty override keeps the default");
    Check(LimitsFor(vram, "2048").hardBytes == 2048 * kMib, "an override replaces the hard limit");
    Check(LimitsFor(vram, "0").hardBytes == 0, "0 turns the budget off");
    Check(LimitsFor(vram, "12x").hardBytes == 4096 * kMib, "a bad override keeps the default");
    Check(LimitsFor(vram, "-1").hardBytes == 4096 * kMib, "a negative override keeps the default");
    Check(LimitsFor(vram, "99999999999999").hardBytes == 4096 * kMib, "a huge override keeps the default");
    Check(LimitsFor(0, nullptr).hardBytes == 0, "no reported video memory has no budget");
}

static void TestOffAndUnderLimit()
{
    Entries entries{{1, 0, 100 * kMib}};
    Check(SelectEvictions(entries, 9000 * kMib, Limits{}, 100000) == 0, "an off budget selects nothing");
    Check(SelectEvictions(entries, 300 * kMib, LimitsForHardBytes(400 * kMib), 100000) == 0, "at the soft limit nothing leaves");
}

static void TestSoftLimitTakesOnlyLongIdle()
{
    const auto limits = LimitsForHardBytes(400 * kMib); // soft 300
    const uint64_t frame = 10000;
    Entries entries{
        {1, frame - kSoftIdleFrames + 1, 50 * kMib},
        {2, frame - kSoftIdleFrames, 50 * kMib},
        {3, frame - 2 * kSoftIdleFrames, 50 * kMib},
    };
    // 390 is over soft, under hard: the two long-idle ones may go, the newer one stays.
    Check(SelectEvictions(entries, 390 * kMib, limits, frame) == 2, "soft pressure takes long-idle textures only");
    Check(entries[0].handle == 3 && entries[1].handle == 2, "the oldest leaves first");
}

static void TestSoftLimitStopsWhenReached()
{
    const auto limits = LimitsForHardBytes(400 * kMib);
    const uint64_t frame = 10000;
    Entries entries{{1, 0, 50 * kMib}, {2, 1, 50 * kMib}, {3, 2, 50 * kMib}};
    Check(SelectEvictions(entries, 340 * kMib, limits, frame) == 1, "eviction stops at the soft limit");
}

static void TestHardLimit()
{
    const auto limits = LimitsForHardBytes(400 * kMib);
    const uint64_t frame = 10000;
    Entries entries{
        {1, frame - kHardIdleFrames, 200 * kMib},
        {2, frame - 1, 200 * kMib},
        {3, frame, 200 * kMib},
    };
    // 600 is over hard: the first leaves; 400 is not over hard, and the rest are young.
    Check(SelectEvictions(entries, 600 * kMib, limits, frame) == 1, "hard pressure takes textures idle for a short time");
    Entries active{{1, frame - 1, 500 * kMib}, {2, frame, 500 * kMib}};
    Check(SelectEvictions(active, 1000 * kMib, limits, frame) == 0, "textures in use stay even above the hard limit");
}

static void TestHardThenSoft()
{
    const auto limits = LimitsForHardBytes(400 * kMib);
    const uint64_t frame = 10000;
    Entries entries{
        {1, frame - kHardIdleFrames, 150 * kMib},
        {2, frame - kHardIdleFrames + 1, 150 * kMib},
    };
    // After the first leaves, 370 is under hard, and the second is too young for soft pressure.
    Check(SelectEvictions(entries, 520 * kMib, limits, frame) == 1, "under the hard limit the soft idle time applies again");
}

static void TestFutureAndOversizedEntries()
{
    const auto limits = LimitsForHardBytes(400 * kMib);
    Entries future{{1, ~0ull, 500 * kMib}};
    Check(SelectEvictions(future, 500 * kMib, limits, 10) == 0, "a never-checked stamp is not idle");
    Entries oversized{{1, 0, 500 * kMib}, {2, 1, 500 * kMib}};
    Check(SelectEvictions(oversized, 450 * kMib, limits, 100000) == 1, "bytes above the total do not wrap");
}

int main()
{
    try
    {
        TestLimits();
        TestOffAndUnderLimit();
        TestSoftLimitTakesOnlyLongIdle();
        TestSoftLimitStopsWhenReached();
        TestHardLimit();
        TestHardThenSoft();
        TestFutureAndOversizedEntries();
        std::printf("texture budget: %zu checks passed\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "texture budget failure: %s\n", error.what());
        return 1;
    }
}
