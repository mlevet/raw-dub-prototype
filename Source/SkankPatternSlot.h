#pragma once
#include "StepPattern.h"
#include "PointCurve.h"
#include <vector>
#include <atomic>

namespace RawDub
{
// One slot in Skank's instrument pattern bank: the step data (on/off,
// pitch, Ghost/Normal/Accent, length - same as every instrument) plus
// the SawMix curve and per-step chord quality, which are pattern-scoped
// musical material, not synth state (see
// project_raw_dub_song_architecture memory). Kick and Bass don't need
// this wrapper yet since they have neither - their banks are just plain
// StepPattern.
//
// chordIsMinor is Skank-specific (unlike on/off/pitch/level, which
// StepPattern gives every instrument uniformly) - Major/Minor isn't
// something Kick or Bass could ever meaningfully have, so it lives here
// rather than in StepPattern itself, same reasoning as sawMixCurve.
// Lets a pattern alternate major/minor per step (e.g. same root,
// major/minor/major/minor) without manual performance - the global
// minorChord knob on SkankSynth remains only the default a newly
// activated step is seeded with, and what an unsequenced manual Trigger
// hit uses (see triggerChord's minorOverride).
struct SkankPatternSlot
{
    explicit SkankPatternSlot (int initialActiveLength)
        : steps (initialActiveLength), chordIsMinor ((size_t) StepPattern::maxLength)
    {
    }

    StepPattern steps;
    PointCurve sawMixCurve;
    std::vector<std::atomic<bool>> chordIsMinor;

    bool getChordIsMinor (int step) const { return chordIsMinor[(size_t) step].load (std::memory_order_relaxed); }
    void setChordIsMinor (int step, bool minor) { chordIsMinor[(size_t) step].store (minor, std::memory_order_relaxed); }

    // For "Make Unique" - a curve/chord-quality alone (with no notes on)
    // doesn't make a slot musically meaningful, so emptiness is judged
    // purely by steps.
    bool isEmpty() const { return steps.isEmpty(); }

    void copyFrom (const SkankPatternSlot& other)
    {
        steps.copyFrom (other.steps);
        sawMixCurve.copyFrom (other.sawMixCurve);
        for (size_t i = 0; i < chordIsMinor.size(); ++i)
            chordIsMinor[i].store (other.chordIsMinor[i].load (std::memory_order_relaxed), std::memory_order_relaxed);
    }
};
}
