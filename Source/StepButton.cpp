#include "StepButton.h"

namespace RawDub
{
void StepButton::setOn (bool shouldBeOn)
{
    if (on != shouldBeOn)
    {
        on = shouldBeOn;
        repaint();
    }
}

void StepButton::setSemitoneOffset (int newOffset)
{
    if (semitoneOffset != newOffset)
    {
        semitoneOffset = newOffset;
        repaint();
    }
}

void StepButton::setLevel (StepLevel newLevel)
{
    if (level != newLevel)
    {
        level = newLevel;
        repaint();
    }
}

void StepButton::setPlayhead (bool isCurrentStep)
{
    if (playhead != isCurrentStep)
    {
        playhead = isCurrentStep;
        repaint();
    }
}

void StepButton::setPitchViewport (int newViewportMin, int newViewportMax)
{
    if (viewportMin != newViewportMin || viewportMax != newViewportMax)
    {
        viewportMin = newViewportMin;
        viewportMax = newViewportMax;
        repaint();
    }
}

void StepButton::setGuidePitches (const std::vector<int>& pitches)
{
    if (guidePitches != pitches)
    {
        guidePitches = pitches;
        repaint();
    }
}

void StepButton::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();

    g.setColour (playhead ? juce::Colours::lightgrey : juce::Colours::white);
    g.fillRect (full);

    // reference layer, not a grid - one thin line per distinct pitch
    // actually used in the pattern, drawn behind the bar so it stays
    // subtle. Same list on every step in the row, so lines drawn at the
    // same pitch line up continuously across the width.
    if (hasPitch)
    {
        g.setColour (juce::Colours::lightgrey);
        for (int pitch : guidePitches)
        {
            double norm = (double) (pitch - viewportMin) / (double) (viewportMax - viewportMin);
            if (norm < 0.0 || norm > 1.0)
                continue; // outside the current viewport - don't draw
            float y = full.getY() + (float) (full.getHeight() * (1.0 - norm));
            g.drawLine (full.getX(), y, full.getRight(), y, 1.0f);
        }
    }

    if (on)
    {
        double norm = hasPitch ? juce::jlimit (0.0, 1.0, (double) (semitoneOffset - viewportMin) / (double) (viewportMax - viewportMin)) : 1.0;
        auto bounds = full;
        auto bar = bounds.removeFromBottom ((float) (bounds.getHeight() * norm));

        juce::Colour fillColour = (level == StepLevel::Ghost) ? juce::Colours::lightgrey
                                                                : juce::Colours::black;
        if (playhead && level != StepLevel::Ghost)
            fillColour = juce::Colours::darkgrey;

        g.setColour (fillColour);
        g.fillRect (bar);

        if (level == StepLevel::Accent)
            paintAccentMarker (g, bar);
    }

    g.setColour (juce::Colours::black);
    g.drawRect (full, 1.0f);
}

void StepButton::paintAccentMarker (juce::Graphics& g, juce::Rectangle<float> bar)
{
    // corner notch - chosen after comparing three visual treatments live
    g.setColour (juce::Colours::white);
    float n = juce::jmin (bar.getWidth(), bar.getHeight()) * 0.4f;
    juce::Path p;
    p.startNewSubPath (bar.getRight(), bar.getY());
    p.lineTo (bar.getRight(), bar.getY() + n);
    p.lineTo (bar.getRight() - n, bar.getY());
    p.closeSubPath();
    g.fillPath (p);
}

void StepButton::mouseDown (const juce::MouseEvent& e)
{
    dragStartX = e.getScreenX();
    dragStartY = e.getScreenY();
    dragStartOffset = semitoneOffset;
    dragStartLevelIndex = static_cast<int> (level);
    dragging = false;
    draggingVertical = false;
}

void StepButton::mouseDrag (const juce::MouseEvent& e)
{
    if (! on)
        return;

    int deltaX = e.getScreenX() - dragStartX;
    int deltaY = dragStartY - e.getScreenY(); // up = positive

    if (! dragging)
    {
        int absX = deltaX < 0 ? -deltaX : deltaX;
        int absY = deltaY < 0 ? -deltaY : deltaY;

        if (hasPitch && absY > dragThresholdPx && absY >= absX)
        {
            dragging = true;
            draggingVertical = true;
        }
        else if (absX > dragThresholdPx)
        {
            dragging = true;
            draggingVertical = false;
        }
        else
        {
            return;
        }
    }

    if (draggingVertical)
    {
        // position relative to this button, which spans the full step
        // row height - within edgeZonePx of the top/bottom edge means
        // "keep scrolling," reported every call (not gated on offset
        // changing) so the owner's timer can keep running even if the
        // cursor holds perfectly still
        float localY = e.position.y;
        int edgeDirection = 0;
        if (localY <= (float) edgeZonePx)
            edgeDirection = 1; // near top = higher pitch direction
        else if (localY >= (float) (getHeight() - edgeZonePx))
            edgeDirection = -1; // near bottom = lower pitch direction

        if (edgeDirection != 0)
        {
            // owner's timer drives scrolling/offset now - don't fight it
            // with a raw pixel-delta recompute (see class comment)
            inEdgeZone = true;
            if (onPitchDrag)
                onPitchDrag (semitoneOffset, edgeDirection);
            return;
        }

        if (inEdgeZone)
        {
            // just left the zone - resync the baseline to wherever the
            // owner's timer left things, so ordinary dragging resumes
            // from here instead of jumping back to the original
            // mouseDown position
            dragStartY = e.getScreenY();
            dragStartOffset = semitoneOffset;
            inEdgeZone = false;
        }

        // clamped to the full underlying range, NOT the current visible
        // window - dragging past the window's edge (without landing in
        // the edge zone, e.g. a fast/coalesced move) is what triggers
        // the owner's fallback "ensure visible" scroll
        int deltaSemitones = (dragStartY - e.getScreenY()) / pixelsPerSemitone;
        int newOffset = juce::jlimit (-maxSemitoneOffset, maxSemitoneOffset, dragStartOffset + deltaSemitones);

        if (newOffset != semitoneOffset)
        {
            semitoneOffset = newOffset;
            repaint();
            if (onPitchDrag)
                onPitchDrag (semitoneOffset, 0);
        }
    }
    else
    {
        int deltaLevels = deltaX / pixelsPerLevel;
        int newIndex = juce::jlimit (0, 2, dragStartLevelIndex + deltaLevels);
        auto newLevel = static_cast<StepLevel> (newIndex);

        if (newLevel != level)
        {
            level = newLevel;
            repaint();
            if (onLevelDrag)
                onLevelDrag (level);
        }
    }
}

void StepButton::mouseUp (const juce::MouseEvent&)
{
    if (! dragging && onToggle)
        onToggle();

    if (draggingVertical)
    {
        // stop any edge-zone autoscroll the owner might currently be running
        if (inEdgeZone && onPitchDrag)
            onPitchDrag (semitoneOffset, 0);
        if (onPitchDragEnd)
            onPitchDragEnd();
    }

    inEdgeZone = false;
    dragging = false;
    draggingVertical = false;
}

void StepButton::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (hasPitch && onPitchWheel)
        onPitchWheel (wheel.deltaY);
}

void StepButton::mouseEnter (const juce::MouseEvent&)
{
    if (hasPitch && onHoverChanged)
        onHoverChanged (true);
}

void StepButton::mouseExit (const juce::MouseEvent&)
{
    if (hasPitch && onHoverChanged)
        onHoverChanged (false);
}
}
