#include "MainComponent.h"

MainComponent::MainComponent()
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (playStopButton);
    playStopButton.onClick = [this]
    {
        if (engine.isPlaying())
            engine.stop();
        else
            engine.play();
    };

    addAndMakeVisible (triggerButton);
    triggerButton.onClick = [this] { engine.requestManualTrigger(); };

    addAndMakeVisible (tempoSlider);
    tempoSlider.setRange (60.0, 180.0, 1.0);
    tempoSlider.setValue (engine.getTempoBpm(), juce::dontSendNotification);
    tempoSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 24);
    tempoSlider.onValueChange = [this] { engine.setTempoBpm (tempoSlider.getValue()); };

    addAndMakeVisible (tempoLabel);
    tempoLabel.attachToComponent (&tempoSlider, true);

    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        auto& btn = stepButtons[(size_t) s];
        btn.setClickingTogglesState (false);
        addAndMakeVisible (btn);
        btn.onClick = [this, s]
        {
            engine.toggleStep (s);
            refreshStepColours();
        };
    }

    addAndMakeVisible (titleLabel);
    titleLabel.setFont (juce::Font (20.0f, juce::Font::bold));

    struct ParamSpec
    {
        const char* name;
        std::atomic<float>* param;
        double minV, maxV, step;
    };

    ParamSpec specs[4] = {
        { "Tune",  &engine.kick.tuneHz,  30.0, 150.0, 1.0  },
        { "Punch", &engine.kick.punchMs, 5.0,  120.0, 1.0  },
        { "Decay", &engine.kick.decayMs, 50.0, 800.0, 1.0  },
        { "Drive", &engine.kick.drive,   0.0,  1.0,   0.01 },
    };

    for (int i = 0; i < 4; ++i)
    {
        auto& row = paramRows[(size_t) i];

        addAndMakeVisible (row.label);
        row.label.setText (specs[i].name, juce::dontSendNotification);

        addAndMakeVisible (row.slider);
        row.slider.setSliderStyle (juce::Slider::LinearHorizontal);
        row.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
        row.slider.setRange (specs[i].minV, specs[i].maxV, specs[i].step);
        row.slider.setValue ((double) specs[i].param->load(), juce::dontSendNotification);

        auto* param = specs[i].param;
        auto* slider = &row.slider;
        row.slider.onValueChange = [param, slider] { param->store ((float) slider->getValue()); };
    }

    refreshStepColours();
    updatePlayButtonText();

    setAudioChannels (0, 2);
    setSize (760, 460);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    setLookAndFeel (nullptr);
    shutdownAudio();
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    engine.prepare (sampleRate, samplesPerBlockExpected);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    juce::AudioBuffer<float> block (bufferToFill.buffer->getArrayOfWritePointers(),
                                     bufferToFill.buffer->getNumChannels(),
                                     bufferToFill.startSample,
                                     bufferToFill.numSamples);
    engine.renderNextBlock (block, bufferToFill.numSamples);
}

void MainComponent::releaseResources() {}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::white);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto transport = area.removeFromTop (44);
    playStopButton.setBounds (transport.removeFromLeft (90));
    transport.removeFromLeft (10);
    triggerButton.setBounds (transport.removeFromLeft (90));
    transport.removeFromLeft (90); // space for the tempo label
    tempoSlider.setBounds (transport.removeFromLeft (280));

    area.removeFromTop (20);

    auto stepRow = area.removeFromTop (60);
    const int stepWidth = stepRow.getWidth() / RawDub::numSteps;
    for (int s = 0; s < RawDub::numSteps; ++s)
        stepButtons[(size_t) s].setBounds (stepRow.removeFromLeft (stepWidth).reduced (2));

    area.removeFromTop (30);

    titleLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (10);

    for (auto& row : paramRows)
    {
        auto rowArea = area.removeFromTop (36);
        row.label.setBounds (rowArea.removeFromLeft (70));
        row.slider.setBounds (rowArea);
        area.removeFromTop (8);
    }
}

void MainComponent::timerCallback()
{
    updatePlayButtonText();

    int step = engine.isPlaying() ? engine.getCurrentStep() : -1;
    if (step != playheadStep)
    {
        playheadStep = step;
        refreshStepColours();
    }
}

void MainComponent::refreshStepColours()
{
    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        bool on = engine.getStep (s);
        bool isPlayhead = (s == playheadStep);

        juce::Colour colour = juce::Colours::white;
        if (on) colour = juce::Colours::black;
        if (isPlayhead) colour = on ? juce::Colours::darkgrey : juce::Colours::lightgrey;

        stepButtons[(size_t) s].setColour (juce::TextButton::buttonColourId, colour);
    }
}

void MainComponent::updatePlayButtonText()
{
    playStopButton.setButtonText (engine.isPlaying() ? "Stop" : "Play");
}