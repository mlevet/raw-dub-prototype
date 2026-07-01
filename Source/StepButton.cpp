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

void StepButton::setAccentStyle (int style)
{
    if (accentStyle != style)
    {
        accentStyle = style;
        repaint();
    }
}

void StepButton::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();

    g.setColour (playhead ? juce::Colours::lightgrey : juce::Colours::white);
    g.fillRect (full);

    if (on)
    {
        double norm = hasPitch ? juce::jlimit (0.0, 1.0, (semitoneOffset + 12) / 24.0) : 1.0;
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
    g.setColour (juce::Colours::white);

    if (accentStyle == 0)
    {
        // corner notch
        float n = juce::jmin (bar.getWidth(), bar.getHeight()) * 0.4f;
        juce::Path p;
        p.startNewSubPath (bar.getRight(), bar.getY());
        p.lineTo (bar.getRight(), bar.getY() + n);
        p.lineTo (bar.getRight() - n, bar.getY());
        p.closeSubPath();
        g.fillPath (p);
    }
    else if (accentStyle == 1)
    {
        // inset frame
        auto inset = bar.reduced (juce::jmin (bar.getWidth(), bar.getHeight()) * 0.22f);
        g.drawRect (inset, 1.5f);
    }
    else
    {
        // accent chevron, borrowed straight from staff notation
        auto cx = bar.getCentreX();
        auto top = bar.getY() + bar.getHeight() * 0.18f;
        auto h = juce::jmin (bar.getWidth(), bar.getHeight()) * 0.32f;

        juce::Path p;
        p.startNewSubPath (cx - h * 0.5f, top);
        p.lineTo (cx + h * 0.5f, top + h * 0.5f);
        p.lineTo (cx - h * 0.5f, top + h);
        g.strokePath (p, juce::PathStrokeType (2.0f));
    }
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
        int deltaSemitones = deltaY / pixelsPerSemitone;
        int newOffset = juce::jlimit (-12, 12, dragStartOffset + deltaSemitones);

        if (newOffset != semitoneOffset)
        {
            semitoneOffset = newOffset;
            repaint();
            if (onPitchDrag)
                onPitchDrag (semitoneOffset);
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

    dragging = false;
}
}
