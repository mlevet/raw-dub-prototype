#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <cmath>
#include <vector>

namespace RawDub
{
// A sparse, draggable-point curve - "a few points connected by lines,"
// not automation, not one value per step (see CurveLaneEditor for the
// matching UI widget). Points are stored as FRACTIONAL positions (0-1
// across whatever the curve spans) rather than step indices, so it's
// independent of any particular pattern Length. The first and last
// points are fixed anchors (always at position 0 and 1) whose value
// can be dragged but which can never move or be removed, so the curve
// always spans the full range.
//
// This is musical material, not synthesis state - it belongs to an
// instrument PATTERN (see project_raw_dub_song_architecture memory:
// "the curve is musical material, not synthesis state"), not to a
// synth. Originally lived inside SkankSynth as sawMixCurve*; extracted
// here, unchanged in behaviour, so it can be embedded per pattern slot
// instead, and reused later for other curve-controlled parameters
// without rewriting this math again.
class PointCurve
{
public:
    // vectors, not std::array: std::array<std::atomic<float>, N> is not
    // movable (moving would require moving each atomic element in
    // place, which atomic doesn't support), which made PointCurve (and
    // anything embedding it, like SkankPatternSlot) unable to live in a
    // std::vector - the pattern bank couldn't grow/reserve. A vector of
    // atomics IS movable (moving just transfers the buffer pointer),
    // matching how StepPattern already stores its own step data.
    PointCurve() : pos ((size_t) maxPoints), val ((size_t) maxPoints) { resetToValue (0.5f); }

    // count is a bare std::atomic<int>, not inside a vector, so it still
    // blocks the implicitly-generated move constructor (atomic itself
    // isn't movable) even though pos/val (vectors) are fine on their
    // own - spelled out explicitly so PointCurve, and anything embedding
    // it, can live in a std::vector (reserve/emplace_back need this).
    PointCurve (PointCurve&& other) noexcept
        : pos (std::move (other.pos)), val (std::move (other.val)), count (other.count.load())
    {
    }

    PointCurve& operator= (PointCurve&& other) noexcept
    {
        pos = std::move (other.pos);
        val = std::move (other.val);
        count.store (other.count.load());
        return *this;
    }

    static constexpr int maxPoints = 16;

    int getPointCount() const { return count.load(); }
    float getPointPosition (int index) const { return pos[(size_t) index].load(); }
    float getPointValue (int index) const { return val[(size_t) index].load(); }
    void setPointValue (int index, float value) { val[(size_t) index].store (juce::jlimit (0.0f, 1.0f, value)); }

    // no-op on the two anchor points (index 0 and the last)
    void setPointPosition (int index, float position)
    {
        int n = count.load();
        if (index <= 0 || index >= n - 1)
            return;

        float lo = pos[(size_t) (index - 1)].load();
        float hi = pos[(size_t) (index + 1)].load();
        position = juce::jlimit (lo + 0.001f, hi - 0.001f, position);
        pos[(size_t) index].store (position);
    }

    // returns the new point's index, or -1 if at capacity
    int insertPoint (float position, float value)
    {
        int n = count.load();
        if (n >= maxPoints)
            return -1;

        position = juce::jlimit (0.0f, 1.0f, position);
        value = juce::jlimit (0.0f, 1.0f, value);

        int insertAt = n - 1; // default: just before the closing anchor
        for (int i = 1; i < n; ++i)
        {
            if (position < pos[(size_t) i].load())
            {
                insertAt = i;
                break;
            }
        }

        for (int i = n; i > insertAt; --i)
        {
            pos[(size_t) i].store (pos[(size_t) (i - 1)].load());
            val[(size_t) i].store (val[(size_t) (i - 1)].load());
        }

        pos[(size_t) insertAt].store (position);
        val[(size_t) insertAt].store (value);
        count.store (n + 1);
        return insertAt;
    }

    // no-op on the two anchor points, and below 2 total points
    void removePoint (int index)
    {
        int n = count.load();
        if (index <= 0 || index >= n - 1)
            return;
        if (n <= 2)
            return;

        for (int i = index; i < n - 1; ++i)
        {
            pos[(size_t) i].store (pos[(size_t) (i + 1)].load());
            val[(size_t) i].store (val[(size_t) (i + 1)].load());
        }
        count.store (n - 1);
    }

    // fraction 0-1 across whatever this curve spans
    float sample (float fraction) const
    {
        int n = count.load();
        fraction = juce::jlimit (0.0f, 1.0f, fraction);

        for (int i = 0; i < n - 1; ++i)
        {
            float p0 = pos[(size_t) i].load();
            float p1 = pos[(size_t) (i + 1)].load();
            if (fraction >= p0 && fraction <= p1)
            {
                float v0 = val[(size_t) i].load();
                float v1 = val[(size_t) (i + 1)].load();
                float t = (p1 > p0) ? (fraction - p0) / (p1 - p0) : 0.0f;
                return v0 + (v1 - v0) * t;
            }
        }
        return val[(size_t) (n - 1)].load();
    }

    // collapses to the two anchors at this value, discarding every interior point
    void resetToValue (float value)
    {
        value = juce::jlimit (0.0f, 1.0f, value);
        pos[0].store (0.0f);
        val[0].store (value);
        pos[1].store (1.0f);
        val[1].store (value);
        count.store (2);
    }

    // True if every point currently has the same value (within a small
    // tolerance) - a curve in this state is behaviourally identical to
    // no curve at all ("a fixed value is simply a flat curve"), used to
    // decide when to auto-remove a curve that's been flattened, whether
    // by the slider or by manually dragging points together.
    bool isFlat (float tolerance = 0.001f) const
    {
        int n = count.load();
        if (n < 2)
            return true;
        float first = val[0].load();
        for (int i = 1; i < n; ++i)
            if (std::abs (val[(size_t) i].load() - first) > tolerance)
                return false;
        return true;
    }

    // For "Make Unique" (see project_raw_dub_song_architecture memory) -
    // vectors of atomics can't be copy-assigned via a plain `=`, so an
    // explicit field-by-field copy is needed to fork one pattern's curve
    // into another.
    void copyFrom (const PointCurve& other)
    {
        int n = other.count.load();
        for (int i = 0; i < n; ++i)
        {
            pos[(size_t) i].store (other.pos[(size_t) i].load());
            val[(size_t) i].store (other.val[(size_t) i].load());
        }
        count.store (n);
    }

private:
    std::vector<std::atomic<float>> pos;
    std::vector<std::atomic<float>> val;
    std::atomic<int> count { 2 };
};
}
