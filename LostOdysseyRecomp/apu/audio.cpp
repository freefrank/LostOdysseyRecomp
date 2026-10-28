#include <stdafx.h>
#include "audio.h"
#include "audio_callback.h"
#include "fast_forward_audio.h"
#include "matrix_surround.h"
#include "test_signal.h"
#include <cpu/guest_thread.h>
#include <debug/fast_forward.h>
#include <kernel/memory.h>
#include <os/guest_code_thread.h>
#include <os/host_scheduling.h>
#include <os/logger.h>
#include <os/thread_name.h>
#include <SDL3/SDL.h>
#include <cmath>
#if defined(__ANDROID__)
#include <diagnostics/android_tester.h>
#endif

namespace apu
{
    namespace
    {
        detail::AudioCallback g_client;
        detail::FastForwardAudio g_fastForwardAudio;
        std::atomic<bool> g_resetFastForwardAudio{ false };
        std::array<float, XAUDIO_NUM_CHANNELS> g_lastAudioSamples{};
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
        std::atomic<uint32_t> g_matrixRear{ 110 };
        std::atomic<uint32_t> g_voiceVolume{ 100 };
        // steady_clock time until which the speaker test plays
        std::atomic<std::chrono::steady_clock::rep> g_testSignalUntil{ 0 };
        std::atomic<float> g_testPosition{ -1.0f };
        std::atomic<uint32_t> g_outputChannels{ 0 };
        // Frames kept queued ahead of the device. A phone's thermally capped
        // CPU can run the driver thread tens of milliseconds late, so Android
        // keeps ~43 ms instead of ~21 ms; a late wakeup within that catches up.
#if LO_PLATFORM_ANDROID
        constexpr uint32_t kQueuedFrames = 8;
#else
        constexpr uint32_t kQueuedFrames = 4;
#endif
        // Times a frame found the queue empty (the device ran dry) and times
        // the driver gave up catching up after a stall; logged with the frames.
        std::atomic<uint32_t> g_starved{ 0 };
        std::atomic<uint32_t> g_clockResets{ 0 };
        // Guest frames per output frame in this period: 1, or under
        // fast-forward up to its rate. The driver runs the guest callback this
        // many times per period. SubmitFrame shortens them by the same factor,
        // preserving pitch unless the user selects the old Speed up mode.
        std::atomic<uint32_t> g_guestFramesPerOutput{ 1 };
        // Driver thread: the most guest frames a fast-forward period may run.
        constexpr uint32_t kMaxBoostFrames = 8;
        uint32_t g_boostFrames = kMaxBoostFrames;
#if LO_PLATFORM_ANDROID
        // steady_clock time the app last returned to the foreground, until the
        // driver has seen the device drain since then; 0 otherwise. After a
        // long pause AAudio can fail to restart the stream without an error
        // callback, and SDL then never takes audio again (#491).
        std::atomic<std::chrono::steady_clock::rep> g_resumedAt{ 0 };
        constexpr auto kResumeDrainTimeout = std::chrono::milliseconds(300);
#endif

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

#if LO_PLATFORM_ANDROID
        // SDL hands the app lifecycle events only to event watchers, on the
        // thread that handles the pause or resume, before the app blocks in the
        // background; they never reach the event queue.
        bool SDLCALL WatchLifecycle(void*, SDL_Event* event)
        {
            if (event->type == SDL_EVENT_DID_ENTER_FOREGROUND)
                g_resumedAt = std::chrono::steady_clock::now().time_since_epoch().count();
            else if (event->type == SDL_EVENT_WILL_ENTER_BACKGROUND)
                g_resumedAt = 0;
            return true;
        }

        // Driver thread, while the device queue stays full: a device that has
        // not drained since the app came back to the foreground is opened again.
        void ReopenIfStalledAfterResume()
        {
            auto resumed = g_resumedAt.load(std::memory_order_relaxed);
            if (!resumed) return;
            const auto now = std::chrono::steady_clock::now().time_since_epoch();
            const auto stalled = now - std::chrono::steady_clock::duration(resumed);
            if (stalled < kResumeDrainTimeout) return;
            std::lock_guard lock(g_deviceMutex);
            if (!g_stream) return;
            // A paused device keeps its queue; the wait starts over when it plays.
            if (g_paused) { g_resumedAt.compare_exchange_strong(resumed, now.count()); return; }
            // Lost to a pause that began meanwhile, or to a newer resume.
            if (!g_resumedAt.compare_exchange_strong(resumed, 0)) return;
            const uint32_t queued = std::max(0, SDL_GetAudioStreamQueued(g_stream)) / FrameBytes();
            LOG_WARNING("audio: device not draining {} ms after resume (queued {}), reopened",
                std::chrono::duration_cast<std::chrono::milliseconds>(stalled).count(), queued);
            OpenDevice(g_surroundOpen);
        }
#endif

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
            os::scheduling::RaiseAudioThreadPriority();
            GuestThreadContext ctx(3);
            constexpr auto framePeriod = std::chrono::microseconds(1000000ull * XAUDIO_NUM_SAMPLES / XAUDIO_SAMPLES_HZ);
            auto next = std::chrono::steady_clock::now();
            while (g_running)
            {
                next += framePeriod;
                // Precise wakeups: a late one drains the few frames SDL has queued.
                os::scheduling::PreciseSleepUntil(next);
                // Avoid a burst of catch-up callbacks after a host stall.
                if (std::chrono::steady_clock::now() - next > framePeriod * kQueuedFrames)
                {
                    next = std::chrono::steady_clock::now();
                    g_clockResets.fetch_add(1, std::memory_order_relaxed);
                }
                if (const bool surround = g_surroundRequested; g_audioReady && surround != g_surroundOpen)
                {
                    std::lock_guard lock(g_deviceMutex);
                    g_surroundOpen = surround;
                    OpenDevice(surround);
                }
                else if (!g_outputChannels && g_device) // a request withdrawn before it was applied
                    g_outputChannels = g_channels;
#if LO_PLATFORM_ANDROID
                auto resumed = g_resumedAt.load(std::memory_order_relaxed);
                bool waited = false;
#endif
                while (QueuedFrames() >= kQueuedFrames)
                {
                    os::scheduling::PreciseSleepFor(std::chrono::milliseconds(1));
#if LO_PLATFORM_ANDROID
                    waited = true;
                    ReopenIfStalledAfterResume();
#endif
                }
#if LO_PLATFORM_ANDROID
                // A full queue went below the limit: the device takes audio again.
                if (resumed && waited) g_resumedAt.compare_exchange_strong(resumed, 0);
#endif

                const auto dispatch = [&](uint32_t frames)
                {
                    g_guestFramesPerOutput.store(frames, std::memory_order_relaxed);
                    for (uint32_t i = 0; i < frames; ++i)
                    {
                        g_client.Dispatch([&](uint32_t callback, uint32_t param)
                        {
                            // Unregister can run on another guest thread or
                            // inside an earlier callback of this boosted group.
                            // Only this driver touches the processor's state.
                            if (g_resetFastForwardAudio.load(std::memory_order_relaxed)
                                && g_resetFastForwardAudio.exchange(false, std::memory_order_relaxed))
                            {
                                g_fastForwardAudio.Reset();
                                g_lastAudioSamples.fill(0);
                            }
                            ctx.ppcContext.r3.u64 = param;
                            g_memory.FindFunction(callback)(ctx.ppcContext, g_memory.base);
                        });
                    }
                };
                if (const uint32_t rate = debug_menu::fast_forward::AudioRate(); rate > 1)
                {
                    // Up to `rate` guest frames per period, as many as fit in
                    // three quarters of it, so the output never runs dry.
                    const uint32_t frames = std::min(rate, g_boostFrames);
                    const auto start = std::chrono::steady_clock::now();
                    dispatch(frames);
                    const auto spent = std::chrono::steady_clock::now() - start;
                    if (spent > framePeriod * 3 / 4)
                        g_boostFrames = std::max(2u, frames - 1);
                    else if (spent < framePeriod / 2 && frames == g_boostFrames)
                        g_boostFrames = std::min(g_boostFrames + 1, kMaxBoostFrames);
                }
                else
                    dispatch(1);
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
#if LO_PLATFORM_ANDROID
        if (g_audioReady) SDL_AddEventWatch(WatchLifecycle, nullptr);
#endif
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

    void SetMatrixRearAngle(uint32_t degrees)
    {
        g_matrixRear = std::clamp(degrees, 90u, 150u);
    }

    void SetVoiceVolume(uint32_t percent)
    {
        g_voiceVolume = std::min(percent, 100u);
    }

    uint32_t VoiceVolume()
    {
        return g_voiceVolume.load(std::memory_order_relaxed);
    }

    void SetTestSignal(bool on)
    {
        const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
        g_testSignalUntil = on ? until.time_since_epoch().count() : 0;
    }

    std::mutex g_uiSoundMutex;
    std::shared_ptr<const std::vector<float>> g_uiSound;
    size_t g_uiSoundPosition = 0;

    void PlayUiSound(std::shared_ptr<const std::vector<float>> samples)
    {
        std::lock_guard lock(g_uiSoundMutex);
        g_uiSound = std::move(samples);
        g_uiSoundPosition = 0;
    }

    float TestSignalPosition()
    {
        return g_testPosition;
    }

    uint32_t OutputChannels()
    {
        return g_outputChannels;
    }

    void UnregisterClient()
    {
        g_client.Unregister();
        g_resetFastForwardAudio.store(true, std::memory_order_relaxed);
        std::lock_guard lock(g_deviceMutex);
        if (g_stream) SDL_ClearAudioStream(g_stream);
    }

    static void EmitFrame(const uint32_t* words);

    void SubmitFrame(const void* samples)
    {
        if (!samples) return;
        const auto* words = static_cast<const uint32_t*>(samples);
        const uint32_t rate = g_guestFramesPerOutput.load(std::memory_order_relaxed);
        if (rate > 1 || g_fastForwardAudio.Active())
        {
            if (rate > 1 && !g_fastForwardAudio.Active())
                g_fastForwardAudio.StartFrom(g_lastAudioSamples);
            g_fastForwardAudio.Process(words, rate, debug_menu::fast_forward::KeepAudioPitch(), EmitFrame);
            return;
        }
        // Normal playback goes straight to the original mixer: no stretch
        // buffers, conversions, FFTs or allocations on this path.
        EmitFrame(words);
    }

    static void EmitFrame(const uint32_t* words)
    {
        std::array<float, XAUDIO_NUM_SAMPLES * 2> stereo{};
        // Guest planes FL, FR, FC, LFE, BL, BR match SDL's 6-channel order
        // FL, FR, FC, LFE, SL/BL, SR/BR, so 5.1 interleaves them unchanged.
        std::array<float, XAUDIO_NUM_SAMPLES * XAUDIO_NUM_CHANNELS> surround;
        // The encoder's filters carry state from frame to frame; this runs
        // on the one thread that submits frames.
        static MatrixSurround matrix;
        static bool matrixOn = false;
        static uint32_t matrixRear = 110;
        if (const bool on = g_matrix; on != matrixOn)
        {
            matrix.Reset();
            matrixOn = on;
        }
        if (const uint32_t rear = g_matrixRear; rear != matrixRear)
        {
            matrix.SetRearAngle(float(rear));
            matrixRear = rear;
        }
        static SpeakerPan pan;
        static bool testOn = false;
        const bool test = std::chrono::steady_clock::now().time_since_epoch().count() < g_testSignalUntil;
        if (test && !testOn) pan.Reset(); // each test starts at front left
        testOn = test;
        std::shared_ptr<const std::vector<float>> ui;
        size_t uiPosition = 0;
        {
            std::lock_guard lock(g_uiSoundMutex);
            if (g_uiSound && g_uiSoundPosition < g_uiSound->size())
            {
                ui = g_uiSound;
                uiPosition = g_uiSoundPosition;
                g_uiSoundPosition += XAUDIO_NUM_SAMPLES;
            }
        }
        float peak = 0;
        float channel[6];
        for (uint32_t i = 0; i < XAUDIO_NUM_SAMPLES; ++i)
        {
            for (uint32_t c = 0; c < 6; ++c)
            {
                const float value = std::bit_cast<float>(ByteSwap(words[c * XAUDIO_NUM_SAMPLES + i]));
                channel[c] = std::isfinite(value) ? value : 0;
            }
            // Keep the guest boundary before speaker-test/UI overlays: those
            // are applied again by this mixer when the stretched frame arrives.
            if (i + 1 == XAUDIO_NUM_SAMPLES)
                std::copy_n(channel, XAUDIO_NUM_CHANNELS, g_lastAudioSamples.begin());
            if (testOn) pan.Next(channel);
            if (ui && uiPosition + i < ui->size())
            {
                channel[0] += (*ui)[uiPosition + i];
                channel[1] += (*ui)[uiPosition + i];
            }
            for (uint32_t c = 0; c < 6; ++c)
                surround[i * 6 + c] = std::clamp(channel[c], -1.0f, 1.0f);
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
        g_testPosition = testOn ? pan.Position() : -1.0f;
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
                const int before = SDL_GetAudioStreamQueued(g_stream);
                if (before == 0 && n > 1) g_starved.fetch_add(1, std::memory_order_relaxed);
                if (before < int(FrameBytes() * 16))
                {
                    if (!SDL_PutAudioStreamData(g_stream, data, int(FrameBytes()))) ++queueErrors;
                }
                else ++queueDrops;
                if (report) queued = std::max(0, SDL_GetAudioStreamQueued(g_stream));
            }
        }
        if (report)
            LOG_INFO("audio frames submitted: {} peak={} queued={} mute={} queue_drops={} queue_errors={} starved={} clock_resets={}",
                n, peak, queued, mute, queueDrops, queueErrors, g_starved.load(std::memory_order_relaxed),
                g_clockResets.load(std::memory_order_relaxed));
#if defined(__ANDROID__)
        if (::diagnostics::android_tester::g_recorder.recording.load(std::memory_order_relaxed))
            ::diagnostics::android_tester::g_recorder.SetAudio(g_starved.load(std::memory_order_relaxed),
                g_clockResets.load(std::memory_order_relaxed));
#endif
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
