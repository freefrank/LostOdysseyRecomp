#pragma once

// Heap census (os/switch/heap_census.cpp): built only with -DLO_SWITCH_HEAP_CENSUS=ON, started by LO_HEAP_CENSUS=1
// in env.txt. Start() reads env.txt's switches, so it is called after they are applied.
namespace os::heap_census
{
#if defined(__SWITCH__) && defined(LO_SWITCH_HEAP_CENSUS)
    void Start();
#else
    inline void Start() {}
#endif
}
