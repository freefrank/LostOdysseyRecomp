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
// at 48 kHz. How each surround splits between Lt and Rt sets where a decoder
// hears it; SetRearAngle moves the surrounds from that split.

#include <algorithm>
#include <array>
#include <cmath>

namespace apu
{
    class MatrixSurround
    {
    public:
        // Pro Logic II coefficients; Major/Minor are the standard surround split.
        static constexpr float Center = 0.7071f, Lfe = 0.5f, Major = 0.8718f, Minor = 0.4899f, Gain = 0.5f;

        // channel: FL, FR, FC, LFE, BL, BR. Writes Lt and Rt at the stereo
        // fold-down's level, before clamping.
        void Encode(const float (&channel)[6], float& lt, float& rt)
        {
            const float frontLeft = channel[0] + Center * channel[2] + Lfe * channel[3];
            const float frontRight = channel[1] + Center * channel[2] + Lfe * channel[3];
            const float rearLeft = m_major * channel[4] + m_minor * channel[5];
            const float rearRight = m_minor * channel[4] + m_major * channel[5];
            lt = Gain * (m_frontLeft.Process(frontLeft) - m_rearLeft.Process(rearLeft));
            rt = Gain * (m_frontRight.Process(frontRight) + m_rearRight.Process(rearRight));
        }

        // A surround split as cos/sin of an angle: Pro Logic II's 0.8718/0.4899
        // (29.3 degrees) is heard about 110 degrees from the front, an even
        // split (45) at the back center and none (0) at the front speakers.
        // Maps a rear angle (degrees from the front, 30-180) to the split
        // through the steering a passive decoder reads (SpeakerPan::Decoded),
        // linear between front right (30), Pro Logic II (110) and the back (180).
        static float SplitForRearAngle(float degrees)
        {
            // A right surround alone steers to 90 + 2 * split.
            const float standard = 90 + 2 * std::atan2(Minor, Major) * 57.29578f;
            const float angle = std::clamp(degrees, 30.0f, 180.0f);
            const float steering = angle <= 110 ? 90 + (angle - 30) / 80 * (standard - 90)
                                                : standard + (angle - 110) / 70 * (180 - standard);
            return (steering - 90) / 2;
        }

        // Where the surrounds are heard, degrees from the front; 110 is Pro Logic II.
        void SetRearAngle(float degrees)
        {
            const float split = SplitForRearAngle(degrees) * 0.017453292f;
            m_major = std::cos(split);
            m_minor = std::sin(split);
        }

        // Clears the filters; keeps the rear angle.
        void Reset()
        {
            const float major = m_major, minor = m_minor;
            *this = {};
            m_major = major;
            m_minor = minor;
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

        // The fronts go through the chain 90 degrees behind the surrounds'.
        Chain<true> m_frontLeft, m_frontRight;
        Chain<false> m_rearLeft, m_rearRight;
        float m_major = Major, m_minor = Minor;
    };
}
