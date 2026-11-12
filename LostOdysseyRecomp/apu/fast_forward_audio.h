#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <memory>
#include "audio.h"
#include "../../thirdparty/signalsmith-stretch/signalsmith-stretch.h"

namespace apu::detail
{
    // Guest input and output are six big-endian float planes. Keeping this
    // processor independent of SDL lets the fixture exercise the runtime path.
    class FastForwardAudio
    {
    public:
        using Frame = std::array<uint32_t, XAUDIO_NUM_SAMPLES * XAUDIO_NUM_CHANNELS>;
        // A short, portable profile also used on Android: no platform FFT
        // dependency or extra split-computation latency. At 2x the latency is
        // 1024 input / 2 + 1024 output samples, or 32 ms (less at higher rates).
        static constexpr int BlockSamples = 2048;
        static constexpr int IntervalSamples = 512;
        static constexpr uint32_t FadeSamples = 128;

        bool Active() const { return rate_ > 1; }

        // The normal mixer can supply its final already-decoded samples once
        // at entry, so the short startup fade begins at the audible boundary.
        void StartFrom(const std::array<float, XAUDIO_NUM_CHANNELS>& samples)
        {
            last_ = samples;
            haveStart_ = true;
        }

        void Reset()
        {
            if (stretch_ && keepPitch_) stretch_->reset();
            rate_ = 1;
            keepPitch_ = false;
            haveStart_ = false;
            summed_ = written_ = remainder_ = fade_ = 0;
            sum_.fill(0);
        }

        template<class Emit>
        void Process(const uint32_t* words, uint32_t rate, bool keepPitch, Emit&& emit)
        {
            if (rate <= 1)
            {
                if (Active() && keepPitch_)
                {
                    // Discard the delayed tail on release, then join the last
                    // stretched sample to current guest audio over 2.7 ms.
                    for (uint32_t c = 0; c < XAUDIO_NUM_CHANNELS; ++c)
                        for (uint32_t i = 0; i < XAUDIO_NUM_SAMPLES; ++i)
                        {
                            const float value = Decode(words[c * XAUDIO_NUM_SAMPLES + i]);
                            const float weight = std::min(1.0f, float(i) / float(FadeSamples));
                            output_[c * XAUDIO_NUM_SAMPLES + i] = Encode(last_[c] + (value - last_[c]) * weight);
                        }
                    emit(output_.data());
                }
                else emit(words);
                Reset();
                return;
            }

            if (!keepPitch)
            {
                // This is the original averaging decimator, including its
                // float accumulation order, sanitisation and partial-frame
                // discard on a rate change. Speed up stays bit-for-bit equal.
                if (rate != rate_ || keepPitch_)
                {
                    summed_ = written_ = 0;
                    sum_.fill(0);
                    if (stretch_ && keepPitch_) stretch_->reset();
                }
                for (uint32_t i = 0; i < XAUDIO_NUM_SAMPLES; ++i)
                {
                    for (uint32_t c = 0; c < XAUDIO_NUM_CHANNELS; ++c)
                        sum_[c] += Decode(words[c * XAUDIO_NUM_SAMPLES + i]);
                    if (++summed_ < rate) continue;
                    for (uint32_t c = 0; c < XAUDIO_NUM_CHANNELS; ++c)
                        output_[c * XAUDIO_NUM_SAMPLES + written_] = Encode(sum_[c] / float(rate));
                    sum_.fill(0);
                    summed_ = 0;
                    if (++written_ == XAUDIO_NUM_SAMPLES)
                    {
                        written_ = 0;
                        EmitOutput(emit);
                    }
                }
            }
            else
            {
                if (!Active() || !keepPitch_)
                {
                    if (!stretch_)
                    {
                        // Explicit seed avoids random-device work on the
                        // audio thread and makes the fixture deterministic.
                        stretch_ = std::make_unique<Stretch>(0);
                        stretch_->configure(XAUDIO_NUM_CHANNELS, BlockSamples, IntervalSamples);
                    }
                    else stretch_->reset();
                    if (!Active() && !haveStart_)
                        for (uint32_t c = 0; c < XAUDIO_NUM_CHANNELS; ++c)
                            last_[c] = Decode(words[c * XAUDIO_NUM_SAMPLES]);
                    start_ = last_;
                    haveStart_ = false;
                    written_ = remainder_ = 0;
                    fade_ = FadeSamples;
                }
                // Keep the same six-channel instance on tempo changes. Its
                // phase evolution follows the new ratio without a reset/gap;
                // upstream locks other channels to the strongest spectral bin.
                if (rate != rate_) remainder_ = 0;
                const uint32_t count = (XAUDIO_NUM_SAMPLES + remainder_) / rate;
                remainder_ = (XAUDIO_NUM_SAMPLES + remainder_) % rate;
                for (uint32_t c = 0; c < XAUDIO_NUM_CHANNELS; ++c)
                    for (uint32_t i = 0; i < XAUDIO_NUM_SAMPLES; ++i)
                        input_[c][i] = Decode(words[c * XAUDIO_NUM_SAMPLES + i]);
                stretch_->process(input_, XAUDIO_NUM_SAMPLES, stretched_, count);
                for (uint32_t i = 0; i < count; ++i)
                {
                    const float weight = 1.0f - float(fade_) / float(FadeSamples);
                    for (uint32_t c = 0; c < XAUDIO_NUM_CHANNELS; ++c)
                    {
                        float value = stretched_[c][i];
                        value = std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0;
                        if (fade_) value = start_[c] + (value - start_[c]) * weight;
                        output_[c * XAUDIO_NUM_SAMPLES + written_] = Encode(value);
                    }
                    if (fade_) --fade_;
                    if (++written_ == XAUDIO_NUM_SAMPLES)
                    {
                        written_ = 0;
                        EmitOutput(emit);
                    }
                }
            }
            rate_ = rate;
            keepPitch_ = keepPitch;
        }

    private:
        using Stretch = signalsmith::stretch::SignalsmithStretch<float>;
        using Planes = std::array<std::array<float, XAUDIO_NUM_SAMPLES>, XAUDIO_NUM_CHANNELS>;

        static uint32_t Swap(uint32_t word)
        {
            return (word >> 24) | ((word >> 8) & 0xFF00u) | ((word << 8) & 0xFF0000u) | (word << 24);
        }
        static float Decode(uint32_t word)
        {
            const float value = std::bit_cast<float>(Swap(word));
            return std::isfinite(value) ? value : 0;
        }
        static uint32_t Encode(float value) { return Swap(std::bit_cast<uint32_t>(value)); }

        template<class Emit>
        void EmitOutput(Emit& emit)
        {
            for (uint32_t c = 0; c < XAUDIO_NUM_CHANNELS; ++c)
                last_[c] = Decode(output_[(c + 1) * XAUDIO_NUM_SAMPLES - 1]);
            emit(output_.data());
        }

        // No allocation/configuration/processing until Keep pitch is active.
        std::unique_ptr<Stretch> stretch_;
        Planes input_{}, stretched_{};
        Frame output_{};
        std::array<float, XAUDIO_NUM_CHANNELS> sum_{}, last_{}, start_{};
        uint32_t rate_ = 1, summed_ = 0, written_ = 0, remainder_ = 0, fade_ = 0;
        bool keepPitch_ = false;
        bool haveStart_ = false;
    };
}
