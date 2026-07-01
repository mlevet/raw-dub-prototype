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

    std::atomic<float> tuneHz    { 55.0f };   // oscillator base frequency
    std::atomic<float> cutoffHz  { 800.0f };  // lowpass filter cutoff
    std::atomic<float> resonance { 0.3f };    // filter resonance, 0-0.95
    std::atomic<float> decayMs   { 350.0f };  // amp envelope decay

private:
    double sampleRate = 44100.0;
    bool active = false;
    double phase = 0.0;
    double t = 0.0;
    double freq = 55.0;
    double decayTau = 0.35;
    double triggerGain = 1.0;
    static constexpr double attackTau = 0.003;

    // state-variable filter (Chamberlin) state
    double svfLow = 0.0;
    double svfBand = 0.0;
    double svfF = 0.3;
    double svfQ = 0.5;
};
}
