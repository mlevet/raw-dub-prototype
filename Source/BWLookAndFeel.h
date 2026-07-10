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

    // Same flat, no-gradient language as the step buttons ("trigs") and
    // drawButtonBackground above - a solid black rectangle on a white
    // track, no rounded ends, no shading. thumbSize == 0 means "content
    // fits, nothing to scroll" (Viewport's own state) - drawn as nothing
    // but the track, same as an empty/inactive control elsewhere in this
    // app rather than a thumb squeezed to some minimum size.
    void drawScrollbar (juce::Graphics& g, juce::ScrollBar& /*scrollbar*/, int x, int y, int width, int height,
                         bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                         bool /*isMouseOver*/, bool /*isMouseDown*/) override
    {
        juce::Rectangle<int> track (x, y, width, height);
        g.setColour (juce::Colours::white);
        g.fillRect (track);

        if (thumbSize <= 0)
            return;

        auto thumb = isScrollbarVertical
            ? juce::Rectangle<int> (x, thumbStartPosition, width, thumbSize)
            : juce::Rectangle<int> (thumbStartPosition, y, thumbSize, height);
        g.setColour (juce::Colours::black);
        g.fillRect (thumb);
    }
};
}