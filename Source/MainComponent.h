#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "BWLookAndFeel.h"
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

    RawDub::AudioEngine engine;
    RawDub::BWLookAndFeel lookAndFeel;

    juce::TextButton playStopButton { "Play" };
    juce::TextButton triggerButton  { "Trigger" };
    juce::Slider tempoSlider;
    juce::Label  tempoLabel { {}, "Tempo" };

    std::array<juce::TextButton, RawDub::numSteps> stepButtons;

    juce::Label titleLabel { {}, "Kick" };

    struct ParamRow
    {
        juce::Label label;
        juce::Slider slider;
    };

    std::array<ParamRow, 4> paramRows;

    int playheadStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};