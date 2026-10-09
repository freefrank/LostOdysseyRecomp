#pragma once

// Speaker test for the Matrix phase row: pink noise circles the listener
// clockwise (front left, center, front right, right surround, left surround),
// holding 0.5 s on each speaker and gliding 1.2 s to the next with a
// constant-power pan. It replaces the game's 5.1 mix, so it goes through the
// same encoding. While it glides between a front and a surround speaker both
// carry it at once, which is where the matrix phase is heard: at the right
// phase the sound moves smoothly from front to rear.

#include "matrix_surround.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
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

        // Gains in guest order (FL, FR, FC, LFE, BL, BR) at `position` along the
        // circle: 0 front left, 1 center, 2 front right, 3 right surround,
        // 4 left surround, fractions while gliding to the next.
        static void Gains(float position, float (&gain)[6])
        {
            std::fill(std::begin(gain), std::end(gain), 0.0f);
            const float wrapped = std::fmod(std::fmod(position, 5.0f) + 5.0f, 5.0f);
            const int leg = std::min(int(wrapped), 4);
            const float turn = (wrapped - float(leg)) * 1.5707963f;
            gain[Order[leg]] = std::cos(turn);
            gain[Order[(leg + 1) % 5]] = std::sin(turn);
        }

        struct Direction
        {
            float degrees, focus;
        };
        // Where a passive matrix decoder would put the sound at `position` with
        // the encoder at `phase` degrees, from ideal Lt/Rt (the all-pass pair
        // taken as an exact 90 degree shift): degrees around the listener,
        // calibrated so a speaker alone lands on its own angle, and focus, 1 on
        // a speaker and smaller where Lt and Rt spread the sound out.
        static Direction Decoded(float position, float phase)
        {
            float gain[6];
            Gains(position, gain);
            const Direction steer = Steer(gain, phase);
            // Each speaker alone, by steering angle, maps to its drawn angle.
            struct Anchor { float steering, drawn; };
            std::array<Anchor, 5> anchors;
            for (int i = 0; i < 5; ++i)
            {
                float alone[6] = {};
                alone[Order[i]] = 1;
                anchors[i] = {Steer(alone, phase).degrees, Angles[i]};
            }
            std::sort(anchors.begin(), anchors.end(), [](const Anchor &a, const Anchor &b) { return a.steering < b.steering; });
            const float psi = steer.degrees < anchors[0].steering ? steer.degrees + 360 : steer.degrees;
            for (int i = 0; i < 5; ++i)
            {
                const Anchor &from = anchors[i], &next = anchors[(i + 1) % 5];
                const float to = i < 4 ? next.steering : next.steering + 360;
                if (psi <= to || i == 4)
                {
                    const float span = std::fmod(next.drawn - from.drawn + 360.0f, 360.0f);
                    const float t = std::clamp((psi - from.steering) / std::max(to - from.steering, 1e-6f), 0.0f, 1.0f);
                    return {std::fmod(from.drawn + span * t, 360.0f), std::min(steer.focus, 1.0f)};
                }
            }
            return steer;
        }

        // Overwrites one sample frame in guest order FL, FR, FC, LFE, BL, BR.
        void Next(float (&channel)[6])
        {
            float gain[6];
            Gains(Position(), gain);
            m_sample = (m_sample + 1) % (Step * 5);
            float noise = Pink();
            if (m_started < FadeIn)
                noise *= float(m_started++) / FadeIn;
            for (int c = 0; c < 6; ++c)
                channel[c] = noise * gain[c];
        }

        // Where the sound is along the circle (see Gains).
        float Position() const
        {
            const uint32_t leg = m_sample / Step, step = m_sample % Step;
            return float(leg) + (step < Hold ? 0.0f : float(step - Hold) / Glide);
        }

        void Reset() { *this = {}; }

    private:
        // Steering from the Lt/Rt powers: left/right and center/surround
        // dominance as an angle (0 center, 90 right, 180 surround) and length.
        static Direction Steer(const float (&gain)[6], float phase)
        {
            using M = MatrixSurround;
            const std::complex<float> rotate = std::polar(1.0f, phase * 0.017453292f);
            const std::complex<float> lt = gain[0] + M::Center * gain[2] - rotate * (M::Major * gain[4] + M::Minor * gain[5]);
            const std::complex<float> rt = gain[1] + M::Center * gain[2] + rotate * (M::Minor * gain[4] + M::Major * gain[5]);
            const float l2 = std::norm(lt), r2 = std::norm(rt), s2 = std::norm(lt + rt), d2 = std::norm(lt - rt);
            if (l2 + r2 <= 1e-12f)
                return {0, 0};
            const float lr = (r2 - l2) / (l2 + r2), cs = (s2 - d2) / (s2 + d2);
            return {std::fmod(std::atan2(lr, cs) * 57.29578f + 360.0f, 360.0f), std::hypot(lr, cs)};
        }

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
