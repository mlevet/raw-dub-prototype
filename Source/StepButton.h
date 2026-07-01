#pragma once
#include <JuceHeader.h>
#include "StepLevel.h"
#include <functional>

namespace RawDub
{
// A step-sequencer cell: click to toggle on/off, horizontal drag to
// cycle Ghost/Normal/Accent, and - if hasPitch is set - vertical drag
// to transpose +/-12 semitones. Pitch and level both live on the step
// itself, not in a separate editor - keeps this a pattern, not a
// piano roll.
class StepButton : public juce::Component
{
public:
    std::function<void()> onToggle;                // click without drag: flip on/off
    std::function<void (int)> onPitchDrag;          // vertical drag while on (pitch-capable steps only)
    std::function<void (StepLevel)> onLevelDrag;    // horizontal drag while on

    void setHasPitch (bool shouldHavePitch) { hasPitch = shouldHavePitch; }
    void setOn (bool shouldBeOn);
    void setSemitoneOffset (int newOffset);
    void setLevel (StepLevel newLevel);
    void setPlayhead (bool isCurrentStep);
    void setAccentStyle (int style); // prototype-only: 0/1/2, for comparing visual treatments

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void paintAccentMarker (juce::Graphics&, juce::Rectangle<float> bar);

    bool hasPitch = false;
    bool on = false;
    bool playhead = false;
    int semitoneOffset = 0;
    StepLevel level = StepLevel::Normal;
    int accentStyle = 0;

    int dragStartX = 0, dragStartY = 0;
    int dragStartOffset = 0;
    int dragStartLevelIndex = 1;
    bool dragging = false;
    bool draggingVertical = false;

    static constexpr int dragThresholdPx = 4;
    static constexpr int pixelsPerSemitone = 6;
    static constexpr int pixelsPerLevel = 30;
};
}
