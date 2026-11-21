#pragma once

// Stereo Schroeder-Moorer reverb (Freeverb topology: 8 damped combs + 4 allpasses per side).
// Own implementation; the tunings are the classic public-domain values scaled to the sample rate.
// Audio thread only, except prepare () which allocates (message thread / prepareToPlay).

#include "Dsp.h"

#include <array>
#include <vector>

namespace voxlift::dsp
{
class StereoReverb final
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;

    /** size 0..1 sets the decay; damping 0..1 darkens the tail (high = darker, kinder to sibilance). */
    void setParams (float size, float damping) noexcept;

    void process (float inL, float inR, float& outL, float& outR) noexcept;

private:
    struct Comb
    {
        std::vector<float> buffer;
        size_t index = 0;
        float store = 0.0f;

        float process (float input, float feedback, float damp) noexcept
        {
            const auto out = buffer[index];
            store = flushDenormal (out * (1.0f - damp) + store * damp);
            buffer[index] = flushDenormal (input + store * feedback);
            if (++index >= buffer.size())
                index = 0;
            return out;
        }
    };

    struct Allpass
    {
        std::vector<float> buffer;
        size_t index = 0;

        float process (float input) noexcept
        {
            const auto delayed = buffer[index];
            buffer[index] = flushDenormal (input + delayed * 0.5f);
            if (++index >= buffer.size())
                index = 0;
            return delayed - input;
        }
    };

    static constexpr int numCombs = 8;
    static constexpr int numAllpasses = 4;

    std::array<Comb, numCombs> combsL, combsR;
    std::array<Allpass, numAllpasses> allpassL, allpassR;
    float feedback = 0.84f;
    float damp = 0.5f;
};
} // namespace voxlift::dsp
