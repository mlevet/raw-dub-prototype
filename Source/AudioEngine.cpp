#include "AudioEngine.h"
#include <cmath>

namespace RawDub
{
void AudioEngine::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    kick.prepare (sampleRate);
    bass.prepare (sampleRate);
}

void AudioEngine::play() { pendingCommand.store (TransportCmd::Play); }
void AudioEngine::stop() { pendingCommand.store (TransportCmd::Stop); }

void AudioEngine::setTempoBpm (double bpm)
{
    tempoBpm.store (juce::jlimit (40.0, 220.0, bpm));
}

double AudioEngine::samplesPerStep() const
{
    const double stepsPerBeat = 4.0; // 16th notes
    const double stepsPerSecond = (tempoBpm.load() / 60.0) * stepsPerBeat;
    return sampleRate / stepsPerSecond;
}

void AudioEngine::advanceStep()
{
    currentStep = (currentStep + 1) % numSteps;
    currentStepAtomic.store (currentStep);

    if (kickPattern.isOn (currentStep))
        kick.trigger (kickPattern.getSemitoneOffset (currentStep), stepLevelGain (kickPattern.getLevel (currentStep)));

    if (bassPattern.isOn (currentStep))
        bass.trigger (bassPattern.getSemitoneOffset (currentStep), stepLevelGain (bassPattern.getLevel (currentStep)));
}

void AudioEngine::renderNextBlock (juce::AudioBuffer<float>& buffer, int numSamples)
{
    buffer.clear();

    // Transport start/stop is only ever applied here, on the audio thread,
    // so currentStep / samplePositionInStep never need cross-thread locking.
    auto cmd = pendingCommand.exchange (TransportCmd::None);
    if (cmd == TransportCmd::Play)
    {
        currentStep = -1;
        samplePositionInStep = 0.0;
        playing.store (true);
        advanceStep();
    }
    else if (cmd == TransportCmd::Stop)
    {
        playing.store (false);
        currentStep = 0;
        currentStepAtomic.store (0);
        samplePositionInStep = 0.0;
    }

    if (manualKickTriggerRequested.exchange (false))
        kick.trigger (0, stepLevelGain (StepLevel::Normal));

    if (manualBassTriggerRequested.exchange (false))
        bass.trigger (0, stepLevelGain (StepLevel::Normal));

    auto* out = buffer.getWritePointer (0);

    if (playing.load())
    {
        int offset = 0;
        int remaining = numSamples;
        const double spStep = samplesPerStep();

        while (remaining > 0)
        {
            int samplesUntilNextStep = juce::jmax (1, (int) std::ceil (spStep - samplePositionInStep));
            int chunk = juce::jmin (remaining, samplesUntilNextStep);

            kick.renderAdd (out + offset, chunk);
            bass.renderAdd (out + offset, chunk);

            samplePositionInStep += chunk;
            offset += chunk;
            remaining -= chunk;

            if (samplePositionInStep >= spStep - 0.0001)
            {
                samplePositionInStep -= spStep;
                if (samplePositionInStep < 0.0)
                    samplePositionInStep = 0.0;
                advanceStep();
            }
        }
    }
    else
    {
        // let a manually-triggered hit ring out even while the transport is stopped
        kick.renderAdd (out, numSamples);
        bass.renderAdd (out, numSamples);
    }

    for (int i = 0; i < numSamples; ++i)
        out[i] = (float) std::tanh (out[i] * 0.9);

    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
}
}
