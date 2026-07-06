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
    static constexpr int bankSize = 16;

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

    // Section-level voicing override (see project_raw_dub_song_architecture
    // memory: instrument params are the track's base identity, Global
    // Pattern overrides are section variations - "verse bass darker,
    // bridge bass more driven"). Read on the audio thread (advanceStep,
    // for whichever Global Pattern is current) and written from the UI
    // thread (toggling/dragging the override), so both fields are
    // atomic - same reasoning as StepPattern's per-step atomics.
    struct ParamOverride
    {
        std::atomic<bool> active { false };
        std::atomic<float> value { 0.0f };
    };

    // Global Patterns - see project_raw_dub_song_architecture memory.
    // A Global Pattern otherwise has no musical data of its own; it's
    // just a saved combination of the three instruments' pattern-bank
    // indices, plus these optional voicing overrides. "used" distinguishes
    // an actual saved combination from an empty slot. kickIndex/bassIndex/
    // skankIndex are only ever touched from the UI thread (saving/
    // recalling is a deliberate action; the audio thread only ever reads
    // the already-atomic currentXIndex setters they get copied into) -
    // but bassDriveOverride/bassCutoffOverride/skankDecayOverride ARE
    // read directly by the audio thread, hence ParamOverride's atomics.
    // Because ParamOverride holds atomics, GlobalPattern can't be
    // copy-assigned as a whole (see saveCurrentAsGlobalPattern/reset()
    // below, which mutate fields individually instead).
    struct GlobalPattern
    {
        bool used = false;
        int kickIndex = 0;
        int bassIndex = 0;
        int skankIndex = 0;
        ParamOverride bassDriveOverride;
        ParamOverride bassCutoffOverride;
        ParamOverride skankDecayOverride;

        void reset()
        {
            used = false;
            kickIndex = bassIndex = skankIndex = 0;
            bassDriveOverride.active.store (false);
            bassDriveOverride.value.store (0.0f);
            bassCutoffOverride.active.store (false);
            bassCutoffOverride.value.store (0.0f);
            skankDecayOverride.active.store (false);
            skankDecayOverride.value.store (0.0f);
        }
    };
    static constexpr int globalPatternBankSize = 16;
    std::array<GlobalPattern, globalPatternBankSize> globalPatterns;

    bool isGlobalPatternUsed (int slot) const { return globalPatterns[(size_t) slot].used; }

    int getCurrentGlobalPatternSlot() const { return currentGlobalPatternSlot.load(); }
    // UI-driven only - just tracks "what am I currently editing," but
    // also now determines which slot's overrides apply during playback,
    // so the audio thread needs to read it too (see advanceStep).
    void setCurrentGlobalPatternSlot (int slot) { currentGlobalPatternSlot.store (slot); }

    // Explicit action only - never created automatically from live
    // jamming. Captures whatever the three instruments' current pattern
    // indices are right now. Deliberately leaves any existing overrides
    // on this slot untouched - those are edited through their own
    // controls, not reset every time the pattern combination is saved.
    void saveCurrentAsGlobalPattern (int slot)
    {
        auto& gp = globalPatterns[(size_t) slot];
        gp.used = true;
        gp.kickIndex = getCurrentKickPatternIndex();
        gp.bassIndex = getCurrentBassPatternIndex();
        gp.skankIndex = getCurrentSkankPatternIndex();
    }

    // Erases this slot's saved combination and overrides (back to an
    // empty/unused slot) - doesn't touch the instruments' live state or
    // any instrument pattern's actual content, only this Global
    // Pattern's own saved reference. Stays "current" either way, same as
    // any other slot - clearing doesn't move you elsewhere.
    void clearGlobalPattern (int slot)
    {
        globalPatterns[(size_t) slot].reset();
    }

    // For "Duplicate" - an override IS part of what's currently live (the
    // slider shows it when active), so branching off the current pattern
    // must carry every section-voicing override along, not silently
    // drop back to the base sound in the new slot.
    void copySectionVoicingOverrides (int fromSlot, int toSlot)
    {
        auto& src = globalPatterns[(size_t) fromSlot];
        auto& dst = globalPatterns[(size_t) toSlot];
        dst.bassDriveOverride.active.store (src.bassDriveOverride.active.load());
        dst.bassDriveOverride.value.store (src.bassDriveOverride.value.load());
        dst.bassCutoffOverride.active.store (src.bassCutoffOverride.active.load());
        dst.bassCutoffOverride.value.store (src.bassCutoffOverride.value.load());
        dst.skankDecayOverride.active.store (src.skankDecayOverride.active.load());
        dst.skankDecayOverride.value.store (src.skankDecayOverride.value.load());
    }

    // For "Duplicate" - finds the next slot with no saved content, or
    // -1 if the whole bank is full. excludeSlot must never be returned:
    // duplicating the current pattern into itself (both empty, e.g. a
    // fresh project sitting on Pattern 1) would be a silent no-op -
    // nothing visibly changes because the "new" slot IS the current one.
    int findFirstEmptyGlobalPatternSlot (int excludeSlot) const
    {
        for (int i = 0; i < globalPatternBankSize; ++i)
            if (i != excludeSlot && ! globalPatterns[(size_t) i].used)
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

    // "Make Unique" (see project_raw_dub_song_architecture memory,
    // shared instrument pattern lifecycle). Per-instrument granularity:
    // if the pattern you're about to edit is shared by more than one
    // Global Pattern, forking only THAT instrument's pattern - not all
    // three - lets you make this section's edit local without silently
    // changing every other section that happens to reuse the same
    // pattern. Sharing is normal/wanted (e.g. same Bass across two
    // sections, different Skank) - it's editing a SHARED pattern in
    // place that would be the surprise, not the sharing itself.

    // Which Global Patterns (1-indexed slot numbers, used ones only)
    // reference a given instrument pattern index right now.
    std::vector<int> globalPatternsReferencingKick (int kickIndex) const
    {
        std::vector<int> result;
        for (int i = 0; i < globalPatternBankSize; ++i)
            if (globalPatterns[(size_t) i].used && globalPatterns[(size_t) i].kickIndex == kickIndex)
                result.push_back (i + 1);
        return result;
    }
    std::vector<int> globalPatternsReferencingBass (int bassIndex) const
    {
        std::vector<int> result;
        for (int i = 0; i < globalPatternBankSize; ++i)
            if (globalPatterns[(size_t) i].used && globalPatterns[(size_t) i].bassIndex == bassIndex)
                result.push_back (i + 1);
        return result;
    }
    std::vector<int> globalPatternsReferencingSkank (int skankIndex) const
    {
        std::vector<int> result;
        for (int i = 0; i < globalPatternBankSize; ++i)
            if (globalPatterns[(size_t) i].used && globalPatterns[(size_t) i].skankIndex == skankIndex)
                result.push_back (i + 1);
        return result;
    }

    // Finds a bank slot with no notes on - safe to treat as an available
    // fork destination, since instrument pattern banks (unlike Global
    // Patterns) have no explicit "used" flag of their own to check instead.
    int findEmptyKickPatternSlot() const
    {
        for (int i = 0; i < bankSize; ++i)
            if (kickBank[(size_t) i].isEmpty())
                return i;
        return -1;
    }
    int findEmptyBassPatternSlot() const
    {
        for (int i = 0; i < bankSize; ++i)
            if (bassBank[(size_t) i].isEmpty())
                return i;
        return -1;
    }
    int findEmptySkankPatternSlot() const
    {
        for (int i = 0; i < bankSize; ++i)
            if (skankBank[(size_t) i].isEmpty())
                return i;
        return -1;
    }

    // Forks the CURRENT pattern into a free bank slot and rewires only
    // the current Global Pattern to it - other Global Patterns that
    // shared the old slot keep pointing at it, untouched. Returns the
    // new slot index (0-indexed), or -1 if the bank is full.
    int makeKickPatternUnique()
    {
        int currentIdx = getCurrentKickPatternIndex();
        int freeSlot = findEmptyKickPatternSlot();
        if (freeSlot < 0)
            return -1;

        kickBank[(size_t) freeSlot].copyFrom (kickBank[(size_t) currentIdx]);
        setCurrentKickPatternIndex (freeSlot);
        saveCurrentAsGlobalPattern (getCurrentGlobalPatternSlot());
        return freeSlot;
    }
    int makeBassPatternUnique()
    {
        int currentIdx = getCurrentBassPatternIndex();
        int freeSlot = findEmptyBassPatternSlot();
        if (freeSlot < 0)
            return -1;

        bassBank[(size_t) freeSlot].copyFrom (bassBank[(size_t) currentIdx]);
        setCurrentBassPatternIndex (freeSlot);
        saveCurrentAsGlobalPattern (getCurrentGlobalPatternSlot());
        return freeSlot;
    }
    int makeSkankPatternUnique()
    {
        int currentIdx = getCurrentSkankPatternIndex();
        int freeSlot = findEmptySkankPatternSlot();
        if (freeSlot < 0)
            return -1;

        skankBank[(size_t) freeSlot].copyFrom (skankBank[(size_t) currentIdx]);
        setCurrentSkankPatternIndex (freeSlot);
        saveCurrentAsGlobalPattern (getCurrentGlobalPatternSlot());
        return freeSlot;
    }

private:
    enum class TransportCmd { None, Play, Stop };

    void advanceStep();
    double samplesPerStep() const;

    std::atomic<int> currentKickIndex { 0 };
    std::atomic<int> currentBassIndex { 0 };
    std::atomic<int> currentSkankIndex { 0 };
    std::atomic<int> currentGlobalPatternSlot { 0 }; // always valid, defaults to Pattern 1

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
