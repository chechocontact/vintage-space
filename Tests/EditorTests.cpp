// Off-screen tests for the real processor and editor (no DAW, no window on screen).

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Theme.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <cstdio>

namespace
{
int failures = 0;
int checks = 0;

#define CHECK(condition)                                                              \
    do                                                                                \
    {                                                                                 \
        ++checks;                                                                     \
        if (! (condition))                                                            \
        {                                                                             \
            ++failures;                                                               \
            std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);         \
        }                                                                             \
    } while (false)

void setParam (VoxLiftProcessor& p, const char* id, float plainValue)
{
    auto* param = p.getParameters().getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
}

float getParam (VoxLiftProcessor& p, const char* id)
{
    return p.getParameters().getRawParameterValue (id)->load();
}

void testContrast()
{
    using namespace theme::colour;
    for (auto background : { canvas, surface, elevated, control, controlHover })
        CHECK (theme::contrastRatio (textPrimary, background) >= 4.5);
    for (auto background : { canvas, surface, elevated, control })
        CHECK (theme::contrastRatio (textSecondary, background) >= 4.5);
    CHECK (theme::contrastRatio (textOnAccent, accent) >= 4.5);
    for (auto component : { controlBorder, accent, focus, warning })
        CHECK (theme::contrastRatio (component, surface) >= 3.0);
    CHECK (theme::contrastRatio (accent, separator) >= 3.0);  // value arc against its track
}

void testEditorAccessibilityAndLayout()
{
    VoxLiftProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> base (processor.createEditor());
    auto& editor = dynamic_cast<VoxLiftEditor&> (*base);

    std::vector<juce::Component*> interactive { &editor.getPresetBox(), &editor.getSettingsButton(),
                                                &editor.getIntensitySlider() };
    for (auto* c : interactive)
    {
        CHECK (c->getTitle().isNotEmpty());
        CHECK (c->getWantsKeyboardFocus());
        auto* tooltip = dynamic_cast<juce::SettableTooltipClient*> (c);
        CHECK (tooltip != nullptr && tooltip->getTooltip().isNotEmpty());
    }
    CHECK (editor.getStageMeter().getTitle().isNotEmpty());

    for (auto [w, h] : { std::pair { theme::window::minWidth, theme::window::minHeight },
                         std::pair { theme::window::defaultWidth, theme::window::defaultHeight },
                         std::pair { theme::window::maxWidth, theme::window::maxHeight } })
    {
        editor.setSize (w, h);
        const auto bounds = editor.getLocalBounds();
        std::vector<juce::Component*> all { &editor.getPresetBox(), &editor.getSettingsButton(),
                                            &editor.getIntensitySlider(), &editor.getStageMeter(),
                                            &editor.getStatusLabel() };
        for (auto* c : all)
        {
            CHECK (bounds.contains (c->getBounds()));
            CHECK (c->getWidth() > 0 && c->getHeight() > 0);
        }
        for (size_t i = 0; i < all.size(); ++i)
            for (size_t j = i + 1; j < all.size(); ++j)
                CHECK (! all[i]->getBounds().intersects (all[j]->getBounds()));

        // The knob stays a usable size even at the minimum window size.
        const auto knob = editor.getIntensitySlider().getBounds();
        CHECK (juce::jmin (knob.getWidth(), knob.getHeight()) >= 160);
    }

    // The preset box offers every preset.
    CHECK (editor.getPresetBox().getNumItems() == voxlift::numPresets);

    // Moving the parameter moves the knob (attachment works both ways).
    setParam (processor, paramIds::intensity, 75.0f);
    // On the message thread the attachment updates synchronously.
    CHECK (std::abs (editor.getIntensitySlider().getValue() - 75.0) < 0.05);

    // The meter reflects the preset: Stutter Build uses the stutter, Vocal Lift does not.
    auto& meter = editor.getStageMeter();
    meter.update (1.0f, 3);
    CHECK (meter.getRows()[5].available && meter.getRows()[5].value == "1/32");
    meter.update (1.0f, 0);
    CHECK (! meter.getRows()[5].available);
    CHECK (meter.getDescription().contains ("Filter"));

    // Painting at every size must not throw or assert.
    juce::Image image (juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true);
    juce::Graphics g (image);
    editor.paintEntireComponent (g, true);
}

void testStateRoundTrip()
{
    VoxLiftProcessor a;
    setParam (a, paramIds::intensity, 55.0f);
    setParam (a, paramIds::preset, 3.0f);
    setParam (a, paramIds::mix, 80.0f);
    setParam (a, paramIds::output, -3.0f);

    juce::MemoryBlock state;
    a.getStateInformation (state);

    VoxLiftProcessor b;
    b.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (std::abs (getParam (b, paramIds::intensity) - 55.0f) < 0.05f);
    CHECK (std::abs (getParam (b, paramIds::preset) - 3.0f) < 0.01f);
    CHECK (std::abs (getParam (b, paramIds::mix) - 80.0f) < 0.05f);
    CHECK (std::abs (getParam (b, paramIds::output) + 3.0f) < 0.05f);

    // A state from a future version is refused rather than half-loaded.
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    xml->setAttribute ("version", VoxLiftProcessor::stateVersion + 1);
    juce::MemoryBlock future;
    juce::AudioProcessor::copyXmlToBinary (*xml, future);
    VoxLiftProcessor c;
    c.setStateInformation (future.getData(), (int) future.getSize());
    CHECK (juce::exactlyEqual (getParam (c, paramIds::intensity), 0.0f));
}

void testProcessorPassThroughAtZero()
{
    VoxLiftProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512), original (2, 512);
    juce::Random random (42);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            original.setSample (ch, i, random.nextFloat() * 0.5f - 0.25f);

    juce::MidiBuffer midi;
    for (int block = 0; block < 20; ++block)
    {
        buffer.makeCopyOf (original);
        p.processBlock (buffer, midi);
    }

    bool exact = true;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            exact = exact && juce::exactlyEqual (buffer.getSample (ch, i), original.getSample (ch, i));
    CHECK (exact);

    // At 110 % the output is silent once the knob has settled.
    setParam (p, paramIds::intensity, 110.0f);
    for (int block = 0; block < 40; ++block)
    {
        buffer.makeCopyOf (original);
        p.processBlock (buffer, midi);
    }
    CHECK (buffer.getMagnitude (0, 512) < 1.0e-5f);
}
/** `EditorTests --snapshot file.png` renders the editor mid-build for the docs. */
void writeSnapshot (const juce::File& file)
{
    VoxLiftProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    setParam (p, paramIds::preset, 4.0f);
    setParam (p, paramIds::intensity, 65.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    for (int block = 0; block < 50; ++block)
    {
        buffer.clear();
        p.processBlock (buffer, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    auto& e = dynamic_cast<VoxLiftEditor&> (*editor);
    e.getStageMeter().update (p.getHeardIntensity(), 4);

    juce::Image image (juce::Image::ARGB, e.getWidth() * 2, e.getHeight() * 2, true);
    juce::Graphics g (image);
    g.addTransform (juce::AffineTransform::scale (2.0f));
    e.paintEntireComponent (g, true);

    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat().writeImageToStream (image, out);
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;

    if (argc == 3 && juce::String (argv[1]) == "--snapshot")
    {
        writeSnapshot (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        return 0;
    }

    testContrast();
    testEditorAccessibilityAndLayout();
    testStateRoundTrip();
    testProcessorPassThroughAtZero();

    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
