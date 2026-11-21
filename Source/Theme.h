#pragma once

// Every visual value lives here (shared standard §4.1). Nothing else hard-codes colours or sizes.

#include <juce_graphics/juce_graphics.h>

#include <algorithm>
#include <cmath>

namespace theme
{
namespace colour
{
    inline const juce::Colour canvas         { 0xff17191e };
    inline const juce::Colour surface        { 0xff23262d };
    inline const juce::Colour elevated       { 0xff2e323b };
    inline const juce::Colour control        { 0xff2b2f37 };
    inline const juce::Colour controlHover   { 0xff353a44 };
    inline const juce::Colour controlPressed { 0xff3f4550 };
    inline const juce::Colour textPrimary    { 0xffe8eaed };
    inline const juce::Colour textSecondary  { 0xffa9afba };
    inline const juce::Colour textOnAccent   { 0xff0d1520 };
    inline const juce::Colour separator      { 0xff3a3f4a };
    inline const juce::Colour controlBorder  { 0xff6a7180 };
    inline const juce::Colour accent         { 0xff5aa9ff };
    inline const juce::Colour focus          { 0xff9cc8ff };
    inline const juce::Colour warning        { 0xffffcc80 };
} // namespace colour

namespace space
{
    constexpr int xs = 4, s = 8, m = 12, l = 16, xl = 24, xxl = 32;
}

constexpr int controlHeight = 28;
constexpr float cornerRadius = 6.0f;
constexpr float focusRing = 2.0f;

namespace type
{
    constexpr float body = 14.0f, label = 13.0f, note = 12.0f, title = 18.0f;
    constexpr float readout = 28.0f;  // percentage in the centre of the Intensity knob
}

namespace window
{
    constexpr int defaultWidth = 620, defaultHeight = 440;
    constexpr int minWidth = 480, minHeight = 400;
    constexpr int maxWidth = 1400, maxHeight = 1000;
}

constexpr int meterRefreshHz = 30;

inline juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

/** Measured text width (labels are sized from the font, never fixed). */
inline float textWidth (const juce::Font& f, const juce::String& text)
{
    return juce::GlyphArrangement::getStringWidth (f, text);
}

/** WCAG 2.2 contrast ratio between two opaque colours. */
inline double contrastRatio (juce::Colour a, juce::Colour b)
{
    auto luminance = [] (juce::Colour c)
    {
        auto channel = [] (float v)
        {
            return v <= 0.04045f ? v / 12.92 : std::pow ((v + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * channel (c.getFloatRed())
             + 0.7152 * channel (c.getFloatGreen())
             + 0.0722 * channel (c.getFloatBlue());
    };
    const auto la = luminance (a), lb = luminance (b);
    return (std::max (la, lb) + 0.05) / (std::min (la, lb) + 0.05);
}
} // namespace theme
