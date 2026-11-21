#pragma once

// Small real-time-safe DSP building blocks. No JUCE, no allocation outside prepare ().
// Every class here is used from the audio thread only, unless noted otherwise.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace voxlift::dsp
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;

inline float dbToGain (float db) noexcept       { return std::pow (10.0f, db / 20.0f); }
inline float clamp01 (float x) noexcept         { return std::clamp (x, 0.0f, 1.0f); }
inline float lerp (float a, float b, float t) noexcept { return a + t * (b - a); }
inline float flushDenormal (float x) noexcept   { return std::abs (x) < 1.0e-15f ? 0.0f : x; }

/** One-pole smoother. Snaps to the target once it is within `snapDistance`, so a settled
    value is exact (needed for the bit-exact bypass at intensity 0). */
class Smoother final
{
public:
    void setTimeConstant (float seconds, float updatesPerSecond) noexcept
    {
        coeff = seconds <= 0.0f ? 1.0f : 1.0f - std::exp (-1.0f / (seconds * updatesPerSecond));
    }

    void setSnapDistance (float distance) noexcept { snapDistance = distance; }

    void reset (float value) noexcept   { current = value; }

    float next (float target) noexcept
    {
        current += coeff * (target - current);
        if (std::abs (target - current) < snapDistance)
            current = target;
        return current;
    }

    float value() const noexcept        { return current; }

private:
    float coeff = 1.0f;
    float current = 0.0f;
    float snapDistance = 1.0e-6f;
};

/** Topology-preserving state-variable filter (Simper/Cytomic). Stays stable when the
    cutoff is modulated quickly, which a riser does all the time. */
class Svf final
{
public:
    void setSampleRate (float newRate) noexcept { sampleRate = newRate; }

    void setParams (float cutoffHz, float q) noexcept
    {
        const auto fc = std::clamp (cutoffHz, 10.0f, 0.45f * sampleRate);
        g = std::tan (pi * fc / sampleRate);
        k = 1.0f / std::max (q, 0.1f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    struct Outputs { float low, band, high; };

    Outputs process (float v0) noexcept
    {
        const auto v3 = v0 - ic2;
        const auto v1 = a1 * ic1 + a2 * v3;
        const auto v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = flushDenormal (2.0f * v1 - ic1);
        ic2 = flushDenormal (2.0f * v2 - ic2);
        return { v2, v1, v0 - k * v1 - v2 };
    }

    float highpass (float x) noexcept         { return process (x).high; }
    float lowpass (float x) noexcept          { return process (x).low; }
    /** Band-pass normalised to unity gain at the centre frequency. */
    float bandpass (float x) noexcept         { return k * process (x).band; }

private:
    float sampleRate = 44100.0f;
    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
};

/** Fractional delay line with linear interpolation. Memory is reserved in prepare (). */
class DelayLine final
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 4)
            size <<= 1;
        buffer.assign (static_cast<size_t> (size), 0.0f);
        mask = size - 1;
        writeIndex = 0;
    }

    void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); writeIndex = 0; }

    float read (float delaySamples) const noexcept
    {
        const auto clamped = std::clamp (delaySamples, 1.0f, static_cast<float> (mask - 2));
        const auto whole = static_cast<int> (clamped);
        const auto frac = clamped - static_cast<float> (whole);
        const auto a = buffer[static_cast<size_t> ((writeIndex - whole) & mask)];
        const auto b = buffer[static_cast<size_t> ((writeIndex - whole - 1) & mask)];
        return a + frac * (b - a);
    }

    void write (float x) noexcept
    {
        buffer[static_cast<size_t> (writeIndex)] = flushDenormal (x);
        writeIndex = (writeIndex + 1) & mask;
    }

private:
    std::vector<float> buffer;
    int mask = 0;
    int writeIndex = 0;
};

/** Peak envelope follower with separate attack and release. */
class EnvelopeFollower final
{
public:
    void setTimes (float attackSeconds, float releaseSeconds, float sampleRate) noexcept
    {
        attack = 1.0f - std::exp (-1.0f / (attackSeconds * sampleRate));
        release = 1.0f - std::exp (-1.0f / (releaseSeconds * sampleRate));
    }

    void reset() noexcept { env = 0.0f; }

    float process (float x) noexcept
    {
        const auto level = std::abs (x);
        env += (level > env ? attack : release) * (level - env);
        env = flushDenormal (env);
        return env;
    }

private:
    float attack = 1.0f, release = 1.0f, env = 0.0f;
};

/** Deterministic white noise (xorshift32), so tests are repeatable. */
class Noise final
{
public:
    explicit Noise (uint32_t seed = 0x9E3779B9u) noexcept : state (seed == 0 ? 1u : seed) {}

    float next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float> (state) * (2.0f / 4294967295.0f) - 1.0f;
    }

private:
    uint32_t state;
};

/** Single-sideband frequency shifter: a Hilbert pair of allpass chains (Niemitalo's
    coefficients) multiplied by a quadrature oscillator that the caller owns.
    Placed inside a delay feedback loop it gives the "barber pole" endless rise. */
class HilbertPair final
{
public:
    void reset() noexcept
    {
        for (auto& s : sectionsA) s = {};
        for (auto& s : sectionsB) s = {};
        delayedB = 0.0f;
    }

    /** Returns the shifted sample for the given oscillator phase (cos, sin). */
    float shift (float x, float cosPhase, float sinPhase) noexcept
    {
        auto re = x;
        for (size_t i = 0; i < 4; ++i)
            re = sectionsA[i].process (re, coeffsA[i]);

        auto im = x;
        for (size_t i = 0; i < 4; ++i)
            im = sectionsB[i].process (im, coeffsB[i]);

        const auto imDelayed = delayedB;
        delayedB = im;
        return re * cosPhase - imDelayed * sinPhase;
    }

private:
    struct Section
    {
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

        float process (float x, float a2) noexcept
        {
            const auto y = flushDenormal (a2 * (x + y2) - x2);
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }
    };

    static constexpr float coeffsA[4] { 0.6923878f * 0.6923878f,
                                        0.9360654322959f * 0.9360654322959f,
                                        0.9882295226860f * 0.9882295226860f,
                                        0.9987488452737f * 0.9987488452737f };
    static constexpr float coeffsB[4] { 0.4021921162426f * 0.4021921162426f,
                                        0.8561710882420f * 0.8561710882420f,
                                        0.9722909545651f * 0.9722909545651f,
                                        0.9952884791278f * 0.9952884791278f };

    Section sectionsA[4] {};
    Section sectionsB[4] {};
    float delayedB = 0.0f;
};
} // namespace voxlift::dsp
