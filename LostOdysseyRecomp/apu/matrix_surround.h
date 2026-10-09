#pragma once

// Matrix surround: folds 5.1 into a stereo pair (Lt/Rt) that an AV receiver's
// Pro Logic II, Dolby Surround or Neural:X decoding steers back to surround
// speakers; elsewhere it plays as ordinary stereo. The surrounds enter 90
// degrees out of phase with the fronts, the left surround mostly in Lt and the
// right surround mostly in Rt, with opposite signs (Pro Logic II encoding):
//   Lt = L + 0.707 C - j (0.8718 Ls + 0.4899 Rs)
//   Rt = R + 0.707 C + j (0.4899 Ls + 0.8718 Rs)
// The 90 degree shift is the phase difference between two all-pass chains
// (Olli Niemitalo's coefficients): within 0.7 degrees from 30 Hz to 23.9 kHz
// at 48 kHz. Mixing the surrounds' outputs of both chains as cos/sin gives any
// other shift for tuning by ear; 0 degrees is the plain anti-phase matrix.

#include <array>
#include <cmath>

namespace apu
{
    class MatrixSurround
    {
    public:
        // channel: FL, FR, FC, LFE, BL, BR. Writes Lt and Rt at the stereo
        // fold-down's level, before clamping.
        void Encode(const float (&channel)[6], float& lt, float& rt)
        {
            constexpr float center = 0.7071f, lfe = 0.5f, major = 0.8718f, minor = 0.4899f, gain = 0.5f;
            const float frontLeft = channel[0] + center * channel[2] + lfe * channel[3];
            const float frontRight = channel[1] + center * channel[2] + lfe * channel[3];
            const float rearLeft = major * channel[4] + minor * channel[5];
            const float rearRight = minor * channel[4] + major * channel[5];
            const float shiftedLeft = m_cos * m_rearLeft.Process(rearLeft) + m_sin * m_rearLeftQuadrature.Process(rearLeft);
            const float shiftedRight = m_cos * m_rearRight.Process(rearRight) + m_sin * m_rearRightQuadrature.Process(rearRight);
            lt = gain * (m_frontLeft.Process(frontLeft) - shiftedLeft);
            rt = gain * (m_frontRight.Process(frontRight) + shiftedRight);
        }

        // Surround phase relative to the fronts, in degrees; 90 by default.
        void SetPhase(float degrees)
        {
            const float radians = degrees * 3.14159265f / 180.0f;
            m_cos = std::cos(radians);
            m_sin = std::sin(radians);
        }

        // Clears the filters; keeps the phase.
        void Reset()
        {
            const float c = m_cos, s = m_sin;
            *this = {};
            m_cos = c;
            m_sin = s;
        }

    private:
        // Four second-order all-pass sections: y = a^2 (x + y[-2]) - x[-2].
        template <bool Delayed>
        struct Chain
        {
            float Process(float x)
            {
                static constexpr std::array<float, 4> squared = Delayed
                    ? std::array<float, 4>{0.6923878f * 0.6923878f, 0.9360654322959f * 0.9360654322959f,
                                           0.9882295226860f * 0.9882295226860f, 0.9987488452737f * 0.9987488452737f}
                    : std::array<float, 4>{0.4021921162426f * 0.4021921162426f, 0.8561710882420f * 0.8561710882420f,
                                           0.9722909545651f * 0.9722909545651f, 0.9952884791278f * 0.9952884791278f};
                for (size_t i = 0; i < squared.size(); ++i)
                {
                    const float y = squared[i] * (x + out[i][1]) - in[i][1];
                    in[i][1] = in[i][0];
                    in[i][0] = x;
                    out[i][1] = out[i][0];
                    out[i][0] = y;
                    x = y;
                }
                if constexpr (Delayed)
                {
                    // The first chain's output lags one more sample.
                    const float y = previous;
                    previous = x;
                    return y;
                }
                else
                    return x;
            }

            std::array<std::array<float, 2>, 4> in{}, out{};
            float previous = 0;
        };

        // Chain<false> runs 90 degrees ahead of Chain<true>, which the fronts
        // and the in-phase part of the surrounds go through.
        Chain<true> m_frontLeft, m_frontRight, m_rearLeft, m_rearRight;
        Chain<false> m_rearLeftQuadrature, m_rearRightQuadrature;
        float m_cos = 0, m_sin = 1;
    };
}
