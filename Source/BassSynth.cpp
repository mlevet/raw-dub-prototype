#include "BassSynth.h"
#include <cmath>

namespace RawDub
{
void BassSynth::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
}

void BassSynth::resetToDefaults()
{
    tuneHz.store (55.0f);
    drive.store (0.15f);
    cutoffHz.store (1200.0f);
    resonance.store (0.1f);
    decayMs.store (2000.0f);
    useAMMode.store (false);
    amRatio.store (1.0f);
    amDepth.store (0.8f);
    volume.store (1.0f);
    pitchEnvAmount.store (0.5f);
    pitchEnvDecayMs.store (100.0f);
    filterEnvAmount.store (300.0f);
    filterEnvDecayMs.store (60.0f);
    driveTransientAmount.store (0.06f);
    delaySend.store (0.0f);
}

void BassSynth::trigger (int semitoneOffset, float levelGain, const BassVoicingOverrides& overrides)
{
    if (active)
    {
        // proper monophonic stop-then-start: don't touch anything yet,
        // just remember what the new note should be and start choking
        // the current one out. applyTrigger() runs once the choke
        // actually finishes (see renderAdd). The override values must be
        // remembered too, not re-read from the current Global Pattern
        // later - by the time the choke finishes, playback may have
        // moved past the pattern this note actually belongs to.
        choking = true;
        chokeStartAmp = currentAmpEnv;
        chokeT = 0.0;
        pendingSemitoneOffset = semitoneOffset;
        pendingLevelGain = levelGain;
        pendingOverrides = overrides;
        return;
    }

    applyTrigger (semitoneOffset, levelGain, overrides);
}

void BassSynth::applyTrigger (int semitoneOffset, float levelGain, const BassVoicingOverrides& overrides)
{
    active = true;
    choking = false;
    phase = 0.0;
    modPhase = 0.0;
    t = 0.0;
    svfLow = 0.0;
    svfBand = 0.0;

    float tuneKnob = (overrides.tuneHz >= 0.0f) ? overrides.tuneHz : tuneHz.load();
    freq = (double) tuneKnob * std::pow (2.0, semitoneOffset / 12.0);

    float pitchEnvAmountKnob = (overrides.pitchEnvAmount >= 0.0f) ? overrides.pitchEnvAmount : pitchEnvAmount.load();
    pitchEnvAmountSnapshot = (double) juce::jlimit (0.0f, 12.0f, pitchEnvAmountKnob);
    float pitchEnvDecayKnob = (overrides.pitchEnvDecayMs >= 0.0f) ? overrides.pitchEnvDecayMs : pitchEnvDecayMs.load();
    pitchEnvDecaySecondsSnapshot = juce::jmax (0.005, (double) pitchEnvDecayKnob / 1000.0);

    float lengthKnob = (overrides.decayMs >= 0.0f) ? overrides.decayMs : decayMs.load();
    holdSeconds = juce::jmax (0.02, (double) lengthKnob / 1000.0);
    triggerGain = (double) levelGain;

    float driveKnob = (overrides.drive >= 0.0f) ? overrides.drive : drive.load();
    driveAmt = (double) driveKnob;
    float driveTransientKnob = (overrides.driveTransientAmount >= 0.0f) ? overrides.driveTransientAmount : driveTransientAmount.load();
    driveTransientAmountSnapshot = (double) juce::jlimit (0.0f, 1.0f, driveTransientKnob);

    amModeSnapshot = useAMMode.load();
    amRatioSnapshot = (double) amRatio.load();
    float amDepthKnob = (overrides.amDepth >= 0.0f) ? overrides.amDepth : amDepth.load();
    amDepthSnapshot = (double) juce::jlimit (0.0f, 1.0f, amDepthKnob);

    // svfF itself is now computed fresh every sample in renderAdd,
    // since Filter Envelope makes the effective cutoff move within the
    // note - only the resolved base value is snapshotted here.
    float cutoffKnob = (overrides.cutoffHz >= 0.0f) ? overrides.cutoffHz : cutoffHz.load();
    baseCutoffSnapshot = (double) juce::jlimit (20.0f, (float) (sampleRate * 0.45), cutoffKnob);
    float filterEnvAmountKnob = (overrides.filterEnvAmount >= 0.0f) ? overrides.filterEnvAmount : filterEnvAmount.load();
    filterEnvAmountSnapshot = (double) juce::jlimit (0.0f, 3000.0f, filterEnvAmountKnob);
    float filterEnvDecayKnob = (overrides.filterEnvDecayMs >= 0.0f) ? overrides.filterEnvDecayMs : filterEnvDecayMs.load();
    filterEnvDecaySecondsSnapshot = juce::jmax (0.005, (double) filterEnvDecayKnob / 1000.0);

    float resonanceKnob = (overrides.resonance >= 0.0f) ? overrides.resonance : resonance.load();
    double res = (double) juce::jlimit (0.0f, 0.95f, resonanceKnob);
    svfQ = 1.0 - res;
}

void BassSynth::renderAdd (float* out, int numSamples)
{
    if (! active)
        return;

    const double dt = 1.0 / sampleRate;
    // read once per block, not per sample - Volume is a real-time mixing
    // control (unlike the voicing params, which only take effect on the
    // next trigger via applyTrigger's snapshots)
    const double vol = (double) volume.load();

    for (int i = 0; i < numSamples; ++i)
    {
        // Phase 1 of BASS_DUB_PRESSURE_ANALYSIS.txt: transient pitch
        // drop, exponential decay to the stable target frequency -
        // starts at the full amount at t=0 (note onset) and is
        // essentially gone within a few decay-time-constants, never
        // touching the sustained pitch. Computed once per sample and
        // applied to BOTH the oscillator and (if active) the AM
        // modulator, so the modulator's harmonic lock to the
        // fundamental (see amRatio's comment) holds even while the
        // pitch is still bending.
        double pitchEnvSemitones = pitchEnvAmountSnapshot * std::exp (-t / pitchEnvDecaySecondsSnapshot);
        double freqEffective = freq * std::pow (2.0, pitchEnvSemitones / 12.0);

        phase += freqEffective * dt;
        if (phase >= 1.0)
            phase -= 1.0;

        double osc = std::sin (2.0 * juce::MathConstants<double>::pi * phase);

        double voiced;
        if (amModeSnapshot)
        {
            // Research A/B mode: locked audio-rate amplitude modulation
            // instead of Drive. Modulator is locked to freq*amRatio (a
            // small set of simple ratios only - see BassSynth.h) so
            // sidebands land exactly on harmonics - no detune, no free
            // rate, no gong/metallic inharmonicity. Bypasses Drive and
            // the fixed warmth entirely so the two mechanisms are
            // compared cleanly, never stacked.
            modPhase += freqEffective * amRatioSnapshot * dt;
            if (modPhase >= 1.0)
                modPhase -= 1.0;

            double modulator = std::sin (2.0 * juce::MathConstants<double>::pi * modPhase);
            double amFactor = 1.0 - amDepthSnapshot * 0.5 + amDepthSnapshot * 0.5 * modulator;
            // fixed makeup gain - see amMakeupGainDb/amMakeupGain in BassSynth.h
            voiced = osc * amFactor * amMakeupGain;
        }
        else
        {
            // One source: a sine, not a mathematically pure one. A perfect
            // sine has literally nothing for Cutoff/Resonance to shape at
            // low Drive - no real analog "sine" source is that pure
            // either. A small, fixed, non-exposed 2nd-harmonic component
            // (sin^2(x) = 0.5 - 0.5cos(2x)) gives the filter something to
            // act on regardless of Drive, without adding Drive's own
            // odd-harmonic (saw/acid-adjacent) character.
            //
            // Important: warmth is computed from the pure sine and added
            // AFTER Drive's tanh stage, not fed into it - keeps the two
            // fully decoupled (measured: feeding warmth into Drive
            // generates real intermodulation that read as "digital
            // dirtiness").
            // Experiment C: drive briefly richer during the attack,
            // decaying back to the knob's value - the circuit
            // behaving differently under the transient, not a second
            // envelope layered on top. See driveTransientAmount's
            // comment in BassSynth.h.
            double transientDrive = driveTransientAmountSnapshot * std::exp (-t / transientDriveDecaySeconds);
            double driveEffective = juce::jlimit (0.0, 1.0, driveAmt + transientDrive);

            double shaped = osc;
            if (driveEffective > 0.0001)
            {
                double k = 1.0 + driveEffective * 12.0;
                shaped = std::tanh (osc * k) / std::tanh (k);
            }

            double warmth = osc * osc - 0.5;
            voiced = shaped + 0.12 * warmth;
        }

        // Filter Envelope: cutoff opens above baseCutoffSnapshot at note
        // onset and exponentially decays back down - same shape as the
        // pitch envelope above, a second facet of the same transient
        // behaviour, never touching the sustained cutoff once settled.
        // Recomputed every sample (unlike a static filter, which only
        // needs its coefficient once per trigger) because the cutoff
        // itself is moving during the note, not just at the start.
        double filterEnvHz = filterEnvAmountSnapshot * std::exp (-t / filterEnvDecaySecondsSnapshot);
        double cutoffEffective = juce::jlimit (20.0, sampleRate * 0.45, baseCutoffSnapshot + filterEnvHz);
        svfF = 2.0 * std::sin (juce::MathConstants<double>::pi * cutoffEffective / sampleRate);

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

            out[i] += (float) (svfLow * envMult * triggerGain * outputGain * vol);

            chokeT += dt;
            if (chokeT >= chokeTau)
                applyTrigger (pendingSemitoneOffset, pendingLevelGain, pendingOverrides);

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
        out[i] += (float) (sample * vol);

        t += dt;
        if (t >= attackTau + holdSeconds + releaseTau)
        {
            active = false;
            break;
        }
    }
}
}
