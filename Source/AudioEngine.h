#pragma once
#include <JuceHeader.h>
#include "KickSynth.h"
#include "BassSynth.h"
#include "StepPattern.h"
#include <atomic>

namespace RawDub
{
class AudioEngine
{
public:
    void prepare (double sampleRate, int blockSize);
    void renderNextBlock (juce::AudioBuffer<float>& buffer, int numSamples);

    void play();
    void stop();
    bool isPlaying() const { return playing.load(); }

    void requestManualKickTrigger() { manualKickTriggerRequested.store (true); }
    void requestManualBassTrigger() { manualBassTriggerRequested.store (true); }

    void setTempoBpm (double bpm);
    double getTempoBpm() const { return tempoBpm.load(); }

    int getCurrentStep() const { return currentStepAtomic.load(); }

    KickSynth kick;
    BassSynth bass;

    StepPattern kickPattern;
    StepPattern bassPattern;

private:
    enum class TransportCmd { None, Play, Stop };

    void advanceStep();
    double samplesPerStep() const;

    double sampleRate = 44100.0;
    std::atomic<double> tempoBpm { 120.0 };
    std::atomic<bool> playing { false };
    std::atomic<TransportCmd> pendingCommand { TransportCmd::None };
    std::atomic<bool> manualKickTriggerRequested { false };
    std::atomic<bool> manualBassTriggerRequested { false };

    // audio-thread-only transport state (mutated exclusively while
    // processing pendingCommand inside renderNextBlock)
    double samplePositionInStep = 0.0;
    int currentStep = 0;
    std::atomic<int> currentStepAtomic { 0 };
};
}
