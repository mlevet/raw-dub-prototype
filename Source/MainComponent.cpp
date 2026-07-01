#include "MainComponent.h"

namespace
{
struct ParamSpec
{
    const char* name;
    std::atomic<float>* param;
    double minV, maxV, step;
};

constexpr int lengthOptions[4] = { 4, 16, 32, 64 };
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

    addAndMakeVisible (saveButton);
    saveButton.onClick = [this] { RawDub::ProjectIO::save (engine); };

    addAndMakeVisible (loadButton);
    loadButton.onClick = [this]
    {
        if (RawDub::ProjectIO::load (engine))
        {
            kickViewPage = 0;
            bassViewPage = 0;
            tempoSlider.setValue (engine.getTempoBpm(), juce::dontSendNotification);
            refreshParamSlidersFromEngine();
            refreshStepColours();
            resized();
        }
    };

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

    addAndMakeVisible (kickClearButton);
    kickClearButton.onClick = [this]
    {
        engine.kickPattern.clearAll();
        refreshStepColours();
    };

    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        auto& btn = kickStepButtons[(size_t) s];
        addAndMakeVisible (btn);
        // local slot s maps to (kickViewPage*numSteps + s) in the pattern -
        // read kickViewPage fresh each click, not captured, so it always
        // reflects whichever page is showing
        btn.onToggle = [this, s]
        {
            engine.kickPattern.toggle (kickViewPage * RawDub::numSteps + s);
            refreshStepColours();
        };
        btn.onLevelDrag = [this, s] (RawDub::StepLevel lvl)
        {
            engine.kickPattern.setLevel (kickViewPage * RawDub::numSteps + s, lvl);
        };
    }

    addAndMakeVisible (kickLengthLabel);
    for (int i = 0; i < 4; ++i)
    {
        auto& btn = kickLengthButtons[(size_t) i];
        btn.setButtonText (juce::String (lengthOptions[i]));
        addAndMakeVisible (btn);
        btn.onClick = [this, i] { setVoiceLength (engine.kickPattern, lengthOptions[i], kickViewPage); };
    }

    for (int p = 0; p < maxPages; ++p)
    {
        auto& btn = kickPageButtons[(size_t) p];
        btn.setButtonText (juce::String (p * RawDub::numSteps + 1) + "-" + juce::String ((p + 1) * RawDub::numSteps));
        addAndMakeVisible (btn);
        btn.onClick = [this, p]
        {
            kickViewPage = p;
            refreshStepColours();
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

    addAndMakeVisible (bassClearButton);
    bassClearButton.onClick = [this]
    {
        engine.bassPattern.clearAll();
        refreshStepColours();
    };

    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        auto& btn = bassStepButtons[(size_t) s];
        btn.setHasPitch (true);
        addAndMakeVisible (btn);
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

    addAndMakeVisible (bassLengthLabel);
    for (int i = 0; i < 4; ++i)
    {
        auto& btn = bassLengthButtons[(size_t) i];
        btn.setButtonText (juce::String (lengthOptions[i]));
        addAndMakeVisible (btn);
        btn.onClick = [this, i] { setVoiceLength (engine.bassPattern, lengthOptions[i], bassViewPage); };
    }

    for (int p = 0; p < maxPages; ++p)
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
        // (saturation) -> filter (Cutoff/Resonance) -> envelope (Length).
        // Length is hold time, not release speed - the release itself is
        // a short fixed internal tail (see BassSynth::releaseTau).
        ParamSpec specs[5] = {
            { "Tune",      &engine.bass.tuneHz,    30.0,  120.0,  1.0  },
            { "Drive",     &engine.bass.drive,     0.0,   1.0,    0.01 },
            { "Cutoff",    &engine.bass.cutoffHz,  100.0, 4000.0, 10.0 },
            { "Resonance", &engine.bass.resonance, 0.0,   0.95,   0.01 },
            { "Length",    &engine.bass.decayMs,   50.0,  4000.0, 10.0 },
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
    setSize (820, 1040);
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

void MainComponent::layoutStepRow (juce::Rectangle<int> stepRow, std::array<RawDub::StepButton, RawDub::numSteps>& buttons,
                                    int activeLength, int viewPage)
{
    int stepsOnThisPage = juce::jlimit (0, RawDub::numSteps, activeLength - viewPage * RawDub::numSteps);
    int stepWidth = stepsOnThisPage > 0 ? stepRow.getWidth() / stepsOnThisPage : 0;

    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        if (s < stepsOnThisPage)
        {
            buttons[(size_t) s].setVisible (true);
            buttons[(size_t) s].setBounds (stepRow.removeFromLeft (stepWidth).reduced (2));
        }
        else
        {
            buttons[(size_t) s].setVisible (false);
        }
    }
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto transport = area.removeFromTop (44);
    playStopButton.setBounds (transport.removeFromLeft (90));
    transport.removeFromLeft (100); // space for the tempo label
    tempoSlider.setBounds (transport.removeFromLeft (280));
    transport.removeFromLeft (20);
    saveButton.setBounds (transport.removeFromLeft (70));
    transport.removeFromLeft (6);
    loadButton.setBounds (transport.removeFromLeft (70));

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
    kickHeader.removeFromLeft (6);
    kickClearButton.setBounds (kickHeader.removeFromLeft (70));

    area.removeFromTop (8);

    auto kickLengthRow = area.removeFromTop (28);
    kickLengthLabel.setBounds (kickLengthRow.removeFromLeft (60));
    for (auto& btn : kickLengthButtons)
    {
        btn.setBounds (kickLengthRow.removeFromLeft (44));
        kickLengthRow.removeFromLeft (6);
    }
    kickLengthRow.removeFromLeft (20);
    int kickNumPages = (engine.kickPattern.getActiveLength() + RawDub::numSteps - 1) / RawDub::numSteps;
    for (int p = 0; p < maxPages; ++p)
    {
        kickPageButtons[(size_t) p].setVisible (p < kickNumPages && kickNumPages > 1);
        kickPageButtons[(size_t) p].setBounds (kickLengthRow.removeFromLeft (64));
        kickLengthRow.removeFromLeft (6);
    }

    area.removeFromTop (8);

    auto kickStepRow = area.removeFromTop (50);
    layoutStepRow (kickStepRow, kickStepButtons, engine.kickPattern.getActiveLength(), kickViewPage);

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
    bassHeader.removeFromLeft (6);
    bassClearButton.setBounds (bassHeader.removeFromLeft (70));

    area.removeFromTop (8);

    auto bassLengthRow = area.removeFromTop (28);
    bassLengthLabel.setBounds (bassLengthRow.removeFromLeft (60));
    for (auto& btn : bassLengthButtons)
    {
        btn.setBounds (bassLengthRow.removeFromLeft (44));
        bassLengthRow.removeFromLeft (6);
    }
    bassLengthRow.removeFromLeft (20);
    int bassNumPages = (engine.bassPattern.getActiveLength() + RawDub::numSteps - 1) / RawDub::numSteps;
    for (int p = 0; p < maxPages; ++p)
    {
        bassPageButtons[(size_t) p].setVisible (p < bassNumPages && bassNumPages > 1);
        bassPageButtons[(size_t) p].setBounds (bassLengthRow.removeFromLeft (64));
        bassLengthRow.removeFromLeft (6);
    }

    area.removeFromTop (8);

    // Bass needs real vertical room: it's the only voice with pitch, and at
    // 50px (Kick's height) each semitone gets under 2px - indistinguishable.
    // 160px gives ~6px/semitone across the +/-12 range, enough to actually
    // read a contour.
    auto bassStepRow = area.removeFromTop (160);
    layoutStepRow (bassStepRow, bassStepButtons, engine.bassPattern.getActiveLength(), bassViewPage);

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
    int kickLength = engine.kickPattern.getActiveLength();
    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        int kickIndex = kickViewPage * RawDub::numSteps + s;
        if (kickIndex >= kickLength)
            continue;

        auto& kickBtn = kickStepButtons[(size_t) s];
        kickBtn.setOn (engine.kickPattern.isOn (kickIndex));
        kickBtn.setLevel (engine.kickPattern.getLevel (kickIndex));
        kickBtn.setPlayhead (kickIndex == kickPlayheadStep);
    }

    int bassLength = engine.bassPattern.getActiveLength();
    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        int bassIndex = bassViewPage * RawDub::numSteps + s;
        if (bassIndex >= bassLength)
            continue;

        auto& bassBtn = bassStepButtons[(size_t) s];
        bassBtn.setOn (engine.bassPattern.isOn (bassIndex));
        bassBtn.setLevel (engine.bassPattern.getLevel (bassIndex));
        bassBtn.setSemitoneOffset (engine.bassPattern.getSemitoneOffset (bassIndex));
        bassBtn.setPlayhead (bassIndex == bassPlayheadStep);
    }

    // page buttons: black = the page you're viewing/editing, grey = the
    // page currently playing (if different) - same black/white/grey
    // language used everywhere else, just applied to page selection
    auto refreshPageButtons = [] (std::array<juce::TextButton, MainComponent::maxPages>& pageButtons,
                                   int viewPage, int playheadStep)
    {
        int playingPage = playheadStep >= 0 ? playheadStep / RawDub::numSteps : -1;
        for (int p = 0; p < (int) pageButtons.size(); ++p)
        {
            auto colour = juce::Colours::white;
            if (p == viewPage)
                colour = juce::Colours::black;
            else if (p == playingPage)
                colour = juce::Colours::lightgrey;

            auto textColour = (p == viewPage) ? juce::Colours::white : juce::Colours::black;

            pageButtons[(size_t) p].setColour (juce::TextButton::buttonColourId, colour);
            pageButtons[(size_t) p].setColour (juce::TextButton::textColourOffId, textColour);
        }
    };

    refreshPageButtons (kickPageButtons, kickViewPage, kickPlayheadStep);
    refreshPageButtons (bassPageButtons, bassViewPage, bassPlayheadStep);

    // length buttons: black = the length currently active for that voice -
    // same convention as page buttons, so you can tell each voice's
    // length at a glance instead of inferring it from the step row
    auto refreshLengthButtons = [] (std::array<juce::TextButton, 4>& lengthButtons, int activeLength)
    {
        for (int i = 0; i < 4; ++i)
        {
            bool isActive = (lengthOptions[i] == activeLength);
            lengthButtons[(size_t) i].setColour (juce::TextButton::buttonColourId, isActive ? juce::Colours::black : juce::Colours::white);
            lengthButtons[(size_t) i].setColour (juce::TextButton::textColourOffId, isActive ? juce::Colours::white : juce::Colours::black);
        }
    };

    refreshLengthButtons (kickLengthButtons, engine.kickPattern.getActiveLength());
    refreshLengthButtons (bassLengthButtons, engine.bassPattern.getActiveLength());
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

// order must match the ParamSpec arrays set up in the constructor -
// Kick: Tune/Punch/Decay/Drive, Bass: Tune/Drive/Cutoff/Resonance/Length
void MainComponent::refreshParamSlidersFromEngine()
{
    kickParamRows[0].slider.setValue ((double) engine.kick.tuneHz.load(),  juce::dontSendNotification);
    kickParamRows[1].slider.setValue ((double) engine.kick.punchMs.load(), juce::dontSendNotification);
    kickParamRows[2].slider.setValue ((double) engine.kick.decayMs.load(), juce::dontSendNotification);
    kickParamRows[3].slider.setValue ((double) engine.kick.drive.load(),   juce::dontSendNotification);

    bassParamRows[0].slider.setValue ((double) engine.bass.tuneHz.load(),    juce::dontSendNotification);
    bassParamRows[1].slider.setValue ((double) engine.bass.drive.load(),     juce::dontSendNotification);
    bassParamRows[2].slider.setValue ((double) engine.bass.cutoffHz.load(),  juce::dontSendNotification);
    bassParamRows[3].slider.setValue ((double) engine.bass.resonance.load(), juce::dontSendNotification);
    bassParamRows[4].slider.setValue ((double) engine.bass.decayMs.load(),   juce::dontSendNotification);
}

void MainComponent::setVoiceLength (RawDub::StepPattern& pattern, int newLength, int& viewPage)
{
    pattern.setActiveLength (newLength);
    viewPage = 0;
    refreshStepColours();
    resized();
}
