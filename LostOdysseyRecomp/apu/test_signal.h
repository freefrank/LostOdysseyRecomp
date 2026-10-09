#pragma once

// Speaker test for the Matrix phase row: pink noise circles the listener
// clockwise (front left, center, front right, right surround, left surround),
// holding 0.5 s on each speaker and gliding 1.2 s to the next with a
// constant-power pan. It replaces the game's 5.1 mix, so it goes through the
// same encoding. While it glides between a front and a surround speaker both
// carry it at once, which is where the matrix phase is heard: at the right
// phase the sound moves smoothly from front to rear.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace apu
{
    class SpeakerPan
    {
    public:
        static constexpr uint32_t Hold = 24000, Glide = 57600, Step = Hold + Glide, FadeIn = 480;
        // Guest planes in order around the listener and their angles, degrees
        // clockwise from the front (ITU-R BS.775: fronts at 30, surrounds at 110).
        static constexpr int Order[5] = {0, 2, 1, 5, 4};
        static constexpr float Angles[5] = {330, 0, 30, 110, 250};

        // Overwrites one sample frame in guest order FL, FR, FC, LFE, BL, BR.
        void Next(float (&channel)[6])
        {
            const uint32_t leg = m_sample / Step, step = m_sample % Step;
            m_sample = (m_sample + 1) % (Step * 5);
            std::fill(std::begin(channel), std::end(channel), 0.0f);
            float noise = Pink();
            if (m_started < FadeIn)
                noise *= float(m_started++) / FadeIn;
            if (step < Hold)
                channel[Order[leg]] = noise;
            else
            {
                const float turn = float(step - Hold) / Glide * 1.5707963f;
                channel[Order[leg]] = noise * std::cos(turn);
                channel[Order[(leg + 1) % 5]] = noise * std::sin(turn);
            }
        }

        // Where the sound is, degrees clockwise from the front.
        float Angle() const
        {
            const uint32_t leg = m_sample / Step, step = m_sample % Step;
            if (step < Hold)
                return Angles[leg];
            const float span = std::fmod(Angles[(leg + 1) % 5] - Angles[leg] + 360.0f, 360.0f);
            return std::fmod(Angles[leg] + span * float(step - Hold) / Glide, 360.0f);
        }

        void Reset() { *this = {}; }

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
        uint32_t m_sample = 0, m_started = 0, m_seed = 22222;
        float m_b0 = 0, m_b1 = 0, m_b2 = 0;
    };
}
