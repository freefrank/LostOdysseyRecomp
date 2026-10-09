#include "guest_code_thread.h"

#if LO_PLATFORM_MACOS || LO_PLATFORM_SWITCH
#include <exception>
#include <future>
#include <mutex>
#include <pthread.h>
#include <system_error>
#include <vector>
#if LO_PLATFORM_SWITCH
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <os/switch_platform.h>
#include <os/switch_cpu_profiler.h>
#endif

namespace os
{
namespace
{
struct Launch
{
    std::function<void()> body;
    std::promise<std::thread::id> id;
    std::shared_ptr<std::atomic<bool>> done;
};

#if LO_PLATFORM_SWITCH
// libnx has no pthread_detach: a thread's stack and kernel object are only
// released by pthread_join. Detached threads are kept here and joined once
// their body has returned (the join then waits at most for the thread's exit).
// Without this every finished guest thread leaked its 4 MiB stack and a slot
// of the process thread limit, and the game soon failed to create threads.
struct Detached
{
    pthread_t thread;
    std::shared_ptr<std::atomic<bool>> done;
};
std::mutex g_detachedMutex;
std::vector<Detached> g_detached;
std::atomic<int> g_live{0};

void ReapDetached()
{
    std::vector<pthread_t> finished;
    {
        std::lock_guard lock(g_detachedMutex);
        for (auto it = g_detached.begin(); it != g_detached.end();)
        {
            if (it->done->load(std::memory_order_acquire))
            {
                finished.push_back(it->thread);
                it = g_detached.erase(it);
            }
            else
                ++it;
        }
    }
    for (pthread_t thread : finished)
        pthread_join(thread, nullptr);
}
#endif

void* RunLaunch(void* argument)
{
    auto* launch = static_cast<Launch*>(argument);
    // Report the id this thread observes through std::this_thread::get_id(),
    // so get_id() on the owner matches it exactly.
    launch->id.set_value(std::this_thread::get_id());
    auto body = std::move(launch->body);
    auto done = std::move(launch->done);
    delete launch;
    body();
#if LO_PLATFORM_SWITCH
    os::switch_cpu_profiler::UnregisterCurrentThread();
#endif
    done->store(true, std::memory_order_release);
    return nullptr;
}
}

void GuestCodeThread::Start(std::function<void()> body)
{
#if LO_PLATFORM_SWITCH
    ReapDetached();
#endif
    auto done = std::make_shared<std::atomic<bool>>(false);
    auto* launch = new Launch{std::move(body), {}, done};
    auto id = launch->id.get_future();
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, kGuestCodeStackSize);
#if LO_PLATFORM_MACOS
    // Guest code is the game loop, audio callback and GPU interrupts; keep it on
    // performance cores rather than letting default QoS drift to efficiency cores.
    pthread_attr_set_qos_class_np(&attributes, QOS_CLASS_USER_INTERACTIVE, 0);
#endif
    pthread_t thread{};
    int error = pthread_create(&thread, &attributes, RunLaunch, launch);
    pthread_attr_destroy(&attributes);
    if (error != 0)
    {
#if LO_PLATFORM_SWITCH
        const std::string memory = switch_platform::MemorySummary();
        std::fprintf(stderr, "pthread_create failed: %s (%d); %d guest code threads alive, %zu detached pending; %s\n",
            std::strerror(error), error, g_live.load(), [] { std::lock_guard lock(g_detachedMutex); return g_detached.size(); }(),
            memory.c_str());
        std::fflush(stderr);
#endif
        delete launch;
        throw std::system_error(error, std::generic_category(), "pthread_create");
    }
#if LO_PLATFORM_SWITCH
    ++g_live;
#endif
    m_handle = reinterpret_cast<void*>(thread);
    m_done = std::move(done);
    m_id = id.get();
    m_joinable = true;
}

GuestCodeThread& GuestCodeThread::operator=(GuestCodeThread&& other) noexcept
{
    if (m_joinable)
        std::terminate();
    m_handle = std::exchange(other.m_handle, nullptr);
    m_done = std::move(other.m_done);
    m_id = std::exchange(other.m_id, std::thread::id{});
    m_joinable = std::exchange(other.m_joinable, false);
    return *this;
}

GuestCodeThread::~GuestCodeThread()
{
    if (m_joinable)
        std::terminate();
}

void GuestCodeThread::join()
{
    if (!m_joinable)
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "join");
    pthread_join(reinterpret_cast<pthread_t>(m_handle), nullptr);
    m_joinable = false;
#if LO_PLATFORM_SWITCH
    --g_live;
#endif
}

void GuestCodeThread::detach()
{
    if (!m_joinable)
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "detach");
#if LO_PLATFORM_SWITCH
    {
        std::lock_guard lock(g_detachedMutex);
        g_detached.push_back({reinterpret_cast<pthread_t>(m_handle), std::move(m_done)});
    }
    --g_live;
    ReapDetached();
#else
    pthread_detach(reinterpret_cast<pthread_t>(m_handle));
#endif
    m_joinable = false;
}
}
#endif
