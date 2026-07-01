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
    AudioEngine();

    void prepare (double sampleRate, int blockSize);
    void renderNextBlock (juce::AudioBuffer<float>& buffer, int numSamples);

    void play();
    void stop();
    bool isPlaying() const { return playing.load(); }

    void requestManualKickTrigger() { manualKickTriggerRequested.store (true); }
    void requestManualBassTrigger() { manualBassTriggerRequested.store (true); }

    void setTempoBpm (double bpm);
    double getTempoBpm() const { return tempoBpm.load(); }

    // Kick and Bass share one clock but loop at their own (independently
    // user-selectable, 4/16/32/64) pattern length - e.g. a 4-bar bassline
    // can play against a 1-bar kick loop. Since all valid lengths are
    // powers of two within {4,16,32,64}, the shorter one always divides
    // evenly into the longer one, so they never drift out of phase.
    int getCurrentKickStep() const { return globalStepAtomic.load() % kickPattern.getActiveLength(); }
    int getCurrentBassStep() const { return globalStepAtomic.load() % bassPattern.getActiveLength(); }

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
    int globalStep = 0;
    std::atomic<int> globalStepAtomic { 0 };
};
}
