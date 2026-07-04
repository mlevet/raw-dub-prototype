#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <array>

namespace RawDub
{
// First Skank experiment - deliberately naive. Goal is to discover the
// instrument's personality by listening, not to build the final
// architecture. Chord-based (not note-based): three simultaneous
// voices (root/third/fifth of a triad), each a simple sine<->saw
// blend, through one drive stage and a short percussive decay. No
// filter, no sustain, no per-voice detune yet - add complexity only
// once listening says something specific is missing.
class SkankSynth
{
public:
    SkankSynth() { resetSawMixLane(); }

    void prepare (double sampleRate);
    // semitoneOffset transposes the whole chord's root - lets a pattern
    // move the chord around per step (progressions), without touching
    // chord shape/voicing, which stays out of scope for now.
    // sawMixOverride: -1 (default) means "use the sawMix knob" (manual
    // Trigger / unsequenced use); 0-1 means "use this instead for this
    // hit" - how the sequencer feeds in a per-step value from the shape
    // lane, see sawMixLane below.
    void triggerChord (int semitoneOffset = 0, float levelGain = 1.0f, float sawMixOverride = -1.0f);
    void renderAdd (float* out, int numSamples);
    void resetToDefaults(); // for "New Project" - restores every param to its shipped default

    std::atomic<float> tuneHz  { 400.0f }; // root pitch - high-pitched by design
    std::atomic<float> sawMix  { 0.5f };   // 0 = pure sine, 1 = pure (naive, unfiltered) saw - manual/default value
    std::atomic<float> decayMs { 150.0f }; // short/choppy by design, no sustain stage
    std::atomic<float> drive   { 0.3f };   // tanh saturation, same shape as Kick/BassSynth
    std::atomic<bool>  minorChord { false }; // false = major triad, true = minor triad
    std::atomic<float> volume  { 1.0f };    // basic level balancing against Kick/Bass, 0-1, applied before the master limiter

    // Sparse SawMix "curve" - a handful of draggable points connected by
    // straight lines (see CurveLaneEditor), not automation, not one
    // value per step. Points are stored as FRACTIONAL positions (0-1
    // across the whole pattern) rather than step indices, so the curve
    // is entirely independent of pattern Length - changing Length just
    // resamples the same shape, no data massaging needed. The first and
    // last points are fixed anchors (always at position 0 and 1) whose
    // value can be dragged but which can never move or be removed, so
    // the curve always spans the full pattern. Deliberately scoped to
    // SawMix only, not a generic modulation system - see
    // feedback_raw_dub_experiment_protocol memory. Slider/curve
    // relationship: moving the sawMix slider collapses this curve back
    // to a flat line at the slider's value (resetSawMixLaneToValue) -
    // the slider is "set a constant," the curve is "compose evolution,"
    // never two independent ways of controlling the same thing.
    static constexpr int maxCurvePoints = 16;
    int getSawMixCurvePointCount() const { return sawMixCurveCount.load(); }
    float getSawMixCurvePointPosition (int index) const { return sawMixCurvePos[(size_t) index].load(); }
    float getSawMixCurvePointValue (int index) const { return sawMixCurveVal[(size_t) index].load(); }
    int insertSawMixCurvePoint (float position, float value); // returns new index, or -1 if at capacity
    void setSawMixCurvePointValue (int index, float value) { sawMixCurveVal[(size_t) index].store (juce::jlimit (0.0f, 1.0f, value)); }
    void setSawMixCurvePointPosition (int index, float position); // no-op on the two anchor points
    void removeSawMixCurvePoint (int index); // no-op on the two anchor points, and below 2 total points
    float sampleSawMixCurve (float fraction) const; // fraction 0-1 across the pattern
    void resetSawMixLaneToValue (float value); // collapses to the two anchors at this value, discarding every interior point
    void resetSawMixLane() { resetSawMixLaneToValue (0.5f); }

private:
    struct Voice
    {
        double phase = 0.0;
        double freq = 400.0;
    };
    std::array<Voice, 3> voices; // root, third, fifth
    std::array<std::atomic<float>, maxCurvePoints> sawMixCurvePos;
    std::array<std::atomic<float>, maxCurvePoints> sawMixCurveVal;
    std::atomic<int> sawMixCurveCount { 2 };

    double sampleRate = 44100.0;
    bool active = false;
    double t = 0.0;
    double decayTau = 0.15;
    double driveAmt = 0.3;
    double sawMixAmt = 0.5;
    double triggerGain = 1.0;
};
}
