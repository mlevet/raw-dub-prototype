#pragma once
#include <JuceHeader.h>
#include <atomic>

namespace RawDub
{
// -1 (default) on any field means "use the matching knob" - same
// sentinel convention as BassVoicingOverrides/KickVoicingOverrides.
struct SnareVoicingOverrides
{
    float tuneHz = -1.0f;
    float noiseMix = -1.0f;
    float cutoffHz = -1.0f;
    float resonance = -1.0f;
    float decayMs = -1.0f;
    float drive = -1.0f;
};

// Minimal synthesized snare - noise + a small tonal body, summed, then
// shaped by one resonant lowpass and one amplitude envelope. Same
// "small number of meaningful controls" philosophy as KickSynth/
// BassSynth: no separate attack stage, no independent filter per
// ingredient (noise and body share one Cutoff/Resonance, same as
// everything downstream of the mix shares one envelope) - the
// ingredients are noise and tonal body; filtering/envelope/drive are
// one thing each applied to their sum, not one thing per ingredient.
// Deliberately not modeled on any specific classic drum machine circuit.
class SnareSynth
{
public:
    void prepare (double sampleRate);
    void trigger (float levelGain = 1.0f, const SnareVoicingOverrides& overrides = {});
    void renderAdd (float* out, int numSamples);
    void resetToDefaults(); // for "New Project" - restores every param to its shipped default

    std::atomic<float> tuneHz    { 180.0f }; // tonal body's pitch
    std::atomic<float> noiseMix  { 0.6f };   // 0 = all tonal body, 1 = all noise
    std::atomic<float> cutoffHz  { 3000.0f };// lowpass over the combined signal - same SVF as Bass's Cutoff
    std::atomic<float> resonance { 0.15f };  // filter resonance, 0-0.95
    std::atomic<float> decayMs   { 150.0f }; // single amplitude envelope, body and noise together
    std::atomic<float> drive     { 0.1f };   // saturation amount, applied after the filter
    std::atomic<float> volume    { 0.8f };   // basic level balancing against the other voices, 0-1
    // Instrument-level send to DubDelay - same convention as Kick/Bass/
    // Skank's delaySend (see those for the full reasoning). Off by
    // default.
    std::atomic<float> delaySend { 0.0f };

private:
    double sampleRate = 44100.0;
    bool active = false;
    double phase = 0.0;
    double t = 0.0;
    double bodyFreq = 180.0;
    double noiseMixAmt = 0.6;
    double decayTau = 0.15;
    double driveAmt = 0.0;
    double triggerGain = 1.0;

    juce::Random noiseRandom;

    // Chamberlin state-variable filter, lowpass tap - identical math to
    // BassSynth's own SVF (see BassSynth.cpp), reused here rather than
    // reinventing a different filter response for a second instrument.
    // svfF/svfQ (the resolved Cutoff/Resonance, as filter coefficients)
    // are snapshotted once at trigger time, same as every other voicing
    // param (curve/override resolution only happens once, at trigger) -
    // NOT re-read from the atomics every block like Volume, despite an
    // earlier version of this file doing exactly that before curves/
    // overrides existed for Snare.
    double svfLow = 0.0;
    double svfBand = 0.0;
    double svfF = 0.3;
    double svfQ = 0.5;

    // same declick approach as KickSynth: retriggering while a previous
    // hit is still decaying blends from the old sample into the new
    // hit's first 2ms, rather than jumping - see KickSynth.h's comment
    // on declickTau for the full reasoning.
    static constexpr double declickTau = 0.002;
    bool declickActive = false;
    double declickFromLevel = 0.0;
    double lastOutputSample = 0.0;
};
}
