#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "BWLookAndFeel.h"
#include "StepButton.h"
#include "CurveLaneEditor.h"
#include "ProjectIO.h"
#include <array>

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
    void updatePlayButtonText();
    void setAccentStyle (int style);
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

    // prototype-only: comparing accent visual treatments, remove once decided
    juce::Label accentStyleLabel { {}, "Accent style (prototype)" };
    juce::TextButton accentStyleAButton { "A" };
    juce::TextButton accentStyleBButton { "B" };
    juce::TextButton accentStyleCButton { "C" };

    struct ParamRow
    {
        juce::Label label;
        juce::Slider slider;
    };

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

    // research switch, not permanent UI - see BassSynth::useAMMode.
    // Ratio deliberately discrete (not a slider) - only simple integer
    // ratios keep AM mode harmonic instead of gong-like.
    juce::TextButton bassHarmonicModeButton { "Harmonic: Drive" };
    juce::Label bassAmRatioLabel { {}, "AM Ratio" };
    std::array<juce::TextButton, 3> bassAmRatioButtons; // 1:1, 2:1, 3:1

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

    // sparse draggable-point SawMix curve - see CurveLaneEditor.h and
    // SkankSynth's sawMixCurve*  methods. Deliberately scoped to SawMix
    // only, no generic modulation routing. Moving the SawMix slider
    // flattens this curve to the slider's value (slider = static state,
    // curve = animated state - see feedback_raw_dub_experiment_protocol
    // memory for the general principle).
    juce::Label skankSawMixLaneLabel { {}, "SawMix Curve" };
    RawDub::CurveLaneEditor skankSawMixLaneEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
