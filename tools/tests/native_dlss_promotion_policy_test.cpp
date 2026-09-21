#include "gpu/scene_copy_promotion_policy.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace gpu::scene_copy_promotion;
namespace {
unsigned checks = 0;
void Require(bool value, const char* message) {
    ++checks;
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
}
int main() {
    Require(CanAppendConstants(0, 512, 256), "two exact-fit constant blocks");
    Require(!CanAppendConstants(1, 512, 256), "alignment can exhaust ring");
    Require(CanAppendConstants(1, 768, 256), "alignment with sufficient space");
    Require(!CanAppendConstants(256, 511, 256, 1), "single block cannot wrap");
    Require(!CanAppendConstants(513, 512, 1), "invalid offset rejected");
    Require(!CanAppendConstants(0, 512, 0), "zero-byte reservation rejected");
    Require(!CanAppendConstants(0, 512, 256, 2, 0), "zero alignment rejected");
    Require(!CanAppendConstants(0, 512, 256, 2, 3), "non-power-of-two alignment rejected");
    constexpr auto max = std::numeric_limits<uint64_t>::max();
    Require(!CanAppendConstants(max - 4, max, 8, 1), "alignment cannot overflow");
    Require(!CanAppendConstants(0, max, max, 2), "combined size cannot overflow");
    Require(CanAppendConstants(max - 255, max, 255, 1), "large exact-fit reservation");
    // Exercise every frame/epoch/storage/depth combination used by the
    // renderer preflight. A Flush changes none of these and preserves mapping.
    for (unsigned flags = 0; flags < 16; ++flags) {
        const bool sameStorage = flags & 1, sameFrame = flags & 2;
        const bool sameEpoch = flags & 4, depthStencil = flags & 8;
        const bool keep = sameStorage && sameFrame && sameEpoch && !depthStencil;
        Require(MustRestore(sameStorage, sameFrame, sameEpoch, depthStencil, 736, 736) == !keep,
            "storage/frame/epoch/depth preflight");
    }
    Require(!MustRestore(true, true, true, false, 720, 736), "padded target retains smaller viewport");
    Require(MustRestore(true, true, true, false, 768, 736), "growing extent restores parked grid");
    std::cout << checks << " promotion policy checks passed\n";
}
