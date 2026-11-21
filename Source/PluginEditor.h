#pragma once

#include "LookAndFeel.h"
#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** Shows how far each effect is in, for the current preset and knob position.
    Text values accompany every bar, so colour never carries the meaning alone. */
class StageMeter final : public juce::Component
{
public:
    StageMeter();

    /** Message thread. */
    void update (float intensity, int presetIndex);
    void paint (juce::Graphics&) override;

    struct Row
    {
        juce::String name;
        float amount = 0.0f;   // 0..1 of what this preset reaches at 100 %
        juce::String value;
        bool available = true; // false when this preset does not use the effect
    };

    const std::array<Row, 6>& getRows() const noexcept { return rows; }

private:
    std::array<Row, 6> rows;
    juce::String lastSummary;
};

/** Secondary settings, opened in a call-out box (progressive disclosure). */
class SettingsPanel final : public juce::Component
{
public:
    explicit SettingsPanel (juce::AudioProcessorValueTreeState&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::Label mixLabel, outputLabel;
    juce::Slider mixSlider, outputSlider;
    juce::AudioProcessorValueTreeState::SliderAttachment mixAttachment, outputAttachment;
};

class VoxLiftEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit VoxLiftEditor (VoxLiftProcessor&);
    ~VoxLiftEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Exposed for EditorTests
    juce::Slider& getIntensitySlider() noexcept      { return intensitySlider; }
    juce::ComboBox& getPresetBox() noexcept          { return presetBox; }
    juce::TextButton& getSettingsButton() noexcept   { return settingsButton; }
    StageMeter& getStageMeter() noexcept             { return meter; }
    juce::Label& getStatusLabel() noexcept           { return statusLabel; }

private:
    void timerCallback() override;
    void showSettings();
    void updateDescription();

    VoxLiftProcessor& processor;
    VoxLiftLookAndFeel lookAndFeel;

    juce::Label titleLabel, subtitleLabel, descriptionLabel, statusLabel;
    juce::ComboBox presetBox;
    juce::TextButton settingsButton { "Settings..." };
    juce::Slider intensitySlider;
    StageMeter meter;

    // Created in the constructor body, after the combo box has its items.
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> intensityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetAttachment;

    juce::TooltipWindow tooltips { this };
    int lastPreset = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxLiftEditor)
};
