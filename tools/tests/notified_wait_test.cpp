#include <notified_wait.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>

int main()
{
    using namespace std::chrono_literals;

    std::mutex mutex;
    std::condition_variable changed;
    bool ready = true;

    // A notification published before the waiter arrives is represented by
    // state, not by the transient notification itself.
    {
        std::unique_lock lock(mutex);
        assert(notified_wait::Until(changed, lock, 100, [&] { return ready; }));
    }

    ready = false;
    std::thread producer([&] {
        std::this_thread::sleep_for(10ms);
        {
            std::lock_guard lock(mutex);
            ready = true;
        }
        changed.notify_one();
    });
    const auto wakeStart = std::chrono::steady_clock::now();
    {
        std::unique_lock lock(mutex);
        assert(notified_wait::Until(changed, lock, 500, [&] { return ready; }));
    }
    producer.join();
    assert(std::chrono::steady_clock::now() - wakeStart < 250ms);

    ready = false;
    const auto timeoutStart = std::chrono::steady_clock::now();
    {
        std::unique_lock lock(mutex);
        assert(!notified_wait::For(changed, lock, 10ms, [&] { return ready; }));
    }
    assert(std::chrono::steady_clock::now() - timeoutStart >= 5ms);

    // CommandProcessor's idle wait uses this shape: a notified write pointer
    // wakes immediately, while the bounded timeout still permits mirror reads
    // and event pumping when a guest store bypasses the MMIO hook.
    std::atomic<uint32_t> writePointer{7};
    const uint32_t observed = writePointer.load();
    std::thread submitter([&] {
        std::this_thread::sleep_for(10ms);
        {
            std::lock_guard lock(mutex);
            writePointer = 9;
        }
        changed.notify_one();
    });
    {
        std::unique_lock lock(mutex);
        assert(notified_wait::For(changed, lock, 500ms,
            [&] { return writePointer.load() != observed; }));
    }
    submitter.join();

    std::printf("notified wait: pre-notify, early wake, deadline, and CP pointer wake passed\n");
}
