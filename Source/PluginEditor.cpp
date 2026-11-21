#include "PluginEditor.h"

using namespace theme;

namespace
{
juce::String divisionName (float beats)
{
    if (beats >= 0.999f) return "1/4";
    if (beats >= 0.499f) return "1/8";
    if (beats >= 0.249f) return "1/16";
    return "1/32";
}

void styleLabel (juce::Label& label, float size, juce::Colour colourToUse, bool bold = false)
{
    label.setFont (font (size, bold));
    label.setColour (juce::Label::textColourId, colourToUse);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setMinimumHorizontalScale (1.0f);
}
} // namespace

//==============================================================================
StageMeter::StageMeter()
{
    setTitle ("Effect stages");
    setAccessible (true);
    const char* names[] { "Filter", "Echo", "Spiral", "Reverb", "Breath", "Stutter" };
    for (size_t i = 0; i < rows.size(); ++i)
        rows[i].name = names[i];
}

void StageMeter::update (float intensity, int presetIndex)
{
    const auto& preset = voxlift::getPreset (presetIndex);
    const auto now = voxlift::computeTargets (intensity, preset);
    const auto full = voxlift::computeTargets (1.0f, preset);

    auto ratio = [] (float value, float max) { return max > 0.0f ? juce::jlimit (0.0f, 1.0f, value / max) : 0.0f; };

    const auto hpSpan = std::log (full.hpHz / preset.hpHz.from);
    rows[0] = { "Filter", hpSpan > 0.0f ? juce::jlimit (0.0f, 1.0f, std::log (now.hpHz / preset.hpHz.from) / hpSpan) : 0.0f,
                juce::String (juce::roundToInt (now.hpHz)) + " Hz", true };
    rows[1] = { "Echo", ratio (now.delaySend, full.delaySend),
                juce::String (juce::roundToInt (now.delayFeedback * 100.0f)) + " % fb", full.delaySend > 0.0f };
    rows[2] = { "Spiral", ratio (now.shiftHz, full.shiftHz),
                "+" + juce::String (now.shiftHz, 1) + " Hz", full.shiftHz > 0.0f };
    rows[3] = { "Reverb", ratio (now.reverbSend, full.reverbSend),
                juce::String (juce::roundToInt (now.reverbSend * 100.0f)) + " %", full.reverbSend > 0.0f };
    rows[4] = { "Breath", ratio (now.noise, full.noise),
                juce::String (juce::roundToInt (ratio (now.noise, full.noise) * 100.0f)) + " %", full.noise > 0.0f };
    rows[5] = { "Stutter", ratio (now.gateDepth, full.gateDepth),
                now.gateDepth > 0.001f ? divisionName (now.gateDivBeats) : juce::String ("off"), full.gateDepth > 0.0f };

    for (auto& row : rows)
        if (! row.available)
            row.value = "not used";

    // Keep the accessible description current without flooding screen readers every frame.
    juce::String summary;
    for (const auto& row : rows)
        summary << row.name << " " << row.value << ". ";
    if (summary != lastSummary)
    {
        lastSummary = summary;
        setDescription (summary);
    }

    repaint();
}

void StageMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (colour::surface);
    g.fillRoundedRectangle (area, cornerRadius);

    area.reduce ((float) space::m, (float) space::m);
    const auto rowHeight = area.getHeight() / (float) rows.size();
    const auto nameWidth = textWidth (font (type::label), "Stutter") + (float) space::m;
    const auto valueWidth = textWidth (font (type::note), "not used") + (float) space::s;

    for (const auto& row : rows)
    {
        auto r = area.removeFromTop (rowHeight);
        const auto text = row.available ? colour::textPrimary : colour::textSecondary;

        g.setFont (font (type::label));
        g.setColour (text);
        g.drawText (row.name, r.removeFromLeft (nameWidth), juce::Justification::centredLeft);

        g.setFont (font (type::note));
        g.setColour (colour::textSecondary);
        g.drawText (row.value, r.removeFromRight (valueWidth), juce::Justification::centredRight);

        const auto bar = r.withSizeKeepingCentre (r.getWidth() - (float) space::s, 6.0f);
        g.setColour (colour::separator);
        g.fillRoundedRectangle (bar, 3.0f);
        if (row.available && row.amount > 0.0f)
        {
            g.setColour (colour::accent);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * row.amount), 3.0f);
        }
    }
}

//==============================================================================
SettingsPanel::SettingsPanel (juce::AudioProcessorValueTreeState& state)
    : mixAttachment (state, paramIds::mix, mixSlider),
      outputAttachment (state, paramIds::output, outputSlider)
{
    for (auto [label, slider, name, help] : { std::tuple { &mixLabel, &mixSlider, "Mix", "Blend between the untouched voice and the riser." },
                                              std::tuple { &outputLabel, &outputSlider, "Output", "Level after the riser." } })
    {
        label->setText (name, juce::dontSendNotification);
        styleLabel (*label, type::label, colour::textPrimary);
        addAndMakeVisible (*label);

        slider->setSliderStyle (juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, controlHeight);
        slider->setTitle (name);
        slider->setTooltip (help);
        slider->setWantsKeyboardFocus (true);
        addAndMakeVisible (*slider);
    }
    setSize (320, space::m * 2 + controlHeight * 2 + space::s);
}

void SettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (colour::elevated);
}

void SettingsPanel::resized()
{
    auto area = getLocalBounds().reduced (space::m);
    const auto labelWidth = juce::roundToInt (textWidth (font (type::label), "Output")) + space::m;
    for (auto [label, slider] : { std::pair { &mixLabel, &mixSlider }, std::pair { &outputLabel, &outputSlider } })
    {
        auto row = area.removeFromTop (controlHeight);
        label->setBounds (row.removeFromLeft (labelWidth));
        slider->setBounds (row);
        area.removeFromTop (space::s);
    }
}

//==============================================================================
VoxLiftEditor::VoxLiftEditor (VoxLiftProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setText ("VoxLift", juce::dontSendNotification);
    styleLabel (titleLabel, type::title, colour::textPrimary, true);
    subtitleLabel.setText ("One-knob vocal riser", juce::dontSendNotification);
    styleLabel (subtitleLabel, type::label, colour::textSecondary);

    for (int i = 0; i < voxlift::numPresets; ++i)
        presetBox.addItem (juce::String (std::string (voxlift::getPreset (i).name)), i + 1);
    presetBox.setTitle ("Preset");
    presetBox.setTooltip ("Each preset is a different recipe of filter, echo, reverb, breath and stutter, tuned for vocals.");
    presetBox.onChange = [this] { updateDescription(); };

    settingsButton.setTitle ("Settings");
    settingsButton.setTooltip ("Mix and output level");
    settingsButton.onClick = [this] { showSettings(); };

    intensitySlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    intensitySlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    intensitySlider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    intensitySlider.setTitle ("Intensity");
    intensitySlider.setTooltip ("Turn up to build. Snap back to 0 for the drop. Above 100 % the voice fades to silence.");
    intensitySlider.setVelocityBasedMode (false);
    intensitySlider.setMouseDragSensitivity (300);

    styleLabel (descriptionLabel, type::body, colour::textSecondary);
    descriptionLabel.setJustificationType (juce::Justification::centredTop);
    styleLabel (statusLabel, type::note, colour::textSecondary);
    statusLabel.setText ("Automate Intensity or map it to a MIDI knob. Double-click the knob to return to 0 %.",
                         juce::dontSendNotification);

    for (auto* c : std::initializer_list<juce::Component*> { &titleLabel, &subtitleLabel, &presetBox, &settingsButton,
                                                             &intensitySlider, &descriptionLabel, &meter, &statusLabel })
        addAndMakeVisible (c);

    // Tab order follows reading order.
    presetBox.setExplicitFocusOrder (1);
    settingsButton.setExplicitFocusOrder (2);
    intensitySlider.setExplicitFocusOrder (3);
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &settingsButton, &intensitySlider })
        c->setWantsKeyboardFocus (true);

    auto& state = processor.getParameters();
    intensityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramIds::intensity, intensitySlider);
    presetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, paramIds::preset, presetBox);
    intensitySlider.setDoubleClickReturnValue (true, 0.0);

    updateDescription();
    timerCallback();

    setResizable (true, true);
    setResizeLimits (window::minWidth, window::minHeight, window::maxWidth, window::maxHeight);
    setSize (window::defaultWidth, window::defaultHeight);
    startTimerHz (meterRefreshHz);
}

VoxLiftEditor::~VoxLiftEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VoxLiftEditor::updateDescription()
{
    const auto index = juce::jmax (0, presetBox.getSelectedItemIndex());
    descriptionLabel.setText (juce::String (std::string (voxlift::getPreset (index).description)), juce::dontSendNotification);
}

void VoxLiftEditor::showSettings()
{
    auto panel = std::make_unique<SettingsPanel> (processor.getParameters());
    panel->setLookAndFeel (&lookAndFeel);
    auto& box = juce::CallOutBox::launchAsynchronously (std::move (panel), settingsButton.getBounds(), this);
    box.setLookAndFeel (&lookAndFeel);
}

void VoxLiftEditor::timerCallback()
{
    const auto preset = juce::jmax (0, presetBox.getSelectedItemIndex());
    meter.update (processor.getHeardIntensity(), preset);

    if (preset != lastPreset)
    {
        lastPreset = preset;
        updateDescription();
    }
}

void VoxLiftEditor::paint (juce::Graphics& g)
{
    g.fillAll (colour::canvas);
    g.setColour (colour::separator);
    g.drawHorizontalLine (space::m * 2 + controlHeight, 0.0f, (float) getWidth());
}

void VoxLiftEditor::resized()
{
    auto area = getLocalBounds().reduced (space::m);

    // Header: title on the left, preset and settings on the right.
    auto header = area.removeFromTop (controlHeight);
    const auto settingsWidth = juce::roundToInt (textWidth (font (type::body), settingsButton.getButtonText())) + space::xl;
    settingsButton.setBounds (header.removeFromRight (settingsWidth));
    header.removeFromRight (space::s);
    presetBox.setBounds (header.removeFromRight (juce::jmin (200, header.getWidth() / 2)));
    header.removeFromRight (space::s);
    const auto titleWidth = juce::roundToInt (textWidth (font (type::title, true), "VoxLift")) + space::s;
    titleLabel.setBounds (header.removeFromLeft (titleWidth));
    subtitleLabel.setBounds (header);

    area.removeFromTop (space::m * 2);

    // Footer
    statusLabel.setBounds (area.removeFromBottom (controlHeight));
    area.removeFromBottom (space::s);

    // Body: knob + preset description on the left, stage meter on the right.
    const auto knobColumn = area.removeFromLeft (juce::jmax (area.getWidth() / 2, 220));
    area.removeFromLeft (space::l);
    auto left = knobColumn;
    descriptionLabel.setBounds (left.removeFromBottom (controlHeight * 2));
    intensitySlider.setBounds (left);
    meter.setBounds (area);
}
