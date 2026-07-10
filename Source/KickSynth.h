#pragma once
#include <JuceHeader.h>
#include <atomic>

namespace RawDub
{
// -1 (default) on any field means "use the matching knob" - same
// sentinel convention as BassVoicingOverrides, see its comment for the
// full reasoning. Volume is deliberately absent, same reason as Bass's.
struct KickVoicingOverrides
{
    float tuneHz = -1.0f;
    float punchMs = -1.0f;
    float decayMs = -1.0f;
    float drive = -1.0f;
};

class KickSynth
{
public:
    void prepare (double sampleRate);
    // semitoneOffset exists for symmetry with BassSynth's trigger() and is
    // applied to the pitch envelope, but nothing in the UI drives it yet
    // (StepButton::hasPitch is false for Kick) - kept uniform so exposing
    // it later needs no data-model change.
    void trigger (int semitoneOffset = 0, float levelGain = 1.0f, const KickVoicingOverrides& overrides = {});
    void renderAdd (float* out, int numSamples);
    void resetToDefaults(); // for "New Project" - restores every param to its shipped default

    std::atomic<float> tuneHz  { 60.0f };   // start pitch of the pitch envelope
    std::atomic<float> punchMs { 40.0f };   // pitch envelope speed
    std::atomic<float> decayMs { 220.0f };  // amplitude decay
    std::atomic<float> drive   { 0.2f };    // saturation amount
    std::atomic<float> volume  { 1.0f };    // basic level balancing against Bass, 0-1, applied before the master limiter
    // Instrument-level send to DubDelay - "which instruments feed the
    // delay," resolved once per instrument, not per-step/per-pattern
    // yet (see AudioEngine::renderNextBlock and DELAY_FEEDBACK_LOOP_
    // ANALYSIS.txt). Off by default - the whole point is choosing which
    // instruments feed the loop, so nothing is routed there unopted-in.
    std::atomic<float> delaySend { 0.0f };

private:
    double sampleRate = 44100.0;
    bool active = false;
    double phase = 0.0;
    double t = 0.0;
    double startFreq = 60.0, endFreq = 40.0;
    double punchTau = 0.02, decayTau = 0.2;
    double driveAmt = 0.0;
    double triggerGain = 1.0;

    // same fix as BassSynth: retriggering while a previous hit is still
    // decaying used to jump straight to the new hit's value in one
    // sample - a real risk here since Kick's decay (220ms default) often
    // has audible amplitude left at the next 16th-note retrigger. Blends
    // from the old level into the new hit over 2ms instead.
    static constexpr double declickTau = 0.002;
    bool declickActive = false;
    double declickFromLevel = 0.0;
    double lastOutputSample = 0.0;
};
}