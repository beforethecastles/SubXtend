#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BassEngine.h"

class SubXtendProcessor : public juce::AudioProcessor
{
public:
    SubXtendProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Peak meters, read by the editor (linear gain, decays in the UI).
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };

private:
    subx::BassEngine engine;
    std::atomic<float>* root = nullptr;
    std::atomic<float>* intensity = nullptr;
    std::atomic<float>* sub = nullptr;
    std::atomic<float>* trim = nullptr;
    std::atomic<float>* lowBand = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubXtendProcessor)
};
