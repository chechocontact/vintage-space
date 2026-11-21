#include "Reverb.h"

namespace voxlift::dsp
{
namespace
{
constexpr int combTunings[8] { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
constexpr int allpassTunings[4] { 556, 441, 341, 225 };
constexpr int stereoSpread = 23;
constexpr float inputGain = 0.015f;
constexpr float outputScale = 3.0f;

size_t scaled (int samplesAt44k, double sampleRate)
{
    return static_cast<size_t> (std::max (1.0, samplesAt44k * sampleRate / 44100.0));
}
} // namespace

void StereoReverb::prepare (double sampleRate)
{
    for (int i = 0; i < numCombs; ++i)
    {
        combsL[(size_t) i].buffer.assign (scaled (combTunings[i], sampleRate), 0.0f);
        combsR[(size_t) i].buffer.assign (scaled (combTunings[i] + stereoSpread, sampleRate), 0.0f);
    }

    for (int i = 0; i < numAllpasses; ++i)
    {
        allpassL[(size_t) i].buffer.assign (scaled (allpassTunings[i], sampleRate), 0.0f);
        allpassR[(size_t) i].buffer.assign (scaled (allpassTunings[i] + stereoSpread, sampleRate), 0.0f);
    }

    reset();
}

void StereoReverb::reset() noexcept
{
    for (auto* combs : { &combsL, &combsR })
        for (auto& c : *combs)
        {
            std::fill (c.buffer.begin(), c.buffer.end(), 0.0f);
            c.index = 0;
            c.store = 0.0f;
        }

    for (auto* allpasses : { &allpassL, &allpassR })
        for (auto& a : *allpasses)
        {
            std::fill (a.buffer.begin(), a.buffer.end(), 0.0f);
            a.index = 0;
        }
}

void StereoReverb::setParams (float size, float damping) noexcept
{
    feedback = 0.7f + 0.28f * clamp01 (size);
    damp = 0.4f * clamp01 (damping) + 0.1f;
}

void StereoReverb::process (float inL, float inR, float& outL, float& outR) noexcept
{
    const auto input = (inL + inR) * inputGain;

    auto sumL = 0.0f;
    auto sumR = 0.0f;
    for (size_t i = 0; i < numCombs; ++i)
    {
        sumL += combsL[i].process (input, feedback, damp);
        sumR += combsR[i].process (input, feedback, damp);
    }

    for (size_t i = 0; i < numAllpasses; ++i)
    {
        sumL = allpassL[i].process (sumL);
        sumR = allpassR[i].process (sumR);
    }

    outL = sumL * outputScale;
    outR = sumR * outputScale;
}
} // namespace voxlift::dsp
