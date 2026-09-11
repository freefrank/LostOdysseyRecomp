// Finite synthetic target: one busy worker and one waiting worker. No game IO.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

std::atomic<bool> stop{false};
std::atomic<std::uint64_t> result{0};
#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline))
#endif
void profiler_fixture_busy() {
    std::uint64_t state = 1234567;
    while (!stop.load(std::memory_order_relaxed)) {
        for (unsigned i = 0; i < 4096; ++i) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
        }
        result.store(state, std::memory_order_relaxed);
    }
}
int main() {
    std::thread busy(profiler_fixture_busy);
    std::thread waiting([] {
        while (!stop.load(std::memory_order_relaxed)) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    });
    std::this_thread::sleep_for(std::chrono::seconds(8));
    stop.store(true, std::memory_order_relaxed);
    busy.join();
    waiting.join();
    std::cout << "Fixture completed: " << result.load() << '\n';
}
