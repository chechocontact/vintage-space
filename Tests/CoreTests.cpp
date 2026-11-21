// Console tests for the JUCE-free core. Exit code 0 = all passed.

#include "../Source/Core/Presets.h"
#include "../Source/Core/RiserEngine.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
int failures = 0;
int checks = 0;

#define CHECK(condition)                                                              \
    do                                                                                \
    {                                                                                 \
        ++checks;                                                                     \
        if (! (condition))                                                            \
        {                                                                             \
            ++failures;                                                               \
            std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);         \
        }                                                                             \
    } while (false)

using namespace voxlift;

constexpr double sampleRate = 48000.0;

/** A vowel-like test signal: harmonics of 220 Hz with a slow amplitude contour. */
std::vector<float> makeVoice (int numSamples, float level = 0.3f)
{
    std::vector<float> v ((size_t) numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        const auto t = static_cast<double> (i) / sampleRate;
        double s = 0.0;
        for (int h = 1; h <= 8; ++h)
            s += std::sin (2.0 * 3.141592653589793 * 220.0 * h * t) / h;
        const auto contour = 0.6 + 0.4 * std::sin (2.0 * 3.141592653589793 * 3.0 * t);
        v[(size_t) i] = static_cast<float> (s * contour * level * 0.5);
    }
    return v;
}

struct Stereo
{
    std::vector<float> l, r;
};

Stereo run (const std::vector<float>& input, Controls controls, int blockSize,
            float intensityFrom, float intensityTo)
{
    RiserEngine engine;
    auto initial = controls;
    initial.intensity = intensityFrom;
    engine.prepare (sampleRate, initial);

    Stereo s { input, input };
    const auto n = static_cast<int> (input.size());
    for (int start = 0; start < n; start += blockSize)
    {
        const auto len = std::min (blockSize, n - start);
        const auto progress = static_cast<float> (start) / static_cast<float> (n);
        controls.intensity = intensityFrom + (intensityTo - intensityFrom) * progress;
        engine.process (s.l.data() + start, s.r.data() + start, len, controls);
    }
    return s;
}

double rms (const std::vector<float>& v, size_t from, size_t to)
{
    double sum = 0.0;
    for (auto i = from; i < to; ++i)
        sum += static_cast<double> (v[i]) * v[i];
    return std::sqrt (sum / static_cast<double> (std::max<size_t> (1, to - from)));
}

bool allFinite (const Stereo& s)
{
    for (size_t i = 0; i < s.l.size(); ++i)
        if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i]))
            return false;
    return true;
}

void testCurves()
{
    const Curve c { 0.2f, 0.6f, 2.0f };
    CHECK (c.apply (0.0f) == 0.0f);
    CHECK (c.apply (0.2f) == 0.0f);
    CHECK (c.apply (0.6f) == 1.0f);
    CHECK (c.apply (1.0f) == 1.0f);
    CHECK (std::abs (c.apply (0.4f) - 0.25f) < 1e-6f);

    const Range hz { 20.0f, 2000.0f, {}, true };
    CHECK (std::abs (hz.at (0.5f) - 200.0f) < 0.01f);  // exponential midpoint
}

void testPresetsAreMonotonicAndBounded()
{
    for (int p = 0; p < numPresets; ++p)
    {
        const auto& preset = getPreset (p);
        CHECK (! preset.name.empty());
        auto previous = computeTargets (0.0f, preset);
        CHECK (previous.wetMaster == 0.0f);
        CHECK (previous.engage == 0.0f);
        CHECK (previous.hole == 1.0f);

        for (int step = 1; step <= 100; ++step)
        {
            const auto t = computeTargets (static_cast<float> (step) / 100.0f, preset);
            CHECK (t.hpHz >= previous.hpHz);          // the voice only ever gets thinner
            CHECK (t.hpQ <= 1.6f + 1e-6f);            // no whistling resonance on vowels
            CHECK (t.hpHz <= 2200.0f + 0.5f);         // never only sibilance left
            CHECK (t.delayFeedback < 0.9f);           // echo loop stays stable
            CHECK (t.gateDivBeats <= previous.gateDivBeats); // stutter only speeds up
            CHECK (t.wetMaster >= previous.wetMaster);
            previous = t;
        }
    }

    // Clamped index
    CHECK (&getPreset (-5) == &getPreset (0));
    CHECK (&getPreset (99) == &getPreset (numPresets - 1));

    // Stutter preset reaches 1/32 (0.125 beats) at full intensity
    CHECK (computeTargets (1.0f, getPreset (3)).gateDivBeats == 0.125f);
}

void testZeroIntensityIsBitExact()
{
    const auto voice = makeVoice (24000);
    for (int p = 0; p < numPresets; ++p)
    {
        Controls c;
        c.preset = p;
        const auto out = run (voice, c, 256, 0.0f, 0.0f);
        bool exact = true;
        for (size_t i = 0; i < voice.size(); ++i)
            exact = exact && out.l[i] == voice[i] && out.r[i] == voice[i];
        CHECK (exact);
    }
}

void testBlockSizeIndependence()
{
    const auto voice = makeVoice (20000);
    Controls c;
    c.preset = 4;
    const auto a = run (voice, c, 1, 0.9f, 0.9f);
    const auto b = run (voice, c, 7, 0.9f, 0.9f);
    const auto d = run (voice, c, 512, 0.9f, 0.9f);
    bool same = true;
    for (size_t i = 0; i < voice.size(); ++i)
        same = same && a.l[i] == b.l[i] && a.l[i] == d.l[i] && a.r[i] == d.r[i];
    CHECK (same);
}

void testSweepStaysFiniteAndBounded()
{
    const auto voice = makeVoice (96000, 0.9f);
    for (int p = 0; p < numPresets; ++p)
    {
        Controls c;
        c.preset = p;
        const auto out = run (voice, c, 128, 0.0f, 1.0f);
        CHECK (allFinite (out));

        float peak = 0.0f;
        for (size_t i = 0; i < out.l.size(); ++i)
            peak = std::max ({ peak, std::abs (out.l[i]), std::abs (out.r[i]) });
        CHECK (peak < 4.0f);   // +12 dBFS headroom; real output sits far below
        std::printf ("  %-15s peak over a full sweep: %.2f\n", std::string (getPreset (p).name).c_str(), peak);
    }

    // Absurd input must not blow up either
    std::vector<float> loud (20000, 0.0f);
    for (size_t i = 0; i < loud.size(); ++i)
        loud[i] = (i % 50 < 25) ? 100.0f : -100.0f;
    Controls c;
    c.preset = 4;
    CHECK (allFinite (run (loud, c, 64, 1.0f, 1.0f)));
}

void testSilenceStaysSilent()
{
    const std::vector<float> silence (48000, 0.0f);
    for (int p = 0; p < numPresets; ++p)
    {
        Controls c;
        c.preset = p;
        const auto out = run (silence, c, 256, 1.0f, 1.0f);
        CHECK (rms (out.l, 0, out.l.size()) < 1e-9);
        CHECK (rms (out.r, 0, out.r.size()) < 1e-9);
    }
}

void testFullIntensityHoleIsSilent()
{
    const auto voice = makeVoice (48000);
    Controls c;
    c.preset = 1;
    const auto out = run (voice, c, 256, maxIntensity, maxIntensity);
    CHECK (rms (out.l, 4800, out.l.size()) < 1e-6);
}

void testIntensityChangesTheSound()
{
    const auto voice = makeVoice (48000);
    for (int p = 0; p < numPresets; ++p)
    {
        Controls c;
        c.preset = p;
        const auto out = run (voice, c, 256, 0.8f, 0.8f);
        double diff = 0.0;
        for (size_t i = 0; i < voice.size(); ++i)
            diff += std::abs (out.l[i] - voice[i]);
        CHECK (diff / static_cast<double> (voice.size()) > 1e-3);
    }
}

void testDropKillsTails()
{
    // Build at 100 % on a sung phrase, snap the knob to 0: within 50 ms the output is the dry voice.
    const auto voice = makeVoice (96000);
    for (int p = 0; p < numPresets; ++p)
    {
        RiserEngine engine;
        Controls c;
        c.preset = p;
        c.intensity = 1.0f;
        engine.prepare (sampleRate, c);

        auto l = voice, r = voice;
        engine.process (l.data(), r.data(), 48000, c);
        c.intensity = 0.0f;
        engine.process (l.data() + 48000, r.data() + 48000, 48000, c);

        const auto settle = static_cast<size_t> (48000 + 0.05 * sampleRate);
        double err = 0.0;
        for (auto i = settle; i < voice.size(); ++i)
            err = std::max (err, static_cast<double> (std::abs (l[i] - voice[i])));
        CHECK (err < 1e-3);
    }
}

void testMixAndOutputGain()
{
    const auto voice = makeVoice (24000);
    Controls c;
    c.preset = 0;
    c.mix = 0.0f;
    const auto dry = run (voice, c, 256, 1.0f, 1.0f);
    bool exact = true;
    for (size_t i = 0; i < voice.size(); ++i)
        exact = exact && dry.l[i] == voice[i];
    CHECK (exact);   // mix 0 = untouched input even at full intensity

    c.mix = 1.0f;
    c.outputDb = -6.0f;
    const auto quiet = run (voice, c, 256, 0.0f, 0.0f);
    CHECK (std::abs (rms (quiet.l, 4800, voice.size()) / rms (voice, 4800, voice.size()) - 0.501) < 0.01);
}
} // namespace

int main()
{
    testCurves();
    testPresetsAreMonotonicAndBounded();
    testZeroIntensityIsBitExact();
    testBlockSizeIndependence();
    testSweepStaysFiniteAndBounded();
    testSilenceStaysSilent();
    testFullIntensityHoleIsSilent();
    testIntensityChangesTheSound();
    testDropKillsTails();
    testMixAndOutputGain();

    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
