#include "KickSynth.h"
#include <cmath>

namespace RawDub
{
void KickSynth::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
}

void KickSynth::resetToDefaults()
{
    tuneHz.store (60.0f);
    punchMs.store (40.0f);
    decayMs.store (220.0f);
    drive.store (0.2f);
    volume.store (1.0f);
}

void KickSynth::trigger (int semitoneOffset, float levelGain)
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
    startFreq = (double) tuneHz.load() * std::pow (2.0, semitoneOffset / 12.0);
    endFreq = startFreq * 0.5;
    punchTau = juce::jmax (0.001, (double) punchMs.load() / 1000.0);
    decayTau = juce::jmax (0.005, (double) decayMs.load() / 1000.0);
    driveAmt = (double) drive.load();
    triggerGain = (double) levelGain;
}

void KickSynth::renderAdd (float* out, int numSamples)
{
    if (! active)
        return;

    const double dt = 1.0 / sampleRate;
    // read once per block, not per sample - Volume is a real-time mixing
    // control (unlike the voicing params, which only take effect on the
    // next trigger), but per-sample atomic reads would be wasteful
    const double vol = (double) volume.load();

    for (int i = 0; i < numSamples; ++i)
    {
        double pitchEnv = std::exp (-t / punchTau);
        double freq = endFreq + (startFreq - endFreq) * pitchEnv;

        phase += freq * dt;
        if (phase >= 1.0)
            phase -= 1.0;

        double s = std::sin (2.0 * juce::MathConstants<double>::pi * phase);
        double ampEnv = std::exp (-t / decayTau);
        double sample = s * ampEnv * triggerGain;

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