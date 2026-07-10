#include "SnareSynth.h"
#include <cmath>

namespace RawDub
{
void SnareSynth::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
}

void SnareSynth::resetToDefaults()
{
    tuneHz.store (180.0f);
    noiseMix.store (0.6f);
    cutoffHz.store (3000.0f);
    resonance.store (0.15f);
    decayMs.store (150.0f);
    drive.store (0.1f);
    volume.store (0.8f);
    delaySend.store (0.0f);
}

void SnareSynth::trigger (float levelGain, const SnareVoicingOverrides& overrides)
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
    phase = 0.0;
    t = 0.0;
    float tuneKnob = (overrides.tuneHz >= 0.0f) ? overrides.tuneHz : tuneHz.load();
    bodyFreq = (double) tuneKnob;
    float noiseMixKnob = (overrides.noiseMix >= 0.0f) ? overrides.noiseMix : noiseMix.load();
    noiseMixAmt = juce::jlimit (0.0, 1.0, (double) noiseMixKnob);
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
    // filter STATE (svfLow/svfBand) does NOT reset on trigger - a fast
    // retrigger inherits whatever resonant ringing is already there, same
    // reasoning as leaving BassSynth's filter state alone between notes
    // (continuous voice, not restarted-per-note); here it just means two
    // closely-spaced hits colour each other slightly, which reads as
    // alive rather than a bug for a percussive voice. Only the filter
    // COEFFICIENTS (svfF/svfQ) are re-resolved per trigger.
}

void SnareSynth::renderAdd (float* out, int numSamples)
{
    if (! active)
        return;

    const double dt = 1.0 / sampleRate;
    const double vol = (double) volume.load();

    for (int i = 0; i < numSamples; ++i)
    {
        phase += bodyFreq * dt;
        if (phase >= 1.0)
            phase -= 1.0;
        double body = std::sin (2.0 * juce::MathConstants<double>::pi * phase);
        double noise = noiseRandom.nextFloat() * 2.0f - 1.0f;
        double raw = body * (1.0 - noiseMixAmt) + noise * noiseMixAmt;

        double high = raw - svfLow - svfQ * svfBand;
        svfBand += svfF * high;
        svfLow  += svfF * svfBand;

        double ampEnv = std::exp (-t / decayTau);
        double sample = svfLow * ampEnv * triggerGain;

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
