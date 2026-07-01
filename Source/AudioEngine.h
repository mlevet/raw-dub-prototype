#pragma once
#include <JuceHeader.h>
#include "KickSynth.h"
#include <array>
#include <atomic>

namespace RawDub
{
constexpr int numSteps = 16;

class AudioEngine
{
public:
    void prepare (double sampleRate, int blockSize);
    void renderNextBlock (juce::AudioBuffer<float>& buffer, int numSamples);

    void play();
    void stop();
    bool isPlaying() const { return playing.load(); }

    void requestManualTrigger() { manualTriggerRequested.store (true); }

    void setTempoBpm (double bpm);
    double getTempoBpm() const { return tempoBpm.load(); }

    void toggleStep (int step)
    {
        auto& s = kickSteps[(size_t) step];
        s.store (! s.load (std::memory_order_relaxed), std::memory_order_relaxed);
    }

    bool getStep (int step) const
    {
        return kickSteps[(size_t) step].load (std::memory_order_relaxed);
    }

    int getCurrentStep() const { return currentStepAtomic.load(); }

    KickSynth kick;

private:
    enum class TransportCmd { None, Play, Stop };

    void advanceStep();
    double samplesPerStep() const;

    std::array<std::atomic<bool>, numSteps> kickSteps {};

    double sampleRate = 44100.0;
    std::atomic<double> tempoBpm { 120.0 };
    std::atomic<bool> playing { false };
    std::atomic<TransportCmd> pendingCommand { TransportCmd::None };
    std::atomic<bool> manualTriggerRequested { false };

    // audio-thread-only transport state (mutated exclusively while
    // processing pendingCommand inside renderNextBlock)
    double samplePositionInStep = 0.0;
    int currentStep = 0;
    std::atomic<int> currentStepAtomic { 0 };
};
}