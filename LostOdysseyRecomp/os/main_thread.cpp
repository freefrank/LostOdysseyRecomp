#include "main_thread.h"
#include <os/platform.h>
#include <os/guest_code_thread.h>

#if LO_PLATFORM_MACOS
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <pthread.h>
#endif

namespace os::main_thread
{
#if LO_PLATFORM_MACOS
namespace
{
struct Request
{
    const std::function<void()>* task;
    bool done = false;
};

std::mutex g_mutex;
std::condition_variable g_wake; // main thread: a request was queued or the body finished
std::condition_variable g_done; // requesters: a request completed
std::deque<Request*> g_queue;
bool g_serving = false;
bool g_finished = false;

struct Body
{
    const std::function<int()>* function;
    int result = 0;
};

void* RunBody(void* argument)
{
    auto* body = static_cast<Body*>(argument);
    body->result = (*body->function)();
    std::lock_guard lock(g_mutex);
    g_finished = true;
    g_wake.notify_one();
    return nullptr;
}

// Runs queued requests with the lock released; returns with it held.
void Drain(std::unique_lock<std::mutex>& lock)
{
    while (!g_queue.empty())
    {
        Request* request = g_queue.front();
        g_queue.pop_front();
        lock.unlock();
        (*request->task)();
        lock.lock();
        request->done = true;
        g_done.notify_all();
    }
}
}

void Run(const std::function<void()>& task)
{
    if (pthread_main_np())
    {
        task();
        return;
    }
    std::unique_lock lock(g_mutex);
    if (!g_serving)
    {
        lock.unlock();
        task();
        return;
    }
    Request request{&task};
    g_queue.push_back(&request);
    g_wake.notify_one();
    g_done.wait(lock, [&] { return request.done; });
}

int RunServing(const std::function<int()>& body, const std::function<void()>& idle)
{
    Body state{&body};
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    // Keep the stack the body had on the main thread (8 MiB by default); a
    // secondary thread otherwise gets 512 KiB on macOS.
    pthread_attr_setstacksize(&attributes, pthread_get_stacksize_np(pthread_self()));
    // The body runs the guest's main thread (see os::GuestCodeThread).
    pthread_attr_set_qos_class_np(&attributes, QOS_CLASS_USER_INTERACTIVE, 0);
    {
        std::lock_guard lock(g_mutex);
        g_serving = true;
        g_finished = false;
    }
    pthread_t thread{};
    const int created = pthread_create(&thread, &attributes, RunBody, &state);
    pthread_attr_destroy(&attributes);
    if (created != 0)
    {
        {
            std::lock_guard lock(g_mutex);
            g_serving = false;
        }
        return body();
    }

    std::unique_lock lock(g_mutex);
    while (true)
    {
        Drain(lock);
        if (g_finished)
            break;
        lock.unlock();
        idle();
        lock.lock();
        // Idle pumping at the Windows window thread's 8 ms cadence.
        g_wake.wait_for(lock, std::chrono::milliseconds(8),
                        [] { return !g_queue.empty() || g_finished; });
    }
    // Later requests run inline on their own thread; finish any already queued.
    g_serving = false;
    Drain(lock);
    lock.unlock();
    pthread_join(thread, nullptr);
    return state.result;
}
#elif LO_PLATFORM_SWITCH
// No main-thread rule on Horizon, but the main thread's stack comes from the
// homebrew loader. Run the guest's main thread with the guest-code stack size.
int RunServing(const std::function<int()>& body, const std::function<void()>&)
{
    int result = 0;
    os::GuestCodeThread thread([&] { result = body(); });
    thread.join();
    return result;
}

void Run(const std::function<void()>& task)
{
    task();
}
#else
int RunServing(const std::function<int()>& body, const std::function<void()>&)
{
    return body();
}

void Run(const std::function<void()>& task)
{
    task();
}
#endif
}
