#include <stdafx.h>
#include "audio.h"
#include "audio_callback.h"
#include <cpu/guest_thread.h>
#include <kernel/memory.h>
#include <os/guest_code_thread.h>
#include <os/host_scheduling.h>
#include <os/logger.h>
#include <os/thread_name.h>
#include <SDL.h>
#include <cmath>

namespace apu
{
    namespace
    {
        detail::AudioCallback g_client;
        std::atomic<uint32_t> g_framesSubmitted{ 0 };
        os::GuestCodeThread g_thread; // runs the guest audio callback
        std::atomic<bool> g_running{ false };
        // A live output change closes the device while guest threads may
        // queue, clear or pause it; every use of g_device holds this lock.
        std::mutex g_deviceMutex;
        SDL_AudioDeviceID g_device = 0;
        uint32_t g_channels = 2; // interleaved channels queued to g_device
        bool g_paused = false;
        bool g_audioReady = false;
        std::atomic<bool> g_surroundRequested{ false };
        bool g_surroundOpen = false; // driver thread after Init
        std::atomic<uint32_t> g_outputChannels{ 0 };

        uint32_t FrameBytes() { return XAUDIO_NUM_SAMPLES * g_channels * sizeof(float); }

        // Caller holds g_deviceMutex. 5.1 probes the device layout first:
        // WASAPI reports its endpoint mix format here, while PulseAudio,
        // PipeWire and Core Audio accept six channels and remix them.
        void OpenDevice(bool surround)
        {
            if (g_device) SDL_CloseAudioDevice(g_device);
            g_device = 0;
            g_channels = 2;
            SDL_AudioSpec desired{};
            desired.freq = XAUDIO_SAMPLES_HZ;
            desired.format = AUDIO_F32SYS;
            desired.samples = 512;
            if (surround)
            {
                desired.channels = 6;
                SDL_AudioSpec obtained{};
                g_device = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
                const int deviceChannels = g_device ? obtained.channels : 0;
                if (g_device && deviceChannels != 6)
                {
                    SDL_CloseAudioDevice(g_device);
                    // A 7.1 device takes the 5.1 channels through SDL's layout conversion.
                    g_device = deviceChannels > 6 ? SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0) : 0;
                }
                if (g_device) g_channels = 6;
                else LOG_WARNING("5.1 output unavailable (device channels {}); using the stereo downmix", deviceChannels);
            }
            if (!g_device)
            {
                desired.channels = 2;
                g_device = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
            }
            if (g_device) SDL_PauseAudioDevice(g_device, g_paused ? 1 : 0);
            if (!g_device) LOG_WARNING("audio device unavailable: {}", SDL_GetError());
            else LOG_INFO("audio output: 48000 Hz {} float, SDL driver {}", g_channels == 6 ? "5.1" : "stereo", SDL_GetCurrentAudioDriver());
            g_outputChannels = g_device ? g_channels : 0;
        }

        // Whole frames waiting in the device queue.
        uint32_t QueuedFrames()
        {
            std::lock_guard lock(g_deviceMutex);
            return g_device ? SDL_GetQueuedAudioSize(g_device) / FrameBytes() : 0;
        }

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
            os::SetCurrentThreadName("Audio Driver");
            GuestThreadContext ctx(3);
            constexpr auto framePeriod = std::chrono::microseconds(1000000ull * XAUDIO_NUM_SAMPLES / XAUDIO_SAMPLES_HZ);
            auto next = std::chrono::steady_clock::now();
            while (g_running)
            {
                next += framePeriod;
                // Precise wakeups: a late one drains the few frames SDL has queued.
                os::scheduling::PreciseSleepUntil(next);
                // Avoid a burst of catch-up callbacks after a host stall.
                if (std::chrono::steady_clock::now() - next > framePeriod * 4)
                    next = std::chrono::steady_clock::now();
                if (const bool surround = g_surroundRequested; g_audioReady && surround != g_surroundOpen)
                {
                    std::lock_guard lock(g_deviceMutex);
                    g_surroundOpen = surround;
                    OpenDevice(surround);
                }
                else if (!g_outputChannels && g_device) // a request withdrawn before it was applied
                    g_outputChannels = g_channels;
                while (QueuedFrames() >= 4)
                    os::scheduling::PreciseSleepFor(std::chrono::milliseconds(1));

                g_client.Dispatch([&](uint32_t callback, uint32_t param)
                {
                    ctx.ppcContext.r3.u64 = param;
                    g_memory.FindFunction(callback)(ctx.ppcContext, g_memory.base);
                });
            }
        }
    }

    void Init(bool surround)
    {
        g_surroundRequested = g_surroundOpen = surround;
        g_audioReady = SDL_InitSubSystem(SDL_INIT_AUDIO) == 0;
        {
            std::lock_guard lock(g_deviceMutex);
            if (g_audioReady) OpenDevice(surround);
            else LOG_WARNING("audio device unavailable: {}", SDL_GetError());
        }
        g_running = true;
        g_thread = os::GuestCodeThread(DriverMain);
        g_thread.detach();
    }

    void RegisterClient(uint32_t callback, uint32_t param)
    {
        LOG_INFO("audio client callback {:#x} param {:#x}", callback, param);
        g_client.Register(callback, param);
    }

    void SetSurround(bool surround)
    {
        g_surroundRequested = surround;
        g_outputChannels = 0; // unknown until the driver thread has applied it
    }

    uint32_t OutputChannels()
    {
        return g_outputChannels;
    }

    void UnregisterClient()
    {
        g_client.Unregister();
        std::lock_guard lock(g_deviceMutex);
        if (g_device) SDL_ClearQueuedAudio(g_device);
    }

    void SubmitFrame(const void* samples)
    {
        if (!samples) return;
        const auto* words = static_cast<const uint32_t*>(samples);
        std::array<float, XAUDIO_NUM_SAMPLES * 2> stereo{};
        // Guest planes FL, FR, FC, LFE, BL, BR match SDL's 6-channel order
        // FL, FR, FC, LFE, SL/BL, SR/BR, so 5.1 interleaves them unchanged.
        std::array<float, XAUDIO_NUM_SAMPLES * XAUDIO_NUM_CHANNELS> surround;
        float peak = 0;
        for (uint32_t i = 0; i < XAUDIO_NUM_SAMPLES; ++i)
        {
            float channel[6];
            for (uint32_t c = 0; c < 6; ++c)
            {
                const float value = std::bit_cast<float>(ByteSwap(words[c * XAUDIO_NUM_SAMPLES + i]));
                channel[c] = std::isfinite(value) ? value : 0;
                surround[i * 6 + c] = std::clamp(channel[c], -1.0f, 1.0f);
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
        if (mute)
        {
            stereo.fill(0);
            surround.fill(0);
        }
        static uint32_t queueDrops = 0, queueErrors = 0;
        const bool report = n == 1 || (n % 1875) == 0; // every ~10 s
        uint32_t queued = 0;
        {
            std::lock_guard lock(g_deviceMutex);
            if (g_device)
            {
                const float* data = g_channels == 6 ? surround.data() : stereo.data();
                if (SDL_GetQueuedAudioSize(g_device) < FrameBytes() * 16)
                {
                    if (SDL_QueueAudio(g_device, data, FrameBytes()) != 0) ++queueErrors;
                }
                else ++queueDrops;
                if (report) queued = SDL_GetQueuedAudioSize(g_device);
            }
        }
        if (report)
            LOG_INFO("audio frames submitted: {} peak={} queued={} mute={} queue_drops={} queue_errors={}", n, peak,
                queued, mute, queueDrops, queueErrors);
    }

    void SetPaused(bool paused)
    {
        std::lock_guard lock(g_deviceMutex);
        g_paused = paused;
        if (g_device)
        {
            SDL_PauseAudioDevice(g_device, paused ? 1 : 0);
        }
    }
}
