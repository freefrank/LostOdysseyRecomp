#include <stdafx.h>
#include "audio.h"
#include <cpu/guest_thread.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <SDL.h>
#include <cmath>

namespace apu
{
    namespace
    {
        std::atomic<uint32_t> g_callback{ 0 };
        std::atomic<uint32_t> g_param{ 0 };
        std::atomic<uint32_t> g_framesSubmitted{ 0 };
        std::thread g_thread;
        std::atomic<bool> g_running{ false };
        SDL_AudioDeviceID g_device = 0;
        constexpr uint32_t kStereoFrameBytes = XAUDIO_NUM_SAMPLES * 2 * sizeof(float);

        void CaptureRequested(const float* stereo, uint32_t frame)
        {
            static const char* requestPath = getenv("LO_AUDIO_CAPTURE_REQUEST");
            if (!requestPath) return;
            static uint64_t serial = 0;
            static uint32_t remaining = 0;
            static std::ofstream output;
            if ((frame % 32) == 0)
            {
                std::ifstream request(requestPath);
                uint64_t next = 0;
                uint32_t seconds = 0;
                if ((request >> next >> seconds) && next && next != serial && seconds <= 60)
                {
                    if (output.is_open()) output.close();
                    serial = next;
                    remaining = 0;
                    if (seconds)
                    {
                        auto path = std::filesystem::path(requestPath);
                        path.replace_filename(path.stem().string() + "-" + std::to_string(serial) + ".f32");
                        std::error_code ec;
                        const bool exists = std::filesystem::exists(path, ec);
                        if (!exists && !ec)
                        {
                            output.clear();
                            output.open(path, std::ios::binary);
                            if (output) remaining = seconds * XAUDIO_SAMPLES_HZ;
                        }
                        if (!remaining) LOG_WARNING("audio capture {} could not create a new file", serial);
                    }
                    LOG_INFO("audio capture request {}: samples={} start_frame={}", serial, remaining, frame);
                }
            }
            if (!remaining) return;
            const auto count = std::min(remaining, uint32_t(XAUDIO_NUM_SAMPLES));
            output.write(reinterpret_cast<const char*>(stereo), count * 2 * sizeof(float));
            remaining -= count;
            if (!output || !remaining)
            {
                LOG_INFO("audio capture {} ended: success={} remaining={}", serial, bool(output), remaining);
                remaining = 0;
                output.close();
            }
        }

        void DriverMain()
        {
            GuestThreadContext ctx(3);
            constexpr auto framePeriod = std::chrono::microseconds(1000000ull * XAUDIO_NUM_SAMPLES / XAUDIO_SAMPLES_HZ);
            auto next = std::chrono::steady_clock::now();
            while (g_running)
            {
                next += framePeriod;
                std::this_thread::sleep_until(next);
                // Avoid a burst of catch-up callbacks after a host stall.
                if (std::chrono::steady_clock::now() - next > framePeriod * 4)
                    next = std::chrono::steady_clock::now();
                while (g_device && SDL_GetQueuedAudioSize(g_device) >= kStereoFrameBytes * 4)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));

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
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0)
        {
            SDL_AudioSpec desired{};
            desired.freq = XAUDIO_SAMPLES_HZ;
            desired.format = AUDIO_F32SYS;
            desired.channels = 2;
            desired.samples = 512;
            g_device = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
            if (g_device) SDL_PauseAudioDevice(g_device, 0);
        }
        if (!g_device) LOG_WARNING("audio device unavailable: {}", SDL_GetError());
        else LOG_INFO("audio output: 48000 Hz stereo float, SDL driver {}", SDL_GetCurrentAudioDriver());
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
        if (g_device) SDL_ClearQueuedAudio(g_device);
    }

    void SubmitFrame(const void* samples)
    {
        if (!samples) return;
        const auto* words = static_cast<const uint32_t*>(samples);
        std::array<float, XAUDIO_NUM_SAMPLES * 2> stereo{};
        float peak = 0;
        for (uint32_t i = 0; i < XAUDIO_NUM_SAMPLES; ++i)
        {
            float channel[6];
            for (uint32_t c = 0; c < 6; ++c)
            {
                const float value = std::bit_cast<float>(ByteSwap(words[c * XAUDIO_NUM_SAMPLES + i]));
                channel[c] = std::isfinite(value) ? value : 0;
            }
            // Guest order: FL, FR, FC, LFE, BL, BR. Include center dialogue
            // and rear effects in the stereo fold-down, with headroom.
            stereo[i * 2] = std::clamp((channel[0] + 0.7071f * (channel[2] + channel[4]) + 0.5f * channel[3]) * 0.5f, -1.0f, 1.0f);
            stereo[i * 2 + 1] = std::clamp((channel[1] + 0.7071f * (channel[2] + channel[5]) + 0.5f * channel[3]) * 0.5f, -1.0f, 1.0f);
            peak = std::max({peak, std::abs(stereo[i * 2]), std::abs(stereo[i * 2 + 1])});
        }
        uint32_t n = ++g_framesSubmitted;
        CaptureRequested(stereo.data(), n);
        // Bounded diagnostic capture, before mute; raw f32le, 48 kHz stereo.
        static std::ofstream capture;
        if (n == 1)
            if (const char* path = getenv("LO_AUDIO_CAPTURE")) capture.open(path, std::ios::binary);
        if (capture.is_open())
        {
            if (n <= 11250) capture.write(reinterpret_cast<const char*>(stereo.data()), sizeof(stereo));
            else capture.close();
            if (n % 188 == 0) capture.flush();
        }
        static const bool mute = [] {
            const char* value = getenv("LO_AUDIO_MUTE");
            if (!value) value = getenv("LO_BACKGROUND");
            return value && std::strcmp(value, "1") == 0;
        }();
        if (mute) stereo.fill(0);
        if (g_device && SDL_GetQueuedAudioSize(g_device) < kStereoFrameBytes * 16)
            SDL_QueueAudio(g_device, stereo.data(), sizeof(stereo));
        if (n == 1 || (n % 1875) == 0) // every ~10 s
            LOG_INFO("audio frames submitted: {} peak={} queued={} mute={}", n, peak,
                g_device ? SDL_GetQueuedAudioSize(g_device) : 0, mute);
    }
}
