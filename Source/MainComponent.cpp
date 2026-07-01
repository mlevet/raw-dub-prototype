#include "MainComponent.h"

namespace
{
struct ParamSpec
{
    const char* name;
    std::atomic<float>* param;
    double minV, maxV, step;
};
}

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

    addAndMakeVisible (tempoSlider);
    tempoSlider.setRange (60.0, 180.0, 1.0);
    tempoSlider.setValue (engine.getTempoBpm(), juce::dontSendNotification);
    tempoSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 24);
    tempoSlider.onValueChange = [this] { engine.setTempoBpm (tempoSlider.getValue()); };

    addAndMakeVisible (tempoLabel);
    tempoLabel.attachToComponent (&tempoSlider, true);

    // --- prototype-only accent style switcher ---
    addAndMakeVisible (accentStyleLabel);
    for (auto* btn : { &accentStyleAButton, &accentStyleBButton, &accentStyleCButton })
        addAndMakeVisible (*btn);
    accentStyleAButton.onClick = [this] { setAccentStyle (0); };
    accentStyleBButton.onClick = [this] { setAccentStyle (1); };
    accentStyleCButton.onClick = [this] { setAccentStyle (2); };

    // --- Kick ---
    addAndMakeVisible (kickTriggerButton);
    kickTriggerButton.onClick = [this] { engine.requestManualKickTrigger(); };

    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        auto& btn = kickStepButtons[(size_t) s];
        addAndMakeVisible (btn);
        btn.onToggle = [this, s]
        {
            engine.kickPattern.toggle (s);
            refreshStepColours();
        };
        btn.onLevelDrag = [this, s] (RawDub::StepLevel lvl)
        {
            engine.kickPattern.setLevel (s, lvl);
        };
    }

    addAndMakeVisible (kickTitleLabel);
    kickTitleLabel.setFont (juce::Font (20.0f, juce::Font::bold));

    {
        ParamSpec specs[4] = {
            { "Tune",  &engine.kick.tuneHz,  30.0, 150.0, 1.0  },
            { "Punch", &engine.kick.punchMs, 5.0,  120.0, 1.0  },
            { "Decay", &engine.kick.decayMs, 50.0, 800.0, 1.0  },
            { "Drive", &engine.kick.drive,   0.0,  1.0,   0.01 },
        };

        for (int i = 0; i < 4; ++i)
        {
            auto& row = kickParamRows[(size_t) i];

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
    }

    // --- Bass ---
    addAndMakeVisible (bassTriggerButton);
    bassTriggerButton.onClick = [this] { engine.requestManualBassTrigger(); };

    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        auto& btn = bassStepButtons[(size_t) s];
        btn.setHasPitch (true);
        addAndMakeVisible (btn);
        // local slot s always maps to (bassViewPage*numSteps + s) in the
        // underlying 64-step pattern - read bassViewPage fresh each click,
        // not captured, so it always reflects whichever page is showing
        btn.onToggle = [this, s]
        {
            engine.bassPattern.toggle (bassViewPage * RawDub::numSteps + s);
            refreshStepColours();
        };
        btn.onPitchDrag = [this, s] (int offset)
        {
            engine.bassPattern.setSemitoneOffset (bassViewPage * RawDub::numSteps + s, offset);
        };
        btn.onLevelDrag = [this, s] (RawDub::StepLevel lvl)
        {
            engine.bassPattern.setLevel (bassViewPage * RawDub::numSteps + s, lvl);
        };
    }

    for (int p = 0; p < (int) bassPageButtons.size(); ++p)
    {
        auto& btn = bassPageButtons[(size_t) p];
        btn.setButtonText (juce::String (p * RawDub::numSteps + 1) + "-" + juce::String ((p + 1) * RawDub::numSteps));
        addAndMakeVisible (btn);
        btn.onClick = [this, p]
        {
            bassViewPage = p;
            refreshStepColours();
        };
    }

    addAndMakeVisible (bassTitleLabel);
    bassTitleLabel.setFont (juce::Font (20.0f, juce::Font::bold));

    {
        // ordered to match the signal chain: oscillator (Tune) -> Drive
        // (saturation) -> filter (Cutoff/Resonance) -> envelope (Decay)
        ParamSpec specs[5] = {
            { "Tune",      &engine.bass.tuneHz,    30.0,  120.0,  1.0  },
            { "Drive",     &engine.bass.drive,     0.0,   1.0,    0.01 },
            { "Cutoff",    &engine.bass.cutoffHz,  100.0, 4000.0, 10.0 },
            { "Resonance", &engine.bass.resonance, 0.0,   0.95,   0.01 },
            { "Decay",     &engine.bass.decayMs,   50.0,  1000.0, 1.0  },
        };

        for (int i = 0; i < 5; ++i)
        {
            auto& row = bassParamRows[(size_t) i];

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
    }

    refreshStepColours();
    updatePlayButtonText();

    setAudioChannels (0, 2);
    setSize (820, 954);
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
    transport.removeFromLeft (100); // space for the tempo label
    tempoSlider.setBounds (transport.removeFromLeft (280));

    area.removeFromTop (12);

    auto accentRow = area.removeFromTop (28);
    accentStyleLabel.setBounds (accentRow.removeFromLeft (200));
    accentStyleAButton.setBounds (accentRow.removeFromLeft (40));
    accentRow.removeFromLeft (6);
    accentStyleBButton.setBounds (accentRow.removeFromLeft (40));
    accentRow.removeFromLeft (6);
    accentStyleCButton.setBounds (accentRow.removeFromLeft (40));

    area.removeFromTop (20);

    // --- Kick section ---
    auto kickHeader = area.removeFromTop (28);
    kickTitleLabel.setBounds (kickHeader.removeFromLeft (200));
    kickTriggerButton.setBounds (kickHeader.removeFromLeft (90));

    area.removeFromTop (8);

    auto kickStepRow = area.removeFromTop (50);
    const int kickStepWidth = kickStepRow.getWidth() / RawDub::numSteps;
    for (int s = 0; s < RawDub::numSteps; ++s)
        kickStepButtons[(size_t) s].setBounds (kickStepRow.removeFromLeft (kickStepWidth).reduced (2));

    area.removeFromTop (10);

    for (auto& row : kickParamRows)
    {
        auto rowArea = area.removeFromTop (36);
        row.label.setBounds (rowArea.removeFromLeft (70));
        row.slider.setBounds (rowArea);
        area.removeFromTop (8);
    }

    area.removeFromTop (24);

    // --- Bass section ---
    auto bassHeader = area.removeFromTop (28);
    bassTitleLabel.setBounds (bassHeader.removeFromLeft (200));
    bassTriggerButton.setBounds (bassHeader.removeFromLeft (90));
    bassHeader.removeFromLeft (20);
    for (auto& btn : bassPageButtons)
    {
        btn.setBounds (bassHeader.removeFromLeft (64));
        bassHeader.removeFromLeft (6);
    }

    area.removeFromTop (8);

    // Bass needs real vertical room: it's the only voice with pitch, and at
    // 50px (Kick's height) each semitone gets under 2px - indistinguishable.
    // 160px gives ~6px/semitone across the +/-12 range, enough to actually
    // read a contour.
    auto bassStepRow = area.removeFromTop (160);
    const int bassStepWidth = bassStepRow.getWidth() / RawDub::numSteps;
    for (int s = 0; s < RawDub::numSteps; ++s)
        bassStepButtons[(size_t) s].setBounds (bassStepRow.removeFromLeft (bassStepWidth).reduced (2));

    area.removeFromTop (10);

    for (auto& row : bassParamRows)
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

    int kickStep = engine.isPlaying() ? engine.getCurrentKickStep() : -1;
    int bassStep = engine.isPlaying() ? engine.getCurrentBassStep() : -1;

    if (kickStep != kickPlayheadStep || bassStep != bassPlayheadStep)
    {
        kickPlayheadStep = kickStep;
        bassPlayheadStep = bassStep;
        refreshStepColours();
    }
}

void MainComponent::refreshStepColours()
{
    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        auto& kickBtn = kickStepButtons[(size_t) s];
        kickBtn.setOn (engine.kickPattern.isOn (s));
        kickBtn.setLevel (engine.kickPattern.getLevel (s));
        kickBtn.setPlayhead (s == kickPlayheadStep);

        int bassIndex = bassViewPage * RawDub::numSteps + s;
        auto& bassBtn = bassStepButtons[(size_t) s];
        bassBtn.setOn (engine.bassPattern.isOn (bassIndex));
        bassBtn.setLevel (engine.bassPattern.getLevel (bassIndex));
        bassBtn.setSemitoneOffset (engine.bassPattern.getSemitoneOffset (bassIndex));
        bassBtn.setPlayhead (bassIndex == bassPlayheadStep);
    }

    // page buttons: black = the page you're viewing/editing, grey = the
    // page currently playing (if different) - same black/white/grey
    // language used everywhere else, just applied to page selection
    int playingPage = bassPlayheadStep >= 0 ? bassPlayheadStep / RawDub::numSteps : -1;
    for (int p = 0; p < (int) bassPageButtons.size(); ++p)
    {
        auto colour = juce::Colours::white;
        if (p == bassViewPage)
            colour = juce::Colours::black;
        else if (p == playingPage)
            colour = juce::Colours::lightgrey;

        auto textColour = (p == bassViewPage) ? juce::Colours::white : juce::Colours::black;

        bassPageButtons[(size_t) p].setColour (juce::TextButton::buttonColourId, colour);
        bassPageButtons[(size_t) p].setColour (juce::TextButton::textColourOffId, textColour);
    }
}

void MainComponent::updatePlayButtonText()
{
    playStopButton.setButtonText (engine.isPlaying() ? "Stop" : "Play");
}

void MainComponent::setAccentStyle (int style)
{
    for (auto& btn : kickStepButtons)
        btn.setAccentStyle (style);
    for (auto& btn : bassStepButtons)
        btn.setAccentStyle (style);
}
