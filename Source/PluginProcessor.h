#pragma once

#include "Core/RiserEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace paramIds
{
    inline constexpr auto intensity = "intensity";
    inline constexpr auto preset = "preset";
    inline constexpr auto mix = "mix";
    inline constexpr auto output = "output";
}

class VoxLiftProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int stateVersion = 1;
    static constexpr auto pluginName = "VoxLift";

    VoxLiftProcessor();
    ~VoxLiftProcessor() override = default;

    // Message thread (host guarantees processBlock is not running)
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    // Audio thread
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                 { return true; }

    const juce::String getName() const override     { return pluginName; }
    bool acceptsMidi() const override               { return false; }
    bool producesMidi() const override              { return false; }
    bool isMidiEffect() const override              { return false; }
    double getTailLengthSeconds() const override    { return 6.0; }  // longest reverb + echo spiral

    int getNumPrograms() override                   { return 1; }
    int getCurrentProgram() override                { return 0; }
    void setCurrentProgram (int) override           {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // Any thread (hosts may call these from a background thread); APVTS state is thread-safe to copy.
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() noexcept { return params; }

    /** Smoothed intensity as the audio thread last heard it (0..1.1). Any thread. */
    float getHeardIntensity() const noexcept { return heardIntensity.load (std::memory_order_relaxed); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    voxlift::Controls readControls() const noexcept;

    juce::AudioProcessorValueTreeState params;
    std::atomic<float>* intensityParam = nullptr;
    std::atomic<float>* presetParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputParam = nullptr;

    voxlift::RiserEngine engine;
    std::atomic<float> heardIntensity { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxLiftProcessor)
};
