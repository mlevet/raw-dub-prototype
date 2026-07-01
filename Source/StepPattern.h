#pragma once
#include <JuceHeader.h>
#include "StepLevel.h"
#include <vector>
#include <atomic>

namespace RawDub
{
constexpr int numSteps = 16;       // steps shown per UI page, for every voice
constexpr int bassNumSteps = 64;   // Bass's actual pattern length: 4 pages of numSteps

// Uniform per-step data for one voice: on/off, pitch, and level -
// every voice gets all three, even if its synth or the UI doesn't
// expose pitch yet. Keeps the model consistent across instruments;
// what changes per voice is how much of it gets exposed (see
// StepButton::hasPitch), and now, how many steps it has (Kick stays
// 16, Bass is 64 shown as 4 pages) - not the shape of the data.
class StepPattern
{
public:
    explicit StepPattern (int lengthIn)
        : length (lengthIn), on ((size_t) lengthIn), levels ((size_t) lengthIn), offsets ((size_t) lengthIn)
    {
        for (auto& l : levels)
            l.store ((int) StepLevel::Normal, std::memory_order_relaxed);
    }

    int size() const { return length; }

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

private:
    int length;
    std::vector<std::atomic<bool>> on;
    std::vector<std::atomic<int>> levels;
    std::vector<std::atomic<int>> offsets;
};
}
