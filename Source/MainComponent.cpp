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

    // an existing single-slot save from before multi-project support -
    // auto-load it so nothing already on disk is silently lost, and treat
    // it as the starting "current file" so a plain Save still works
    {
        auto legacyFile = RawDub::ProjectIO::getDefaultProjectFile();
        if (legacyFile.existsAsFile() && RawDub::ProjectIO::load (engine, legacyFile))
            currentProjectFile = legacyFile;
    }

    addAndMakeVisible (saveButton);
    saveButton.onClick = [this]
    {
        if (currentProjectFile != juce::File())
            RawDub::ProjectIO::save (engine, currentProjectFile);
        else
            saveAsButton.triggerClick();
    };

    addAndMakeVisible (saveAsButton);
    saveAsButton.onClick = [this]
    {
        auto initial = currentProjectFile != juce::File()
                           ? currentProjectFile
                           : RawDub::ProjectIO::getDefaultProjectFile().getParentDirectory().getChildFile ("Untitled.json");

        fileChooser = std::make_unique<juce::FileChooser> ("Save Project As...", initial, "*.json");
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File())
                    return;
                if (! file.hasFileExtension ("json"))
                    file = file.withFileExtension ("json");

                if (RawDub::ProjectIO::save (engine, file))
                    currentProjectFile = file;
            });
    };

    addAndMakeVisible (openButton);
    openButton.onClick = [this]
    {
        auto initial = currentProjectFile != juce::File() ? currentProjectFile : RawDub::ProjectIO::getDefaultProjectFile();

        fileChooser = std::make_unique<juce::FileChooser> ("Open Project...", initial, "*.json");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File())
                    return;

                if (RawDub::ProjectIO::load (engine, file))
                {
                    currentProjectFile = file;
                    kickViewPage = 0;
                    bassViewPage = 0;
                    skankViewPage = 0;
                    engine.setCurrentGlobalPatternSlot (0);
                    tempoSlider.setValue (engine.getTempoBpm(), juce::dontSendNotification);
                    refreshParamSlidersFromEngine();
                    refreshStepColours();
                    skankSawMixLaneEditor.setGridDivisions (engine.skankPattern().getActiveLength());
                    resized();
                }
            });
    };

    addAndMakeVisible (newProjectButton);
    newProjectButton.onClick = [this]
    {
        juce::NativeMessageBox::showOkCancelBox (juce::AlertWindow::WarningIcon, "New Project",
            "Start a new project? Unsaved changes will be lost.", this,
            juce::ModalCallbackFunction::create ([this] (int result)
            {
                if (result == 0)
                    return;

                engine.resetToDefaults();
                currentProjectFile = juce::File();
                kickViewPage = 0;
                bassViewPage = 0;
                skankViewPage = 0;
                engine.setCurrentGlobalPatternSlot (0);
                tempoSlider.setValue (engine.getTempoBpm(), juce::dontSendNotification);
                refreshParamSlidersFromEngine();
                refreshStepColours();
                skankSawMixLaneEditor.setGridDivisions (engine.skankPattern().getActiveLength());
                resized();
            }));
    };

    // --- Global Patterns (see project_raw_dub_song_architecture memory) ---
    addAndMakeVisible (globalPatternsLabel);

    // There's always a "current" Global Pattern - editing/saving is
    // always about that one, never a mode or a target to pick (see
    // project_raw_dub_song_architecture memory).
    auto recallGlobalPatternUI = [this] (int slot)
    {
        engine.setCurrentGlobalPatternSlot (slot);
        if (engine.recallGlobalPattern (slot))
        {
            kickViewPage = 0;
            bassViewPage = 0;
            skankViewPage = 0;
            refreshStepColours();
            skankSawMixLaneEditor.setGridDivisions (engine.skankPattern().getActiveLength());
            skankSawMixLaneEditor.repaint();
            resized();
        }
        refreshBassOverrideControls(); // different section, possibly different override state
        refreshSkankOverrideControls();
        refreshPatternSharingIndicators();
        refreshGlobalPatternButtons();
    };

    for (int i = 0; i < RawDub::AudioEngine::globalPatternBankSize; ++i)
    {
        auto& btn = globalPatternButtons[(size_t) i];
        btn.setButtonText (juce::String (i + 1));
        addAndMakeVisible (btn);
        // click = recall (if it has content) and become the pattern
        // you're now editing - always the same meaning, never a mode
        btn.onClick = [recallGlobalPatternUI, i] { recallGlobalPatternUI (i); };
    }

    addAndMakeVisible (saveGlobalPatternButton);
    saveGlobalPatternButton.onClick = [this]
    {
        engine.saveCurrentAsGlobalPattern (engine.getCurrentGlobalPatternSlot());
        refreshGlobalPatternButtons();
    };

    addAndMakeVisible (duplicateGlobalPatternButton);
    duplicateGlobalPatternButton.onClick = [this]
    {
        // branches off into the next free slot with whatever's
        // currently live, and switches editing there - doesn't touch
        // the instruments (they're already exactly what's being
        // duplicated) or the pattern it branched from. Any active
        // override on the source pattern carries over too - it's part
        // of what's currently live (the slider shows it), so the
        // duplicate should actually sound like what you branched from.
        int sourceSlot = engine.getCurrentGlobalPatternSlot();
        int freeSlot = engine.findFirstEmptyGlobalPatternSlot (sourceSlot);
        if (freeSlot < 0)
            return; // bank full - nothing to duplicate into

        engine.saveCurrentAsGlobalPattern (freeSlot);
        engine.copySectionVoicingOverrides (sourceSlot, freeSlot);
        engine.setCurrentGlobalPatternSlot (freeSlot);
        refreshBassOverrideControls();
        refreshSkankOverrideControls();
        refreshPatternSharingIndicators();
        refreshGlobalPatternButtons();
    };

    addAndMakeVisible (clearGlobalPatternButton);
    clearGlobalPatternButton.onClick = [this]
    {
        engine.clearGlobalPattern (engine.getCurrentGlobalPatternSlot());
        refreshBassOverrideControls();
        refreshSkankOverrideControls();
        refreshGlobalPatternButtons();
    };

    // --- Instrument tabs: only one instrument's section is shown at a
    // time, not all three stacked/scrolled ---
    {
        const char* names[3] = { "Kick", "Bass", "Skank" };
        for (int i = 0; i < 3; ++i)
        {
            auto& btn = instrumentTabButtons[(size_t) i];
            btn.setButtonText (names[i]);
            addAndMakeVisible (btn);
            btn.onClick = [this, i] { switchInstrumentTab (i); };
        }
    }

    // --- Kick ---
    addAndMakeVisible (kickTriggerButton);
    kickTriggerButton.onClick = [this] { engine.requestManualKickTrigger(); };

    addAndMakeVisible (kickClearButton);
    kickClearButton.onClick = [this]
    {
        engine.kickPattern().clearAll();
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
            engine.kickPattern().toggle (kickViewPage * RawDub::numSteps + s);
            refreshStepColours();
        };
        btn.onLevelDrag = [this, s] (RawDub::StepLevel lvl)
        {
            engine.kickPattern().setLevel (kickViewPage * RawDub::numSteps + s, lvl);
        };
    }

    addAndMakeVisible (kickLengthLabel);
    for (int i = 0; i < 4; ++i)
    {
        auto& btn = kickLengthButtons[(size_t) i];
        btn.setButtonText (juce::String (lengthOptions[i]));
        addAndMakeVisible (btn);
        btn.onClick = [this, i] { setVoiceLength (engine.kickPattern(), lengthOptions[i], kickViewPage); };
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

    addAndMakeVisible (kickPatternBankLabel);
    for (int i = 0; i < RawDub::AudioEngine::bankSize; ++i)
    {
        auto& btn = kickPatternBankButtons[(size_t) i];
        btn.setButtonText (juce::String (i + 1)); // 1-indexed display
        addAndMakeVisible (btn);
        btn.onClick = [this, i]
        {
            engine.setCurrentKickPatternIndex (i);
            kickViewPage = 0;
            refreshStepColours();
            refreshPatternSharingIndicators();
            resized();
        };
    }

    addAndMakeVisible (kickSharedLabel);
    addAndMakeVisible (kickMakeUniqueButton);
    kickMakeUniqueButton.onClick = [this]
    {
        engine.makeKickPatternUnique();
        refreshPatternSharingIndicators();
        refreshGlobalPatternButtons();
    };

    addAndMakeVisible (kickTitleLabel);
    kickTitleLabel.setFont (juce::Font (20.0f, juce::Font::bold));

    {
        ParamSpec specs[5] = {
            { "Tune",   &engine.kick.tuneHz,  30.0, 150.0, 1.0  },
            { "Punch",  &engine.kick.punchMs, 5.0,  120.0, 1.0  },
            { "Decay",  &engine.kick.decayMs, 50.0, 800.0, 1.0  },
            { "Drive",  &engine.kick.drive,   0.0,  1.0,   0.01 },
            { "Volume", &engine.kick.volume,  0.0,  1.0,   0.01 },
        };

        for (int i = 0; i < 5; ++i)
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
        engine.bassPattern().clearAll();
        refreshStepColours();
    };

    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        auto& btn = bassStepButtons[(size_t) s];
        btn.setHasPitch (true);
        addAndMakeVisible (btn);
        btn.onToggle = [this, s]
        {
            engine.bassPattern().toggle (bassViewPage * RawDub::numSteps + s);
            refreshStepColours();
        };
        btn.onPitchDrag = [this, s] (int offset)
        {
            engine.bassPattern().setSemitoneOffset (bassViewPage * RawDub::numSteps + s, offset);
        };
        btn.onLevelDrag = [this, s] (RawDub::StepLevel lvl)
        {
            engine.bassPattern().setLevel (bassViewPage * RawDub::numSteps + s, lvl);
        };
    }

    addAndMakeVisible (bassLengthLabel);
    for (int i = 0; i < 4; ++i)
    {
        auto& btn = bassLengthButtons[(size_t) i];
        btn.setButtonText (juce::String (lengthOptions[i]));
        addAndMakeVisible (btn);
        btn.onClick = [this, i] { setVoiceLength (engine.bassPattern(), lengthOptions[i], bassViewPage); };
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

    addAndMakeVisible (bassPatternBankLabel);
    for (int i = 0; i < RawDub::AudioEngine::bankSize; ++i)
    {
        auto& btn = bassPatternBankButtons[(size_t) i];
        btn.setButtonText (juce::String (i + 1));
        addAndMakeVisible (btn);
        btn.onClick = [this, i]
        {
            engine.setCurrentBassPatternIndex (i);
            bassViewPage = 0;
            refreshStepColours();
            refreshPatternSharingIndicators();
            resized();
        };
    }

    addAndMakeVisible (bassSharedLabel);
    addAndMakeVisible (bassMakeUniqueButton);
    bassMakeUniqueButton.onClick = [this]
    {
        engine.makeBassPatternUnique();
        refreshPatternSharingIndicators();
        refreshGlobalPatternButtons();
    };

    addAndMakeVisible (bassTitleLabel);
    bassTitleLabel.setFont (juce::Font (20.0f, juce::Font::bold));

    addAndMakeVisible (bassHarmonicModeButton);
    bassHarmonicModeButton.onClick = [this]
    {
        bool nowAM = ! engine.bass.useAMMode.load();
        engine.bass.useAMMode.store (nowAM);
        bassHarmonicModeButton.setButtonText (nowAM ? "Harmonic: AM" : "Harmonic: Drive");
    };

    addAndMakeVisible (bassAmRatioLabel);

    {
        const char* ratioLabels[3] = { "1:1", "2:1", "3:1" };
        const float ratioValues[3] = { 1.0f, 2.0f, 3.0f };

        for (int i = 0; i < 3; ++i)
        {
            auto& b = bassAmRatioButtons[(size_t) i];
            addAndMakeVisible (b);
            b.setButtonText (ratioLabels[i]);
            b.setClickingTogglesState (true);
            b.setRadioGroupId (5001);
            b.setToggleState (i == 0, juce::dontSendNotification);

            float value = ratioValues[i];
            b.onClick = [this, value] { engine.bass.amRatio.store (value); };
        }
    }

    {
        // ordered to match the signal chain: oscillator (Tune) -> Drive
        // (saturation) -> filter (Cutoff/Resonance) -> envelope (Length).
        // Length is hold time, not release speed - the release itself is
        // a short fixed internal tail (see BassSynth::releaseTau). AM
        // Depth only matters in AM mode (research switch, see
        // BassSynth::useAMMode) - harmless when Drive mode is active.
        ParamSpec specs[7] = {
            { "Tune",      &engine.bass.tuneHz,    30.0,  120.0,  1.0  },
            { "Drive",     &engine.bass.drive,     0.0,   1.0,    0.01 },
            { "Cutoff",    &engine.bass.cutoffHz,  100.0, 4000.0, 10.0 },
            { "Resonance", &engine.bass.resonance, 0.0,   0.95,   0.01 },
            { "Length",    &engine.bass.decayMs,   50.0,  4000.0, 10.0 },
            { "AM Depth",  &engine.bass.amDepth,   0.0,   1.0,    0.01 },
            { "Volume",    &engine.bass.volume,    0.0,   1.0,    0.01 },
        };

        for (int i = 0; i < 7; ++i)
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

        // Drive/Cutoff get overridden below to be override-aware instead
        // of always writing straight to the base atomic - see
        // bassDriveOverrideButton/bassCutoffOverrideButton.
        addAndMakeVisible (bassDriveOverrideButton);
        bassDriveOverrideButton.onClick = [this]
        {
            auto& gp = engine.globalPatterns[(size_t) engine.getCurrentGlobalPatternSlot()];
            bool newActive = ! gp.bassDriveOverride.active.load();
            // seed with the current base value so turning an override on
            // never silently jumps the sound - it starts as a no-op
            if (newActive)
                gp.bassDriveOverride.value.store (engine.bass.drive.load());
            gp.bassDriveOverride.active.store (newActive);
            refreshBassOverrideControls();
        };
        bassParamRows[1].slider.onValueChange = [this]
        {
            auto& gp = engine.globalPatterns[(size_t) engine.getCurrentGlobalPatternSlot()];
            float v = (float) bassParamRows[1].slider.getValue();
            if (gp.bassDriveOverride.active.load())
                gp.bassDriveOverride.value.store (v);
            else
                engine.bass.drive.store (v);
        };

        addAndMakeVisible (bassCutoffOverrideButton);
        bassCutoffOverrideButton.onClick = [this]
        {
            auto& gp = engine.globalPatterns[(size_t) engine.getCurrentGlobalPatternSlot()];
            bool newActive = ! gp.bassCutoffOverride.active.load();
            if (newActive)
                gp.bassCutoffOverride.value.store (engine.bass.cutoffHz.load());
            gp.bassCutoffOverride.active.store (newActive);
            refreshBassOverrideControls();
        };
        bassParamRows[2].slider.onValueChange = [this]
        {
            auto& gp = engine.globalPatterns[(size_t) engine.getCurrentGlobalPatternSlot()];
            float v = (float) bassParamRows[2].slider.getValue();
            if (gp.bassCutoffOverride.active.load())
                gp.bassCutoffOverride.value.store (v);
            else
                engine.bass.cutoffHz.store (v);
        };
    }

    // --- Skank (sequenced, see SkankSynth.h) ---
    addAndMakeVisible (skankTitleLabel);
    skankTitleLabel.setFont (juce::Font (20.0f, juce::Font::bold));

    addAndMakeVisible (skankTriggerButton);
    skankTriggerButton.onClick = [this] { engine.requestManualSkankTrigger(); };

    addAndMakeVisible (skankClearButton);
    skankClearButton.onClick = [this]
    {
        engine.skankPattern().clearAll();
        engine.skankSawMixCurve().resetToValue (0.5f);
        refreshStepColours();
        skankSawMixLaneEditor.repaint();
    };

    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        auto& btn = skankStepButtons[(size_t) s];
        btn.setHasPitch (true); // transposes the whole chord's root - see SkankSynth::triggerChord
        addAndMakeVisible (btn);
        btn.onToggle = [this, s]
        {
            engine.skankPattern().toggle (skankViewPage * RawDub::numSteps + s);
            refreshStepColours();
        };
        btn.onPitchDrag = [this, s] (int offset)
        {
            engine.skankPattern().setSemitoneOffset (skankViewPage * RawDub::numSteps + s, offset);
        };
        btn.onLevelDrag = [this, s] (RawDub::StepLevel lvl)
        {
            engine.skankPattern().setLevel (skankViewPage * RawDub::numSteps + s, lvl);
        };
    }

    addAndMakeVisible (skankLengthLabel);
    for (int i = 0; i < 4; ++i)
    {
        auto& btn = skankLengthButtons[(size_t) i];
        btn.setButtonText (juce::String (lengthOptions[i]));
        addAndMakeVisible (btn);
        btn.onClick = [this, i]
        {
            setVoiceLength (engine.skankPattern(), lengthOptions[i], skankViewPage);
            skankSawMixLaneEditor.setGridDivisions (lengthOptions[i]);
        };
    }

    for (int p = 0; p < maxPages; ++p)
    {
        auto& btn = skankPageButtons[(size_t) p];
        btn.setButtonText (juce::String (p * RawDub::numSteps + 1) + "-" + juce::String ((p + 1) * RawDub::numSteps));
        addAndMakeVisible (btn);
        btn.onClick = [this, p]
        {
            skankViewPage = p;
            refreshStepColours();
        };
    }

    addAndMakeVisible (skankPatternBankLabel);
    for (int i = 0; i < RawDub::AudioEngine::bankSize; ++i)
    {
        auto& btn = skankPatternBankButtons[(size_t) i];
        btn.setButtonText (juce::String (i + 1));
        addAndMakeVisible (btn);
        btn.onClick = [this, i]
        {
            engine.setCurrentSkankPatternIndex (i);
            skankViewPage = 0;
            refreshStepColours();
            refreshPatternSharingIndicators();
            skankSawMixLaneEditor.setGridDivisions (engine.skankPattern().getActiveLength());
            skankSawMixLaneEditor.repaint();
            resized();
        };
    }

    addAndMakeVisible (skankSharedLabel);
    addAndMakeVisible (skankMakeUniqueButton);
    skankMakeUniqueButton.onClick = [this]
    {
        engine.makeSkankPatternUnique();
        refreshPatternSharingIndicators();
        skankSawMixLaneEditor.setGridDivisions (engine.skankPattern().getActiveLength());
        skankSawMixLaneEditor.repaint();
        refreshGlobalPatternButtons();
    };

    addAndMakeVisible (skankMajorButton);
    addAndMakeVisible (skankMinorButton);
    skankMajorButton.setClickingTogglesState (true);
    skankMinorButton.setClickingTogglesState (true);
    skankMajorButton.setRadioGroupId (6001);
    skankMinorButton.setRadioGroupId (6001);
    skankMajorButton.setToggleState (true, juce::dontSendNotification);
    skankMajorButton.onClick = [this] { engine.skank.minorChord.store (false); };
    skankMinorButton.onClick = [this] { engine.skank.minorChord.store (true); };

    {
        ParamSpec specs[5] = {
            { "Tune",    &engine.skank.tuneHz,  200.0, 800.0, 1.0  },
            { "SawMix",  &engine.skank.sawMix,  0.0,   1.0,   0.01 },
            { "Decay",   &engine.skank.decayMs, 30.0,  500.0, 1.0  },
            { "Drive",   &engine.skank.drive,   0.0,   1.0,   0.01 },
            { "Volume",  &engine.skank.volume,  0.0,   1.0,   0.01 },
        };

        for (int i = 0; i < 5; ++i)
        {
            auto& row = skankParamRows[(size_t) i];

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

        // SawMix specifically: the slider is "set a constant," the curve
        // is "compose evolution" - never two independent ways of
        // controlling the same thing. Moving the slider collapses the
        // curve back to a flat line at the slider's value; see
        // SkankSynth::resetSawMixLaneToValue.
        skankParamRows[1].slider.onValueChange = [this]
        {
            float v = (float) skankParamRows[1].slider.getValue();
            engine.skank.sawMix.store (v);
            engine.skankSawMixCurve().resetToValue (v);
            skankSawMixLaneEditor.repaint();
        };

        // Decay gets the same section-level override as Bass Drive/Cutoff
        addAndMakeVisible (skankDecayOverrideButton);
        skankDecayOverrideButton.onClick = [this]
        {
            auto& gp = engine.globalPatterns[(size_t) engine.getCurrentGlobalPatternSlot()];
            bool newActive = ! gp.skankDecayOverride.active.load();
            if (newActive)
                gp.skankDecayOverride.value.store (engine.skank.decayMs.load());
            gp.skankDecayOverride.active.store (newActive);
            refreshSkankOverrideControls();
        };
        skankParamRows[2].slider.onValueChange = [this]
        {
            auto& gp = engine.globalPatterns[(size_t) engine.getCurrentGlobalPatternSlot()];
            float v = (float) skankParamRows[2].slider.getValue();
            if (gp.skankDecayOverride.active.load())
                gp.skankDecayOverride.value.store (v);
            else
                engine.skank.decayMs.store (v);
        };
    }

    addAndMakeVisible (skankSawMixLaneLabel);
    addAndMakeVisible (skankSawMixLaneEditor);
    skankSawMixLaneEditor.setGridDivisions (engine.skankPattern().getActiveLength());
    skankSawMixLaneEditor.getPointCount = [this] { return engine.skankSawMixCurve().getPointCount(); };
    skankSawMixLaneEditor.getPointPosition = [this] (int i) { return engine.skankSawMixCurve().getPointPosition (i); };
    skankSawMixLaneEditor.getPointValue = [this] (int i) { return engine.skankSawMixCurve().getPointValue (i); };
    skankSawMixLaneEditor.onPointValueChanged = [this] (int i, float v) { engine.skankSawMixCurve().setPointValue (i, v); };
    skankSawMixLaneEditor.onPointPositionChanged = [this] (int i, float p) { engine.skankSawMixCurve().setPointPosition (i, p); };
    skankSawMixLaneEditor.onAddPoint = [this] (float p, float v) { return engine.skankSawMixCurve().insertPoint (p, v); };
    skankSawMixLaneEditor.onRemovePoint = [this] (int i) { engine.skankSawMixCurve().removePoint (i); };

    // --- component groups for the instrument tabs (see switchInstrumentTab) ---
    kickComponents = { &kickTriggerButton, &kickClearButton, &kickTitleLabel, &kickLengthLabel, &kickPatternBankLabel,
                        &kickSharedLabel, &kickMakeUniqueButton };
    for (auto& btn : kickStepButtons) kickComponents.push_back (&btn);
    for (auto& row : kickParamRows) { kickComponents.push_back (&row.label); kickComponents.push_back (&row.slider); }
    for (auto& btn : kickLengthButtons) kickComponents.push_back (&btn);
    for (auto& btn : kickPageButtons) kickComponents.push_back (&btn);
    for (auto& btn : kickPatternBankButtons) kickComponents.push_back (&btn);

    bassComponents = { &bassTriggerButton, &bassClearButton, &bassTitleLabel, &bassLengthLabel, &bassPatternBankLabel,
                        &bassHarmonicModeButton, &bassAmRatioLabel, &bassDriveOverrideButton, &bassCutoffOverrideButton,
                        &bassSharedLabel, &bassMakeUniqueButton };
    for (auto& btn : bassStepButtons) bassComponents.push_back (&btn);
    for (auto& row : bassParamRows) { bassComponents.push_back (&row.label); bassComponents.push_back (&row.slider); }
    for (auto& btn : bassLengthButtons) bassComponents.push_back (&btn);
    for (auto& btn : bassPageButtons) bassComponents.push_back (&btn);
    for (auto& btn : bassPatternBankButtons) bassComponents.push_back (&btn);
    for (auto& btn : bassAmRatioButtons) bassComponents.push_back (&btn);

    skankComponents = { &skankTriggerButton, &skankClearButton, &skankTitleLabel, &skankMajorButton, &skankMinorButton,
                         &skankLengthLabel, &skankPatternBankLabel, &skankSawMixLaneLabel, &skankSawMixLaneEditor,
                         &skankDecayOverrideButton, &skankSharedLabel, &skankMakeUniqueButton };
    for (auto& btn : skankStepButtons) skankComponents.push_back (&btn);
    for (auto& row : skankParamRows) { skankComponents.push_back (&row.label); skankComponents.push_back (&row.slider); }
    for (auto& btn : skankLengthButtons) skankComponents.push_back (&btn);
    for (auto& btn : skankPageButtons) skankComponents.push_back (&btn);
    for (auto& btn : skankPatternBankButtons) skankComponents.push_back (&btn);

    switchInstrumentTab (0);

    refreshStepColours();
    updatePlayButtonText();

    setAudioChannels (0, 2);
    setSize (820, 900);
    startTimerHz (30);
}

void MainComponent::switchInstrumentTab (int tab)
{
    currentInstrumentTab = tab;

    for (auto* c : kickComponents)  c->setVisible (tab == 0);
    for (auto* c : bassComponents)  c->setVisible (tab == 1);
    for (auto* c : skankComponents) c->setVisible (tab == 2);

    for (int i = 0; i < 3; ++i)
    {
        bool isActive = (i == tab);
        instrumentTabButtons[(size_t) i].setColour (juce::TextButton::buttonColourId, isActive ? juce::Colours::black : juce::Colours::white);
        instrumentTabButtons[(size_t) i].setColour (juce::TextButton::textColourOffId, isActive ? juce::Colours::white : juce::Colours::black);
    }

    resized();
    refreshStepColours();
    // sharing indicators have their own conditional visibility (only
    // shown when actually shared) that the blanket show/hide above
    // doesn't know about - re-correct it now
    refreshPatternSharingIndicators();
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
    tempoSlider.setBounds (transport.removeFromLeft (260));
    transport.removeFromLeft (16);
    saveButton.setBounds (transport.removeFromLeft (55));
    transport.removeFromLeft (4);
    saveAsButton.setBounds (transport.removeFromLeft (85));
    transport.removeFromLeft (4);
    openButton.setBounds (transport.removeFromLeft (70));
    transport.removeFromLeft (4);
    newProjectButton.setBounds (transport.removeFromLeft (55));

    area.removeFromTop (12);

    auto globalPatternsRow = area.removeFromTop (28);
    globalPatternsLabel.setBounds (globalPatternsRow.removeFromLeft (90));
    for (auto& btn : globalPatternButtons)
    {
        btn.setBounds (globalPatternsRow.removeFromLeft (24));
        globalPatternsRow.removeFromLeft (2);
    }
    globalPatternsRow.removeFromLeft (8);
    saveGlobalPatternButton.setBounds (globalPatternsRow.removeFromLeft (60));
    globalPatternsRow.removeFromLeft (4);
    duplicateGlobalPatternButton.setBounds (globalPatternsRow.removeFromLeft (70));
    globalPatternsRow.removeFromLeft (4);
    clearGlobalPatternButton.setBounds (globalPatternsRow.removeFromLeft (50));

    area.removeFromTop (20);

    auto tabsRow = area.removeFromTop (32);
    instrumentTabButtons[0].setBounds (tabsRow.removeFromLeft (100));
    tabsRow.removeFromLeft (6);
    instrumentTabButtons[1].setBounds (tabsRow.removeFromLeft (100));
    tabsRow.removeFromLeft (6);
    instrumentTabButtons[2].setBounds (tabsRow.removeFromLeft (100));

    area.removeFromTop (16);

    // Only the active instrument's section is shown - each starts from
    // the same area rather than stacking, so switching tabs doesn't
    // require scrolling to reach whichever one is now visible.
    if (currentInstrumentTab == 0)
        layoutKickSection (area);
    else if (currentInstrumentTab == 1)
        layoutBassSection (area);
    else
        layoutSkankSection (area);
}

void MainComponent::layoutKickSection (juce::Rectangle<int> area)
{
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
    int kickNumPages = (engine.kickPattern().getActiveLength() + RawDub::numSteps - 1) / RawDub::numSteps;
    for (int p = 0; p < maxPages; ++p)
    {
        kickPageButtons[(size_t) p].setVisible (p < kickNumPages && kickNumPages > 1);
        kickPageButtons[(size_t) p].setBounds (kickLengthRow.removeFromLeft (64));
        kickLengthRow.removeFromLeft (6);
    }

    area.removeFromTop (8);

    auto kickPatternBankRow = area.removeFromTop (28);
    kickPatternBankLabel.setBounds (kickPatternBankRow.removeFromLeft (50));
    for (auto& btn : kickPatternBankButtons)
    {
        btn.setBounds (kickPatternBankRow.removeFromLeft (22));
        kickPatternBankRow.removeFromLeft (2);
    }
    kickPatternBankRow.removeFromLeft (8);
    kickSharedLabel.setBounds (kickPatternBankRow.removeFromLeft (130));
    kickMakeUniqueButton.setBounds (kickPatternBankRow.removeFromLeft (90));

    area.removeFromTop (8);

    auto kickStepRow = area.removeFromTop (50);
    layoutStepRow (kickStepRow, kickStepButtons, engine.kickPattern().getActiveLength(), kickViewPage);

    area.removeFromTop (10);

    for (auto& row : kickParamRows)
    {
        auto rowArea = area.removeFromTop (36);
        row.label.setBounds (rowArea.removeFromLeft (70));
        row.slider.setBounds (rowArea);
        area.removeFromTop (8);
    }
}

void MainComponent::layoutBassSection (juce::Rectangle<int> area)
{
    auto bassHeader = area.removeFromTop (28);
    bassTitleLabel.setBounds (bassHeader.removeFromLeft (200));
    bassTriggerButton.setBounds (bassHeader.removeFromLeft (90));
    bassHeader.removeFromLeft (6);
    bassClearButton.setBounds (bassHeader.removeFromLeft (70));
    bassHeader.removeFromLeft (6);
    bassHarmonicModeButton.setBounds (bassHeader.removeFromLeft (120));

    area.removeFromTop (8);

    auto bassLengthRow = area.removeFromTop (28);
    bassLengthLabel.setBounds (bassLengthRow.removeFromLeft (60));
    for (auto& btn : bassLengthButtons)
    {
        btn.setBounds (bassLengthRow.removeFromLeft (44));
        bassLengthRow.removeFromLeft (6);
    }
    bassLengthRow.removeFromLeft (20);
    int bassNumPages = (engine.bassPattern().getActiveLength() + RawDub::numSteps - 1) / RawDub::numSteps;
    for (int p = 0; p < maxPages; ++p)
    {
        bassPageButtons[(size_t) p].setVisible (p < bassNumPages && bassNumPages > 1);
        bassPageButtons[(size_t) p].setBounds (bassLengthRow.removeFromLeft (64));
        bassLengthRow.removeFromLeft (6);
    }

    area.removeFromTop (8);

    auto bassPatternBankRow = area.removeFromTop (28);
    bassPatternBankLabel.setBounds (bassPatternBankRow.removeFromLeft (50));
    for (auto& btn : bassPatternBankButtons)
    {
        btn.setBounds (bassPatternBankRow.removeFromLeft (22));
        bassPatternBankRow.removeFromLeft (2);
    }
    bassPatternBankRow.removeFromLeft (8);
    bassSharedLabel.setBounds (bassPatternBankRow.removeFromLeft (130));
    bassMakeUniqueButton.setBounds (bassPatternBankRow.removeFromLeft (90));

    area.removeFromTop (8);

    auto bassAmRatioRow = area.removeFromTop (28);
    bassAmRatioLabel.setBounds (bassAmRatioRow.removeFromLeft (70));
    for (auto& btn : bassAmRatioButtons)
    {
        btn.setBounds (bassAmRatioRow.removeFromLeft (50));
        bassAmRatioRow.removeFromLeft (6);
    }

    area.removeFromTop (8);

    // Bass needs real vertical room: it's the only voice with pitch, and at
    // 50px (Kick's height) each semitone gets under 2px - indistinguishable.
    // 160px gives ~6px/semitone across the +/-12 range, enough to actually
    // read a contour.
    auto bassStepRow = area.removeFromTop (160);
    layoutStepRow (bassStepRow, bassStepButtons, engine.bassPattern().getActiveLength(), bassViewPage);

    area.removeFromTop (10);

    for (int i = 0; i < (int) bassParamRows.size(); ++i)
    {
        auto& row = bassParamRows[(size_t) i];
        auto rowArea = area.removeFromTop (36);
        row.label.setBounds (rowArea.removeFromLeft (70));

        // Drive (1) and Cutoff (2) reserve room on the right for their
        // section-level override toggle - see bassDriveOverrideButton
        if (i == 1)
            bassDriveOverrideButton.setBounds (rowArea.removeFromRight (80));
        else if (i == 2)
            bassCutoffOverrideButton.setBounds (rowArea.removeFromRight (80));

        row.slider.setBounds (rowArea);
        area.removeFromTop (8);
    }
}

void MainComponent::layoutSkankSection (juce::Rectangle<int> area)
{
    auto skankHeader = area.removeFromTop (28);
    skankTitleLabel.setBounds (skankHeader.removeFromLeft (150));
    skankTriggerButton.setBounds (skankHeader.removeFromLeft (90));
    skankHeader.removeFromLeft (6);
    skankClearButton.setBounds (skankHeader.removeFromLeft (70));

    area.removeFromTop (8);

    auto skankLengthRow = area.removeFromTop (28);
    skankLengthLabel.setBounds (skankLengthRow.removeFromLeft (60));
    for (auto& btn : skankLengthButtons)
    {
        btn.setBounds (skankLengthRow.removeFromLeft (44));
        skankLengthRow.removeFromLeft (6);
    }
    skankLengthRow.removeFromLeft (20);
    int skankNumPages = (engine.skankPattern().getActiveLength() + RawDub::numSteps - 1) / RawDub::numSteps;
    for (int p = 0; p < maxPages; ++p)
    {
        skankPageButtons[(size_t) p].setVisible (p < skankNumPages && skankNumPages > 1);
        skankPageButtons[(size_t) p].setBounds (skankLengthRow.removeFromLeft (64));
        skankLengthRow.removeFromLeft (6);
    }

    area.removeFromTop (8);

    auto skankPatternBankRow = area.removeFromTop (28);
    skankPatternBankLabel.setBounds (skankPatternBankRow.removeFromLeft (50));
    for (auto& btn : skankPatternBankButtons)
    {
        btn.setBounds (skankPatternBankRow.removeFromLeft (22));
        skankPatternBankRow.removeFromLeft (2);
    }
    skankPatternBankRow.removeFromLeft (8);
    skankSharedLabel.setBounds (skankPatternBankRow.removeFromLeft (130));
    skankMakeUniqueButton.setBounds (skankPatternBankRow.removeFromLeft (90));

    area.removeFromTop (8);

    auto skankStepRow = area.removeFromTop (120);
    layoutStepRow (skankStepRow, skankStepButtons, engine.skankPattern().getActiveLength(), skankViewPage);

    area.removeFromTop (10);

    auto skankChordRow = area.removeFromTop (28);
    skankMajorButton.setBounds (skankChordRow.removeFromLeft (70));
    skankChordRow.removeFromLeft (6);
    skankMinorButton.setBounds (skankChordRow.removeFromLeft (70));

    area.removeFromTop (8);

    for (int i = 0; i < (int) skankParamRows.size(); ++i)
    {
        auto& row = skankParamRows[(size_t) i];
        auto rowArea = area.removeFromTop (36);
        row.label.setBounds (rowArea.removeFromLeft (70));

        // Decay (2) reserves room for its section-level override toggle
        if (i == 2)
            skankDecayOverrideButton.setBounds (rowArea.removeFromRight (80));

        row.slider.setBounds (rowArea);
        area.removeFromTop (8);
    }

    area.removeFromTop (8);
    skankSawMixLaneLabel.setBounds (area.removeFromTop (20));
    area.removeFromTop (4);
    skankSawMixLaneEditor.setBounds (area.removeFromTop (70));
}

void MainComponent::timerCallback()
{
    updatePlayButtonText();

    int kickStep = engine.isPlaying() ? engine.getCurrentKickStep() : -1;
    int bassStep = engine.isPlaying() ? engine.getCurrentBassStep() : -1;
    int skankStep = engine.isPlaying() ? engine.getCurrentSkankStep() : -1;

    if (kickStep != kickPlayheadStep || bassStep != bassPlayheadStep || skankStep != skankPlayheadStep)
    {
        kickPlayheadStep = kickStep;
        bassPlayheadStep = bassStep;
        skankPlayheadStep = skankStep;
        refreshStepColours();
    }
}

void MainComponent::refreshStepColours()
{
    int kickLength = engine.kickPattern().getActiveLength();
    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        int kickIndex = kickViewPage * RawDub::numSteps + s;
        if (kickIndex >= kickLength)
            continue;

        auto& kickBtn = kickStepButtons[(size_t) s];
        kickBtn.setOn (engine.kickPattern().isOn (kickIndex));
        kickBtn.setLevel (engine.kickPattern().getLevel (kickIndex));
        kickBtn.setPlayhead (kickIndex == kickPlayheadStep);
    }

    int bassLength = engine.bassPattern().getActiveLength();
    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        int bassIndex = bassViewPage * RawDub::numSteps + s;
        if (bassIndex >= bassLength)
            continue;

        auto& bassBtn = bassStepButtons[(size_t) s];
        bassBtn.setOn (engine.bassPattern().isOn (bassIndex));
        bassBtn.setLevel (engine.bassPattern().getLevel (bassIndex));
        bassBtn.setSemitoneOffset (engine.bassPattern().getSemitoneOffset (bassIndex));
        bassBtn.setPlayhead (bassIndex == bassPlayheadStep);
    }

    int skankLength = engine.skankPattern().getActiveLength();
    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        int skankIndex = skankViewPage * RawDub::numSteps + s;
        if (skankIndex >= skankLength)
            continue;

        auto& skankBtn = skankStepButtons[(size_t) s];
        skankBtn.setOn (engine.skankPattern().isOn (skankIndex));
        skankBtn.setLevel (engine.skankPattern().getLevel (skankIndex));
        skankBtn.setSemitoneOffset (engine.skankPattern().getSemitoneOffset (skankIndex));
        skankBtn.setPlayhead (skankIndex == skankPlayheadStep);
    }

    skankSawMixLaneEditor.setPlayheadStep (skankPlayheadStep);

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
    refreshPageButtons (skankPageButtons, skankViewPage, skankPlayheadStep);

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

    refreshLengthButtons (kickLengthButtons, engine.kickPattern().getActiveLength());
    refreshLengthButtons (bassLengthButtons, engine.bassPattern().getActiveLength());
    refreshLengthButtons (skankLengthButtons, engine.skankPattern().getActiveLength());

    // pattern bank buttons: black = the pattern slot currently selected
    // for that instrument - same convention as Length/page buttons
    auto refreshBankButtons = [] (std::array<juce::TextButton, RawDub::AudioEngine::bankSize>& bankButtons, int currentIndex)
    {
        for (int i = 0; i < RawDub::AudioEngine::bankSize; ++i)
        {
            bool isActive = (i == currentIndex);
            bankButtons[(size_t) i].setColour (juce::TextButton::buttonColourId, isActive ? juce::Colours::black : juce::Colours::white);
            bankButtons[(size_t) i].setColour (juce::TextButton::textColourOffId, isActive ? juce::Colours::white : juce::Colours::black);
        }
    };

    refreshBankButtons (kickPatternBankButtons, engine.getCurrentKickPatternIndex());
    refreshBankButtons (bassPatternBankButtons, engine.getCurrentBassPatternIndex());
    refreshBankButtons (skankPatternBankButtons, engine.getCurrentSkankPatternIndex());

    refreshGlobalPatternButtons();
}

// black = the currently recalled/saved-to slot; unused (never-saved)
// slots read visibly different (grey text) from saved-but-not-current
// ones, so it's clear at a glance which numbers actually hold anything
void MainComponent::refreshGlobalPatternButtons()
{
    for (int i = 0; i < RawDub::AudioEngine::globalPatternBankSize; ++i)
    {
        auto& btn = globalPatternButtons[(size_t) i];
        bool isCurrent = (i == engine.getCurrentGlobalPatternSlot());
        bool isUsed = engine.isGlobalPatternUsed (i);

        auto bg = isCurrent ? juce::Colours::black : juce::Colours::white;
        auto fg = isCurrent ? juce::Colours::white : (isUsed ? juce::Colours::black : juce::Colours::lightgrey);

        btn.setColour (juce::TextButton::buttonColourId, bg);
        btn.setColour (juce::TextButton::textColourOffId, fg);
    }
}

void MainComponent::updatePlayButtonText()
{
    playStopButton.setButtonText (engine.isPlaying() ? "Stop" : "Play");
}

// order must match the ParamSpec arrays set up in the constructor -
// Kick: Tune/Punch/Decay/Drive/Volume, Bass: Tune/Drive/Cutoff/Resonance/Length/AM Depth/Volume
void MainComponent::refreshParamSlidersFromEngine()
{
    kickParamRows[0].slider.setValue ((double) engine.kick.tuneHz.load(),  juce::dontSendNotification);
    kickParamRows[1].slider.setValue ((double) engine.kick.punchMs.load(), juce::dontSendNotification);
    kickParamRows[2].slider.setValue ((double) engine.kick.decayMs.load(), juce::dontSendNotification);
    kickParamRows[3].slider.setValue ((double) engine.kick.drive.load(),   juce::dontSendNotification);
    kickParamRows[4].slider.setValue ((double) engine.kick.volume.load(),  juce::dontSendNotification);

    bassParamRows[0].slider.setValue ((double) engine.bass.tuneHz.load(),    juce::dontSendNotification);
    // Drive/Cutoff (rows 1-2) are handled by refreshBassOverrideControls()
    // below instead - they show the current Global Pattern's override
    // value when one is active, not always the base value.
    bassParamRows[3].slider.setValue ((double) engine.bass.resonance.load(), juce::dontSendNotification);
    bassParamRows[4].slider.setValue ((double) engine.bass.decayMs.load(),   juce::dontSendNotification);
    bassParamRows[5].slider.setValue ((double) engine.bass.amDepth.load(),   juce::dontSendNotification);
    bassParamRows[6].slider.setValue ((double) engine.bass.volume.load(),    juce::dontSendNotification);
    refreshBassOverrideControls();

    bool useAM = engine.bass.useAMMode.load();
    bassHarmonicModeButton.setButtonText (useAM ? "Harmonic: AM" : "Harmonic: Drive");
    float ratio = engine.bass.amRatio.load();
    bassAmRatioButtons[0].setToggleState (ratio == 1.0f, juce::dontSendNotification);
    bassAmRatioButtons[1].setToggleState (ratio == 2.0f, juce::dontSendNotification);
    bassAmRatioButtons[2].setToggleState (ratio == 3.0f, juce::dontSendNotification);

    skankParamRows[0].slider.setValue ((double) engine.skank.tuneHz.load(),  juce::dontSendNotification);
    skankParamRows[1].slider.setValue ((double) engine.skank.sawMix.load(),  juce::dontSendNotification);
    // Decay (row 2) is handled by refreshSkankOverrideControls() below
    // instead - shows the current Global Pattern's override when active.
    skankParamRows[3].slider.setValue ((double) engine.skank.drive.load(),   juce::dontSendNotification);
    skankParamRows[4].slider.setValue ((double) engine.skank.volume.load(),  juce::dontSendNotification);
    refreshSkankOverrideControls();
    refreshPatternSharingIndicators();
    bool isMinor = engine.skank.minorChord.load();
    skankMajorButton.setToggleState (! isMinor, juce::dontSendNotification);
    skankMinorButton.setToggleState (isMinor, juce::dontSendNotification);
    skankSawMixLaneEditor.repaint();
}

// Drive/Cutoff show whichever value is actually in effect for the
// current Global Pattern (override if active, base otherwise), and the
// two toggle buttons reflect whether that pattern has one active - see
// AudioEngine::ParamOverride and project_raw_dub_song_architecture memory.
void MainComponent::refreshBassOverrideControls()
{
    auto& gp = engine.globalPatterns[(size_t) engine.getCurrentGlobalPatternSlot()];

    bool driveActive = gp.bassDriveOverride.active.load();
    bassDriveOverrideButton.setColour (juce::TextButton::buttonColourId, driveActive ? juce::Colours::black : juce::Colours::white);
    bassDriveOverrideButton.setColour (juce::TextButton::textColourOffId, driveActive ? juce::Colours::white : juce::Colours::black);
    bassParamRows[1].slider.setValue (driveActive ? (double) gp.bassDriveOverride.value.load() : (double) engine.bass.drive.load(),
                                       juce::dontSendNotification);

    bool cutoffActive = gp.bassCutoffOverride.active.load();
    bassCutoffOverrideButton.setColour (juce::TextButton::buttonColourId, cutoffActive ? juce::Colours::black : juce::Colours::white);
    bassCutoffOverrideButton.setColour (juce::TextButton::textColourOffId, cutoffActive ? juce::Colours::white : juce::Colours::black);
    bassParamRows[2].slider.setValue (cutoffActive ? (double) gp.bassCutoffOverride.value.load() : (double) engine.bass.cutoffHz.load(),
                                       juce::dontSendNotification);
}

// same idea as refreshBassOverrideControls, for Skank's Decay override
void MainComponent::refreshSkankOverrideControls()
{
    auto& gp = engine.globalPatterns[(size_t) engine.getCurrentGlobalPatternSlot()];

    bool decayActive = gp.skankDecayOverride.active.load();
    skankDecayOverrideButton.setColour (juce::TextButton::buttonColourId, decayActive ? juce::Colours::black : juce::Colours::white);
    skankDecayOverrideButton.setColour (juce::TextButton::textColourOffId, decayActive ? juce::Colours::white : juce::Colours::black);
    skankParamRows[2].slider.setValue (decayActive ? (double) gp.skankDecayOverride.value.load() : (double) engine.skank.decayMs.load(),
                                        juce::dontSendNotification);
}

// "Make Unique" indicators - see AudioEngine::globalPatternsReferencingKick
// etc and project_raw_dub_song_architecture memory. Only shown when the
// current pattern is referenced by more than one Global Pattern - sharing
// by exactly one (or zero, e.g. never saved anywhere) isn't "shared" in
// any way that needs surfacing, editing in place is already safe.
void MainComponent::refreshPatternSharingIndicators()
{
    auto formatSharedWith = [] (const std::vector<int>& slots) -> juce::String
    {
        juce::String s = "Also in: ";
        for (size_t i = 0; i < slots.size(); ++i)
        {
            if (i > 0)
                s << ", ";
            s << slots[i];
        }
        return s;
    };

    auto kickRefs = engine.globalPatternsReferencingKick (engine.getCurrentKickPatternIndex());
    bool kickShared = kickRefs.size() > 1;
    kickSharedLabel.setVisible (kickShared);
    kickMakeUniqueButton.setVisible (kickShared);
    if (kickShared)
        kickSharedLabel.setText (formatSharedWith (kickRefs), juce::dontSendNotification);

    auto bassRefs = engine.globalPatternsReferencingBass (engine.getCurrentBassPatternIndex());
    bool bassShared = bassRefs.size() > 1;
    bassSharedLabel.setVisible (bassShared);
    bassMakeUniqueButton.setVisible (bassShared);
    if (bassShared)
        bassSharedLabel.setText (formatSharedWith (bassRefs), juce::dontSendNotification);

    auto skankRefs = engine.globalPatternsReferencingSkank (engine.getCurrentSkankPatternIndex());
    bool skankShared = skankRefs.size() > 1;
    skankSharedLabel.setVisible (skankShared);
    skankMakeUniqueButton.setVisible (skankShared);
    if (skankShared)
        skankSharedLabel.setText (formatSharedWith (skankRefs), juce::dontSendNotification);
}

void MainComponent::setVoiceLength (RawDub::StepPattern& pattern, int newLength, int& viewPage)
{
    pattern.setActiveLength (newLength);
    viewPage = 0;
    refreshStepColours();
    resized();
}
