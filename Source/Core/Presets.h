#pragma once

// Vocal-tuned macro maps. One Intensity value (0..1) is turned into every effect setting
// through these curves. Pure data plus pure functions: safe from any thread.

#include <array>
#include <cmath>
#include <string_view>

namespace voxlift
{
/** Where along the knob an effect comes in, where it is fully in, and how it ramps.
    shape > 1 starts slowly (good for things that get harsh on voice), < 1 starts fast. */
struct Curve
{
    float start = 0.0f;
    float end = 1.0f;
    float shape = 1.0f;

    float apply (float x) const noexcept
    {
        if (end <= start)
            return x >= end ? 1.0f : 0.0f;
        const auto t = (x - start) / (end - start);
        const auto clamped = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return std::pow (clamped, shape);
    }

    bool operator== (const Curve&) const = default;
};

/** A value that moves from `from` to `to` following a curve; exponential for frequencies. */
struct Range
{
    float from = 0.0f;
    float to = 0.0f;
    Curve curve {};
    bool exponential = false;

    float at (float x) const noexcept
    {
        const auto a = curve.apply (x);
        if (exponential && from > 0.0f && to > 0.0f)
            return from * std::pow (to / from, a);
        return from + a * (to - from);
    }

    bool operator== (const Range&) const = default;
};

struct Preset
{
    std::string_view name;
    std::string_view description;

    // Dry vocal chain
    Range hpHz;          // rising high-pass: the core of the build-up
    Range hpQ;           // resonance; kept moderate so the voice does not whistle
    Range lpHz;          // optional narrowing ("radio") from the top
    Range saturation;    // 0..1 soft drive on the dry voice
    Range gateDepth;     // 0..1 tempo-synced stutter on the dry voice
    float gateDivStartBeats = 0.5f;  // gate rate doubles from here ...
    float gateDivEndBeats = 0.5f;    // ... down to here as the gate curve rises
    Range dryGain;       // dry level trim so the overall loudness stays even

    // Wet returns
    float delayBeats = 0.75f;
    Range delaySend;
    Range delayFeedback;
    Range shiftHz;       // frequency shift inside the echo loop: rising spiral
    Range reverbSend;
    Range reverbSize;
    float reverbDamping = 0.6f;
    Range noise;         // breath noise, follows the vocal envelope (linear gain)
    Range width;         // M/S width of the wet returns
    Range duck;          // 0..1: how much the wet ducks under the sung words

    bool operator== (const Preset&) const = default;
};

constexpr int numPresets = 6;

/** Index is clamped, so any value is safe. */
const Preset& getPreset (int index) noexcept;

/** Highest intensity the knob reaches; 1.0..1.1 fades everything to silence before the drop. */
constexpr float maxIntensity = 1.1f;

/** Intensity below which the processed dry path cross-fades to the untouched input. */
constexpr float engageWidth = 0.03f;
} // namespace voxlift
