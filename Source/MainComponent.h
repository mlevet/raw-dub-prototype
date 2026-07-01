#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "BWLookAndFeel.h"
#include "StepButton.h"
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
    juce::TextButton saveButton { "Save" };
    juce::TextButton loadButton { "Load" };

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
    std::array<ParamRow, 4> kickParamRows;
    juce::Label kickLengthLabel { {}, "Length" };
    std::array<juce::TextButton, 4> kickLengthButtons;
    std::array<juce::TextButton, maxPages> kickPageButtons;
    int kickViewPage = 0;

    // Bass - same idea, length selectable 4/16/32/64.
    juce::TextButton bassTriggerButton { "Trigger" };
    juce::TextButton bassClearButton { "Clear" };
    std::array<RawDub::StepButton, RawDub::numSteps> bassStepButtons;
    juce::Label bassTitleLabel { {}, "Bass" };
    std::array<ParamRow, 5> bassParamRows;
    juce::Label bassLengthLabel { {}, "Length" };
    std::array<juce::TextButton, 4> bassLengthButtons;
    std::array<juce::TextButton, maxPages> bassPageButtons;
    int bassViewPage = 0;

    int kickPlayheadStep = -1;
    int bassPlayheadStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
