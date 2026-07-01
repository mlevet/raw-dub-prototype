#pragma once
#include <JuceHeader.h>
#include <atomic>

namespace RawDub
{
class BassSynth
{
public:
    void prepare (double sampleRate);
    void trigger (int semitoneOffset = 0, float levelGain = 1.0f);
    void renderAdd (float* out, int numSamples);

    // Oscillator -> Drive (saturation) -> lowpass -> envelope. Filter is
    // tuned as tone colour (defaults keep the fundamental intact), not as
    // the main expressive gesture - see MELODIC_EDITOR_DESIGN-adjacent
    // notes on early UK digidub reference synths (CS-01, CZ series).
    std::atomic<float> tuneHz    { 55.0f };    // oscillator base frequency
    std::atomic<float> drive     { 0.25f };    // saturation before the filter, same curve as KickSynth
    std::atomic<float> cutoffHz  { 1200.0f };  // lowpass filter cutoff
    std::atomic<float> resonance { 0.1f };     // filter resonance, 0-0.95
    std::atomic<float> decayMs   { 350.0f };   // amp envelope decay

private:
    double sampleRate = 44100.0;
    bool active = false;
    double phase = 0.0;
    double t = 0.0;
    double freq = 55.0;
    double decayTau = 0.35;
    double triggerGain = 1.0;
    double driveAmt = 0.0;
    static constexpr double attackTau = 0.003;
    // the SVF's own gain overshoots unity (~1.3 peak) even with zero
    // drive, which was hitting the master limiter hard enough to clamp
    // every note near the same ceiling regardless of Drive - this gives
    // real headroom so Drive differences actually survive to the output
    static constexpr double outputGain = 0.6;

    // state-variable filter (Chamberlin) state
    double svfLow = 0.0;
    double svfBand = 0.0;
    double svfF = 0.3;
    double svfQ = 0.5;
};
}
