#include "PluginProcessor.h"
#include "PluginEditor.h"

VoxLiftProcessor::VoxLiftProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "PARAMS", createLayout())
{
    intensityParam = params.getRawParameterValue (paramIds::intensity);
    presetParam = params.getRawParameterValue (paramIds::preset);
    mixParam = params.getRawParameterValue (paramIds::mix);
    outputParam = params.getRawParameterValue (paramIds::output);
}

juce::AudioProcessorValueTreeState::ParameterLayout VoxLiftProcessor::createLayout()
{
    juce::StringArray presetNames;
    for (int i = 0; i < voxlift::numPresets; ++i)
        presetNames.add (juce::String (std::string (voxlift::getPreset (i).name)));

    auto percent = juce::AudioParameterFloatAttributes()
                       .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " %"; })
                       .withLabel ("%");

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { paramIds::intensity, 1 }, "Intensity",
        juce::NormalisableRange<float> (0.0f, voxlift::maxIntensity * 100.0f, 0.1f), 0.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { paramIds::preset, 1 }, "Preset", presetNames, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { paramIds::mix, 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { paramIds::output, 1 }, "Output",
        juce::NormalisableRange<float> (-12.0f, 6.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; })
            .withLabel ("dB")));
    return layout;
}

bool VoxLiftProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo out; mono or stereo in, because vocal tracks are often mono.
    const auto in = layouts.getMainInputChannelSet();
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

voxlift::Controls VoxLiftProcessor::readControls() const noexcept
{
    voxlift::Controls c;
    c.intensity = intensityParam->load() / 100.0f;
    c.preset = static_cast<int> (presetParam->load());
    c.mix = mixParam->load() / 100.0f;
    c.outputDb = outputParam->load();
    return c;
}

void VoxLiftProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate, readControls());  // allocates; never concurrent with processBlock
}

void VoxLiftProcessor::reset()
{
    engine.reset (readControls());
}

void VoxLiftProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();

    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    auto controls = readControls();

    if (auto* host = getPlayHead())
    {
        if (const auto position = host->getPosition())
        {
            controls.bpm = position->getBpm().orFallback (120.0);
            if (position->getIsPlaying())
                if (const auto ppq = position->getPpqPosition())
                    controls.ppqAtBlockStart = *ppq;
        }
    }

    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples, controls);
    heardIntensity.store (engine.getCurrentIntensity(), std::memory_order_relaxed);
}

void VoxLiftProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root (pluginName);
    root.setAttribute ("version", stateVersion);
    if (auto xml = params.copyState().createXml())
        root.addChildElement (xml.release());
    copyXmlToBinary (root, destData);
}

void VoxLiftProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto root = getXmlFromBinary (data, sizeInBytes);
    if (root == nullptr || ! root->hasTagName (pluginName))
        return;

    // Accept this and older versions; refuse states written by a future version.
    if (root->getIntAttribute ("version", 1) > stateVersion)
        return;

    if (auto* state = root->getChildByName (params.state.getType()))
        params.replaceState (juce::ValueTree::fromXml (*state));  // APVTS handles thread hand-off
}

juce::AudioProcessorEditor* VoxLiftProcessor::createEditor()
{
    return new VoxLiftEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoxLiftProcessor();
}
