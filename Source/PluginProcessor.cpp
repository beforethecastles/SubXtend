#include "PluginProcessor.h"
#include "PluginEditor.h"

SubXtendProcessor::SubXtendProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    root      = apvts.getRawParameterValue ("root");
    intensity = apvts.getRawParameterValue ("intensity");
    sub       = apvts.getRawParameterValue ("sub");
    trim      = apvts.getRawParameterValue ("trim");
    lowBand   = apvts.getRawParameterValue ("lowband");
}

juce::AudioProcessorValueTreeState::ParameterLayout SubXtendProcessor::createLayout()
{
    using namespace juce;

    auto db = [] (float v, int) { return String (v, 1) + " dB"; };
    auto hz = [] (float v, int) { return String (juce::roundToInt (v)) + " Hz"; };
    auto subText = [] (float v, int) { return v <= subx::BassEngine::kSubOffDb + 0.01f ? String ("Off") : String (v, 1) + " dB"; };

    NormalisableRange<float> rootRange (32.0f, 256.0f, 1.0f);
    rootRange.setSkewForCentre (90.0f);   // log-ish: 32 → 90 → 256

    return {
        std::make_unique<AudioParameterFloat> (ParameterID { "root", 1 }, "Root", rootRange, 80.0f,
                                               AudioParameterFloatAttributes().withStringFromValueFunction (hz).withLabel ("Hz")),
        std::make_unique<AudioParameterFloat> (ParameterID { "intensity", 1 }, "Intensity",
                                               NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
                                               AudioParameterFloatAttributes().withStringFromValueFunction (db).withLabel ("dB")),
        std::make_unique<AudioParameterFloat> (ParameterID { "sub", 1 }, "Sub",
                                               NormalisableRange<float> (subx::BassEngine::kSubOffDb, 0.0f, 0.1f), 0.0f,
                                               AudioParameterFloatAttributes().withStringFromValueFunction (subText).withLabel ("dB")),
        std::make_unique<AudioParameterFloat> (ParameterID { "trim", 1 }, "Gain Trim",
                                               NormalisableRange<float> (-30.0f, 0.0f, 0.1f), 0.0f,
                                               AudioParameterFloatAttributes().withStringFromValueFunction (db).withLabel ("dB")),
        std::make_unique<AudioParameterBool> (ParameterID { "lowband", 1 }, "Low Band", true),
    };
}

bool SubXtendProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void SubXtendProcessor::prepareToPlay (double sampleRate, int)
{
    engine.setParams ({ root->load(), intensity->load(), sub->load(), trim->load(), lowBand->load() > 0.5f });
    engine.prepare (sampleRate, getTotalNumOutputChannels());
}

void SubXtendProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = getTotalNumInputChannels();
    for (int ch = numCh; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto peakOf = [&] {
        float pk = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            pk = juce::jmax (pk, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
        return pk;
    };

    inputPeak = juce::jmax (inputPeak.load(), peakOf());

    engine.setParams ({ root->load(), intensity->load(), sub->load(), trim->load(), lowBand->load() > 0.5f });
    engine.process (buffer.getArrayOfWritePointers(), numCh, buffer.getNumSamples());

    outputPeak = juce::jmax (outputPeak.load(), peakOf());
}

void SubXtendProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SubXtendProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SubXtendProcessor::createEditor() { return new SubXtendEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SubXtendProcessor(); }
