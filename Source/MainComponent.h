#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "BWLookAndFeel.h"
#include "StepButton.h"
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

    RawDub::AudioEngine engine;
    RawDub::BWLookAndFeel lookAndFeel;

    juce::TextButton playStopButton { "Play" };
    juce::Slider tempoSlider;
    juce::Label  tempoLabel { {}, "Tempo" };

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

    // Kick
    juce::TextButton kickTriggerButton { "Trigger" };
    std::array<RawDub::StepButton, RawDub::numSteps> kickStepButtons;
    juce::Label kickTitleLabel { {}, "Kick" };
    std::array<ParamRow, 4> kickParamRows;

    // Bass
    juce::TextButton bassTriggerButton { "Trigger" };
    std::array<RawDub::StepButton, RawDub::numSteps> bassStepButtons;
    juce::Label bassTitleLabel { {}, "Bass" };
    std::array<ParamRow, 4> bassParamRows;

    int playheadStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
