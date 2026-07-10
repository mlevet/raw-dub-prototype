#include "HiHatSynth.h"
#include <cmath>

namespace RawDub
{
void HiHatSynth::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
}

void HiHatSynth::resetToDefaults()
{
    cutoffHz.store (6000.0f);
    resonance.store (0.1f);
    decayMs.store (60.0f);
    drive.store (0.1f);
    volume.store (0.7f);
    delaySend.store (0.0f);
}

void HiHatSynth::trigger (float levelGain, const HiHatVoicingOverrides& overrides)
{
    if (active)
    {
        declickActive = true;
        declickFromLevel = lastOutputSample;
    }
    else
    {
        declickActive = false;
    }

    active = true;
    t = 0.0;
    float decayKnob = (overrides.decayMs >= 0.0f) ? overrides.decayMs : decayMs.load();
    decayTau = juce::jmax (0.005, (double) decayKnob / 1000.0);
    float driveKnob = (overrides.drive >= 0.0f) ? overrides.drive : drive.load();
    driveAmt = (double) driveKnob;
    triggerGain = (double) levelGain;

    float cutoffKnob = (overrides.cutoffHz >= 0.0f) ? overrides.cutoffHz : cutoffHz.load();
    double cutoff = juce::jlimit (20.0, sampleRate * 0.45, (double) cutoffKnob);
    float resonanceKnob = (overrides.resonance >= 0.0f) ? overrides.resonance : resonance.load();
    double res = juce::jlimit (0.0, 0.95, (double) resonanceKnob);
    svfF = 2.0 * std::sin (juce::MathConstants<double>::pi * cutoff / sampleRate);
    svfQ = 1.0 - res;
    // filter state carries over between hits, same reasoning as
    // SnareSynth::trigger's comment on not resetting svfLow/svfBand
}

void HiHatSynth::renderAdd (float* out, int numSamples)
{
    if (! active)
        return;

    const double dt = 1.0 / sampleRate;
    const double vol = (double) volume.load();

    for (int i = 0; i < numSamples; ++i)
    {
        double noise = noiseRandom.nextFloat() * 2.0f - 1.0f;

        double high = noise - svfLow - svfQ * svfBand;
        svfBand += svfF * high;
        svfLow  += svfF * svfBand;

        double ampEnv = std::exp (-t / decayTau);
        double sample = high * ampEnv * triggerGain;

        if (driveAmt > 0.0001)
        {
            double k = 1.0 + driveAmt * 6.0;
            sample = std::tanh (sample * k) / std::tanh (k);
        }

        if (declickActive)
        {
            if (t < declickTau)
            {
                double blend = t / declickTau;
                sample = declickFromLevel * (1.0 - blend) + sample * blend;
            }
            else
            {
                declickActive = false;
            }
        }

        out[i] += (float) (sample * vol);
        lastOutputSample = sample;

        t += dt;
        if (ampEnv < 0.001)
        {
            active = false;
            break;
        }
    }
}
}
