#include "BassSynth.h"
#include <cmath>

namespace RawDub
{
void BassSynth::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
}

void BassSynth::trigger (int semitoneOffset, float levelGain)
{
    active = true;
    phase = 0.0;
    t = 0.0;
    svfLow = 0.0;
    svfBand = 0.0;

    freq = (double) tuneHz.load() * std::pow (2.0, semitoneOffset / 12.0);
    decayTau = juce::jmax (0.02, (double) decayMs.load() / 1000.0);
    triggerGain = (double) levelGain;

    double cutoff = (double) juce::jlimit (20.0f, (float) (sampleRate * 0.45), cutoffHz.load());
    svfF = 2.0 * std::sin (juce::MathConstants<double>::pi * cutoff / sampleRate);

    double res = (double) juce::jlimit (0.0f, 0.95f, resonance.load());
    svfQ = 1.0 - res;
}

void BassSynth::renderAdd (float* out, int numSamples)
{
    if (! active)
        return;

    const double dt = 1.0 / sampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        phase += freq * dt;
        if (phase >= 1.0)
            phase -= 1.0;
        double saw = 2.0 * phase - 1.0;

        double high = saw - svfLow - svfQ * svfBand;
        svfBand += svfF * high;
        svfLow  += svfF * svfBand;

        double ampEnv;
        if (t < attackTau)
            ampEnv = t / attackTau;
        else
            ampEnv = std::exp (-(t - attackTau) / decayTau);

        out[i] += (float) (svfLow * ampEnv * triggerGain);

        t += dt;
        if (t > attackTau && ampEnv < 0.001)
        {
            active = false;
            break;
        }
    }
}
}
