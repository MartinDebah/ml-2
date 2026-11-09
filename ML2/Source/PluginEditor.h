#pragma once

#include "PluginProcessor.h"

//==============================================================================
// Та сама кремово-червона гама серії ML, але інші тони: панель сіріша, червоний глибший.
namespace Pal
{
    const juce::Colour page    { 0xff1d1d1a }, panel   { 0xffd3cfc1 }, ink     { 0xff1a1a17 },
                       muted   { 0xff5a574d }, line    { 0xffa9a38e }, accent  { 0xffa8221c },
                       onAcc   { 0xfffff5ee }, sig     { 0xffff4530 }, well    { 0xff151613 },
                       cap     { 0xff121212 }, ptr     { 0xfff0ede4 }, hot     { 0xffff5a40 },
                       wave    { 0xff6b675c }, winInk  { 0xffefe7d4 }, winDim  { 0xff8c8676 },
                       winLine { 0xff34332d }, pad     { 0xff2e2f2b }, padInk  { 0xffebe5d6 },
                       padEdge { 0xff11120f };
}

class GlitchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GlitchLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool hover, bool down) override;

    static void drawPad (juce::Graphics&, juce::Rectangle<float>, juce::Colour base, bool pressed);
    static void drawLed (juce::Graphics&, juce::Rectangle<float>, bool on, juce::Colour onColour);
};

//==============================================================================
// Великий пад ручного зриву
class GlitchPad : public juce::Button
{
public:
    GlitchPad (const juce::String& glyph, const juce::String& label) : juce::Button (label), glyph (glyph) {}
    void paintButton (juce::Graphics&, bool hover, bool down) override;
    float flash = 0.0f;
private:
    juce::String glyph;
};

//==============================================================================
class GlitchEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GlitchEditor (GlitchProcessor&);
    ~GlitchEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawSlicer (juce::Graphics&, juce::Rectangle<float>);
    void makeWearTexture();

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    GlitchProcessor& proc;
    GlitchLookAndFeel lnf;

    std::array<Knob, 7> knobs;   // chance len | size ramp pitch | crush vol
    juce::ToggleButton lockBtn;
    std::array<juce::ToggleButton, GlitchProcessor::NumFx> fxBtns;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, GlitchProcessor::NumFx> fxA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lockA;
    std::vector<std::unique_ptr<GlitchPad>> pads;

    juce::Rectangle<int> titleArea, slicerArea, padLabelArea;
    std::array<juce::Rectangle<int>, 3> sections;
    juce::Image wear;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchEditor)
};
