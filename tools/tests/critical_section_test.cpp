#include <kernel/critical_section.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <thread>

static void Check(bool value)
{
    if (!value) { std::fputs("critical section regression failed\n", stderr); std::abort(); }
}

// Deliberately decode bytes as a guest lwz, independently of the HLE helper.
static uint32_t GuestLoad(const void* p)
{
    const auto* b = static_cast<const unsigned char*>(p);
    return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | b[3];
}

int main()
{
    using namespace GuestCriticalSection;
    int32_t recursion = 0;
    uint32_t owner = 0;
    constexpr uint32_t mainThread = 0x00403000;
    constexpr uint32_t workerThread = 0x01e30000;
    Enter(recursion, owner, mainThread);
    Check(GuestLoad(&recursion) == 1 && GuestLoad(&owner) == mainThread);
    Check(TryEnter(recursion, owner, mainThread));
    Check(GuestLoad(&recursion) == 2);
    Check(!TryEnter(recursion, owner, workerThread));
    Check(GuestLoad(&recursion) == 2 && GuestLoad(&owner) == mainThread);

    // sub_82CC3FD0 reads CS+20, releases that many times, then reacquires.
    const uint32_t count = GuestLoad(&recursion);
    for (uint32_t i = 0; i < count; ++i) Leave(recursion, owner);
    Check(GuestLoad(&recursion) == 0 && GuestLoad(&owner) == 0);
    std::thread handoff([&] {
        Enter(recursion, owner, workerThread);
        Check(GuestLoad(&owner) == workerThread && GuestLoad(&recursion) == 1);
        Leave(recursion, owner);
    });
    handoff.join();
    for (uint32_t i = 0; i < count; ++i) Enter(recursion, owner, mainThread);
    Check(GuestLoad(&recursion) == count && GuestLoad(&owner) == mainThread);
    for (uint32_t i = 0; i < count; ++i) Leave(recursion, owner);

    uint32_t protectedCounter = 0;
    std::array<std::thread, 4> workers;
    for (uint32_t t = 0; t < workers.size(); ++t)
        workers[t] = std::thread([&, t] {
            const uint32_t id = 0x01003000 + t * 0x1000;
            for (unsigned i = 0; i < 25000; ++i)
            {
                Enter(recursion, owner, id);
                Check(GuestLoad(&owner) == id && GuestLoad(&recursion) == 1);
                Enter(recursion, owner, id);
                Check(GuestLoad(&recursion) == 2);
                ++protectedCounter;
                Leave(recursion, owner);
                Leave(recursion, owner);
            }
        });
    for (auto& worker : workers) worker.join();
    Check(protectedCounter == 100000 && recursion == 0 && owner == 0);
    std::puts("PASS: guest big-endian words, release/reacquire, try-enter, 100000 contended recursive acquisitions");
}
