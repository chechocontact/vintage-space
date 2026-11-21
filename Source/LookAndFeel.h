#pragma once

// The single LookAndFeel for every control (shared standard §4.1). Depends only on Theme.h.

#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

class VoxLiftLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VoxLiftLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& background,
                               bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics&, const juce::Path&, juce::Image&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override       { return theme::font (theme::type::body); }
    juce::Font getPopupMenuFont() override                      { return theme::font (theme::type::body); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return theme::font (theme::type::body); }

    /** Focus ring shared by every control: 2 px, focus colour. */
    static void drawFocusRing (juce::Graphics&, juce::Rectangle<float> bounds, float radius);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxLiftLookAndFeel)
};
