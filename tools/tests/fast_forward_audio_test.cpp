#include <apu/fast_forward_audio.h>

#include <chrono>
#include <cstdio>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <time.h>
#endif

namespace
{
    using Processor = apu::detail::FastForwardAudio;
    using Frame = Processor::Frame;
    constexpr uint32_t Samples = XAUDIO_NUM_SAMPLES, Channels = XAUDIO_NUM_CHANNELS;
    constexpr double SampleRate = XAUDIO_SAMPLES_HZ;

    void Check(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    double ThreadCpuSeconds()
    {
#if defined(_WIN32)
        FILETIME created{}, exited{}, kernel{}, user{};
        Check(GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user) != 0, "GetThreadTimes failed");
        const uint64_t ticks = (uint64_t(kernel.dwHighDateTime) << 32) + kernel.dwLowDateTime
            + (uint64_t(user.dwHighDateTime) << 32) + user.dwLowDateTime;
        return double(ticks) * 1e-7;
#else
        timespec time{};
        Check(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) == 0, "thread CPU clock failed");
        return double(time.tv_sec) + double(time.tv_nsec) * 1e-9;
#endif
    }

    uint32_t Swap(uint32_t word)
    {
        return (word >> 24) | ((word >> 8) & 0xFF00u) | ((word << 8) & 0xFF0000u) | (word << 24);
    }
    uint32_t Encode(float value) { return Swap(std::bit_cast<uint32_t>(value)); }
    float Decode(uint32_t word) { return std::bit_cast<float>(Swap(word)); }

    Frame Tone(uint64_t offset)
    {
        Frame frame;
        for (uint32_t i = 0; i < Samples; ++i)
        {
            const double time = double(offset + i) / SampleRate;
            const float front = float(0.35 * std::sin(2 * std::numbers::pi * 440 * time));
            const float center = float(0.3 * std::sin(2 * std::numbers::pi * 880 * time));
            const float values[] = {front, front, center, -center, -front, front};
            for (uint32_t c = 0; c < Channels; ++c) frame[c * Samples + i] = Encode(values[c]);
        }
        return frame;
    }

    struct Output
    {
        std::array<std::vector<float>, Channels> planes;
        std::vector<Frame> frames;
        void operator()(const uint32_t* words)
        {
            Frame frame;
            std::copy_n(words, frame.size(), frame.begin());
            frames.push_back(frame);
            for (uint32_t c = 0; c < Channels; ++c)
                for (uint32_t i = 0; i < Samples; ++i)
                    planes[c].push_back(Decode(words[c * Samples + i]));
        }
    };

    // An independent copy of the old runtime algorithm, intentionally kept
    // separate from the production helper so exact-byte regressions are caught.
    class Legacy
    {
    public:
        void Process(const Frame& frame, uint32_t rate, Output& output)
        {
            if (rate <= 1)
            {
                rate_ = 0;
                output(frame.data());
                return;
            }
            if (rate != rate_)
            {
                summed_ = written_ = 0;
                sum_.fill(0);
            }
            for (uint32_t i = 0; i < Samples; ++i)
            {
                for (uint32_t c = 0; c < Channels; ++c)
                {
                    const float value = Decode(frame[c * Samples + i]);
                    sum_[c] += std::isfinite(value) ? value : 0;
                }
                if (++summed_ < rate) continue;
                for (uint32_t c = 0; c < Channels; ++c)
                    output_[c * Samples + written_] = Encode(sum_[c] / float(rate));
                sum_.fill(0);
                summed_ = 0;
                if (++written_ == Samples)
                {
                    written_ = 0;
                    output(output_.data());
                }
            }
            rate_ = rate;
        }
    private:
        Frame output_{};
        std::array<float, Channels> sum_{};
        uint32_t summed_ = 0, written_ = 0, rate_ = 0;
    };

    double Pitch(const std::vector<float>& signal)
    {
        // Ignore start latency and the final analysis window. Interpolated
        // rising zero crossings measure frequency without choosing FFT bins.
        const size_t first = size_t(SampleRate / 10);
        const size_t end = signal.size() - size_t(SampleRate / 20);
        std::vector<double> crossings;
        for (size_t i = first + 1; i < end; ++i)
            if (signal[i - 1] <= 0 && signal[i] > 0)
                crossings.push_back(double(i - 1) - signal[i - 1] / double(signal[i] - signal[i - 1]));
        Check(crossings.size() > 30, "not enough audible tone cycles");
        return (crossings.size() - 1) * SampleRate / (crossings.back() - crossings.front());
    }

    float ValidateSamples(const Output& output)
    {
        float peak = 0;
        for (const auto& channel : output.planes)
            for (float value : channel)
            {
                Check(std::isfinite(value), "non-finite output sample");
                peak = std::max(peak, std::abs(value));
            }
        Check(peak < 0.95f, "unexpected clipping/level burst");
        return peak;
    }

    void CheckPitchAndLength(uint32_t rate)
    {
        Processor processor;
        Output output;
        // 6.144 seconds of guest audio, with whole output frames at every rate.
        constexpr uint32_t periods = 1152;
        uint64_t offset = 0;
        double coldMs = 0;
        for (uint32_t frame = 0; frame < periods; ++frame)
        {
            const auto tone = Tone(offset);
            const auto before = std::chrono::steady_clock::now();
            processor.Process(tone.data(), rate, true, output);
            if (!frame) coldMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - before).count();
            offset += Samples;
        }
        const size_t expected = size_t(periods) * Samples / rate;
        Check(output.planes[0].size() == expected, "wrong stretched output duration");
        const double fl = Pitch(output.planes[0]), fc = Pitch(output.planes[2]);
        Check(std::abs(fl - 440) < 2, "FL pitch was not preserved");
        Check(std::abs(fc - 880) < 2, "FC pitch was not preserved");
        const float peak = ValidateSamples(output);
        float phaseError = 0;
        float initialPhaseError = 0;
        double energy = 0;
        size_t firstAudible = expected;
        for (size_t i = 0; i < expected; ++i)
        {
            const auto& p = output.planes;
            const float error = std::max({std::abs(p[0][i] - p[1][i]),
                std::abs(p[0][i] + p[4][i]), std::abs(p[0][i] - p[5][i]), std::abs(p[2][i] + p[3][i])});
            if (i > size_t(SampleRate / 10)) phaseError = std::max(phaseError, error);
            else initialPhaseError = std::max(initialPhaseError, error);
            energy += double(p[0][i]) * p[0][i];
            if (firstAudible == expected && std::abs(p[0][i]) > 0.02f) firstAudible = i;
        }
        Check(phaseError < 0.0005f, "six-channel phase lock changed equal/inverted channels");
        // The first windows are zero-padded at reset; measure their small
        // inter-channel error separately instead of assuming settled analysis.
        Check(initialPhaseError < 0.00075f, "six-channel startup phase error is too large");
        Check(std::sqrt(energy / expected) > 0.1, "stretched tone became inaudible");
        // Include the frame's delivery time: the first output frame cannot be
        // emitted until its final guest frame arrives.
        const double audibleMs = double((firstAudible / Samples + 1) * Samples) * 1000 / SampleRate;
        Check(audibleMs < 50, "fast-forward startup exceeds 50 ms");
        std::printf("pitch rate=%u FL=%.3f FC=%.3f samples=%zu expected=%zu peak=%.4f phase_error=%.7f startup_phase_error=%.7f first_audible_ms=%.2f cold_init_process_ms=%.3f\n",
            rate, fl, fc, output.planes[0].size(), expected, peak, phaseError, initialPhaseError, audibleMs, coldMs);
    }

    void CheckLegacy()
    {
        Processor processor;
        Legacy legacy;
        Output actual, expected;
        uint64_t offset = 0;
        // Non-divisor rates and partial frames/rate changes matter here.
        for (uint32_t rate : {2u, 3u, 4u, 6u, 8u, 3u, 1u, 6u, 2u, 1u})
            for (uint32_t i = 0; i < 47; ++i)
            {
                auto tone = Tone(offset);
                if (i == 5)
                {
                    tone[17] = Encode(std::numeric_limits<float>::quiet_NaN());
                    tone[Samples + 33] = Encode(std::numeric_limits<float>::infinity());
                }
                processor.Process(tone.data(), rate, false, actual);
                legacy.Process(tone, rate, expected);
                offset += Samples;
            }
        Check(actual.frames == expected.frames, "Speed up differs from the original decimator");
        std::printf("legacy exact_frames=%zu rates=2,3,4,6,8 partial_changes=pass\n", actual.frames.size());
    }

    void CheckTransitions()
    {
        Processor processor;
        Output output;
        uint64_t offset = 0;
        size_t periods = 0;
        // Adaptive dispatch can change the effective rate every output period.
        for (uint32_t iteration = 0; iteration < 18; ++iteration)
            for (uint32_t rate : {2u, 3u, 4u, 6u, 8u, 6u, 4u, 3u})
            {
                for (uint32_t i = 0; i < rate; ++i)
                {
                    const auto tone = Tone(offset);
                    processor.Process(tone.data(), rate, true, output);
                    offset += Samples;
                }
                ++periods;
                Check(output.frames.size() == periods, "adaptive rate changes drifted output duration");
            }
        ValidateSamples(output);
        float maxStep = 0;
        for (const auto& channel : output.planes)
            for (size_t i = 1; i < channel.size(); ++i)
                maxStep = std::max(maxStep, std::abs(channel[i] - channel[i - 1]));
        Check(maxStep < 0.2f, "adaptive tempo change caused a discontinuity");
        const float last = output.planes[0].back();
        auto tone = Tone(offset);
        // Release while a pitch output frame is only partly filled.
        processor.Process(tone.data(), 8, true, output);
        offset += Samples;
        tone = Tone(offset);
        processor.Process(tone.data(), 1, true, output);
        Check(!processor.Active(), "stopping did not reset processor state");
        Check(output.planes[0][periods * Samples] == last, "stop boundary is discontinuous");
        Check(output.frames.size() == periods + 1, "stop emitted a delayed/partial tail");
        // Normal playback returns the original words unchanged after release.
        processor.Process(tone.data(), 1, true, output);
        Check(output.frames.back() == tone, "normal playback was processed");

        Output restart;
        Frame silent{};
        for (uint32_t i = 0; i < 24; ++i) processor.Process(silent.data(), 2, true, restart);
        for (const auto& channel : restart.planes)
            for (float value : channel) Check(value == 0, "restart leaked a stale stretched tail");

        Processor entry;
        Output entryOutput;
        const std::array<float, Channels> boundary = {0.2f, -0.1f, 0.3f, -0.3f, 0.1f, 0.2f};
        entry.StartFrom(boundary);
        for (uint32_t i = 0; i < 2; ++i)
        {
            tone = Tone(i * Samples);
            entry.Process(tone.data(), 2, true, entryOutput);
        }
        for (uint32_t c = 0; c < Channels; ++c)
            Check(entryOutput.planes[c][0] == boundary[c], "start boundary is discontinuous");

        for (bool keep : {false, true, false, true})
            for (uint32_t i = 0; i < 32; ++i)
            {
                tone = Tone(offset);
                if (i == 7) tone[13] = Encode(std::numeric_limits<float>::quiet_NaN());
                processor.Process(tone.data(), 4, keep, output);
                offset += Samples;
            }
        ValidateSamples(output);
        std::printf("transitions adaptive_periods=%zu max_step=%.4f start_boundary=continuous stop_partial=pass normal_bypass=exact restart_tail=none mode_changes=pass\n", periods, maxStep);
    }

    void Benchmark()
    {
        // Prepare input outside timing; unlike the tone assertions this uses
        // different frequencies on every channel to exercise the shared FFT.
        std::vector<Frame> input(256);
        for (size_t f = 0; f < input.size(); ++f)
            for (uint32_t c = 0; c < Channels; ++c)
                for (uint32_t i = 0; i < Samples; ++i)
                    input[f][c * Samples + i] = Encode(float(0.25 * std::sin(
                        2 * std::numbers::pi * (220 + 137 * c) * double(f * Samples + i) / SampleRate)));
        for (uint32_t rate : {2u, 3u, 4u, 6u, 8u})
        {
            Processor processor;
            uint64_t outputFrames = 0;
            const auto emit = [&](const uint32_t*) { ++outputFrames; };
            // Include a representative steady state, excluding one-time FFT
            // table allocation/configuration from the throughput measurement.
            for (uint32_t i = 0; i < rate * 32; ++i) processor.Process(input[i % input.size()].data(), rate, true, emit);
            outputFrames = 0;
            constexpr uint32_t periods = 4096;
            std::vector<double> costs;
            const auto start = std::chrono::steady_clock::now();
            const double cpuStart = ThreadCpuSeconds();
            for (uint32_t period = 0; period < periods; ++period)
            {
                const auto before = std::chrono::steady_clock::now();
                for (uint32_t i = 0; i < rate; ++i)
                    processor.Process(input[(period * rate + i) % input.size()].data(), rate, true, emit);
                costs.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - before).count());
            }
            const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            const double cpu = ThreadCpuSeconds() - cpuStart;
            const double seconds = double(outputFrames * Samples) / SampleRate;
            std::sort(costs.begin(), costs.end());
            std::printf("benchmark host_portable_profile=%d/%d rate=%u audio_s=%.3f thread_cpu_ms=%.3f one_core_percent=%.3f elapsed_ms=%.3f period_p95_us=%.2f period_max_us=%.2f\n",
                Processor::BlockSamples, Processor::IntervalSamples, rate, seconds, cpu * 1000, cpu / seconds * 100, elapsed * 1000,
                costs[costs.size() * 95 / 100], costs.back());
        }
    }
}

int main()
{
    try
    {
        for (uint32_t rate : {2u, 4u, 8u}) CheckPitchAndLength(rate);
        CheckLegacy();
        CheckTransitions();
        Benchmark();
        std::puts("fast-forward audio fixture passed (host CPU measurements, not Android hardware)");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "fast-forward audio fixture failed: %s\n", error.what());
        return 1;
    }
}
