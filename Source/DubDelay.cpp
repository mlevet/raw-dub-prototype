#include "DubDelay.h"
#include <cmath>

namespace RawDub
{
namespace
{
constexpr double maxTimeMs = 1500.0;
constexpr double bufferHeadroomMs = 2000.0; // > maxTimeMs, so Time can sit right at its ceiling with room to spare
}

void DubDelay::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    buffer.assign ((size_t) (bufferHeadroomMs / 1000.0 * sampleRate) + 1, 0.0f);
    writeIndex = 0;
    toneState = 0.0;
}

void DubDelay::resetToDefaults()
{
    timeMs.store (350.0f);
    feedback.store (0.4f);
    toneHz.store (3000.0f);
    drive.store (0.15f);
    wet.store (0.35f);
    bypass.store (false);
}

void DubDelay::clearBuffer()
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    writeIndex = 0;
    toneState = 0.0;
}

void DubDelay::process (float* mixOut, const float* sendInput, int numSamples)
{
    if (buffer.empty() || bypass.load())
        return;

    // read once per block, not per sample - matches every other
    // instrument's snapshot convention (BassSynth::renderAdd etc.)
    double time = juce::jlimit (10.0, maxTimeMs, (double) timeMs.load());
    int delaySamples = juce::jlimit (1, (int) buffer.size() - 1, (int) (time / 1000.0 * sampleRate));
    double fb = juce::jlimit (0.0, (double) maxFeedback, (double) feedback.load());
    double toneCutoff = juce::jlimit (200.0, sampleRate * 0.45, (double) toneHz.load());
    double toneCoeff = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * toneCutoff / sampleRate);
    double driveAmt = juce::jlimit (0.0, 1.0, (double) drive.load());
    double wetAmt = juce::jlimit (0.0, 1.0, (double) wet.load());
    int bufferSize = (int) buffer.size();

    for (int i = 0; i < numSamples; ++i)
    {
        float input = sendInput[i];

        int readIndex = writeIndex - delaySamples;
        if (readIndex < 0)
            readIndex += bufferSize;
        double delayed = (double) buffer[(size_t) readIndex];

        // Tone: one-pole lowpass, INSIDE the loop - darkens every pass,
        // not just the final output. See class comment.
        toneState += toneCoeff * (delayed - toneState);

        // Drive: tanh saturation, INSIDE the loop, same shaping curve
        // BassSynth::drive already established. This is also what keeps
        // the loop bounded at high Feedback - see class comment on why
        // that alone isn't treated as sufficient (Feedback is capped
        // separately, well below 1.0).
        double driven = toneState;
        if (driveAmt > 0.0001)
        {
            double k = 1.0 + driveAmt * 12.0;
            driven = std::tanh (toneState * k) / std::tanh (k);
        }

        double feedbackSignal = driven * fb;
        buffer[(size_t) writeIndex] = (float) (input + feedbackSignal);

        mixOut[i] += (float) (driven * wetAmt);

        writeIndex = (writeIndex + 1 == bufferSize) ? 0 : writeIndex + 1;
    }
}
}
