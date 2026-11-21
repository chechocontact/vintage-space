#include "RiserEngine.h"

#include <cmath>

namespace voxlift
{
namespace
{
constexpr float intensitySmoothingSeconds = 0.004f;  // fast enough that a snap to 0 is a drop
constexpr float intensitySnap = 1.0e-4f;             // 0.01 % of the knob: inaudible, lets the drop settle exactly
constexpr float derivedSmoothingSeconds = 0.008f;    // hides zipper noise when switching presets
constexpr float delayTimeSmoothingSeconds = 0.05f;
constexpr float gainSmoothingSeconds = 0.02f;
constexpr float gateEdgeSeconds = 0.0015f;           // click-free stutter edges

constexpr Curve wetMasterCurve { 0.0f, 0.15f, 0.7f };

constexpr float feedbackHighPassHz = 300.0f;   // echoes stay out of the chest/mud range
constexpr float feedbackLowPassHz = 6500.0f;   // and do not pile up sibilance
constexpr float reverbHighPassHz = 250.0f;
constexpr float reverbReturnGain = 0.45f;     // keeps a full build within ~+6 dB of the dry voice
constexpr float echoReturnGain = 0.8f;

using FloatArray = std::array<float, 17>;

FloatArray toArray (const Targets& t) noexcept
{
    return { t.hpHz, t.hpQ, t.lpHz, t.saturation, t.gateDepth, t.dryGain,
             t.delaySend, t.delayFeedback, t.shiftHz, t.reverbSend, t.reverbSize, t.noise,
             t.width, t.duck, t.wetMaster, t.engage, t.hole };
}

Targets fromArray (const FloatArray& a, float gateDivBeats) noexcept
{
    return { a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11],
             a[12], a[13], a[14], a[15], a[16], gateDivBeats };
}

float delaySamplesFor (const Preset& preset, double bpm, float sampleRate) noexcept
{
    const auto safeBpm = std::clamp (bpm, 20.0, 400.0);
    const auto seconds = static_cast<float> (preset.delayBeats * 60.0 / safeBpm);
    return std::min (seconds, RiserEngine::maxDelaySeconds - 0.01f) * sampleRate;
}
} // namespace

Targets computeTargets (float intensityValue, const Preset& p) noexcept
{
    const auto x = std::clamp (intensityValue, 0.0f, maxIntensity);
    const auto xb = std::min (x, 1.0f);

    Targets t {};
    t.hpHz = p.hpHz.at (xb);
    t.hpQ = p.hpQ.at (xb);
    t.lpHz = p.lpHz.at (xb);
    t.saturation = p.saturation.at (xb);
    t.gateDepth = p.gateDepth.at (xb);
    t.dryGain = p.dryGain.at (xb);
    t.delaySend = p.delaySend.at (xb);
    t.delayFeedback = p.delayFeedback.at (xb);
    t.shiftHz = p.shiftHz.at (xb);
    t.reverbSend = p.reverbSend.at (xb);
    t.reverbSize = p.reverbSize.at (xb);
    t.noise = p.noise.at (xb);
    t.width = p.width.at (xb);
    t.duck = p.duck.at (xb);
    t.wetMaster = wetMasterCurve.apply (xb);
    t.engage = dsp::clamp01 (x / engageWidth);
    t.hole = 1.0f - dsp::clamp01 ((x - 1.0f) / (maxIntensity - 1.0f));

    // Stutter rate doubles in whole steps (1/8 -> 1/16 -> 1/32) as the gate curve rises.
    const auto steps = static_cast<int> (std::lround (std::log2 (std::max (1.0f, p.gateDivStartBeats / std::max (p.gateDivEndBeats, 1.0e-3f)))));
    const auto a = p.gateDepth.curve.apply (xb);
    const auto n = std::min (steps, static_cast<int> (a * static_cast<float> (steps + 1) - 1.0e-4f));
    t.gateDivBeats = p.gateDivStartBeats / static_cast<float> (1 << std::max (n, 0));
    return t;
}

void RiserEngine::prepare (double newSampleRate, const Controls& initial)
{
    sampleRate = static_cast<float> (newSampleRate);
    const auto maxDelay = static_cast<int> (maxDelaySeconds * sampleRate) + 8;
    delayL.prepare (maxDelay);
    delayR.prepare (maxDelay);
    reverb.prepare (newSampleRate);

    for (auto* f : { &hpL, &hpR, &lpL, &lpR, &fbHpL, &fbHpR, &fbLpL, &fbLpR,
                     &verbHpL, &verbHpR, &noiseBpL, &noiseBpR })
        f->setSampleRate (sampleRate);

    const auto controlRate = sampleRate / static_cast<float> (controlInterval);
    intensity.setTimeConstant (intensitySmoothingSeconds, sampleRate);
    intensity.setSnapDistance (intensitySnap);
    for (auto& d : derived)
        d.setTimeConstant (derivedSmoothingSeconds, controlRate);
    delaySamples.setTimeConstant (delayTimeSmoothingSeconds, controlRate);
    outputGain.setTimeConstant (gainSmoothingSeconds, sampleRate);
    mix.setTimeConstant (gainSmoothingSeconds, sampleRate);
    gateSmooth.setTimeConstant (gateEdgeSeconds, sampleRate);

    duckEnv.setTimes (0.005f, 0.15f, sampleRate);
    breathEnv.setTimes (0.01f, 0.4f, sampleRate);

    reset (initial);
}

void RiserEngine::reset (const Controls& initial) noexcept
{
    const auto& preset = getPreset (initial.preset);
    const auto x = std::clamp (initial.intensity, 0.0f, maxIntensity);
    intensity.reset (x);

    current = computeTargets (x, preset);
    const auto values = toArray (current);
    for (size_t i = 0; i < derived.size(); ++i)
        derived[i].reset (values[i]);
    gateDivBeats = current.gateDivBeats;

    delaySamples.reset (delaySamplesFor (preset, initial.bpm, sampleRate));
    outputGain.reset (dsp::dbToGain (initial.outputDb));
    mix.reset (dsp::clamp01 (initial.mix));
    gateSmooth.reset (1.0f);

    for (auto* f : { &hpL, &hpR, &lpL, &lpR, &fbHpL, &fbHpR, &fbLpL, &fbLpR,
                     &verbHpL, &verbHpR, &noiseBpL, &noiseBpR })
        f->reset();

    delayL.reset();
    delayR.reset();
    shifterL.reset();
    shifterR.reset();
    reverb.reset();
    duckEnv.reset();
    breathEnv.reset();
    shiftCos = 1.0f;
    shiftSin = 0.0f;
    beat = initial.ppqAtBlockStart.value_or (0.0);
    controlCountdown = 0;
}

void RiserEngine::updateControl (const Controls& controls) noexcept
{
    const auto& preset = getPreset (controls.preset);
    const auto target = computeTargets (intensity.value(), preset);
    const auto targetValues = toArray (target);

    FloatArray values {};
    for (size_t i = 0; i < derived.size(); ++i)
        values[i] = derived[i].next (targetValues[i]);

    // wetMaster, engage and hole follow the (already smoothed) knob directly: an extra
    // smoothing stage here would blur the drop.
    for (size_t i = 14; i < derived.size(); ++i)
    {
        derived[i].reset (targetValues[i]);
        values[i] = targetValues[i];
    }

    gateDivBeats = target.gateDivBeats;
    current = fromArray (values, gateDivBeats);
    reverbDamping = preset.reverbDamping;

    hpL.setParams (current.hpHz, current.hpQ);
    hpR.setParams (current.hpHz, current.hpQ);
    lpL.setParams (current.lpHz, 0.707f);
    lpR.setParams (current.lpHz, 0.707f);
    fbHpL.setParams (feedbackHighPassHz, 0.707f);
    fbHpR.setParams (feedbackHighPassHz, 0.707f);
    fbLpL.setParams (feedbackLowPassHz, 0.707f);
    fbLpR.setParams (feedbackLowPassHz, 0.707f);
    verbHpL.setParams (reverbHighPassHz, 0.707f);
    verbHpR.setParams (reverbHighPassHz, 0.707f);

    const auto noiseCentre = std::clamp (current.hpHz * 3.0f, 2500.0f, 10000.0f);
    noiseBpL.setParams (noiseCentre, 0.9f);
    noiseBpR.setParams (noiseCentre, 0.9f);

    reverb.setParams (current.reverbSize, reverbDamping);

    delaySamples.next (delaySamplesFor (preset, controls.bpm, sampleRate));

    const auto w = dsp::twoPi * current.shiftHz / sampleRate;
    shiftStepCos = std::cos (w);
    shiftStepSin = std::sin (w);
    const auto norm = 1.0f / std::sqrt (shiftCos * shiftCos + shiftSin * shiftSin);
    shiftCos *= norm;
    shiftSin *= norm;

    beatsPerSample = std::clamp (controls.bpm, 20.0, 400.0) / 60.0 / static_cast<double> (sampleRate);
}

void RiserEngine::process (float* left, float* right, int numSamples, const Controls& controls) noexcept
{
    if (controls.ppqAtBlockStart.has_value())
        beat = *controls.ppqAtBlockStart;

    const auto intensityTarget = std::clamp (controls.intensity, 0.0f, maxIntensity);
    const auto gainTarget = dsp::dbToGain (controls.outputDb);
    const auto mixTarget = dsp::clamp01 (controls.mix);

    for (int i = 0; i < numSamples; ++i)
    {
        intensity.next (intensityTarget);

        if (controlCountdown == 0)
        {
            updateControl (controls);
            controlCountdown = controlInterval;
        }
        --controlCountdown;

        const auto inL = left[i];
        const auto inR = right[i];

        const auto duckLevel = duckEnv.process (std::max (std::abs (inL), std::abs (inR)));
        const auto breathLevel = breathEnv.process (0.5f * (inL + inR));

        // --- Dry vocal chain: high-pass, optional top cut, soft drive.
        auto dL = lpL.lowpass (hpL.highpass (inL));
        auto dR = lpR.lowpass (hpR.highpass (inR));

        if (current.saturation > 0.0f)
        {
            const auto drive = 1.0f + current.saturation * 5.0f;
            const auto makeup = 1.0f + 0.5f * current.saturation;
            dL = dsp::lerp (dL, std::tanh (drive * dL) / drive * makeup, current.saturation);
            dR = dsp::lerp (dR, std::tanh (drive * dR) / drive * makeup, current.saturation);
        }

        // --- Tempo-synced stutter on the dry voice only; the returns keep washing underneath.
        auto phase = std::fmod (beat, static_cast<double> (gateDivBeats)) / gateDivBeats;
        if (phase < 0.0)
            phase += 1.0;
        const auto gateTarget = phase < 0.5 ? 1.0f : 1.0f - current.gateDepth;
        const auto gate = gateSmooth.next (gateTarget);
        beat += beatsPerSample;

        const auto dryL = dsp::lerp (inL, dL * gate, current.engage) * current.dryGain;
        const auto dryR = dsp::lerp (inR, dR * gate, current.engage) * current.dryGain;

        // --- Ping-pong echo with a frequency shifter in the loop (rising spiral).
        const auto ds = delaySamples.value();
        const auto echoL = delayL.read (ds);
        const auto echoR = delayR.read (ds);
        const auto loopL = shifterL.shift (fbLpL.lowpass (fbHpL.highpass (echoR)), shiftCos, shiftSin);
        const auto loopR = shifterR.shift (fbLpR.lowpass (fbHpR.highpass (echoL)), shiftCos, shiftSin);
        const auto send = 0.5f * (dL + dR) * current.delaySend;
        delayL.write (send + std::tanh (loopL * current.delayFeedback));
        delayR.write (std::tanh (loopR * current.delayFeedback));

        const auto nextCos = shiftCos * shiftStepCos - shiftSin * shiftStepSin;
        shiftSin = shiftSin * shiftStepCos + shiftCos * shiftStepSin;
        shiftCos = nextCos;

        // --- Reverb, fed by the filtered voice and a little of the echoes.
        const auto verbInL = verbHpL.highpass ((dL + 0.25f * echoL) * current.reverbSend);
        const auto verbInR = verbHpR.highpass ((dR + 0.25f * echoR) * current.reverbSend);
        float verbL = 0.0f, verbR = 0.0f;
        reverb.process (verbInL, verbInR, verbL, verbR);

        // --- Breath noise that follows the singer, so silence stays silent.
        const auto breath = current.noise * std::min (1.0f, breathLevel * 4.0f);
        const auto nL = noiseBpL.bandpass (noiseL.next()) * breath;
        const auto nR = noiseBpR.bandpass (noiseR.next()) * breath;

        auto wetL = echoReturnGain * echoL + reverbReturnGain * verbL + nL;
        auto wetR = echoReturnGain * echoR + reverbReturnGain * verbR + nR;

        const auto mid = 0.5f * (wetL + wetR);
        const auto side = 0.5f * (wetL - wetR) * current.width;
        const auto duckGain = 1.0f - current.duck * std::min (1.0f, duckLevel * 4.0f);
        const auto wetGain = duckGain * current.wetMaster;
        wetL = (mid + side) * wetGain;
        wetR = (mid - side) * wetGain;

        const auto outL = (dryL + wetL) * current.hole;
        const auto outR = (dryR + wetR) * current.hole;

        const auto m = mix.next (mixTarget);
        const auto g = outputGain.next (gainTarget);
        left[i] = dsp::lerp (inL, outL, m) * g;
        right[i] = dsp::lerp (inR, outR, m) * g;
    }
}
} // namespace voxlift
