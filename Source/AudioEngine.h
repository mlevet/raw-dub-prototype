#pragma once
#include <JuceHeader.h>
#include "KickSynth.h"
#include "BassSynth.h"
#include "SkankSynth.h"
#include "StepPattern.h"
#include "SkankPatternSlot.h"
#include <atomic>
#include <vector>
#include <array>

namespace RawDub
{
// Instrument pattern banks - see project_raw_dub_song_architecture
// memory. Each instrument owns a bank of independently-saved patterns
// (Kick 1/2/3..., etc); AudioEngine tracks which slot is currently
// selected for live editing/playback per instrument. This is the
// foundational layer Global Patterns and Song mode will be built on
// top of later - neither exists yet. The musical material (steps,
// pitch, level, length, and for Skank the SawMix curve) lives in these
// banks, never inside the synth objects, which only ever hold
// synthesis parameters.
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
    void requestManualSkankTrigger() { manualSkankTriggerRequested.store (true); }

    void setTempoBpm (double bpm);
    double getTempoBpm() const { return tempoBpm.load(); }

    // "New Project" - stops transport, resets tempo/patterns/synth params
    // to their shipped defaults. Mutates existing atomics in place, never
    // reconstructs the engine, so it's safe to call from the message
    // thread while the audio thread may be mid-callback.
    void resetToDefaults();

    // Kick and Bass share one clock but loop at their own (independently
    // user-selectable, 4/16/32/64) pattern length - e.g. a 4-bar bassline
    // can play against a 1-bar kick loop. Since all valid lengths are
    // powers of two within {4,16,32,64}, the shorter one always divides
    // evenly into the longer one, so they never drift out of phase.
    int getCurrentKickStep() const { return globalStepAtomic.load() % kickPattern().getActiveLength(); }
    int getCurrentBassStep() const { return globalStepAtomic.load() % bassPattern().getActiveLength(); }
    int getCurrentSkankStep() const { return globalStepAtomic.load() % skankPattern().getActiveLength(); }

    KickSynth kick;
    BassSynth bass;
    SkankSynth skank;

    // Pattern banks: fixed size, matching StepPattern's own "always
    // allocate the max, only activeLength varies" philosophy - avoids
    // any dynamic growth complexity for a first pass. 1-indexed in the
    // UI, 0-indexed internally.
    static constexpr int bankSize = 8;

    std::vector<StepPattern> kickBank;
    std::vector<StepPattern> bassBank;
    std::vector<SkankPatternSlot> skankBank;

    int getCurrentKickPatternIndex() const { return currentKickIndex.load(); }
    int getCurrentBassPatternIndex() const { return currentBassIndex.load(); }
    int getCurrentSkankPatternIndex() const { return currentSkankIndex.load(); }
    void setCurrentKickPatternIndex (int index) { currentKickIndex.store (juce::jlimit (0, bankSize - 1, index)); }
    void setCurrentBassPatternIndex (int index) { currentBassIndex.store (juce::jlimit (0, bankSize - 1, index)); }
    void setCurrentSkankPatternIndex (int index) { currentSkankIndex.store (juce::jlimit (0, bankSize - 1, index)); }

    // Always refers to whichever bank slot is currently selected - the
    // "live" pattern being edited/played, same mental model as before
    // pattern banks existed, just now backed by a bank instead of a
    // single instance. currentXIndex is atomic: selected from the UI
    // thread, read every step from the audio thread.
    StepPattern& kickPattern() { return kickBank[(size_t) currentKickIndex.load()]; }
    const StepPattern& kickPattern() const { return kickBank[(size_t) currentKickIndex.load()]; }
    StepPattern& bassPattern() { return bassBank[(size_t) currentBassIndex.load()]; }
    const StepPattern& bassPattern() const { return bassBank[(size_t) currentBassIndex.load()]; }
    StepPattern& skankPattern() { return skankBank[(size_t) currentSkankIndex.load()].steps; }
    const StepPattern& skankPattern() const { return skankBank[(size_t) currentSkankIndex.load()].steps; }
    PointCurve& skankSawMixCurve() { return skankBank[(size_t) currentSkankIndex.load()].sawMixCurve; }
    const PointCurve& skankSawMixCurve() const { return skankBank[(size_t) currentSkankIndex.load()].sawMixCurve; }

    // Global Patterns - see project_raw_dub_song_architecture memory.
    // A Global Pattern has no musical data of its own; it's just a
    // saved combination of the three instruments' pattern-bank indices.
    // "used" distinguishes an actual saved combination from an empty
    // slot. Only ever touched from the UI thread (saving/recalling is a
    // deliberate action, never read directly by the audio thread - it
    // only ever goes through the already-atomic currentXIndex setters),
    // so no atomics needed here.
    struct GlobalPattern
    {
        bool used = false;
        int kickIndex = 0;
        int bassIndex = 0;
        int skankIndex = 0;
    };
    static constexpr int globalPatternBankSize = 8;
    std::array<GlobalPattern, globalPatternBankSize> globalPatterns;

    bool isGlobalPatternUsed (int slot) const { return globalPatterns[(size_t) slot].used; }

    // Explicit action only - never created automatically from live
    // jamming. Captures whatever the three instruments' current pattern
    // indices are right now.
    void saveCurrentAsGlobalPattern (int slot)
    {
        globalPatterns[(size_t) slot] = { true, getCurrentKickPatternIndex(), getCurrentBassPatternIndex(), getCurrentSkankPatternIndex() };
    }

    // For "Duplicate" - finds the next slot with no saved content, or
    // -1 if the whole bank is full.
    int findFirstEmptyGlobalPatternSlot() const
    {
        for (int i = 0; i < globalPatternBankSize; ++i)
            if (! globalPatterns[(size_t) i].used)
                return i;
        return -1;
    }

    // Switches all three instruments' current pattern index at once to
    // match the saved combination. Returns false (no-op) if the slot
    // was never saved.
    bool recallGlobalPattern (int slot)
    {
        auto& gp = globalPatterns[(size_t) slot];
        if (! gp.used)
            return false;

        setCurrentKickPatternIndex (gp.kickIndex);
        setCurrentBassPatternIndex (gp.bassIndex);
        setCurrentSkankPatternIndex (gp.skankIndex);
        return true;
    }

private:
    enum class TransportCmd { None, Play, Stop };

    void advanceStep();
    double samplesPerStep() const;

    std::atomic<int> currentKickIndex { 0 };
    std::atomic<int> currentBassIndex { 0 };
    std::atomic<int> currentSkankIndex { 0 };

    double sampleRate = 44100.0;
    std::atomic<double> tempoBpm { 120.0 };
    std::atomic<bool> playing { false };
    std::atomic<TransportCmd> pendingCommand { TransportCmd::None };
    std::atomic<bool> manualKickTriggerRequested { false };
    std::atomic<bool> manualBassTriggerRequested { false };
    std::atomic<bool> manualSkankTriggerRequested { false };

    // audio-thread-only transport state (mutated exclusively while
    // processing pendingCommand inside renderNextBlock)
    double samplePositionInStep = 0.0;
    int globalStep = 0;
    std::atomic<int> globalStepAtomic { 0 };
};
}
