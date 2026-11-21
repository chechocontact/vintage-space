#pragma once

// The one-knob vocal riser. No JUCE.
//
// Threads: prepare () allocates and must be called from prepareToPlay (never while
// process () runs). process () and reset () are audio-thread only and never allocate.
// Output does not depend on the host block size: control values are updated on an
// internal 16-sample clock that runs across block boundaries.

#include "Dsp.h"
#include "Presets.h"
#include "Reverb.h"

#include <array>
#include <optional>

namespace voxlift
{
struct Controls
{
    float intensity = 0.0f;           // 0 .. maxIntensity (1.1)
    int preset = 0;
    float mix = 1.0f;                 // 0..1
    float outputDb = 0.0f;
    double bpm = 120.0;
    std::optional<double> ppqAtBlockStart; // set when the host is playing, to phase-lock the stutter
};

/** Every derived setting, as computed from (intensity, preset). Exposed for tests and the UI. */
struct Targets
{
    float hpHz, hpQ, lpHz, saturation, gateDepth, dryGain;
    float delaySend, delayFeedback, shiftHz, reverbSend, reverbSize, noise, width, duck;
    float wetMaster;  // all returns scale with this, so snapping the knob to 0 kills the tails (the drop)
    float engage;     // 0 = untouched input, 1 = processed dry chain
    float hole;       // 1 below 100 %, fades to 0 at 110 %
    float gateDivBeats;
};

Targets computeTargets (float intensity, const Preset& preset) noexcept;

class RiserEngine final
{
public:
    static constexpr int controlInterval = 16;
    static constexpr float maxDelaySeconds = 4.0f;

    void prepare (double sampleRate, const Controls& initial);
    void reset (const Controls& initial) noexcept;
    void process (float* left, float* right, int numSamples, const Controls& controls) noexcept;

    /** Smoothed intensity as heard; lets the UI animate in step with the audio. */
    float getCurrentIntensity() const noexcept { return intensity.value(); }

private:
    void updateControl (const Controls& controls) noexcept;

    float sampleRate = 44100.0f;

    // Smoothed control state
    dsp::Smoother intensity;
    std::array<dsp::Smoother, 17> derived;  // one per float field of Targets (excluding gateDivBeats)
    Targets current {};
    dsp::Smoother delaySamples;
    dsp::Smoother outputGain;
    dsp::Smoother mix;
    int controlCountdown = 0;
    float gateDivBeats = 0.5f;
    float reverbDamping = 0.6f;

    // Transport
    double beat = 0.0;
    double beatsPerSample = 0.0;

    // Dry chain
    dsp::Svf hpL, hpR, lpL, lpR;
    dsp::EnvelopeFollower duckEnv, breathEnv;
    dsp::Smoother gateSmooth;

    // Echo
    dsp::DelayLine delayL, delayR;
    dsp::Svf fbHpL, fbHpR, fbLpL, fbLpR;
    dsp::HilbertPair shifterL, shifterR;
    float shiftCos = 1.0f, shiftSin = 0.0f, shiftStepCos = 1.0f, shiftStepSin = 0.0f;

    // Reverb
    dsp::Svf verbHpL, verbHpR;
    dsp::StereoReverb reverb;

    // Breath noise
    dsp::Noise noiseL { 0x1234567u }, noiseR { 0x7654321u };
    dsp::Svf noiseBpL, noiseBpR;
};
} // namespace voxlift
