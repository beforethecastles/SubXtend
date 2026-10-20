#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace subx::ui
{
const juce::Colour bg       { 0xff15171c };
const juce::Colour panel    { 0xff1e2128 };
const juce::Colour track    { 0xff2c313b };
const juce::Colour accent   { 0xffff8a3d };
const juce::Colour accent2  { 0xff4fc3f7 };
const juce::Colour text     { 0xffe8e9ec };
const juce::Colour textDim  { 0xff8a8f9a };

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

// Knob with a title above and the parameter value below.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title,
          juce::Colour colour, const juce::String& tooltip);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::Slider slider;
    juce::Label value;
    juce::String title;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class Meter : public juce::Component
{
public:
    explicit Meter (juce::String name) : label (std::move (name)) {}
    void push (float linearPeak);
    void paint (juce::Graphics&) override;
    float getHoldDb() const { return holdDb; }
    void resetHold() { holdDb = -100.0f; }

private:
    juce::String label;
    float levelDb = -100.0f, holdDb = -100.0f;
};
} // namespace subx::ui

class SubXtendEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SubXtendEditor (SubXtendProcessor&);
    ~SubXtendEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    SubXtendProcessor& proc;
    subx::ui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    subx::ui::Knob root, intensity, sub, trim;
    juce::ToggleButton lowBand { "LOW BAND" };
    juce::AudioProcessorValueTreeState::ButtonAttachment lowBandAttachment;

    subx::ui::Meter inMeter { "IN" }, outMeter { "OUT" };
    juce::Rectangle<int> headroomArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubXtendEditor)
};
