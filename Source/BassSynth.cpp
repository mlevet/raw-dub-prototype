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
    if (active)
    {
        // proper monophonic stop-then-start: don't touch anything yet,
        // just remember what the new note should be and start choking
        // the current one out. applyTrigger() runs once the choke
        // actually finishes (see renderAdd).
        choking = true;
        chokeStartAmp = currentAmpEnv;
        chokeT = 0.0;
        pendingSemitoneOffset = semitoneOffset;
        pendingLevelGain = levelGain;
        return;
    }

    applyTrigger (semitoneOffset, levelGain);
}

void BassSynth::applyTrigger (int semitoneOffset, float levelGain)
{
    active = true;
    choking = false;
    phase = 0.0;
    t = 0.0;
    svfLow = 0.0;
    svfBand = 0.0;

    freq = (double) tuneHz.load() * std::pow (2.0, semitoneOffset / 12.0);
    holdSeconds = juce::jmax (0.02, (double) decayMs.load() / 1000.0);
    triggerGain = (double) levelGain;
    driveAmt = (double) drive.load();

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
        // One source: a sine, not a mathematically pure one. A perfect
        // sine has literally nothing for Cutoff/Resonance to shape at low
        // Drive - no real analog "sine" source is that pure either. A
        // small, fixed, non-exposed 2nd-harmonic component (sin^2(x) =
        // 0.5 - 0.5cos(2x)) gives the filter something to act on
        // regardless of Drive, without adding Drive's own odd-harmonic
        // (saw/acid-adjacent) character.
        //
        // Important: warmth is computed from the pure sine and added
        // AFTER Drive's tanh stage, not fed into it - keeps the two
        // fully decoupled (measured: feeding warmth into Drive generates
        // real intermodulation that read as "digital dirtiness").
        phase += freq * dt;
        if (phase >= 1.0)
            phase -= 1.0;

        double osc = std::sin (2.0 * juce::MathConstants<double>::pi * phase);

        double shaped = osc;
        if (driveAmt > 0.0001)
        {
            double k = 1.0 + driveAmt * 12.0;
            shaped = std::tanh (osc * k) / std::tanh (k);
        }

        double warmth = osc * osc - 0.5;
        double voiced = shaped + 0.12 * warmth;

        double high = voiced - svfLow - svfQ * svfBand;
        svfBand += svfF * high;
        svfLow  += svfF * svfBand;

        if (choking)
        {
            // continue the SAME oscillator/filter (not reset) but force
            // amplitude into a fast, smooth (raised-cosine, no kink)
            // release to true silence, starting from wherever the
            // envelope actually was - never a jump, always a real stop.
            double blend = chokeT / chokeTau;
            double envMult = chokeStartAmp * (0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * blend));
            currentAmpEnv = envMult;

            out[i] += (float) (svfLow * envMult * triggerGain * outputGain);

            chokeT += dt;
            if (chokeT >= chokeTau)
                applyTrigger (pendingSemitoneOffset, pendingLevelGain);

            continue;
        }

        double ampEnv;
        if (t < attackTau)
        {
            // quadratic ease-out: steep initial rise (2x a linear ramp's
            // slope at t=0) instead of a symmetric S-curve's zero slope
            // at the very start - asserts itself immediately, still
            // lands with zero slope into the hold (no corner/click)
            double x = t / attackTau;
            ampEnv = 1.0 - (1.0 - x) * (1.0 - x);
        }
        else if (t < attackTau + holdSeconds)
        {
            ampEnv = 1.0;
        }
        else if (t < attackTau + holdSeconds + releaseTau)
        {
            // raised-cosine, same family as the attack curve: zero slope
            // at both ends, so it leaves Hold's flat 1.0 with no kink and
            // lands on exactly 0 with no kink either. A plain exponential
            // has maximum slope right at this boundary - a measured
            // derivative discontinuity (0 -> ~-10/s) that was audible as
            // a tick on every note reaching its natural release.
            double releaseProgress = (t - attackTau - holdSeconds) / releaseTau;
            ampEnv = 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * releaseProgress);
        }
        else
        {
            ampEnv = 0.0;
        }

        currentAmpEnv = ampEnv;

        double sample = svfLow * ampEnv * triggerGain * outputGain;
        out[i] += (float) sample;

        t += dt;
        if (t >= attackTau + holdSeconds + releaseTau)
        {
            active = false;
            break;
        }
    }
}
}
