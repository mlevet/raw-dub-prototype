#pragma once
#include <JuceHeader.h>
#include "StepLevel.h"
#include "PointCurve.h"
#include <vector>
#include <atomic>
#include <map>

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
        curves.clear();
    }

    void setLevel (int step, StepLevel level)
    {
        levels[(size_t) step].store ((int) level, std::memory_order_relaxed);
    }

    StepLevel getLevel (int step) const
    {
        return (StepLevel) levels[(size_t) step].load (std::memory_order_relaxed);
    }

    // +/-36 (three octaves each way) - widened from +/-12 to allow the
    // sudden low-register drops real dub basslines use. The UI only
    // ever shows a fixed-size (24-semitone) window onto this range at
    // once and scrolls it during a drag rather than growing the window
    // itself - see StepButton::setPitchViewport and
    // project_raw_dub_song_architecture memory.
    static constexpr int maxSemitoneOffset = 36;

    void setSemitoneOffset (int step, int offset)
    {
        offsets[(size_t) step].store (juce::jlimit (-maxSemitoneOffset, maxSemitoneOffset, offset), std::memory_order_relaxed);
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

    // Sparse, generic per-parameter curve storage - see
    // project_raw_dub_song_architecture memory, "any continuous parameter
    // can become curve-capable." Keyed by a plain int (cast from
    // whichever instrument's ParamID enum, e.g. BassParamID) rather than
    // a fixed field per parameter, so adding a new curve-capable
    // parameter never requires touching StepPattern itself. Absence of
    // an entry for a given id means "flat/no curve" - falls back to the
    // instrument's base value (and section override, if active) exactly
    // as before. An entry is only created the moment a curve is
    // explicitly requested for that parameter on this pattern, seeded
    // flat at the value it should represent when nothing has been drawn
    // yet (mirrors PointCurve::resetToValue - "a fixed value is simply a
    // flat curve").
    bool hasCurve (int paramId) const { return curves.find (paramId) != curves.end(); }

    // For ProjectIO serialization - iterate every curve this pattern
    // actually has (sparse, so usually few or none), not a fixed set.
    const std::map<int, PointCurve>& getCurves() const { return curves; }

    PointCurve* findCurve (int paramId)
    {
        auto it = curves.find (paramId);
        return it != curves.end() ? &it->second : nullptr;
    }

    const PointCurve* findCurve (int paramId) const
    {
        auto it = curves.find (paramId);
        return it != curves.end() ? &it->second : nullptr;
    }

    PointCurve& getOrCreateCurve (int paramId, float initialNormalizedValue)
    {
        auto it = curves.find (paramId);
        if (it != curves.end())
            return it->second;
        auto result = curves.try_emplace (paramId);
        result.first->second.resetToValue (initialNormalizedValue);
        return result.first->second;
    }

    void removeCurve (int paramId) { curves.erase (paramId); }

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

        curves.clear();
        for (const auto& [id, curve] : other.curves)
            curves.try_emplace (id).first->second.copyFrom (curve);
    }

private:
    int activeLength;
    std::vector<std::atomic<bool>> on;
    std::vector<std::atomic<int>> levels;
    std::vector<std::atomic<int>> offsets;
    std::map<int, PointCurve> curves;
};
}
