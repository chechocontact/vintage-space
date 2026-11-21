#include "LookAndFeel.h"

using namespace theme;

VoxLiftLookAndFeel::VoxLiftLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colour::canvas);
    setColour (juce::Label::textColourId, colour::textPrimary);
    setColour (juce::Slider::textBoxTextColourId, colour::textPrimary);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, colour::control);
    setColour (juce::TextButton::buttonColourId, colour::control);
    setColour (juce::TextButton::buttonOnColourId, colour::accent);
    setColour (juce::TextButton::textColourOffId, colour::textPrimary);
    setColour (juce::TextButton::textColourOnId, colour::textOnAccent);
    setColour (juce::ComboBox::backgroundColourId, colour::control);
    setColour (juce::ComboBox::textColourId, colour::textPrimary);
    setColour (juce::ComboBox::outlineColourId, colour::controlBorder);
    setColour (juce::ComboBox::arrowColourId, colour::textSecondary);
    setColour (juce::ComboBox::focusedOutlineColourId, colour::focus);
    setColour (juce::PopupMenu::backgroundColourId, colour::elevated);
    setColour (juce::PopupMenu::textColourId, colour::textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colour::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, colour::textOnAccent);
    setColour (juce::TooltipWindow::backgroundColourId, colour::elevated);
    setColour (juce::TooltipWindow::textColourId, colour::textPrimary);
    setColour (juce::TooltipWindow::outlineColourId, colour::separator);
}

void VoxLiftLookAndFeel::drawFocusRing (juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
{
    g.setColour (colour::focus);
    g.drawRoundedRectangle (bounds.reduced (focusRing * 0.5f), radius, focusRing);
}

void VoxLiftLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float startAngle, float endAngle,
                                           juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced ((float) space::s);
    const auto diameter = std::min (bounds.getWidth(), bounds.getHeight());
    const auto area = bounds.withSizeKeepingCentre (diameter, diameter);
    const auto centre = area.getCentre();
    const auto trackWidth = std::max (6.0f, diameter * 0.06f);
    const auto radius = diameter * 0.5f - trackWidth;
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);
    const auto enabled = slider.isEnabled();

    // Body
    g.setColour (slider.isMouseButtonDown() ? colour::controlPressed
                 : slider.isMouseOverOrDragging() ? colour::controlHover : colour::control);
    g.fillEllipse (area.reduced (trackWidth * 1.8f));
    g.setColour (colour::controlBorder);
    g.drawEllipse (area.reduced (trackWidth * 1.8f), 1.0f);

    // Track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (colour::separator);
    g.strokePath (track, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc (100..110 % drawn in the warning colour: the silent pre-drop zone)
    const auto range = slider.getRange();
    const auto hundred = (float) slider.valueToProportionOfLength (std::min (100.0, range.getEnd()));
    const auto hundredAngle = startAngle + hundred * (endAngle - startAngle);

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, std::min (angle, hundredAngle), true);
    g.setColour (enabled ? colour::accent : colour::controlBorder);
    g.strokePath (value, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (angle > hundredAngle)
    {
        juce::Path over;
        over.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, hundredAngle, angle, true);
        g.setColour (colour::warning);
        g.strokePath (over, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Pointer
    const auto inner = radius - trackWidth * 2.2f;
    const auto tip = centre.getPointOnCircumference (inner, angle);
    const auto base = centre.getPointOnCircumference (inner * 0.55f, angle);
    g.setColour (colour::textPrimary);
    g.drawLine ({ base, tip }, std::max (3.0f, trackWidth * 0.5f));

    // Readout
    g.setFont (font (std::max (type::body, diameter * 0.11f), true));
    g.setColour (colour::textPrimary);
    g.drawFittedText (slider.getTextFromValue (slider.getValue()),
                      area.withSizeKeepingCentre (diameter * 0.5f, diameter * 0.16f).toNearestInt()
                          .translated (0, (int) (diameter * 0.2f)),
                      juce::Justification::centred, 1);

    if (slider.hasKeyboardFocus (true))
        drawFocusRing (g, area, diameter * 0.5f);
}

void VoxLiftLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float, float, juce::Slider::SliderStyle,
                                           juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto track = bounds.withSizeKeepingCentre (bounds.getWidth(), 6.0f);
    g.setColour (colour::separator);
    g.fillRoundedRectangle (track, 3.0f);
    g.setColour (slider.isEnabled() ? colour::accent : colour::controlBorder);
    g.fillRoundedRectangle (track.withRight (sliderPos), 3.0f);

    const auto thumb = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ sliderPos, bounds.getCentreY() });
    g.setColour (slider.isMouseButtonDown() ? colour::controlPressed : colour::textPrimary);
    g.fillEllipse (thumb);

    if (slider.hasKeyboardFocus (true))
        drawFocusRing (g, bounds, cornerRadius);
}

void VoxLiftLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                               const juce::Colour&, bool highlighted, bool down)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto fill = button.getToggleState() ? colour::accent
              : down ? colour::controlPressed
              : highlighted ? colour::controlHover : colour::control;
    if (! button.isEnabled())
        fill = fill.withMultipliedAlpha (0.5f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, cornerRadius);
    g.setColour (colour::controlBorder);
    g.drawRoundedRectangle (bounds, cornerRadius, 1.0f);

    if (button.hasKeyboardFocus (true))
        drawFocusRing (g, bounds, cornerRadius);
}

void VoxLiftLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                       int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    g.setColour (isButtonDown ? colour::controlPressed
                 : box.isMouseOver (true) ? colour::controlHover : colour::control);
    g.fillRoundedRectangle (bounds, cornerRadius);
    g.setColour (colour::controlBorder);
    g.drawRoundedRectangle (bounds, cornerRadius, 1.0f);

    juce::Path arrow;
    const auto cx = bounds.getRight() - (float) space::l;
    const auto cy = bounds.getCentreY();
    arrow.addTriangle (cx - 5.0f, cy - 2.5f, cx + 5.0f, cy - 2.5f, cx, cy + 3.5f);
    g.setColour (colour::textSecondary);
    g.fillPath (arrow);

    if (box.hasKeyboardFocus (true))
        drawFocusRing (g, bounds, cornerRadius);
}

void VoxLiftLookAndFeel::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g,
                                                   const juce::Path& path, juce::Image&)
{
    g.setColour (colour::elevated);
    g.fillPath (path);
    g.setColour (colour::separator);
    g.strokePath (path, juce::PathStrokeType (1.0f));
}
