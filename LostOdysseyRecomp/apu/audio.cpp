#include <stdafx.h>
#include "audio.h"
#include "audio_callback.h"
#include "matrix_surround.h"
#include <cpu/guest_thread.h>
#include <kernel/memory.h>
#include <os/guest_code_thread.h>
#include <os/host_scheduling.h>
#include <os/logger.h>
#include <os/thread_name.h>
#include <SDL3/SDL.h>
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
        SDL_AudioStream* g_stream = nullptr;
        uint32_t g_channels = 2; // interleaved channels queued to g_device
        bool g_paused = false;
        bool g_audioReady = false;
        std::atomic<bool> g_surroundRequested{ false };
        bool g_surroundOpen = false; // driver thread after Init
        std::atomic<bool> g_matrix{ false }; // matrix-encode the stereo frames
        std::atomic<uint32_t> g_matrixPhase{ 90 };
        std::atomic<uint32_t> g_outputChannels{ 0 };

        uint32_t FrameBytes() { return XAUDIO_NUM_SAMPLES * g_channels * sizeof(float); }

        // Caller holds g_deviceMutex. 5.1 needs a device that mixes six or
        // more channels. PulseAudio, PipeWire and Core Audio accept six on
        // any device and remix them, so their layout is checked before
        // opening. Before opening, WASAPI reports the endpoint's hardware
        // format as of startup: it stays stereo when an encoder (Dolby
        // Digital Live, DTS Interactive over S/PDIF) takes a 5.1 mix, and
        // misses a speaker layout changed while the game runs. The open
        // device reports the live mix format, so WASAPI opens six and
        // checks that.
        void OpenDevice(bool surround)
        {
            if (g_stream) SDL_DestroyAudioStream(g_stream);
            g_stream = nullptr;
            if (g_device) SDL_CloseAudioDevice(g_device);
            g_device = 0;
            g_channels = 2;
            SDL_AudioSpec desired{SDL_AUDIO_F32, 2, XAUDIO_SAMPLES_HZ};
            SDL_AudioSpec preferred{};
            const int listedChannels = SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &preferred, nullptr)
                ? preferred.channels : 0;
            int mixChannels = 0;
            const char* driver = SDL_GetCurrentAudioDriver();
            const bool wasapi = driver && SDL_strcmp(driver, "wasapi") == 0;
            if (surround && (wasapi || listedChannels >= 6))
                desired.channels = 6;
            g_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
            if (g_device)
            {
                SDL_AudioSpec obtained{};
                if (!SDL_GetAudioDeviceFormat(g_device, &obtained, nullptr))
                {
                    SDL_CloseAudioDevice(g_device);
                    g_device = 0;
                }
                else if ((mixChannels = obtained.channels) < 6)
                    desired.channels = 2; // the open device takes the stereo downmix
            }
            if (!g_device && desired.channels == 6)
            {
                desired.channels = 2;
                g_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
            }
            if (surround && desired.channels != 6)
                LOG_WARNING("5.1 output unavailable (device channels listed {} mix {}); using the stereo downmix",
                    listedChannels, mixChannels);
            if (g_device)
            {
                SDL_AudioSpec obtained{};
                if (SDL_GetAudioDeviceFormat(g_device, &obtained, nullptr))
                    g_stream = SDL_CreateAudioStream(&desired, &obtained);
                if (!g_stream || !SDL_BindAudioStream(g_device, g_stream))
                {
                    if (g_stream) SDL_DestroyAudioStream(g_stream);
                    g_stream = nullptr;
                    SDL_CloseAudioDevice(g_device);
                    g_device = 0;
                }
            }
            if (g_device)
            {
                g_channels = desired.channels;
                if (g_paused) SDL_PauseAudioDevice(g_device);
                else SDL_ResumeAudioDevice(g_device);
            }
            if (!g_device) LOG_WARNING("audio device unavailable: {}", SDL_GetError());
            else LOG_INFO("audio output: 48000 Hz {} float, device channels listed {} mix {}, SDL driver {}",
                g_channels == 6 ? "5.1" : "stereo", listedChannels, mixChannels, driver ? driver : "none");
            g_outputChannels = g_device ? g_channels : 0;
        }

        // Whole frames waiting in the device queue.
        uint32_t QueuedFrames()
        {
            std::lock_guard lock(g_deviceMutex);
            return g_stream ? std::max(0, SDL_GetAudioStreamQueued(g_stream)) / FrameBytes() : 0;
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

    void Init(Output output)
    {
        const bool surround = output == Output::Surround;
        g_matrix = output == Output::Matrix;
        g_surroundRequested = g_surroundOpen = surround;
        // SDL3 picks 1024-frame device buffers at 48 kHz; keep SDL2's 512.
        SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512");
        g_audioReady = SDL_InitSubSystem(SDL_INIT_AUDIO);
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

    void SetOutput(Output output)
    {
        g_matrix = output == Output::Matrix; // stereo frames, same device
        const bool surround = output == Output::Surround;
        if (g_surroundRequested.exchange(surround) != surround)
            g_outputChannels = 0; // unknown until the driver thread has applied it
    }

    void SetMatrixPhase(uint32_t degrees)
    {
        g_matrixPhase = std::min(degrees, 180u);
    }

    uint32_t OutputChannels()
    {
        return g_outputChannels;
    }

    void UnregisterClient()
    {
        g_client.Unregister();
        std::lock_guard lock(g_deviceMutex);
        if (g_stream) SDL_ClearAudioStream(g_stream);
    }

    void SubmitFrame(const void* samples)
    {
        if (!samples) return;
        const auto* words = static_cast<const uint32_t*>(samples);
        std::array<float, XAUDIO_NUM_SAMPLES * 2> stereo{};
        // Guest planes FL, FR, FC, LFE, BL, BR match SDL's 6-channel order
        // FL, FR, FC, LFE, SL/BL, SR/BR, so 5.1 interleaves them unchanged.
        std::array<float, XAUDIO_NUM_SAMPLES * XAUDIO_NUM_CHANNELS> surround;
        // The encoder's filters carry state from frame to frame; this runs
        // on the one thread that submits frames.
        static MatrixSurround matrix;
        static bool matrixOn = false;
        static uint32_t matrixPhase = 90;
        if (const bool on = g_matrix; on != matrixOn)
        {
            matrix.Reset();
            matrixOn = on;
        }
        if (const uint32_t phase = g_matrixPhase; phase != matrixPhase)
        {
            matrix.SetPhase(float(phase));
            matrixPhase = phase;
        }
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
            float left, right;
            if (matrixOn)
                matrix.Encode(channel, left, right);
            else
            {
                // Guest order: FL, FR, FC, LFE, BL, BR. Include center dialogue
                // and rear effects in the stereo fold-down, with headroom.
                left = (channel[0] + 0.7071f * (channel[2] + channel[4]) + 0.5f * channel[3]) * 0.5f;
                right = (channel[1] + 0.7071f * (channel[2] + channel[5]) + 0.5f * channel[3]) * 0.5f;
            }
            stereo[i * 2] = std::clamp(left, -1.0f, 1.0f);
            stereo[i * 2 + 1] = std::clamp(right, -1.0f, 1.0f);
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
            if (g_stream)
            {
                const float* data = g_channels == 6 ? surround.data() : stereo.data();
                if (SDL_GetAudioStreamQueued(g_stream) < int(FrameBytes() * 16))
                {
                    if (!SDL_PutAudioStreamData(g_stream, data, int(FrameBytes()))) ++queueErrors;
                }
                else ++queueDrops;
                if (report) queued = std::max(0, SDL_GetAudioStreamQueued(g_stream));
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
            if (paused) SDL_PauseAudioDevice(g_device);
            else SDL_ResumeAudioDevice(g_device);
        }
    }
}
