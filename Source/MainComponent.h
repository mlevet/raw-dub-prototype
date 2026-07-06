#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "BWLookAndFeel.h"
#include "StepButton.h"
#include "CurveLaneEditor.h"
#include "ProjectIO.h"
#include <array>
#include <vector>

class MainComponent : public juce::AudioAppComponent, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshStepColours();
    void refreshGlobalPatternButtons();
    void updatePlayButtonText();
    void refreshParamSlidersFromEngine();

    // shared between Kick and Bass: changing length resets the view to
    // page 1; resized() recomputes page-button visibility from the new
    // active length
    void setVoiceLength (RawDub::StepPattern& pattern, int newLength, int& viewPage);
    void layoutStepRow (juce::Rectangle<int> stepRow, std::array<RawDub::StepButton, RawDub::numSteps>& buttons,
                        int activeLength, int viewPage);

    static constexpr int maxPages = RawDub::StepPattern::maxLength / RawDub::numSteps; // 4

    RawDub::AudioEngine engine;
    RawDub::BWLookAndFeel lookAndFeel;

    juce::TextButton playStopButton { "Play" };
    juce::Slider tempoSlider;
    juce::Label  tempoLabel { {}, "Tempo" };
    // multi-project: each project is a standalone JSON file, chosen via a
    // normal file chooser. Save writes to currentProjectFile if one is
    // already set, else behaves like Save As. currentProjectFile empty =
    // unsaved new project.
    juce::TextButton saveButton { "Save" };
    juce::TextButton saveAsButton { "Save As..." };
    juce::TextButton openButton { "Open..." };
    juce::TextButton newProjectButton { "New" };
    juce::File currentProjectFile;
    std::unique_ptr<juce::FileChooser> fileChooser; // must outlive an async chooser dialog

    // Global Patterns - see project_raw_dub_song_architecture memory. A
    // Global Pattern has no musical data of its own, just a saved
    // combination of the three instruments' current pattern-bank
    // indices. There is always a "current" one (like there's always a
    // current pattern per instrument) - clicking a number recalls it
    // (if it has content) and makes it the thing you're now editing.
    // Save always writes to whichever one is current - never a mode,
    // never a target to pick. Duplicate branches off into the next
    // free slot and switches editing there, for "this deserves to
    // become its own pattern" without disturbing the original.
    // Project-level, not per-instrument, so it sits above the
    // instrument sections.
    juce::Label globalPatternsLabel { {}, "Global Patterns" };
    std::array<juce::TextButton, RawDub::AudioEngine::globalPatternBankSize> globalPatternButtons;
    juce::TextButton saveGlobalPatternButton { "Save" };
    juce::TextButton duplicateGlobalPatternButton { "Duplicate" };
    // Erases the current slot's saved combination/overrides (back to
    // empty) - doesn't touch the instruments' live state or any
    // instrument pattern's actual content, only this Global Pattern's
    // own reference.
    juce::TextButton clearGlobalPatternButton { "Clear" };
    // "current slot" itself now lives on AudioEngine (see
    // getCurrentGlobalPatternSlot/setCurrentGlobalPatternSlot) - it
    // determines which section's voicing overrides apply during
    // playback, not just which button is highlighted, so the engine is
    // the single source of truth rather than MainComponent keeping its
    // own copy.

    struct ParamRow
    {
        juce::Label label;
        juce::Slider slider;
    };

    // Only one instrument's section is shown at a time - the others are
    // hidden entirely rather than stacked/scrolled. Each instrument's
    // full set of components is collected into one of these vectors
    // (populated once, in the constructor) so switching tabs is just
    // "hide these, show those" instead of touching every component
    // individually inline. currentInstrumentTab: 0=Kick, 1=Bass, 2=Skank.
    void switchInstrumentTab (int tab);
    void layoutKickSection (juce::Rectangle<int> area);
    void layoutBassSection (juce::Rectangle<int> area);
    void layoutSkankSection (juce::Rectangle<int> area);
    int currentInstrumentTab = 0;
    std::array<juce::TextButton, 3> instrumentTabButtons; // Kick / Bass / Skank
    std::vector<juce::Component*> kickComponents, bassComponents, skankComponents;

    // Kick - length selectable 4/16/32/64, shown/edited 16 steps per page,
    // same paging mechanism as Bass.
    juce::TextButton kickTriggerButton { "Trigger" };
    juce::TextButton kickClearButton { "Clear" };
    std::array<RawDub::StepButton, RawDub::numSteps> kickStepButtons;
    juce::Label kickTitleLabel { {}, "Kick" };
    std::array<ParamRow, 5> kickParamRows; // Tune, Punch, Decay, Drive, Volume
    juce::Label kickLengthLabel { {}, "Length" };
    std::array<juce::TextButton, 4> kickLengthButtons;
    std::array<juce::TextButton, maxPages> kickPageButtons;
    int kickViewPage = 0;

    // Instrument pattern bank - see project_raw_dub_song_architecture
    // memory. Selects which of AudioEngine::bankSize saved patterns is
    // currently live for editing/playback. 1-indexed in the UI, 0-indexed
    // internally (AudioEngine::setCurrentKickPatternIndex etc).
    juce::Label kickPatternBankLabel { {}, "Pattern" };
    std::array<juce::TextButton, RawDub::AudioEngine::bankSize> kickPatternBankButtons;

    // "Make Unique" - see AudioEngine::makeKickPatternUnique and
    // project_raw_dub_song_architecture memory (shared instrument
    // pattern lifecycle). Sharing across Global Patterns is normal and
    // often wanted; this pair only becomes visible when the current
    // Kick pattern is actually referenced by more than one, so editing
    // in the common (unshared) case stays exactly as frictionless as
    // before this existed.
    juce::Label kickSharedLabel;
    juce::TextButton kickMakeUniqueButton { "Make Unique" };

    // Bass - same idea, length selectable 4/16/32/64.
    juce::TextButton bassTriggerButton { "Trigger" };
    juce::TextButton bassClearButton { "Clear" };
    std::array<RawDub::StepButton, RawDub::numSteps> bassStepButtons;
    juce::Label bassTitleLabel { {}, "Bass" };
    std::array<ParamRow, 7> bassParamRows; // Tune, Drive, Cutoff, Resonance, Length, AM Depth, Volume
    juce::Label bassLengthLabel { {}, "Length" };
    std::array<juce::TextButton, 4> bassLengthButtons;
    std::array<juce::TextButton, maxPages> bassPageButtons;
    int bassViewPage = 0;

    juce::Label bassPatternBankLabel { {}, "Pattern" };
    std::array<juce::TextButton, RawDub::AudioEngine::bankSize> bassPatternBankButtons;

    // "Make Unique" - see kickSharedLabel above for the full explanation
    juce::Label bassSharedLabel;
    juce::TextButton bassMakeUniqueButton { "Make Unique" };

    // research switch, not permanent UI - see BassSynth::useAMMode.
    // Ratio deliberately discrete (not a slider) - only simple integer
    // ratios keep AM mode harmonic instead of gong-like.
    juce::TextButton bassHarmonicModeButton { "Harmonic: Drive" };
    juce::Label bassAmRatioLabel { {}, "AM Ratio" };
    std::array<juce::TextButton, 3> bassAmRatioButtons; // 1:1, 2:1, 3:1

    // Section-level voicing overrides for Drive/Cutoff - see
    // AudioEngine::ParamOverride and project_raw_dub_song_architecture
    // memory. Off: the Drive/Cutoff slider edits the instrument's base
    // value (bass.drive/cutoffHz), same as every other param. On: the
    // slider instead edits the CURRENT Global Pattern's override, base
    // left untouched - so the same Bass pattern can voice differently
    // per section without ever touching every pattern that shares it.
    // Deliberately just these two params for now, not a generic
    // per-param mechanism - Volume stays global (a future mixer/
    // performance concern, not this voicing pass).
    juce::TextButton bassDriveOverrideButton { "Override" };
    juce::TextButton bassCutoffOverrideButton { "Override" };
    void refreshBassOverrideControls();

    int kickPlayheadStep = -1;
    int bassPlayheadStep = -1;

    // Skank - now sequenced like Kick/Bass (variable length, paging,
    // per-step pitch for progressions - see SkankSynth.h). Chord
    // shape/voicing is deliberately still fixed (Major/Minor triad,
    // global not per-step) - synthesis is not finished, this is about
    // judging the instrument in musical context, not a final build.
    juce::TextButton skankTriggerButton { "Trigger" };
    juce::TextButton skankClearButton { "Clear" };
    std::array<RawDub::StepButton, RawDub::numSteps> skankStepButtons;
    juce::Label skankTitleLabel { {}, "Skank" };
    juce::TextButton skankMajorButton { "Major" };
    juce::TextButton skankMinorButton { "Minor" };
    std::array<ParamRow, 5> skankParamRows; // Tune, SawMix, Decay, Drive, Volume
    juce::Label skankLengthLabel { {}, "Length" };
    std::array<juce::TextButton, 4> skankLengthButtons;
    std::array<juce::TextButton, maxPages> skankPageButtons;
    int skankViewPage = 0;
    int skankPlayheadStep = -1;

    juce::Label skankPatternBankLabel { {}, "Pattern" };
    std::array<juce::TextButton, RawDub::AudioEngine::bankSize> skankPatternBankButtons;

    // "Make Unique" - see kickSharedLabel above for the full explanation
    juce::Label skankSharedLabel;
    juce::TextButton skankMakeUniqueButton { "Make Unique" };
    void refreshPatternSharingIndicators();

    // Section-level voicing override for Decay - same idea as Bass's
    // Drive/Cutoff overrides (see bassDriveOverrideButton). SawMix
    // deliberately doesn't get one of these: it's already always a
    // curve (see skankSawMixLaneEditor below), and a fixed "override"
    // is just a flat curve - no separate mechanism needed for it.
    juce::TextButton skankDecayOverrideButton { "Override" };
    void refreshSkankOverrideControls();

    // sparse draggable-point SawMix curve - see CurveLaneEditor.h and
    // PointCurve (lives in the current Skank pattern slot, via
    // AudioEngine::skankSawMixCurve() - not in SkankSynth, see
    // project_raw_dub_song_architecture memory). Deliberately scoped to
    // SawMix only, no generic modulation routing. Moving the SawMix
    // slider flattens the curve to the slider's value (slider = static
    // state, curve = animated state).
    juce::Label skankSawMixLaneLabel { {}, "SawMix Curve" };
    RawDub::CurveLaneEditor skankSawMixLaneEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
