#pragma once
#include <JuceHeader.h>
#include "StepLevel.h"
#include <vector>
#include <atomic>

namespace RawDub
{
constexpr int numSteps = 16; // steps shown per UI page, for every voice

// Uniform per-step data for one voice: on/off, pitch, and level -
// every voice gets all three, even if its synth or the UI doesn't
// expose pitch yet. Keeps the model consistent across instruments;
// what changes per voice is how much of it gets exposed (see
// StepButton::hasPitch), and how many steps are currently active
// (4/16/32/64, user-selectable per voice) - not the shape of the data.
//
// Storage always covers maxLength steps regardless of the current
// active length, so shrinking (e.g. 64 -> 16) and growing back never
// loses step data - it's just outside the active loop meanwhile.
class StepPattern
{
public:
    static constexpr int maxLength = 64;

    explicit StepPattern (int initialActiveLength)
        : activeLength (initialActiveLength), on ((size_t) maxLength), levels ((size_t) maxLength), offsets ((size_t) maxLength)
    {
        for (auto& l : levels)
            l.store ((int) StepLevel::Normal, std::memory_order_relaxed);
    }

    void setActiveLength (int newLength) { activeLength = newLength; }
    int getActiveLength() const { return activeLength; }

    void toggle (int step)
    {
        auto& s = on[(size_t) step];
        bool newState = ! s.load (std::memory_order_relaxed);
        s.store (newState, std::memory_order_relaxed);
        if (newState)
            levels[(size_t) step].store ((int) StepLevel::Normal, std::memory_order_relaxed);
    }

    bool isOn (int step) const
    {
        return on[(size_t) step].load (std::memory_order_relaxed);
    }

    void setOn (int step, bool value)
    {
        on[(size_t) step].store (value, std::memory_order_relaxed);
    }

    void clearAll()
    {
        for (auto& s : on)
            s.store (false, std::memory_order_relaxed);
        for (auto& l : levels)
            l.store ((int) StepLevel::Normal, std::memory_order_relaxed);
        for (auto& o : offsets)
            o.store (0, std::memory_order_relaxed);
    }

    void setLevel (int step, StepLevel level)
    {
        levels[(size_t) step].store ((int) level, std::memory_order_relaxed);
    }

    StepLevel getLevel (int step) const
    {
        return (StepLevel) levels[(size_t) step].load (std::memory_order_relaxed);
    }

    void setSemitoneOffset (int step, int offset)
    {
        offsets[(size_t) step].store (juce::jlimit (-12, 12, offset), std::memory_order_relaxed);
    }

    int getSemitoneOffset (int step) const
    {
        return offsets[(size_t) step].load (std::memory_order_relaxed);
    }

    // For "Make Unique" (see project_raw_dub_song_architecture memory) -
    // no notes on means nothing musically distinguishes this slot, so
    // it's safe to treat as an available fork destination even without
    // an explicit "used" flag like Global Patterns have.
    bool isEmpty() const
    {
        for (auto& s : on)
            if (s.load (std::memory_order_relaxed))
                return false;
        return true;
    }

    // Vectors of atomics aren't copy-assignable via a plain `=` (same
    // reason PointCurve needs an explicit move constructor - see
    // PointCurve.h), so an explicit field-by-field copy is needed here
    // too, for "Make Unique" forking one pattern's content into another.
    void copyFrom (const StepPattern& other)
    {
        activeLength = other.activeLength;
        for (size_t i = 0; i < (size_t) maxLength; ++i)
        {
            on[i].store (other.on[i].load (std::memory_order_relaxed), std::memory_order_relaxed);
            levels[i].store (other.levels[i].load (std::memory_order_relaxed), std::memory_order_relaxed);
            offsets[i].store (other.offsets[i].load (std::memory_order_relaxed), std::memory_order_relaxed);
        }
    }

private:
    int activeLength;
    std::vector<std::atomic<bool>> on;
    std::vector<std::atomic<int>> levels;
    std::vector<std::atomic<int>> offsets;
};
}
