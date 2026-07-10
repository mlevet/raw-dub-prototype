#pragma once
#include <JuceHeader.h>
#include <atomic>

namespace RawDub
{
// -1 (default) on any field means "use the matching knob" - same
// sentinel convention as the other instruments' voicing overrides.
struct HiHatVoicingOverrides
{
    float cutoffHz = -1.0f;
    float resonance = -1.0f;
    float decayMs = -1.0f;
    float drive = -1.0f;
};

// Closed hi-hat - the simplest voice in the instrument: noise shaped by
// one resonant highpass and one amplitude envelope, no tonal body at
// all (SnareSynth has one, this deliberately doesn't - "even simpler"
// than Snare, per the same three-drums-different-weight design as Kick/
// Snare/HiHat generally). Highpass rather than Snare's lowpass: a
// closed hat's whole identity is high-frequency noise, so cutting lows
// rather than keeping them is what actually gets there with the fewest
// controls - not a nod to any specific classic drum machine circuit.
class HiHatSynth
{
public:
    void prepare (double sampleRate);
    void trigger (float levelGain = 1.0f, const HiHatVoicingOverrides& overrides = {});
    void renderAdd (float* out, int numSamples);
    void resetToDefaults(); // for "New Project" - restores every param to its shipped default

    std::atomic<float> cutoffHz  { 6000.0f }; // highpass over the noise - same SVF as Bass/Snare's Cutoff, high tap instead of low
    std::atomic<float> resonance { 0.1f };    // filter resonance, 0-0.95
    std::atomic<float> decayMs   { 60.0f };   // single amplitude envelope - short by default, a closed hat's defining trait
    std::atomic<float> drive     { 0.1f };    // saturation amount, applied after the filter
    std::atomic<float> volume    { 0.7f };    // basic level balancing against the other voices, 0-1
    // Instrument-level send to DubDelay - same convention as the other
    // three voices' delaySend. Off by default.
    std::atomic<float> delaySend { 0.0f };

private:
    double sampleRate = 44100.0;
    bool active = false;
    double t = 0.0;
    double decayTau = 0.06;
    double driveAmt = 0.0;
    double triggerGain = 1.0;

    juce::Random noiseRandom;

    // Chamberlin state-variable filter, HIGHPASS tap this time (Snare
    // and Bass both tap the lowpass output) - see SnareSynth.h's comment
    // on reusing the same filter math for the reasoning. svfF/svfQ
    // snapshotted once at trigger time, same as SnareSynth - see its
    // header comment for why (curve/override resolution happens once,
    // at trigger, not by re-reading the atomics every block).
    double svfLow = 0.0;
    double svfBand = 0.0;
    double svfF = 0.3;
    double svfQ = 0.5;

    // same declick approach as Kick/Snare - see KickSynth.h's comment on
    // declickTau for the full reasoning.
    static constexpr double declickTau = 0.002;
    bool declickActive = false;
    double declickFromLevel = 0.0;
    double lastOutputSample = 0.0;
};
}
