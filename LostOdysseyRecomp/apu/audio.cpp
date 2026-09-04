#include <stdafx.h>
#include "audio.h"
#include <cpu/guest_thread.h>
#include <kernel/memory.h>
#include <os/logger.h>

namespace apu
{
    namespace
    {
        std::atomic<uint32_t> g_callback{ 0 };
        std::atomic<uint32_t> g_param{ 0 };
        std::atomic<uint32_t> g_framesSubmitted{ 0 };
        std::thread g_thread;
        std::atomic<bool> g_running{ false };

        void DriverMain()
        {
            GuestThreadContext ctx(3);
            constexpr auto framePeriod = std::chrono::microseconds(1000000ull * XAUDIO_NUM_SAMPLES / XAUDIO_SAMPLES_HZ);
            auto next = std::chrono::steady_clock::now();
            while (g_running)
            {
                next += framePeriod;
                std::this_thread::sleep_until(next);

                uint32_t callback = g_callback.load();
                if (!callback)
                    continue;

                ctx.ppcContext.r3.u64 = g_param.load();
                g_memory.FindFunction(callback)(ctx.ppcContext, g_memory.base);
            }
        }
    }

    void Init()
    {
        g_running = true;
        g_thread = std::thread(DriverMain);
        g_thread.detach();
    }

    void RegisterClient(uint32_t callback, uint32_t param)
    {
        LOG_INFO("audio client callback {:#x} param {:#x}", callback, param);
        g_param = param;
        g_callback = callback;
    }

    void UnregisterClient()
    {
        g_callback = 0;
    }

    void SubmitFrame(const void* samples)
    {
        uint32_t n = ++g_framesSubmitted;
        if (n == 1 || (n % 1875) == 0) // every ~10 s
            LOG_INFO("audio frames submitted: {}", n);
    }
}
