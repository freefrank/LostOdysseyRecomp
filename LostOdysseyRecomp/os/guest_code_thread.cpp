#include "guest_code_thread.h"

#if LO_PLATFORM_MACOS || LO_PLATFORM_SWITCH
#include <exception>
#include <future>
#include <pthread.h>
#include <system_error>

namespace os
{
namespace
{
struct Launch
{
    std::function<void()> body;
    std::promise<std::thread::id> id;
};

void* RunLaunch(void* argument)
{
    auto* launch = static_cast<Launch*>(argument);
    // Report the id this thread observes through std::this_thread::get_id(),
    // so get_id() on the owner matches it exactly.
    launch->id.set_value(std::this_thread::get_id());
    auto body = std::move(launch->body);
    delete launch;
    body();
    return nullptr;
}
}

void GuestCodeThread::Start(std::function<void()> body)
{
    auto* launch = new Launch{std::move(body), {}};
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
    const int error = pthread_create(&thread, &attributes, RunLaunch, launch);
    pthread_attr_destroy(&attributes);
    if (error != 0)
    {
        delete launch;
        throw std::system_error(error, std::generic_category(), "pthread_create");
    }
    m_handle = reinterpret_cast<void*>(thread);
    m_id = id.get();
    m_joinable = true;
}

GuestCodeThread& GuestCodeThread::operator=(GuestCodeThread&& other) noexcept
{
    if (m_joinable)
        std::terminate();
    m_handle = std::exchange(other.m_handle, nullptr);
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
}

void GuestCodeThread::detach()
{
    if (!m_joinable)
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "detach");
    pthread_detach(reinterpret_cast<pthread_t>(m_handle));
    m_joinable = false;
}
}
#endif
