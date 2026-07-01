#pragma once
#include <JuceHeader.h>

namespace RawDub
{
class BWLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BWLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colours::white);

        setColour (juce::TextButton::buttonColourId,   juce::Colours::white);
        setColour (juce::TextButton::buttonOnColourId, juce::Colours::black);
        setColour (juce::TextButton::textColourOffId,  juce::Colours::black);
        setColour (juce::TextButton::textColourOnId,   juce::Colours::white);

        setColour (juce::Slider::backgroundColourId,       juce::Colours::lightgrey);
        setColour (juce::Slider::trackColourId,             juce::Colours::black);
        setColour (juce::Slider::thumbColourId,              juce::Colours::black);
        setColour (juce::Slider::textBoxTextColourId,       juce::Colours::black);
        setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::black);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::white);

        setColour (juce::Label::textColourId, juce::Colours::black);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                bool /*shouldDrawButtonAsHighlighted*/, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        g.setColour (backgroundColour);
        g.fillRect (bounds);
        g.setColour (juce::Colours::black);
        g.drawRect (bounds, shouldDrawButtonAsDown ? 2.0f : 1.0f);
    }
};
}