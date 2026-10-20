#include "PluginEditor.h"

using namespace subx::ui;

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::TooltipWindow::backgroundColourId, panel.brighter (0.1f));
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, track);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float thickness = radius * 0.14f;
    const float arcRadius = radius - thickness * 0.5f;
    const auto colour = s.findColour (juce::Slider::rotarySliderFillColourId);

    // Bipolar knobs (e.g. Intensity, range -x..+x) fill from 12 o'clock.
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float fromAngle = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    const float toAngle = startAngle + pos * (endAngle - startAngle);

    juce::Path bgArc, valueArc;
    bgArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                            juce::jmin (fromAngle, toAngle), juce::jmax (fromAngle, toAngle), true);

    const juce::PathStrokeType stroke (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (track);
    g.strokePath (bgArc, stroke);
    g.setColour (s.isEnabled() ? colour : colour.withSaturation (0.0f));
    g.strokePath (valueArc, stroke);

    // Knob body
    const float bodyR = arcRadius - thickness * 1.3f;
    g.setGradientFill (juce::ColourGradient (panel.brighter (0.25f), centre.x, centre.y - bodyR,
                                             panel.darker (0.4f), centre.x, centre.y + bodyR, false));
    g.fillEllipse (juce::Rectangle<float> (bodyR * 2, bodyR * 2).withCentre (centre));
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (juce::Rectangle<float> (bodyR * 2, bodyR * 2).withCentre (centre), 1.0f);

    // Pointer
    const auto tip = centre.getPointOnCircumference (bodyR * 0.85f, toAngle);
    const auto base = centre.getPointOnCircumference (bodyR * 0.35f, toAngle);
    g.setColour (text);
    g.drawLine ({ base, tip }, juce::jmax (2.0f, radius * 0.05f));
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();

    g.setColour (on ? accent2.withAlpha (0.18f) : panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? accent2 : (highlighted ? textDim : track));
    g.drawRoundedRectangle (r, 6.0f, 1.5f);

    auto led = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.36f);
    g.setColour (on ? accent2 : track);
    g.fillEllipse (led);

    g.setColour (on ? text : textDim);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (b.getButtonText() + (on ? "  IN" : "  OUT"), r.withTrimmedRight (r.getHeight() * 0.4f),
                juce::Justification::centred);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& t,
            juce::Colour colour, const juce::String& tooltip)
    : title (t), attachment (state, paramId, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    slider.setTooltip (tooltip);
    slider.setDoubleClickReturnValue (true, (double) state.getParameterRange (paramId)
                                                .convertFrom0to1 (state.getParameter (paramId)->getDefaultValue()));
    slider.setMouseDragSensitivity (300);
    addAndMakeVisible (slider);

    auto* param = state.getParameter (paramId);
    value.setJustificationType (juce::Justification::centred);
    value.setColour (juce::Label::textColourId, text);
    value.setFont (juce::FontOptions (14.0f));
    value.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (value);

    auto refresh = [this, param] { value.setText (param->getCurrentValueAsText(), juce::dontSendNotification); };
    slider.onValueChange = refresh;
    refresh();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (22);
    value.setBounds (r.removeFromBottom (22));
    slider.setBounds (r);
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (textDim);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (20), juce::Justification::centred);
}

//==============================================================================
void Meter::push (float linearPeak)
{
    const float db = juce::Decibels::gainToDecibels (linearPeak, -100.0f);
    levelDb = db > levelDb ? db : juce::jmax (-100.0f, levelDb - 1.2f);   // ~36 dB/s fall at 30 fps
    holdDb = juce::jmax (holdDb, db);
}

void Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto labelArea = r.removeFromBottom (18.0f);
    auto bar = r.reduced (4.0f, 2.0f);

    g.setColour (track);
    g.fillRoundedRectangle (bar, 3.0f);

    const float minDb = -48.0f;
    auto norm = [&] (float d) { return juce::jlimit (0.0f, 1.0f, (d - minDb) / -minDb); };

    auto fill = bar.withTop (bar.getBottom() - bar.getHeight() * norm (levelDb));
    juce::ColourGradient grad (accent2, 0, bar.getBottom(), juce::Colours::red, 0, bar.getY(), false);
    grad.addColour (0.75, accent);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (fill, 3.0f);

    if (holdDb > minDb)
    {
        const float hy = bar.getBottom() - bar.getHeight() * norm (holdDb);
        g.setColour (holdDb > -0.1f ? juce::Colours::red : text);
        g.fillRect (bar.getX(), hy - 1.0f, bar.getWidth(), 2.0f);
    }

    g.setColour (textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (label, labelArea, juce::Justification::centred);
}

//==============================================================================
SubXtendEditor::SubXtendEditor (SubXtendProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      root (p.apvts, "root", "Root", accent2,
            "Crossover frequency. Harmonics are generated from everything below it."),
      intensity (p.apvts, "intensity", "Intensity", accent,
                 "Level of the generated harmonics. 0 dB is loudness-matched to the original low band."),
      sub (p.apvts, "sub", "Sub", accent2.withSaturation (0.4f),
           "Level of the original low band. Turn it down to free up headroom and let the harmonics carry the bass."),
      trim (p.apvts, "trim", "Gain Trim", juce::Colour (0xffb0b4bd),
            "Output trim for level matching."),
      lowBandAttachment (p.apvts, "lowband", lowBand)
{
    setLookAndFeel (&lnf);

    for (auto* c : { &root, &intensity, &sub, &trim })
        addAndMakeVisible (*c);

    lowBand.setTooltip ("Turn OUT to hear only the generated harmonics.");
    addAndMakeVisible (lowBand);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    setSize (640, 360);
    startTimerHz (30);
}

SubXtendEditor::~SubXtendEditor() { setLookAndFeel (nullptr); }

void SubXtendEditor::timerCallback()
{
    inMeter.push (proc.inputPeak.exchange (0.0f));
    outMeter.push (proc.outputPeak.exchange (0.0f));
    inMeter.repaint();
    outMeter.repaint();
    repaint (headroomArea);
}

void SubXtendEditor::mouseDown (const juce::MouseEvent& e)
{
    if (headroomArea.contains (e.getPosition()))
    {
        inMeter.resetHold();
        outMeter.resetHold();
    }
}

void SubXtendEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    auto header = getLocalBounds().removeFromTop (54).reduced (20, 0);
    g.setColour (text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("SubXtend", header, juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("PSYCHOACOUSTIC BASS EXTENSION", header, juce::Justification::centredRight);

    g.setColour (panel);
    g.fillRoundedRectangle (getLocalBounds().withTrimmedTop (54).reduced (12).toFloat(), 10.0f);

    // Headroom readout: peak-hold difference between input and output
    const float in = inMeter.getHoldDb(), out = outMeter.getHoldDb();
    juce::String txt = "--";
    if (in > -90.0f && out > -90.0f)
    {
        const float diff = in - out;
        txt = (diff >= 0 ? "+" : "") + juce::String (diff, 1) + " dB";
    }
    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("HEADROOM", headroomArea.withHeight (14), juce::Justification::centred);
    g.setColour (in - out >= 0.0f ? accent : text);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (txt, headroomArea.withTrimmedTop (14), juce::Justification::centred);
}

void SubXtendEditor::resized()
{
    auto r = getLocalBounds().withTrimmedTop (54).reduced (24);

    auto meters = r.removeFromRight (86);
    headroomArea = meters.removeFromBottom (36);
    meters.removeFromBottom (6);
    inMeter.setBounds (meters.removeFromLeft (43));
    outMeter.setBounds (meters);

    r.removeFromRight (16);
    auto bottom = r.removeFromBottom (34);
    lowBand.setBounds (bottom.withSizeKeepingCentre (170, 30));
    r.removeFromBottom (10);

    // Root & Intensity are the main controls — make them bigger.
    const int unit = r.getWidth() / 7;
    root.setBounds (r.removeFromLeft (unit * 2));
    intensity.setBounds (r.removeFromLeft (unit * 2));
    auto small = r.withTrimmedTop (r.getHeight() / 5);
    sub.setBounds (small.removeFromLeft (small.getWidth() / 2));
    trim.setBounds (small);
}
