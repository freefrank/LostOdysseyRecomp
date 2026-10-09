#pragma once

// Speaker test noise for the Matrix phase row: bursts of pink noise walk the
// five main channels clockwise like a receiver's test tone (front left,
// center, front right, right surround, left surround), 1 s each with a 0.3 s
// gap. It replaces the game's 5.1 mix, so it goes through the same encoding.

#include <algorithm>
#include <cstdint>

namespace apu
{
    class ChannelWalk
    {
    public:
        static constexpr uint32_t Burst = 48000, Gap = 14400, Fade = 480; // 1 s, 0.3 s, 10 ms
        static constexpr int Order[5] = {0, 2, 1, 5, 4}; // guest planes FL, FC, FR, BR, BL

        // Overwrites one sample frame in guest order FL, FR, FC, LFE, BL, BR.
        void Next(float (&channel)[6])
        {
            const uint32_t step = m_sample % (Burst + Gap);
            const int speaker = Order[m_sample / (Burst + Gap)];
            m_sample = (m_sample + 1) % ((Burst + Gap) * 5);
            std::fill(std::begin(channel), std::end(channel), 0.0f);
            const float noise = Pink();
            if (step < Burst)
                channel[speaker] = noise * std::min({1.0f, float(step) / Fade, float(Burst - step) / Fade});
        }

        void Reset() { *this = {}; }

        // Guest channel of the current step (its burst and the gap after it).
        int Speaker() const { return Order[m_sample / (Burst + Gap)]; }

    private:
        // Paul Kellet's economy pink filter over a linear congruential source,
        // scaled to about -20 dBFS RMS.
        float Pink()
        {
            m_seed = m_seed * 1664525u + 1013904223u;
            const float white = float(int32_t(m_seed)) * (1.0f / 2147483648.0f);
            m_b0 = 0.99765f * m_b0 + white * 0.0990460f;
            m_b1 = 0.96300f * m_b1 + white * 0.2965164f;
            m_b2 = 0.57000f * m_b2 + white * 1.0526913f;
            return (m_b0 + m_b1 + m_b2 + white * 0.1848f) * Scale;
        }

        static constexpr float Scale = 0.057f;
        uint32_t m_sample = 0, m_seed = 22222;
        float m_b0 = 0, m_b1 = 0, m_b2 = 0;
    };
}
