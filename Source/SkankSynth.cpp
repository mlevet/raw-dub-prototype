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
    resetSawMixLane();
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

void SkankSynth::resetSawMixLaneToValue (float value)
{
    value = juce::jlimit (0.0f, 1.0f, value);
    sawMixCurvePos[0].store (0.0f);
    sawMixCurveVal[0].store (value);
    sawMixCurvePos[1].store (1.0f);
    sawMixCurveVal[1].store (value);
    sawMixCurveCount.store (2);
}

int SkankSynth::insertSawMixCurvePoint (float position, float value)
{
    int count = sawMixCurveCount.load();
    if (count >= maxCurvePoints)
        return -1;

    position = juce::jlimit (0.0f, 1.0f, position);
    value = juce::jlimit (0.0f, 1.0f, value);

    int insertAt = count - 1; // default: just before the closing anchor
    for (int i = 1; i < count; ++i)
    {
        if (position < sawMixCurvePos[(size_t) i].load())
        {
            insertAt = i;
            break;
        }
    }

    for (int i = count; i > insertAt; --i)
    {
        sawMixCurvePos[(size_t) i].store (sawMixCurvePos[(size_t) (i - 1)].load());
        sawMixCurveVal[(size_t) i].store (sawMixCurveVal[(size_t) (i - 1)].load());
    }

    sawMixCurvePos[(size_t) insertAt].store (position);
    sawMixCurveVal[(size_t) insertAt].store (value);
    sawMixCurveCount.store (count + 1);
    return insertAt;
}

void SkankSynth::setSawMixCurvePointPosition (int index, float position)
{
    int count = sawMixCurveCount.load();
    if (index <= 0 || index >= count - 1)
        return; // anchors (first/last) never move

    float lo = sawMixCurvePos[(size_t) (index - 1)].load();
    float hi = sawMixCurvePos[(size_t) (index + 1)].load();
    position = juce::jlimit (lo + 0.001f, hi - 0.001f, position);
    sawMixCurvePos[(size_t) index].store (position);
}

void SkankSynth::removeSawMixCurvePoint (int index)
{
    int count = sawMixCurveCount.load();
    if (index <= 0 || index >= count - 1)
        return; // can't remove anchors
    if (count <= 2)
        return;

    for (int i = index; i < count - 1; ++i)
    {
        sawMixCurvePos[(size_t) i].store (sawMixCurvePos[(size_t) (i + 1)].load());
        sawMixCurveVal[(size_t) i].store (sawMixCurveVal[(size_t) (i + 1)].load());
    }
    sawMixCurveCount.store (count - 1);
}

float SkankSynth::sampleSawMixCurve (float fraction) const
{
    int count = sawMixCurveCount.load();
    fraction = juce::jlimit (0.0f, 1.0f, fraction);

    for (int i = 0; i < count - 1; ++i)
    {
        float p0 = sawMixCurvePos[(size_t) i].load();
        float p1 = sawMixCurvePos[(size_t) (i + 1)].load();
        if (fraction >= p0 && fraction <= p1)
        {
            float v0 = sawMixCurveVal[(size_t) i].load();
            float v1 = sawMixCurveVal[(size_t) (i + 1)].load();
            float mixT = (p1 > p0) ? (fraction - p0) / (p1 - p0) : 0.0f;
            return v0 + (v1 - v0) * mixT;
        }
    }
    return sawMixCurveVal[(size_t) (count - 1)].load();
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
