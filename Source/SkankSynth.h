#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <array>

namespace RawDub
{
// -1 (default) on any field means "use the matching knob" - same
// sentinel convention as the other instruments' voicing overrides.
// SawMix is deliberately absent - it's already always-a-curve via its
// own separate mechanism (sawMixOverride below / SkankPatternSlot::
// sawMixCurve), never a flat value, so it was never a candidate for
// this system. Chord quality (minor/major) is discrete, not continuous,
// so it stays its own separate minorOverride param too.
struct SkankVoicingOverrides
{
    float tuneHz = -1.0f;
    float decayMs = -1.0f;
    float drive = -1.0f;
};

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
    void prepare (double sampleRate);
    // semitoneOffset transposes the whole chord's root - lets a pattern
    // move the chord around per step (progressions), without touching
    // chord shape/voicing, which stays out of scope for now.
    // sawMixOverride: -1 (default) means "use the sawMix knob" (manual
    // Trigger / unsequenced use); 0-1 means "use this instead for this
    // hit" - how the sequencer feeds in a per-step value from the shape
    // lane, see sawMixLane below.
    // overrides: Tune/Decay/Drive section-level voicing override, same
    // -1 sentinel convention (see SkankVoicingOverrides and BassSynth's
    // BassVoicingOverrides).
    // minorOverride: same convention again (-1 = use the minorChord knob,
    // 0 = force major, 1 = force minor) - this is how a sequenced step's
    // OWN chord quality gets applied (see SkankPatternSlot::chordIsMinor);
    // manual/unsequenced Trigger has no per-step data to read, so it
    // leaves this at -1 and gets the knob like every other manual hit.
    void triggerChord (int semitoneOffset = 0, float levelGain = 1.0f, float sawMixOverride = -1.0f,
                        const SkankVoicingOverrides& overrides = {}, int minorOverride = -1);
    void renderAdd (float* out, int numSamples);
    void resetToDefaults(); // for "New Project" - restores every param to its shipped default

    std::atomic<float> tuneHz  { 400.0f }; // root pitch - high-pitched by design
    std::atomic<float> sawMix  { 0.5f };   // 0 = pure sine, 1 = pure (naive, unfiltered) saw - manual/default value
    std::atomic<float> decayMs { 150.0f }; // short/choppy by design, no sustain stage
    std::atomic<float> drive   { 0.3f };   // tanh saturation, same shape as Kick/BassSynth
    std::atomic<bool>  minorChord { false }; // false = major triad, true = minor triad
    std::atomic<float> volume  { 1.0f };    // basic level balancing against Kick/Bass, 0-1, applied before the master limiter
    // Instrument-level send to DubDelay - "which instruments feed the
    // delay," resolved once per instrument, not per-step/per-pattern
    // yet (see AudioEngine::renderNextBlock and DELAY_FEEDBACK_LOOP_
    // ANALYSIS.txt). Off by default - the whole point is choosing which
    // instruments feed the loop, so nothing is routed there unopted-in.
    std::atomic<float> delaySend { 0.0f };

    // The SawMix curve (points, not one-value-per-step) used to live
    // here as sawMixCurve* members. Per project_raw_dub_song_architecture:
    // a curve is musical material, not synthesis state, so it now lives
    // in the instrument PATTERN (see SkankPatternSlot) instead - this
    // synth only ever receives the already-evaluated value via
    // triggerChord's sawMixOverride, exactly as before.

private:
    struct Voice
    {
        double phase = 0.0;
        double freq = 400.0;
    };
    std::array<Voice, 3> voices; // root, third, fifth

    double sampleRate = 44100.0;
    bool active = false;
    double t = 0.0;
    double decayTau = 0.15;
    double driveAmt = 0.3;
    double sawMixAmt = 0.5;
    double triggerGain = 1.0;
};
}
