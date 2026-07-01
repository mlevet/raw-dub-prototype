#pragma once
#include <JuceHeader.h>
#include <atomic>

namespace RawDub
{
class KickSynth
{
public:
    void prepare (double sampleRate);
    // semitoneOffset exists for symmetry with BassSynth's trigger() and is
    // applied to the pitch envelope, but nothing in the UI drives it yet
    // (StepButton::hasPitch is false for Kick) - kept uniform so exposing
    // it later needs no data-model change.
    void trigger (int semitoneOffset = 0, float levelGain = 1.0f);
    void renderAdd (float* out, int numSamples);

    std::atomic<float> tuneHz  { 60.0f };   // start pitch of the pitch envelope
    std::atomic<float> punchMs { 40.0f };   // pitch envelope speed
    std::atomic<float> decayMs { 220.0f };  // amplitude decay
    std::atomic<float> drive   { 0.2f };    // saturation amount

private:
    double sampleRate = 44100.0;
    bool active = false;
    double phase = 0.0;
    double t = 0.0;
    double startFreq = 60.0, endFreq = 40.0;
    double punchTau = 0.02, decayTau = 0.2;
    double driveAmt = 0.0;
    double triggerGain = 1.0;
};
}