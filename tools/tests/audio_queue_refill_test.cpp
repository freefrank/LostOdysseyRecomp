#include <apu/audio_queue_refill.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>

int main()
{
    using apu::detail::RefillAudioQueue;
    constexpr uint32_t target = 8;
    for (uint32_t initial = 0; initial <= 16; ++initial)
    {
        uint32_t queue = initial, calls = 0;
        const auto count = RefillAudioQueue(target, [&] { return queue; }, [] { return true; },
            [&] { ++calls; ++queue; });
        assert(count == calls && calls <= target - 1);
        assert(queue == std::max(initial, std::min(target, initial + target - 1)));
    }
    uint32_t calls = 0;
    assert(RefillAudioQueue(target, [] { return 0u; }, [] { return false; }, [&] { ++calls; }) == 0);
    assert(calls == 0); // paused/unavailable device
    assert(RefillAudioQueue(target, [] { return 0u; }, [] { return true; }, [&] { ++calls; }) == 1);
    assert(calls == 1); // unregistered client/failed submission: no busy loop
    uint32_t queue = 0;
    calls = 0;
    assert(RefillAudioQueue(target, [&] { return queue; }, [&] { return calls < 2; },
        [&] { ++calls; ++queue; }) == 2); // pause while recovering
    assert(queue == 2);
    assert(RefillAudioQueue(0, [] { return 0u; }, [] { return true; }, [&] { ++calls; }) == 0);

    // Synthetic device: 256/512/1024-sample consumption bursts, independent
    // render clocks, and repeated >43 ms scheduling stalls. Compare the old
    // reset-and-single-dispatch producer to the production recovery helper.
    uint32_t improved = 0;
    for (uint32_t fps : {30u, 60u})
    for (uint32_t burst : {1u, 2u, 4u})
    {
        uint32_t legacy = 1, recovered = 1, legacyDry = 0, recoveredDry = 0;
        for (uint32_t tick = 0; tick < 1875; ++tick)
        {
            const bool stall = tick && tick % (fps * 3) == 0;
            const uint32_t consume = stall ? 12 : (tick % burst == 0 ? burst : 0);
            legacyDry += consume > legacy;
            recoveredDry += consume > recovered;
            legacy = legacy > consume ? legacy - consume : 0;
            recovered = recovered > consume ? recovered - consume : 0;
            if (legacy < target) ++legacy;
            if (recovered < target) ++recovered;
            RefillAudioQueue(target, [&] { return recovered; }, [] { return true; }, [&] { ++recovered; });
            assert(recovered == target && recovered <= 8);
        }
        assert(recoveredDry <= legacyDry);
        improved += recoveredDry < legacyDry;
        std::cout << fps << " FPS / " << burst * 256 << " sample bursts: legacy empty=" << legacyDry
                  << " recovered=" << recoveredDry << '\n';
    }
    assert(improved > 0);
    std::cout << "PASS bounded production audio reserve recovery; no Android playback claim\n";
}
