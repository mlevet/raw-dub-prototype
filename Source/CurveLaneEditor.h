#pragma once
#include <JuceHeader.h>
#include <functional>

namespace RawDub
{
// Sparse, draggable control points connected by straight lines - "draw
// a few points on an old synth," not automation, not a value-per-step
// bar lane. Styled like a small piano-roll grid (vertical gridlines,
// y-axis 0 at bottom to max at top), but points sit at FRACTIONAL
// positions (0-1 across the whole pattern), not tied to individual
// steps or pattern Length. The first and last points are fixed anchors
// (position 0 and 1) - their value can be dragged but they can never
// move or be removed, so the curve always spans the full pattern.
class CurveLaneEditor : public juce::Component
{
public:
    std::function<int()> getPointCount;
    std::function<float (int)> getPointPosition;
    std::function<float (int)> getPointValue;
    std::function<void (int, float)> onPointValueChanged;
    std::function<void (int, float)> onPointPositionChanged;
    std::function<int (float, float)> onAddPoint; // (position, value) -> new index, or -1 if at capacity
    std::function<void (int)> onRemovePoint;

    void setGridDivisions (int divisions) { gridDivisions = divisions; repaint(); }

    // Synchronizes with the transport, not with which page of the step
    // grid happens to be showing - the curve describes time across the
    // whole pattern, so a moving playhead is the natural way to relate
    // it back to "1-16 / 17-32" paging on the trigger grid above it.
    // step = -1 hides it (stopped). Highlights the whole step's cell
    // (same lightgrey fill as StepButton's own playhead), not a thin
    // marker, so this grid and the trigger grid read as one language.
    void setPlayheadStep (int step)
    {
        if (step == playheadStep)
            return;
        playheadStep = step;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::white);

        // same treatment as the trigger grid's playhead (StepButton) -
        // fill the whole step's cell in lightgrey, not a thin marker,
        // so the two grids read as one coherent visual language
        if (playheadStep >= 0 && gridDivisions > 0)
        {
            float x0 = (float) getWidth() * (float) playheadStep / (float) gridDivisions;
            float x1 = (float) getWidth() * (float) (playheadStep + 1) / (float) gridDivisions;
            g.setColour (juce::Colours::lightgrey);
            g.fillRect (x0, 0.0f, x1 - x0, (float) getHeight());
        }

        g.setColour (juce::Colours::black);
        g.drawRect (getLocalBounds(), 1);

        if (gridDivisions > 1)
        {
            g.setColour (juce::Colours::lightgrey);
            for (int i = 1; i < gridDivisions; ++i)
            {
                float x = (float) getWidth() * (float) i / (float) gridDivisions;
                g.drawVerticalLine ((int) x, 0.0f, (float) getHeight());
            }
        }

        if (! getPointCount || ! getPointPosition || ! getPointValue)
            return;

        int count = getPointCount();
        if (count < 2)
            return;

        juce::Path path;
        for (int i = 0; i < count; ++i)
        {
            auto p = toScreen (getPointPosition (i), getPointValue (i));
            if (i == 0)
                path.startNewSubPath (p);
            else
                path.lineTo (p);
        }
        g.setColour (juce::Colours::black);
        g.strokePath (path, juce::PathStrokeType (2.0f));

        for (int i = 0; i < count; ++i)
        {
            auto p = toScreen (getPointPosition (i), getPointValue (i));
            g.fillEllipse (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        int idx = findPointNear (e.position);
        if (idx >= 0)
        {
            draggingIndex = idx;
        }
        else
        {
            auto [pos, val] = fromScreen (e.position);
            draggingIndex = onAddPoint ? onAddPoint (pos, val) : -1;
        }
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (draggingIndex < 0)
            return;
        auto [pos, val] = fromScreen (e.position);
        if (onPointPositionChanged)
            onPointPositionChanged (draggingIndex, pos);
        if (onPointValueChanged)
            onPointValueChanged (draggingIndex, val);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        draggingIndex = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        int idx = findPointNear (e.position);
        if (idx >= 0 && onRemovePoint)
        {
            onRemovePoint (idx);
            repaint();
        }
    }

private:
    juce::Point<float> toScreen (float position, float value) const
    {
        return { position * (float) getWidth(), (1.0f - value) * (float) getHeight() };
    }

    std::pair<float, float> fromScreen (juce::Point<float> p) const
    {
        float pos = juce::jlimit (0.0f, 1.0f, p.x / (float) juce::jmax (1, getWidth()));
        float val = juce::jlimit (0.0f, 1.0f, 1.0f - (p.y / (float) juce::jmax (1, getHeight())));
        return { pos, val };
    }

    int findPointNear (juce::Point<float> p) const
    {
        if (! getPointCount || ! getPointPosition || ! getPointValue)
            return -1;
        int count = getPointCount();
        for (int i = 0; i < count; ++i)
        {
            auto sp = toScreen (getPointPosition (i), getPointValue (i));
            if (sp.getDistanceFrom (p) < 8.0f)
                return i;
        }
        return -1;
    }

    int gridDivisions = 16;
    int draggingIndex = -1;
    int playheadStep = -1;
};
}
