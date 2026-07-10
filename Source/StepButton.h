#pragma once
#include <JuceHeader.h>
#include "StepLevel.h"
#include <functional>
#include <vector>

namespace RawDub
{
// A step-sequencer cell: click to toggle on/off, horizontal drag to
// cycle Ghost/Normal/Accent, and - if hasPitch is set - vertical drag
// to transpose pitch. Pitch and level both live on the step itself,
// not in a separate editor - keeps this a pattern, not a piano roll.
//
// The underlying pitch range (StepPattern::maxSemitoneOffset, +/-36) is
// wider than what's visibly drawn at once - the bar only ever renders a
// fixed-size (viewportMax-viewportMin) window onto it, set via
// setPitchViewport. This lets a drag reach a much lower/higher register
// (real dub basslines drop suddenly, several octaves down) via one
// continuous gesture: dragging past the current window's edge is
// reported through onPitchDrag same as any other move, and the owner
// (MainComponent) decides whether/how to scroll the window in response
// - StepButton itself has no opinion about scrolling, it just always
// draws whatever window it's told to. See
// project_raw_dub_song_architecture memory: dynamic window GROWTH was
// considered and rejected (it would make the same interval draw a
// different height depending on what else is in the pattern) in favour
// of a fixed-size window that only slides.
class StepButton : public juce::Component
{
public:
    std::function<void()> onToggle;                // click without drag: flip on/off
    // vertical drag while on (pitch-capable steps only). offset is the
    // usual absolute semitone value. edgeZoneDirection is 0 normally;
    // +1/-1 while the cursor is held within a small zone near the
    // top/bottom of the step (see edgeZonePx) - StepButton stops
    // computing offset from raw pixel delta while in the zone (the
    // owner's timer drives continued scrolling instead, since holding
    // still shouldn't stop the scroll); it resumes normal drag-by-pixel
    // behaviour, resynced from wherever things ended up, once the
    // cursor leaves the zone. Also fired with edgeZoneDirection=0 once
    // on mouseUp, purely to tell the owner to stop scrolling.
    std::function<void (int offset, int edgeZoneDirection)> onPitchDrag;
    std::function<void()> onPitchDragEnd;           // fired once on mouseUp, only if a vertical pitch drag was in progress
    std::function<void (StepLevel)> onLevelDrag;    // horizontal drag while on
    std::function<void (float)> onPitchWheel;       // mouse wheel/trackpad over a pitch-capable step
    std::function<void (bool)> onHoverChanged;      // true = cursor entered, false = exited (pitch-capable steps only)

    void setHasPitch (bool shouldHavePitch) { hasPitch = shouldHavePitch; }
    void setOn (bool shouldBeOn);
    void setSemitoneOffset (int newOffset);
    void setLevel (StepLevel newLevel);
    void setPlayhead (bool isCurrentStep);
    void setPitchViewport (int newViewportMin, int newViewportMax);
    // one entry per DISTINCT pitch currently used by an active step
    // anywhere in the pattern (not one per step) - draws a thin
    // reference line at each, so repeated pitches/intervals are visible
    // by eye even on steps that aren't themselves at that pitch. Same
    // list handed to every step in the row, so the lines align across
    // the whole width. Only pitches within the current viewport draw
    // anything - this is deliberately not a full semitone grid.
    void setGuidePitches (const std::vector<int>& pitches);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void paintAccentMarker (juce::Graphics&, juce::Rectangle<float> bar); // corner notch - the only style, chosen after comparing three

    bool hasPitch = false;
    bool on = false;
    bool playhead = false;
    int semitoneOffset = 0;
    StepLevel level = StepLevel::Normal;

    std::vector<int> guidePitches;

    // default matches the pre-viewport +/-12 behaviour, in case a
    // pitch-capable step is ever used before its owner calls
    // setPitchViewport explicitly
    int viewportMin = -12;
    int viewportMax = 12;

    int dragStartX = 0, dragStartY = 0;
    int dragStartOffset = 0;
    int dragStartLevelIndex = 1;
    bool dragging = false;
    bool draggingVertical = false;
    bool inEdgeZone = false; // tracks the transition so the drag baseline can resync on exit

    static constexpr int dragThresholdPx = 4;
    static constexpr int pixelsPerSemitone = 6;
    static constexpr int pixelsPerLevel = 30;
    static constexpr int maxSemitoneOffset = 36; // matches StepPattern::maxSemitoneOffset
    static constexpr int edgeZonePx = 20; // top/bottom band that triggers continuous autoscroll
};
}
