#include "../../LostOdysseyRecomp/hid/test_input_pulse.h"
#include <cassert>
#include <cstdio>
int main()
{
    hid::TestInputPulse pulse;
    pulse.Set(10, 2);
    // Mid-update acceptance cannot partially press the current update.
    for (int i = 0; i < 100; ++i) assert(!pulse.Active(10) && pulse.Pending(10));
    for (int i = 0; i < 100; ++i) assert(pulse.Active(11));
    assert(pulse.Active(12));
    assert(!pulse.Active(13) && !pulse.Pending(13));
    // A stall does not consume duration; cancellation does not need a tick.
    pulse.Set(20, 5);
    for (int i = 0; i < 10000; ++i) assert(pulse.Active(21));
    pulse.Set(21, 0);
    assert(!pulse.Active(21) && !pulse.Pending(21));
    // Replacement schedules the new command for a complete future tick.
    pulse.Set(30, 4);
    assert(pulse.Active(31));
    pulse.Set(31, 1);
    assert(!pulse.Active(31) && pulse.Pending(31));
    assert(pulse.Active(32) && !pulse.Active(33));
    assert(!pulse.Active(1000));
    pulse.Set(1000, 1);
    pulse.Set(1000, 0); // cancel even when no next engine update ever arrives
    assert(!pulse.Active(1000) && !pulse.Active(1001));
    puts("PASS: same-tick reads, next-tick start, bounded duration, stalls, cancellation and replacement (not menu consumption)");
}
