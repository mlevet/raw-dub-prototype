#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>

namespace RawDub
{
// A feedback loop, not a clean digital delay - see
// DELAY_FEEDBACK_LOOP_ANALYSIS.txt. Modelled on an old homemade rig
// (audio out -> piezo -> physical coupling -> piezo -> mic in ->
// software -> back to output): the point was never the echo itself, it
// was that everything passing through the loop got progressively
// darker and grittier, because the loop's own colouration (damping,
// saturation) sits INSIDE the feedback path and compounds every pass,
// not applied once to the final output. "Each repeat should sound less
// like a copy and more like something the signal survived."
//
// Signal flow per sample:
//   delay line (Time) -> Tone (one-pole lowpass, IN the loop)
//                      -> Drive (tanh saturation, IN the loop)
//                      -> x Feedback -> back into the delay line
// Input feeds the loop at a fixed level; Wet controls only how much of
// the loop's (already coloured) output returns to the main mix -
// keeps "how much reaches the loop" and "how much comes back" as one
// simple control rather than two, per the first-pass scope.
//
// A true send/return: process() takes a SEPARATE input buffer (the
// weighted sum of each instrument's own DelaySend amount - see
// AudioEngine::renderNextBlock) and adds the wet output into the main
// mix buffer - deliberately different shape from Kick/Bass/Skank's
// renderAdd (which only ever add their own generated voice), and from
// this class's own first pass (which read input and wrote output to
// the SAME buffer, tapping the whole mix rather than per-instrument
// sends - see project history for why that changed).
//
// Explicitly NOT built here, per the first-pass scope: stereo/ping-
// pong, multitap, shimmer, reverse, any modulation/wander, tempo sync.
// Feedback is deliberately capped well below 1.0 (not just under it) -
// tanh bounds amplitude but a saturating loop can still converge to a
// musically dead, unchanging drone at feedback near unity; capping
// lower keeps the loop inside territory that actually decays.
class DubDelay
{
public:
    void prepare (double sampleRate);
    // sendInput is the pre-mixed, pre-weighted send sum (see
    // AudioEngine::renderNextBlock) - read-only, this instrument's own
    // signal, not the main mix. mixOut is where the wet output gets
    // ADDED (never overwritten) - almost always the same buffer the
    // instruments already rendered their dry signal into.
    //
    // feedbackAmt/toneCutoffHz/driveAmt/wetAmt are the ALREADY-RESOLVED
    // values for this block (curve/override/base - see AudioEngine::
    // resolveDelayParams) - this class has no idea whether a value came
    // from a curve, an override, or is just the feedback/toneHz/drive/
    // wet knob passed through, same separation of concerns every synth's
    // trigger() already has. Time is deliberately NOT resolved this way
    // and stays read internally from timeMs - see ParamID.h's
    // DelayParamID comment on why animating Time isn't safe yet.
    void process (float* mixOut, const float* sendInput, int numSamples,
                  float feedbackAmt, float toneCutoffHz, float driveAmt, float wetAmt);
    void resetToDefaults(); // for "New Project" - restores every param to its shipped default
    // Zeroes the delay line and filter state - called on New/Open
    // Project so old audio energy never lingers into a freshly loaded
    // project. Deliberately separate from resetToDefaults(): params and
    // buffer contents are different kinds of state, and some future
    // caller (e.g. a manual "clear delay" action) may want to reset one
    // without the other.
    void clearBuffer();

    std::atomic<float> timeMs   { 350.0f };  // delay length, 10-1500ms - not tempo-synced, see class comment
    std::atomic<float> feedback { 0.4f };    // 0 - maxFeedback (capped well below 1.0, see maxFeedback)
    std::atomic<float> toneHz   { 3000.0f }; // lowpass cutoff INSIDE the loop - lower = darkens faster per repeat
    std::atomic<float> drive    { 0.15f };   // tanh saturation INSIDE the loop, 0-1 - same shaping curve BassSynth::drive uses
    std::atomic<float> wet      { 0.35f };   // 0-1, how much of the loop's output returns to the mix
    std::atomic<bool> bypass    { false };   // for A/B comparison during voicing - not a permanent feature

    // Capped well below 1.0 deliberately, not just under it - see class
    // comment. A saturating loop can sound "stuck" (a fixed, undecaying
    // drone) well before hitting genuine numerical instability, so the
    // safe range for "still musically decaying" is narrower than "won't
    // blow up."
    static constexpr float maxFeedback = 0.95f;

private:
    double sampleRate = 44100.0;
    std::vector<float> buffer;
    int writeIndex = 0;
    double toneState = 0.0; // one-pole lowpass filter state, lives inside the loop
};
}
