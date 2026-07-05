#include "SkankSynth.h"
#include <cmath>

namespace RawDub
{
void SkankSynth::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
}

void SkankSynth::resetToDefaults()
{
    tuneHz.store (400.0f);
    sawMix.store (0.5f);
    decayMs.store (150.0f);
    drive.store (0.3f);
    minorChord.store (false);
    volume.store (1.0f);
}

void SkankSynth::triggerChord (int semitoneOffset, float levelGain, float sawMixOverride)
{
    active = true;
    t = 0.0;

    double root = (double) tuneHz.load() * std::pow (2.0, semitoneOffset / 12.0);
    bool minor = minorChord.load();
    double thirdSemitones = minor ? 3.0 : 4.0;

    voices[0].phase = 0.0;
    voices[0].freq = root;
    voices[1].phase = 0.0;
    voices[1].freq = root * std::pow (2.0, thirdSemitones / 12.0);
    voices[2].phase = 0.0;
    voices[2].freq = root * std::pow (2.0, 7.0 / 12.0);

    decayTau = juce::jmax (0.02, (double) decayMs.load() / 1000.0);
    driveAmt = (double) drive.load();
    sawMixAmt = (sawMixOverride >= 0.0f) ? (double) sawMixOverride : (double) sawMix.load();
    triggerGain = (double) levelGain;
}

void SkankSynth::renderAdd (float* out, int numSamples)
{
    if (! active)
        return;

    const double dt = 1.0 / sampleRate;
    const double vol = (double) volume.load();

    for (int i = 0; i < numSamples; ++i)
    {
        double mix = 0.0;
        for (auto& v : voices)
        {
            v.phase += v.freq * dt;
            if (v.phase >= 1.0)
                v.phase -= 1.0;

            double sine = std::sin (2.0 * juce::MathConstants<double>::pi * v.phase);
            // naive, unfiltered saw - deliberately not anti-aliased. At
            // this register the resulting aliasing is a feature, not a
            // bug, for a "dry/metallic/raw/digital" character
            double saw = 2.0 * v.phase - 1.0;
            mix += sine * (1.0 - sawMixAmt) + saw * sawMixAmt;
        }
        mix /= 3.0;

        if (driveAmt > 0.0001)
        {
            double k = 1.0 + driveAmt * 10.0;
            mix = std::tanh (mix * k) / std::tanh (k);
        }

        double ampEnv = std::exp (-t / decayTau);
        out[i] += (float) (mix * ampEnv * triggerGain * vol * 0.7);

        t += dt;
        if (ampEnv < 0.001)
        {
            active = false;
            break;
        }
    }
}
}
