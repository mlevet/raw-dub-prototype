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
                    snareViewPage = 0;
                    hihatViewPage = 0;
                    engine.setCurrentGlobalPatternSlot (0);
                    tempoSlider.setValue (engine.getTempoBpm(), juce::dontSendNotification);
                    refreshBassPitchViewport (true);
                    refreshSkankPitchViewport (true);
                    // a freshly loaded project should start fully calm,
                    // regardless of whatever was expanded before loading
                    for (auto* row : allCurveableRows)
                        row->curveExpanded = false;
                    refreshParamSlidersFromEngine();
                    refreshStepColours();
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
                snareViewPage = 0;
                hihatViewPage = 0;
                engine.setCurrentGlobalPatternSlot (0);
                tempoSlider.setValue (engine.getTempoBpm(), juce::dontSendNotification);
                refreshBassPitchViewport (true);
                refreshSkankPitchViewport (true);
                // a new project should start fully calm, regardless of
                // whatever was expanded before
                for (auto* row : allCurveableRows)
                    row->curveExpanded = false;
                refreshParamSlidersFromEngine();
                refreshStepColours();
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
            snareViewPage = 0;
            hihatViewPage = 0;
            refreshBassPitchViewport (true);
            refreshSkankPitchViewport (true);
            refreshStepColours();
            resized();
        }
        refreshAllOverrideControls(); // different section, possibly different override state - also refreshes each row's curve state (see refreshOverrideControls)
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
        refreshAllOverrideControls();
        refreshPatternSharingIndicators();
        refreshGlobalPatternButtons();
    };

    addAndMakeVisible (clearGlobalPatternButton);
    clearGlobalPatternButton.onClick = [this]
    {
        engine.clearGlobalPattern (engine.getCurrentGlobalPatternSlot());
        refreshAllOverrideControls();
        refreshGlobalPatternButtons();
    };

    // --- Instrument tabs: only one instrument's section is shown at a
    // time, not all three stacked/scrolled ---
    {
        const char* names[6] = { "Kick", "Bass", "Skank", "Snare", "Hi-Hat", "Delay" };
        for (int i = 0; i < 6; ++i)
        {
            auto& btn = instrumentTabButtons[(size_t) i];
            btn.setButtonText (names[i]);
            addAndMakeVisible (btn);
            btn.onClick = [this, i] { switchInstrumentTab (i); };
        }
    }

    // pageViewport/pageContent - see MainComponent.h's comment on
    // pageContent. Every component added below (Kick through Delay) is a
    // child of pageContent, not of MainComponent directly.
    addAndMakeVisible (pageViewport);
    pageViewport.setViewedComponent (&pageContent, false); // false: pageContent is a member, not owned/deleted by the Viewport
    pageViewport.setScrollBarThickness (14);

    // --- Kick ---
    pageContent.addAndMakeVisible (kickTriggerButton);
    kickTriggerButton.onClick = [this] { engine.requestManualKickTrigger(); };

    pageContent.addAndMakeVisible (kickClearButton);
    kickClearButton.onClick = [this]
    {
        engine.kickPattern().clearAll();
        refreshStepColours();
    };

    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        auto& btn = kickStepButtons[(size_t) s];
        pageContent.addAndMakeVisible (btn);
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

    pageContent.addAndMakeVisible (kickLengthLabel);
    for (int i = 0; i < 4; ++i)
    {
        auto& btn = kickLengthButtons[(size_t) i];
        btn.setButtonText (juce::String (lengthOptions[i]));
        pageContent.addAndMakeVisible (btn);
        btn.onClick = [this, i] { setVoiceLength (engine.kickPattern(), lengthOptions[i], kickViewPage); };
    }

    for (int p = 0; p < maxPages; ++p)
    {
        auto& btn = kickPageButtons[(size_t) p];
        btn.setButtonText (juce::String (p * RawDub::numSteps + 1) + "-" + juce::String ((p + 1) * RawDub::numSteps));
        pageContent.addAndMakeVisible (btn);
        btn.onClick = [this, p]
        {
            kickViewPage = p;
            refreshStepColours();
        };
    }

    pageContent.addAndMakeVisible (kickPatternBankLabel);
    for (int i = 0; i < RawDub::AudioEngine::bankSize; ++i)
    {
        auto& btn = kickPatternBankButtons[(size_t) i];
        btn.setButtonText (juce::String (i + 1)); // 1-indexed display
        pageContent.addAndMakeVisible (btn);
        btn.onClick = [this, i]
        {
            engine.setCurrentKickPatternIndex (i);
            kickViewPage = 0;
            refreshStepColours();
            refreshPatternSharingIndicators();
            resized();
        };
    }

    pageContent.addAndMakeVisible (kickSharedLabel);
    pageContent.addAndMakeVisible (kickMakeUniqueButton);
    kickMakeUniqueButton.onClick = [this]
    {
        engine.makeKickPatternUnique();
        refreshPatternSharingIndicators();
        refreshGlobalPatternButtons();
    };

    pageContent.addAndMakeVisible (kickTitleLabel);
    kickTitleLabel.setFont (juce::Font (20.0f, juce::Font::bold));

    {
        using RawDub::KickParamID;
        auto kickPattern = [this]() -> RawDub::StepPattern& { return engine.kickPattern(); };
        auto kickOverrides = [this]() -> RawDub::AudioEngine::OverrideMap& { return engine.globalPatterns[(size_t) engine.getCurrentGlobalPatternSlot()].kickOverrides; };

        // currentInstrumentTab 0 = Kick, see switchInstrumentTab.
        setupCurveableParam (kickCurveRows[0], "Tune",       engine.kick.tuneHz,    30.0, 150.0, 1.0,  (int) KickParamID::Tune,      0, kickPattern, kickOverrides);
        setupCurveableParam (kickCurveRows[1], "Punch",      engine.kick.punchMs,   5.0,  120.0, 1.0,  (int) KickParamID::Punch,     0, kickPattern, kickOverrides);
        setupCurveableParam (kickCurveRows[2], "Decay",      engine.kick.decayMs,   50.0, 800.0, 1.0,  (int) KickParamID::Decay,     0, kickPattern, kickOverrides);
        setupCurveableParam (kickCurveRows[3], "Drive",      engine.kick.drive,     0.0,  1.0,   0.01, (int) KickParamID::Drive,     0, kickPattern, kickOverrides);
        setupCurveableParam (kickCurveRows[4], "Delay Send", engine.kick.delaySend, 0.0,  1.0,   0.01, (int) KickParamID::DelaySend, 0, kickPattern, kickOverrides);

        // Volume - continuously-read mixing control, plain row, see
        // kickPlainRows' comment in MainComponent.h.
        ParamSpec plainSpecs[1] = {
            { "Volume", &engine.kick.volume, 0.0, 1.0, 0.01 },
        };
        for (int i = 0; i < 1; ++i)
        {
            auto& row = kickPlainRows[(size_t) i];

            pageContent.addAndMakeVisible (row.label);
            row.label.setText (plainSpecs[i].name, juce::dontSendNotification);

            pageContent.addAndMakeVisible (row.slider);
            row.slider.setSliderStyle (juce::Slider::LinearHorizontal);
            row.slider.setScrollWheelEnabled (false); // page now scrolls under the mouse - a slider must never eat that wheel event
            row.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
            row.slider.setRange (plainSpecs[i].minV, plainSpecs[i].maxV, plainSpecs[i].step);
            row.slider.setValue ((double) plainSpecs[i].param->load(), juce::dontSendNotification);

            auto* param = plainSpecs[i].param;
            auto* slider = &row.slider;
            row.slider.onValueChange = [param, slider] { param->store ((float) slider->getValue()); };
        }
    }

    // --- Bass ---
    pageContent.addAndMakeVisible (bassTriggerButton);
    bassTriggerButton.onClick = [this] { engine.requestManualBassTrigger(); };

    pageContent.addAndMakeVisible (bassClearButton);
    bassClearButton.onClick = [this]
    {
        engine.bassPattern().clearAll(); // also clears this pattern's curves - see StepPattern::clearAll
        refreshBassPitchViewport (true); // nothing left to show - back to the default center
        refreshStepColours();
        for (auto& row : bassCurveRows)
            refreshCurveRow (row);
        resized();
    };

    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        auto& btn = bassStepButtons[(size_t) s];
        btn.setHasPitch (true);
        pageContent.addAndMakeVisible (btn);
        btn.onToggle = [this, s]
        {
            int stepIndex = bassViewPage * RawDub::numSteps + s;
            engine.bassPattern().toggle (stepIndex);
            // newly activated steps default to wherever you're currently
            // looking, not always the root - matches the existing
            // precedent that toggling on already resets level to Normal
            if (engine.bassPattern().isOn (stepIndex))
                engine.bassPattern().setSemitoneOffset (stepIndex, bassPitchViewportMin + pitchViewportSize / 2);
            refreshStepColours();
        };
        btn.onPitchDrag = [this, s] (int offset, int edgeDirection)
        {
            int stepIndex = bassViewPage * RawDub::numSteps + s;
            bassDraggingStepIndex = stepIndex;

            if (edgeDirection != 0)
            {
                // held in the edge zone - the timer takes over driving
                // both the offset and the viewport from here (see
                // tickPitchEdgeScroll/timerCallback)
                bassPitchEdgeScrollDirection = edgeDirection;
                bassPitchEdgeScrollStepIndex = stepIndex;
                return;
            }
            bassPitchEdgeScrollDirection = 0;

            engine.bassPattern().setSemitoneOffset (stepIndex, offset);
            refreshBassGuideLines(); // reflect the live-updated pitch as you drag

            // fallback for a fast/coalesced move that jumps straight past
            // the window without ever registering inside the edge zone -
            // scroll by the minimum amount needed to keep it in view
            if (offset < bassPitchViewportMin)
                bassPitchViewportMin = offset;
            else if (offset > bassPitchViewportMin + pitchViewportSize)
                bassPitchViewportMin = offset - pitchViewportSize;
            else
                return;
            refreshBassPitchViewport (false);
        };
        btn.onPitchDragEnd = [this]
        {
            bassDraggingStepIndex = -1;
            refreshBassGuideLines(); // hides again unless still hovering an active step
        };
        btn.onHoverChanged = [this, s] (bool entering)
        {
            bassHoveredStepIndex = entering ? (bassViewPage * RawDub::numSteps + s) : -1;
            refreshBassGuideLines();
        };
        btn.onPitchWheel = [this] (float wheelDeltaY)
        {
            // trackpad/wheel scroll over the grid, independent of dragging
            // a note - deltaY sign convention: positive = scroll up.
            // Accumulate rather than round-and-discard each event: most
            // deltas are well under 1.0, so computing a fresh
            // round(delta * scale) every single call rounds to zero
            // every time for a gentle scroll and it never goes anywhere.
            bassPitchWheelAccumulator += wheelDeltaY * 24.0f;
            int semitoneShift = (int) bassPitchWheelAccumulator;
            if (semitoneShift == 0)
                return;
            bassPitchWheelAccumulator -= (float) semitoneShift;
            bassPitchViewportMin = juce::jlimit (-RawDub::StepPattern::maxSemitoneOffset,
                                                  RawDub::StepPattern::maxSemitoneOffset - pitchViewportSize,
                                                  bassPitchViewportMin + semitoneShift);
            refreshBassPitchViewport (false);
        };
        btn.onLevelDrag = [this, s] (RawDub::StepLevel lvl)
        {
            engine.bassPattern().setLevel (bassViewPage * RawDub::numSteps + s, lvl);
        };
    }

    pageContent.addAndMakeVisible (bassLengthLabel);
    for (int i = 0; i < 4; ++i)
    {
        auto& btn = bassLengthButtons[(size_t) i];
        btn.setButtonText (juce::String (lengthOptions[i]));
        pageContent.addAndMakeVisible (btn);
        btn.onClick = [this, i] { setVoiceLength (engine.bassPattern(), lengthOptions[i], bassViewPage); };
    }

    for (int p = 0; p < maxPages; ++p)
    {
        auto& btn = bassPageButtons[(size_t) p];
        btn.setButtonText (juce::String (p * RawDub::numSteps + 1) + "-" + juce::String ((p + 1) * RawDub::numSteps));
        pageContent.addAndMakeVisible (btn);
        btn.onClick = [this, p]
        {
            bassViewPage = p;
            refreshStepColours();
        };
    }

    pageContent.addAndMakeVisible (bassPitchRangeLabel);
    bassPitchRangeLabel.setJustificationType (juce::Justification::centredRight);

    pageContent.addAndMakeVisible (bassPatternBankLabel);
    for (int i = 0; i < RawDub::AudioEngine::bankSize; ++i)
    {
        auto& btn = bassPatternBankButtons[(size_t) i];
        btn.setButtonText (juce::String (i + 1));
        pageContent.addAndMakeVisible (btn);
        btn.onClick = [this, i]
        {
            engine.setCurrentBassPatternIndex (i);
            bassViewPage = 0;
            refreshBassPitchViewport (true);
            refreshStepColours();
            refreshPatternSharingIndicators();
            // curves are pattern-scoped (see StepPattern::curves) - a
            // different Bass pattern may have entirely different curves
            // active, so every row's toggle/editor visibility needs
            // resyncing, not just the step grid.
            for (auto& row : bassCurveRows)
                refreshCurveRow (row);
            resized();
        };
    }

    pageContent.addAndMakeVisible (bassSharedLabel);
    pageContent.addAndMakeVisible (bassMakeUniqueButton);
    bassMakeUniqueButton.onClick = [this]
    {
        engine.makeBassPatternUnique();
        refreshBassPitchViewport (true);
        refreshPatternSharingIndicators();
        refreshGlobalPatternButtons();
        for (auto& row : bassCurveRows)
            refreshCurveRow (row);
        resized();
    };

    pageContent.addAndMakeVisible (bassTitleLabel);
    bassTitleLabel.setFont (juce::Font (20.0f, juce::Font::bold));

    pageContent.addAndMakeVisible (bassHarmonicModeButton);
    bassHarmonicModeButton.onClick = [this]
    {
        bool nowAM = ! engine.bass.useAMMode.load();
        engine.bass.useAMMode.store (nowAM);
        bassHarmonicModeButton.setButtonText (nowAM ? "Harmonic: AM" : "Harmonic: Drive");
    };

    pageContent.addAndMakeVisible (bassAmRatioLabel);

    {
        const char* ratioLabels[3] = { "1:1", "2:1", "3:1" };
        const float ratioValues[3] = { 1.0f, 2.0f, 3.0f };

        for (int i = 0; i < 3; ++i)
        {
            auto& b = bassAmRatioButtons[(size_t) i];
            pageContent.addAndMakeVisible (b);
            b.setButtonText (ratioLabels[i]);
            b.setClickingTogglesState (true);
            b.setRadioGroupId (5001);
            b.setToggleState (i == 0, juce::dontSendNotification);

            float value = ratioValues[i];
            b.onClick = [this, value] { engine.bass.amRatio.store (value); };
        }
    }

    {
        using RawDub::BassParamID;
        // every continuous Bass param is curve+override-capable now (see
        // project_raw_dub_song_architecture memory's 2026-07-10 entries) -
        // getPattern/getOverrideMap are the same for all eleven rows, so
        // defined once here rather than repeated per call.
        auto bassPattern = [this]() -> RawDub::StepPattern& { return engine.bassPattern(); };
        auto bassOverrides = [this]() -> RawDub::AudioEngine::OverrideMap& { return engine.globalPatterns[(size_t) engine.getCurrentGlobalPatternSlot()].bassOverrides; };

        // ordered to match the signal chain: oscillator (Tune) -> Drive
        // (saturation) -> filter (Cutoff/Resonance) -> envelope (Length),
        // AM Depth, then the three Transient modules. currentInstrumentTab
        // 1 = Bass, see switchInstrumentTab.
        setupCurveableParam (bassCurveRows[0], "Tune",   engine.bass.tuneHz,    30.0,  120.0,  1.0,  (int) BassParamID::Tune,   1, bassPattern, bassOverrides);
        setupCurveableParam (bassCurveRows[1], "Drive",  engine.bass.drive,     0.0,   1.0,    0.01, (int) BassParamID::Drive,  1, bassPattern, bassOverrides);
        setupCurveableParam (bassCurveRows[2], "Cutoff", engine.bass.cutoffHz,  100.0, 4000.0, 10.0, (int) BassParamID::Cutoff, 1, bassPattern, bassOverrides);
        setupCurveableParam (bassCurveRows[3], "Resonance", engine.bass.resonance, 0.0, 0.95, 0.01,  (int) BassParamID::Resonance, 1, bassPattern, bassOverrides);
        setupCurveableParam (bassCurveRows[4], "Length", engine.bass.decayMs,   50.0,  4000.0, 10.0, (int) BassParamID::Length, 1, bassPattern, bassOverrides);
        setupCurveableParam (bassCurveRows[5], "AM Depth", engine.bass.amDepth, 0.0,   1.0,    0.01, (int) BassParamID::AmDepth, 1, bassPattern, bassOverrides);
        setupCurveableParam (bassCurveRows[6], "Amount", engine.bass.pitchEnvAmount,   0.0, 12.0,   0.1,  (int) BassParamID::PitchEnvAmount, 1, bassPattern, bassOverrides);
        setupCurveableParam (bassCurveRows[7], "Decay",  engine.bass.pitchEnvDecayMs,  10.0, 300.0, 5.0,  (int) BassParamID::PitchEnvDecay, 1, bassPattern, bassOverrides);
        setupCurveableParam (bassCurveRows[8], "Amount", engine.bass.filterEnvAmount,  0.0, 3000.0, 10.0, (int) BassParamID::FilterEnvAmount, 1, bassPattern, bassOverrides);
        setupCurveableParam (bassCurveRows[9], "Decay",  engine.bass.filterEnvDecayMs, 10.0, 300.0, 5.0,  (int) BassParamID::FilterEnvDecay, 1, bassPattern, bassOverrides);
        setupCurveableParam (bassCurveRows[10], "Amount", engine.bass.driveTransientAmount, 0.0, 1.0, 0.01, (int) BassParamID::DriveTransientAmount, 1, bassPattern, bassOverrides);
        setupCurveableParam (bassCurveRows[11], "Delay Send", engine.bass.delaySend, 0.0, 1.0, 0.01, (int) BassParamID::DelaySend, 1, bassPattern, bassOverrides);

        // Volume - continuously-read mixing control, plain row, see
        // bassPlainRows' comment in MainComponent.h.
        ParamSpec plainSpecs[1] = {
            { "Volume", &engine.bass.volume, 0.0, 1.0, 0.01 },
        };
        for (int i = 0; i < 1; ++i)
        {
            auto& row = bassPlainRows[(size_t) i];
            pageContent.addAndMakeVisible (row.label);
            row.label.setText (plainSpecs[i].name, juce::dontSendNotification);
            pageContent.addAndMakeVisible (row.slider);
            row.slider.setSliderStyle (juce::Slider::LinearHorizontal);
            row.slider.setScrollWheelEnabled (false); // page now scrolls under the mouse - a slider must never eat that wheel event
            row.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
            row.slider.setRange (plainSpecs[i].minV, plainSpecs[i].maxV, plainSpecs[i].step);
            row.slider.setValue ((double) plainSpecs[i].param->load(), juce::dontSendNotification);
            auto* param = plainSpecs[i].param;
            auto* slider = &row.slider;
            row.slider.onValueChange = [param, slider] { param->store ((float) slider->getValue()); };
        }

        pageContent.addAndMakeVisible (bassPitchTransientLabel);
        pageContent.addAndMakeVisible (bassFilterTransientLabel);
        pageContent.addAndMakeVisible (bassDriveTransientLabel);
        bassPitchTransientLabel.setFont (juce::Font (14.0f, juce::Font::bold));
        bassFilterTransientLabel.setFont (juce::Font (14.0f, juce::Font::bold));
        bassDriveTransientLabel.setFont (juce::Font (14.0f, juce::Font::bold));
    }

    // --- Skank (sequenced, see SkankSynth.h) ---
    pageContent.addAndMakeVisible (skankTitleLabel);
    skankTitleLabel.setFont (juce::Font (20.0f, juce::Font::bold));

    pageContent.addAndMakeVisible (skankTriggerButton);
    skankTriggerButton.onClick = [this] { engine.requestManualSkankTrigger(); };

    pageContent.addAndMakeVisible (skankClearButton);
    skankClearButton.onClick = [this]
    {
        engine.skankPattern().clearAll();
        refreshSkankPitchViewport (true); // nothing left to show - back to the default center
        refreshStepColours();
    };

    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        auto& btn = skankStepButtons[(size_t) s];
        btn.setHasPitch (true); // transposes the whole chord's root - see SkankSynth::triggerChord
        pageContent.addAndMakeVisible (btn);
        btn.onToggle = [this, s]
        {
            int stepIndex = skankViewPage * RawDub::numSteps + s;
            engine.skankPattern().toggle (stepIndex);
            if (engine.skankPattern().isOn (stepIndex))
            {
                // newly activated steps are seeded with the current global
                // chord-quality default (see skankChordQualityButtons/
                // skankMajorButton) and default to wherever you're
                // currently looking pitch-wise, not always the root
                engine.skankPatternSlot().setChordIsMinor (stepIndex, engine.skank.minorChord.load());
                engine.skankPattern().setSemitoneOffset (stepIndex, skankPitchViewportMin + pitchViewportSize / 2);
            }
            refreshStepColours();
        };
        btn.onPitchDrag = [this, s] (int offset, int edgeDirection)
        {
            int stepIndex = skankViewPage * RawDub::numSteps + s;
            skankDraggingStepIndex = stepIndex;

            if (edgeDirection != 0)
            {
                skankPitchEdgeScrollDirection = edgeDirection;
                skankPitchEdgeScrollStepIndex = stepIndex;
                return;
            }
            skankPitchEdgeScrollDirection = 0;

            engine.skankPattern().setSemitoneOffset (stepIndex, offset);
            refreshSkankGuideLines();

            if (offset < skankPitchViewportMin)
                skankPitchViewportMin = offset;
            else if (offset > skankPitchViewportMin + pitchViewportSize)
                skankPitchViewportMin = offset - pitchViewportSize;
            else
                return;
            refreshSkankPitchViewport (false);
        };
        btn.onPitchDragEnd = [this]
        {
            skankDraggingStepIndex = -1;
            refreshSkankGuideLines();
        };
        btn.onHoverChanged = [this, s] (bool entering)
        {
            skankHoveredStepIndex = entering ? (skankViewPage * RawDub::numSteps + s) : -1;
            refreshSkankGuideLines();
        };
        btn.onPitchWheel = [this] (float wheelDeltaY)
        {
            skankPitchWheelAccumulator += wheelDeltaY * 24.0f;
            int semitoneShift = (int) skankPitchWheelAccumulator;
            if (semitoneShift == 0)
                return;
            skankPitchWheelAccumulator -= (float) semitoneShift;
            skankPitchViewportMin = juce::jlimit (-RawDub::StepPattern::maxSemitoneOffset,
                                                   RawDub::StepPattern::maxSemitoneOffset - pitchViewportSize,
                                                   skankPitchViewportMin + semitoneShift);
            refreshSkankPitchViewport (false);
        };
        btn.onLevelDrag = [this, s] (RawDub::StepLevel lvl)
        {
            engine.skankPattern().setLevel (skankViewPage * RawDub::numSteps + s, lvl);
        };
    }

    // per-step chord quality lane - click cycles Major/Minor for that step
    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        auto& btn = skankChordQualityButtons[(size_t) s];
        pageContent.addAndMakeVisible (btn);
        btn.onClick = [this, s]
        {
            int stepIndex = skankViewPage * RawDub::numSteps + s;
            bool minor = engine.skankPatternSlot().getChordIsMinor (stepIndex);
            engine.skankPatternSlot().setChordIsMinor (stepIndex, ! minor);
            refreshStepColours();
        };
    }

    pageContent.addAndMakeVisible (skankLengthLabel);
    for (int i = 0; i < 4; ++i)
    {
        auto& btn = skankLengthButtons[(size_t) i];
        btn.setButtonText (juce::String (lengthOptions[i]));
        pageContent.addAndMakeVisible (btn);
        btn.onClick = [this, i] { setVoiceLength (engine.skankPattern(), lengthOptions[i], skankViewPage); };
    }

    for (int p = 0; p < maxPages; ++p)
    {
        auto& btn = skankPageButtons[(size_t) p];
        btn.setButtonText (juce::String (p * RawDub::numSteps + 1) + "-" + juce::String ((p + 1) * RawDub::numSteps));
        pageContent.addAndMakeVisible (btn);
        btn.onClick = [this, p]
        {
            skankViewPage = p;
            refreshStepColours();
        };
    }

    pageContent.addAndMakeVisible (skankPitchRangeLabel);
    skankPitchRangeLabel.setJustificationType (juce::Justification::centredRight);

    pageContent.addAndMakeVisible (skankPatternBankLabel);
    for (int i = 0; i < RawDub::AudioEngine::bankSize; ++i)
    {
        auto& btn = skankPatternBankButtons[(size_t) i];
        btn.setButtonText (juce::String (i + 1));
        pageContent.addAndMakeVisible (btn);
        btn.onClick = [this, i]
        {
            engine.setCurrentSkankPatternIndex (i);
            skankViewPage = 0;
            refreshSkankPitchViewport (true);
            refreshStepColours();
            refreshPatternSharingIndicators();
            // curves are pattern-scoped (see StepPattern::curves) - a
            // different Skank pattern may have entirely different curves
            // active, so every row's toggle/editor visibility needs
            // resyncing, not just the step grid (see bassPatternBankButtons
            // for the same reasoning).
            for (auto& row : skankCurveRows)
                refreshCurveRow (row);
            resized();
        };
    }

    pageContent.addAndMakeVisible (skankSharedLabel);
    pageContent.addAndMakeVisible (skankMakeUniqueButton);
    skankMakeUniqueButton.onClick = [this]
    {
        engine.makeSkankPatternUnique();
        refreshSkankPitchViewport (true);
        refreshPatternSharingIndicators();
        refreshGlobalPatternButtons();
        for (auto& row : skankCurveRows)
            refreshCurveRow (row);
        resized();
    };

    pageContent.addAndMakeVisible (skankMajorButton);
    pageContent.addAndMakeVisible (skankMinorButton);
    skankMajorButton.setClickingTogglesState (true);
    skankMinorButton.setClickingTogglesState (true);
    skankMajorButton.setRadioGroupId (6001);
    skankMinorButton.setRadioGroupId (6001);
    skankMajorButton.setToggleState (true, juce::dontSendNotification);
    skankMajorButton.onClick = [this] { engine.skank.minorChord.store (false); };
    skankMinorButton.onClick = [this] { engine.skank.minorChord.store (true); };

    {
        using RawDub::SkankParamID;
        auto skankPattern = [this]() -> RawDub::StepPattern& { return engine.skankPattern(); };
        auto skankOverrides = [this]() -> RawDub::AudioEngine::OverrideMap& { return engine.globalPatterns[(size_t) engine.getCurrentGlobalPatternSlot()].skankOverrides; };

        // currentInstrumentTab 2 = Skank, see switchInstrumentTab.
        setupCurveableParam (skankCurveRows[0], "Tune",  engine.skank.tuneHz,  200.0, 800.0, 1.0,  (int) SkankParamID::Tune,  2, skankPattern, skankOverrides);
        setupCurveableParam (skankCurveRows[1], "Decay", engine.skank.decayMs, 30.0, 500.0, 1.0,  (int) SkankParamID::Decay, 2, skankPattern, skankOverrides);
        setupCurveableParam (skankCurveRows[2], "Drive", engine.skank.drive,   0.0,  1.0,   0.01, (int) SkankParamID::Drive, 2, skankPattern, skankOverrides);
        setupCurveableParam (skankCurveRows[3], "Delay Send", engine.skank.delaySend, 0.0, 1.0, 0.01, (int) SkankParamID::DelaySend, 2, skankPattern, skankOverrides);
        setupCurveableParam (skankCurveRows[4], "SawMix", engine.skank.sawMix, 0.0, 1.0, 0.01, (int) SkankParamID::SawMix, 2, skankPattern, skankOverrides);

        // Volume - continuously-read mixing control, plain row, see
        // skankPlainRows' comment in MainComponent.h.
        ParamSpec plainSpecs[1] = {
            { "Volume", &engine.skank.volume, 0.0, 1.0, 0.01 },
        };
        for (int i = 0; i < 1; ++i)
        {
            auto& row = skankPlainRows[(size_t) i];

            pageContent.addAndMakeVisible (row.label);
            row.label.setText (plainSpecs[i].name, juce::dontSendNotification);

            pageContent.addAndMakeVisible (row.slider);
            row.slider.setSliderStyle (juce::Slider::LinearHorizontal);
            row.slider.setScrollWheelEnabled (false);
            row.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
            row.slider.setRange (plainSpecs[i].minV, plainSpecs[i].maxV, plainSpecs[i].step);
            row.slider.setValue ((double) plainSpecs[i].param->load(), juce::dontSendNotification);

            auto* param = plainSpecs[i].param;
            auto* slider = &row.slider;
            row.slider.onValueChange = [param, slider] { param->store ((float) slider->getValue()); };
        }
    }

    // --- Snare ---
    pageContent.addAndMakeVisible (snareTriggerButton);
    snareTriggerButton.onClick = [this] { engine.requestManualSnareTrigger(); };

    pageContent.addAndMakeVisible (snareClearButton);
    snareClearButton.onClick = [this]
    {
        engine.snarePattern().clearAll();
        refreshStepColours();
    };

    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        auto& btn = snareStepButtons[(size_t) s];
        pageContent.addAndMakeVisible (btn);
        btn.onToggle = [this, s]
        {
            engine.snarePattern().toggle (snareViewPage * RawDub::numSteps + s);
            refreshStepColours();
        };
        btn.onLevelDrag = [this, s] (RawDub::StepLevel lvl)
        {
            engine.snarePattern().setLevel (snareViewPage * RawDub::numSteps + s, lvl);
        };
    }

    pageContent.addAndMakeVisible (snareLengthLabel);
    for (int i = 0; i < 4; ++i)
    {
        auto& btn = snareLengthButtons[(size_t) i];
        btn.setButtonText (juce::String (lengthOptions[i]));
        pageContent.addAndMakeVisible (btn);
        btn.onClick = [this, i] { setVoiceLength (engine.snarePattern(), lengthOptions[i], snareViewPage); };
    }

    for (int p = 0; p < maxPages; ++p)
    {
        auto& btn = snarePageButtons[(size_t) p];
        btn.setButtonText (juce::String (p * RawDub::numSteps + 1) + "-" + juce::String ((p + 1) * RawDub::numSteps));
        pageContent.addAndMakeVisible (btn);
        btn.onClick = [this, p]
        {
            snareViewPage = p;
            refreshStepColours();
        };
    }

    pageContent.addAndMakeVisible (snarePatternBankLabel);
    for (int i = 0; i < RawDub::AudioEngine::bankSize; ++i)
    {
        auto& btn = snarePatternBankButtons[(size_t) i];
        btn.setButtonText (juce::String (i + 1));
        pageContent.addAndMakeVisible (btn);
        btn.onClick = [this, i]
        {
            engine.setCurrentSnarePatternIndex (i);
            snareViewPage = 0;
            refreshStepColours();
            refreshPatternSharingIndicators();
            resized();
        };
    }

    pageContent.addAndMakeVisible (snareSharedLabel);
    pageContent.addAndMakeVisible (snareMakeUniqueButton);
    snareMakeUniqueButton.onClick = [this]
    {
        engine.makeSnarePatternUnique();
        refreshPatternSharingIndicators();
        refreshGlobalPatternButtons();
    };

    pageContent.addAndMakeVisible (snareTitleLabel);
    snareTitleLabel.setFont (juce::Font (20.0f, juce::Font::bold));

    {
        using RawDub::SnareParamID;
        auto snarePattern = [this]() -> RawDub::StepPattern& { return engine.snarePattern(); };
        auto snareOverrides = [this]() -> RawDub::AudioEngine::OverrideMap& { return engine.globalPatterns[(size_t) engine.getCurrentGlobalPatternSlot()].snareOverrides; };

        // currentInstrumentTab 3 = Snare, see switchInstrumentTab.
        setupCurveableParam (snareCurveRows[0], "Tune",      engine.snare.tuneHz,    50.0,  500.0,  1.0,  (int) SnareParamID::Tune,      3, snarePattern, snareOverrides);
        setupCurveableParam (snareCurveRows[1], "Noise Mix", engine.snare.noiseMix,  0.0,   1.0,    0.01, (int) SnareParamID::NoiseMix,  3, snarePattern, snareOverrides);
        setupCurveableParam (snareCurveRows[2], "Cutoff",    engine.snare.cutoffHz,  200.0, 8000.0, 10.0, (int) SnareParamID::Cutoff,    3, snarePattern, snareOverrides);
        setupCurveableParam (snareCurveRows[3], "Resonance", engine.snare.resonance, 0.0,   0.95,   0.01, (int) SnareParamID::Resonance, 3, snarePattern, snareOverrides);
        setupCurveableParam (snareCurveRows[4], "Decay",     engine.snare.decayMs,   30.0,  500.0,  1.0,  (int) SnareParamID::Decay,     3, snarePattern, snareOverrides);
        setupCurveableParam (snareCurveRows[5], "Drive",     engine.snare.drive,     0.0,   1.0,    0.01, (int) SnareParamID::Drive,     3, snarePattern, snareOverrides);
        setupCurveableParam (snareCurveRows[6], "Delay Send", engine.snare.delaySend, 0.0,  1.0,    0.01, (int) SnareParamID::DelaySend, 3, snarePattern, snareOverrides);

        // Volume - continuously-read mixing control, plain row, see
        // snarePlainRows' comment in MainComponent.h.
        ParamSpec plainSpecs[1] = {
            { "Volume", &engine.snare.volume, 0.0, 1.0, 0.01 },
        };
        for (int i = 0; i < 1; ++i)
        {
            auto& row = snarePlainRows[(size_t) i];

            pageContent.addAndMakeVisible (row.label);
            row.label.setText (plainSpecs[i].name, juce::dontSendNotification);

            pageContent.addAndMakeVisible (row.slider);
            row.slider.setSliderStyle (juce::Slider::LinearHorizontal);
            row.slider.setScrollWheelEnabled (false);
            row.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
            row.slider.setRange (plainSpecs[i].minV, plainSpecs[i].maxV, plainSpecs[i].step);
            row.slider.setValue ((double) plainSpecs[i].param->load(), juce::dontSendNotification);

            auto* param = plainSpecs[i].param;
            auto* slider = &row.slider;
            row.slider.onValueChange = [param, slider] { param->store ((float) slider->getValue()); };
        }
    }

    // --- Hi-Hat (closed) ---
    pageContent.addAndMakeVisible (hihatTriggerButton);
    hihatTriggerButton.onClick = [this] { engine.requestManualHiHatTrigger(); };

    pageContent.addAndMakeVisible (hihatClearButton);
    hihatClearButton.onClick = [this]
    {
        engine.hihatPattern().clearAll();
        refreshStepColours();
    };

    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        auto& btn = hihatStepButtons[(size_t) s];
        pageContent.addAndMakeVisible (btn);
        btn.onToggle = [this, s]
        {
            engine.hihatPattern().toggle (hihatViewPage * RawDub::numSteps + s);
            refreshStepColours();
        };
        btn.onLevelDrag = [this, s] (RawDub::StepLevel lvl)
        {
            engine.hihatPattern().setLevel (hihatViewPage * RawDub::numSteps + s, lvl);
        };
    }

    pageContent.addAndMakeVisible (hihatLengthLabel);
    for (int i = 0; i < 4; ++i)
    {
        auto& btn = hihatLengthButtons[(size_t) i];
        btn.setButtonText (juce::String (lengthOptions[i]));
        pageContent.addAndMakeVisible (btn);
        btn.onClick = [this, i] { setVoiceLength (engine.hihatPattern(), lengthOptions[i], hihatViewPage); };
    }

    for (int p = 0; p < maxPages; ++p)
    {
        auto& btn = hihatPageButtons[(size_t) p];
        btn.setButtonText (juce::String (p * RawDub::numSteps + 1) + "-" + juce::String ((p + 1) * RawDub::numSteps));
        pageContent.addAndMakeVisible (btn);
        btn.onClick = [this, p]
        {
            hihatViewPage = p;
            refreshStepColours();
        };
    }

    pageContent.addAndMakeVisible (hihatPatternBankLabel);
    for (int i = 0; i < RawDub::AudioEngine::bankSize; ++i)
    {
        auto& btn = hihatPatternBankButtons[(size_t) i];
        btn.setButtonText (juce::String (i + 1));
        pageContent.addAndMakeVisible (btn);
        btn.onClick = [this, i]
        {
            engine.setCurrentHiHatPatternIndex (i);
            hihatViewPage = 0;
            refreshStepColours();
            refreshPatternSharingIndicators();
            resized();
        };
    }

    pageContent.addAndMakeVisible (hihatSharedLabel);
    pageContent.addAndMakeVisible (hihatMakeUniqueButton);
    hihatMakeUniqueButton.onClick = [this]
    {
        engine.makeHiHatPatternUnique();
        refreshPatternSharingIndicators();
        refreshGlobalPatternButtons();
    };

    pageContent.addAndMakeVisible (hihatTitleLabel);
    hihatTitleLabel.setFont (juce::Font (20.0f, juce::Font::bold));

    {
        using RawDub::HiHatParamID;
        auto hihatPattern = [this]() -> RawDub::StepPattern& { return engine.hihatPattern(); };
        auto hihatOverrides = [this]() -> RawDub::AudioEngine::OverrideMap& { return engine.globalPatterns[(size_t) engine.getCurrentGlobalPatternSlot()].hihatOverrides; };

        // currentInstrumentTab 4 = Hi-Hat, see switchInstrumentTab.
        setupCurveableParam (hihatCurveRows[0], "Cutoff",    engine.hihat.cutoffHz,  2000.0, 14000.0, 10.0, (int) HiHatParamID::Cutoff,    4, hihatPattern, hihatOverrides);
        setupCurveableParam (hihatCurveRows[1], "Resonance", engine.hihat.resonance, 0.0,    0.95,    0.01, (int) HiHatParamID::Resonance, 4, hihatPattern, hihatOverrides);
        setupCurveableParam (hihatCurveRows[2], "Decay",     engine.hihat.decayMs,   20.0,   400.0,   1.0,  (int) HiHatParamID::Decay,     4, hihatPattern, hihatOverrides);
        setupCurveableParam (hihatCurveRows[3], "Drive",     engine.hihat.drive,     0.0,    1.0,     0.01, (int) HiHatParamID::Drive,     4, hihatPattern, hihatOverrides);
        setupCurveableParam (hihatCurveRows[4], "Delay Send", engine.hihat.delaySend, 0.0,   1.0,     0.01, (int) HiHatParamID::DelaySend, 4, hihatPattern, hihatOverrides);

        // Volume - continuously-read mixing control, plain row, see
        // hihatPlainRows' comment in MainComponent.h.
        ParamSpec plainSpecs[1] = {
            { "Volume", &engine.hihat.volume, 0.0, 1.0, 0.01 },
        };
        for (int i = 0; i < 1; ++i)
        {
            auto& row = hihatPlainRows[(size_t) i];

            pageContent.addAndMakeVisible (row.label);
            row.label.setText (plainSpecs[i].name, juce::dontSendNotification);

            pageContent.addAndMakeVisible (row.slider);
            row.slider.setSliderStyle (juce::Slider::LinearHorizontal);
            row.slider.setScrollWheelEnabled (false);
            row.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
            row.slider.setRange (plainSpecs[i].minV, plainSpecs[i].maxV, plainSpecs[i].step);
            row.slider.setValue ((double) plainSpecs[i].param->load(), juce::dontSendNotification);

            auto* param = plainSpecs[i].param;
            auto* slider = &row.slider;
            row.slider.onValueChange = [param, slider] { param->store ((float) slider->getValue()); };
        }
    }

    // --- Delay (send effect on the whole mix, not sequenced - see DubDelay.h) ---
    pageContent.addAndMakeVisible (delayTitleLabel);
    delayTitleLabel.setFont (juce::Font (20.0f, juce::Font::bold));

    // Length - delayPattern is a curve+Length container, not a
    // sequenced pattern (see AudioEngine::delayPattern's comment) -
    // same 4/16/32/64 options every instrument's Length row already
    // offers, no separate paging UI since there's no step grid to page
    // through.
    pageContent.addAndMakeVisible (delayLengthLabel);
    for (int i = 0; i < 4; ++i)
    {
        auto& btn = delayLengthButtons[(size_t) i];
        btn.setButtonText (juce::String (lengthOptions[i]));
        pageContent.addAndMakeVisible (btn);
        btn.onClick = [this, i]
        {
            engine.delayPattern.setActiveLength (lengthOptions[i]);
            refreshStepColours();
            for (auto& row : delayCurveRows)
                refreshCurveRow (row);
            resized();
        };
    }

    // Time stays a plain slider - see ParamID.h's DelayParamID comment
    // on why it isn't curve/override-capable yet.
    pageContent.addAndMakeVisible (delayTimeRow.label);
    delayTimeRow.label.setText ("Time", juce::dontSendNotification);
    pageContent.addAndMakeVisible (delayTimeRow.slider);
    delayTimeRow.slider.setSliderStyle (juce::Slider::LinearHorizontal);
    delayTimeRow.slider.setScrollWheelEnabled (false);
    delayTimeRow.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
    delayTimeRow.slider.setRange (10.0, 1500.0, 5.0);
    delayTimeRow.slider.setValue ((double) engine.delay.timeMs.load(), juce::dontSendNotification);
    delayTimeRow.slider.onValueChange = [this] { engine.delay.timeMs.store ((float) delayTimeRow.slider.getValue()); };

    {
        using RawDub::DelayParamID;
        auto delayPattern = [this]() -> RawDub::StepPattern& { return engine.delayPattern; };
        auto delayOverrides = [this]() -> RawDub::AudioEngine::OverrideMap& { return engine.globalPatterns[(size_t) engine.getCurrentGlobalPatternSlot()].delayOverrides; };

        // currentInstrumentTab 5 = Delay, see switchInstrumentTab.
        setupCurveableParam (delayCurveRows[0], "Feedback", engine.delay.feedback, 0.0, (double) RawDub::DubDelay::maxFeedback, 0.01, (int) DelayParamID::Feedback, 5, delayPattern, delayOverrides);
        setupCurveableParam (delayCurveRows[1], "Tone",     engine.delay.toneHz,   200.0, 8000.0, 10.0, (int) DelayParamID::Tone,     5, delayPattern, delayOverrides);
        setupCurveableParam (delayCurveRows[2], "Drive",    engine.delay.drive,    0.0,   1.0,    0.01, (int) DelayParamID::Drive,    5, delayPattern, delayOverrides);
        setupCurveableParam (delayCurveRows[3], "Wet",      engine.delay.wet,      0.0,   1.0,    0.01, (int) DelayParamID::Wet,      5, delayPattern, delayOverrides);
    }

    // A/B comparison toggle, same black/white language as the rest of
    // the app - not a permanent "feature," just makes evaluating the
    // loop's contribution to the mix practical during voicing.
    pageContent.addAndMakeVisible (delayBypassButton);
    delayBypassButton.onClick = [this]
    {
        bool newBypass = ! engine.delay.bypass.load();
        engine.delay.bypass.store (newBypass);
        delayBypassButton.setColour (juce::TextButton::buttonColourId, newBypass ? juce::Colours::black : juce::Colours::white);
        delayBypassButton.setColour (juce::TextButton::textColourOffId, newBypass ? juce::Colours::white : juce::Colours::black);
    };

    // --- component groups for the instrument tabs (see switchInstrumentTab) ---
    kickComponents = { &kickTriggerButton, &kickClearButton, &kickTitleLabel, &kickLengthLabel, &kickPatternBankLabel,
                        &kickSharedLabel, &kickMakeUniqueButton };
    for (auto& btn : kickStepButtons) kickComponents.push_back (&btn);
    for (auto& row : kickCurveRows)
    {
        kickComponents.push_back (&row.label);
        kickComponents.push_back (&row.slider);
        kickComponents.push_back (&row.curveEditor);
    }
    for (auto& row : kickPlainRows) { kickComponents.push_back (&row.label); kickComponents.push_back (&row.slider); }
    for (auto& btn : kickLengthButtons) kickComponents.push_back (&btn);
    for (auto& btn : kickPageButtons) kickComponents.push_back (&btn);
    for (auto& btn : kickPatternBankButtons) kickComponents.push_back (&btn);

    bassComponents = { &bassTriggerButton, &bassClearButton, &bassTitleLabel, &bassLengthLabel, &bassPatternBankLabel,
                        &bassHarmonicModeButton, &bassAmRatioLabel,
                        &bassSharedLabel, &bassMakeUniqueButton, &bassPitchRangeLabel };
    for (auto& btn : bassStepButtons) bassComponents.push_back (&btn);
    for (auto& row : bassCurveRows)
    {
        bassComponents.push_back (&row.label);
        bassComponents.push_back (&row.slider);
        bassComponents.push_back (&row.curveEditor);
    }
    for (auto& row : bassPlainRows) { bassComponents.push_back (&row.label); bassComponents.push_back (&row.slider); }
    bassComponents.push_back (&bassPitchTransientLabel);
    bassComponents.push_back (&bassFilterTransientLabel);
    bassComponents.push_back (&bassDriveTransientLabel);
    for (auto& btn : bassLengthButtons) bassComponents.push_back (&btn);
    for (auto& btn : bassPageButtons) bassComponents.push_back (&btn);
    for (auto& btn : bassPatternBankButtons) bassComponents.push_back (&btn);
    for (auto& btn : bassAmRatioButtons) bassComponents.push_back (&btn);

    skankComponents = { &skankTriggerButton, &skankClearButton, &skankTitleLabel, &skankMajorButton, &skankMinorButton,
                         &skankLengthLabel, &skankPatternBankLabel,
                         &skankSharedLabel, &skankMakeUniqueButton, &skankPitchRangeLabel };
    for (auto& btn : skankStepButtons) skankComponents.push_back (&btn);
    for (auto& btn : skankChordQualityButtons) skankComponents.push_back (&btn);
    for (auto& row : skankCurveRows)
    {
        skankComponents.push_back (&row.label);
        skankComponents.push_back (&row.slider);
        skankComponents.push_back (&row.curveEditor);
    }
    for (auto& row : skankPlainRows) { skankComponents.push_back (&row.label); skankComponents.push_back (&row.slider); }
    for (auto& btn : skankLengthButtons) skankComponents.push_back (&btn);
    for (auto& btn : skankPageButtons) skankComponents.push_back (&btn);
    for (auto& btn : skankPatternBankButtons) skankComponents.push_back (&btn);

    snareComponents = { &snareTriggerButton, &snareClearButton, &snareTitleLabel, &snareLengthLabel, &snarePatternBankLabel,
                         &snareSharedLabel, &snareMakeUniqueButton };
    for (auto& btn : snareStepButtons) snareComponents.push_back (&btn);
    for (auto& row : snareCurveRows)
    {
        snareComponents.push_back (&row.label);
        snareComponents.push_back (&row.slider);
        snareComponents.push_back (&row.curveEditor);
    }
    for (auto& row : snarePlainRows) { snareComponents.push_back (&row.label); snareComponents.push_back (&row.slider); }
    for (auto& btn : snareLengthButtons) snareComponents.push_back (&btn);
    for (auto& btn : snarePageButtons) snareComponents.push_back (&btn);
    for (auto& btn : snarePatternBankButtons) snareComponents.push_back (&btn);

    hihatComponents = { &hihatTriggerButton, &hihatClearButton, &hihatTitleLabel, &hihatLengthLabel, &hihatPatternBankLabel,
                         &hihatSharedLabel, &hihatMakeUniqueButton };
    for (auto& btn : hihatStepButtons) hihatComponents.push_back (&btn);
    for (auto& row : hihatCurveRows)
    {
        hihatComponents.push_back (&row.label);
        hihatComponents.push_back (&row.slider);
        hihatComponents.push_back (&row.curveEditor);
    }
    for (auto& row : hihatPlainRows) { hihatComponents.push_back (&row.label); hihatComponents.push_back (&row.slider); }
    for (auto& btn : hihatLengthButtons) hihatComponents.push_back (&btn);
    for (auto& btn : hihatPageButtons) hihatComponents.push_back (&btn);
    for (auto& btn : hihatPatternBankButtons) hihatComponents.push_back (&btn);

    delayComponents = { &delayTitleLabel, &delayBypassButton, &delayLengthLabel, &delayTimeRow.label, &delayTimeRow.slider };
    for (auto& btn : delayLengthButtons) delayComponents.push_back (&btn);
    for (auto& row : delayCurveRows)
    {
        delayComponents.push_back (&row.label);
        delayComponents.push_back (&row.slider);
        delayComponents.push_back (&row.curveEditor);
    }

    switchInstrumentTab (0);

    refreshBassPitchViewport (false);
    refreshSkankPitchViewport (false);
    refreshStepColours();
    updatePlayButtonText();

    setAudioChannels (0, 2);
    // Raised from 900 - Bass's 3 independent Transient modules (12 param
    // rows + 3 headings) need more vertical room than the original size
    // comfortably gave; window is resizable (see Main.cpp) so this is
    // only the first-launch default, not a hard limit.
    setSize (820, 1000);
    startTimerHz (30);
}

void MainComponent::switchInstrumentTab (int tab)
{
    currentInstrumentTab = tab;

    for (auto* c : kickComponents)  c->setVisible (tab == 0);
    for (auto* c : bassComponents)  c->setVisible (tab == 1);
    for (auto* c : skankComponents) c->setVisible (tab == 2);
    for (auto* c : snareComponents) c->setVisible (tab == 3);
    for (auto* c : hihatComponents) c->setVisible (tab == 4);
    for (auto* c : delayComponents) c->setVisible (tab == 5);

    for (int i = 0; i < 6; ++i)
    {
        bool isActive = (i == tab);
        instrumentTabButtons[(size_t) i].setColour (juce::TextButton::buttonColourId, isActive ? juce::Colours::black : juce::Colours::white);
        instrumentTabButtons[(size_t) i].setColour (juce::TextButton::textColourOffId, isActive ? juce::Colours::white : juce::Colours::black);
    }

    resized();
    // each tab starts scrolled to the top - a scroll position from
    // whichever tab was showing before has no meaning on this one
    pageViewport.setViewPosition (0, 0);
    refreshStepColours();
    // sharing indicators have their own conditional visibility (only
    // shown when actually shared) that the blanket show/hide above
    // doesn't know about - re-correct it now
    refreshPatternSharingIndicators();
    // same problem for every instrument's curve rows: the blanket show
    // above just force-showed every row's curveEditor regardless of
    // whether that row's curve is actually active/expanded - re-correct
    // it now, same pattern as sharing indicators. refreshCurveRow itself
    // gates visibility on currentInstrumentTab == row.ownerTab, so this
    // is safe to run over every row regardless of which tab is now active.
    for (auto* row : allCurveableRows)
        refreshCurveRow (*row);
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

// Positions each step's chord-quality button in the same columns as
// skankStepButtons above it (see layoutStepRow) - visibility/text itself
// is refreshed separately in refreshStepColours since it needs to
// change on every toggle, not just on layout.
void MainComponent::layoutSkankChordQualityLane (juce::Rectangle<int> laneRow)
{
    int activeLength = engine.skankPattern().getActiveLength();
    int stepsOnThisPage = juce::jlimit (0, RawDub::numSteps, activeLength - skankViewPage * RawDub::numSteps);
    int stepWidth = stepsOnThisPage > 0 ? laneRow.getWidth() / stepsOnThisPage : 0;

    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        if (s < stepsOnThisPage)
            skankChordQualityButtons[(size_t) s].setBounds (laneRow.removeFromLeft (stepWidth).reduced (2, 0));
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
    tabsRow.removeFromLeft (6);
    instrumentTabButtons[3].setBounds (tabsRow.removeFromLeft (100));
    tabsRow.removeFromLeft (6);
    instrumentTabButtons[4].setBounds (tabsRow.removeFromLeft (100));
    tabsRow.removeFromLeft (6);
    instrumentTabButtons[5].setBounds (tabsRow.removeFromLeft (100));

    area.removeFromTop (16);

    // Only the active instrument's section is shown - each lays out into
    // pageContent starting from y=0, not stacking with the others. What
    // used to be handed `area` directly now goes through pageViewport
    // instead, so a tab whose content is taller than the window (Bass,
    // with several curves/transient rows expanded) scrolls rather than
    // clipping silently at the bottom - see pageContent's comment in
    // MainComponent.h.
    pageViewport.setBounds (area);
    // Width is deliberately narrower than the viewport itself, by the
    // scrollbar's own thickness plus a clear gap - a fixed gutter on the
    // right for it, rather than letting it sit flush against (or
    // overlap) the rightmost slider/value box. Without the extra gap the
    // thumb reads as touching the value boxes rather than as a separate
    // strip of its own.
    constexpr int scrollbarGap = 20;
    int contentWidth = juce::jmax (200, area.getWidth() - pageViewport.getScrollBarThickness() - scrollbarGap);
    // Height is generous and gets trimmed to whatever the layout
    // function actually used (its return value) right after - avoids
    // needing a two-pass layout just to know the height up front.
    juce::Rectangle<int> contentArea (0, 0, contentWidth, 4000);
    int usedHeight = 0;
    if (currentInstrumentTab == 0)
        usedHeight = layoutKickSection (contentArea);
    else if (currentInstrumentTab == 1)
        usedHeight = layoutBassSection (contentArea);
    else if (currentInstrumentTab == 2)
        usedHeight = layoutSkankSection (contentArea);
    else if (currentInstrumentTab == 3)
        usedHeight = layoutSnareSection (contentArea);
    else if (currentInstrumentTab == 4)
        usedHeight = layoutHiHatSection (contentArea);
    else
        usedHeight = layoutDelaySection (contentArea);
    pageContent.setSize (contentWidth, usedHeight);
}

int MainComponent::layoutKickSection (juce::Rectangle<int> area)
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

    auto layoutCurveRow = [&] (CurveableParamRow& row)
    {
        auto rowArea = area.removeFromTop (36);
        row.label.setBounds (rowArea.removeFromLeft (90));
        row.slider.setBounds (rowArea);
        area.removeFromTop (8);

        if (activeCurveExists (row) && row.curveExpanded)
        {
            row.curveEditor.setBounds (area.removeFromTop (70));
            area.removeFromTop (6);
        }
    };
    for (auto& row : kickCurveRows)
        layoutCurveRow (row);

    for (auto& row : kickPlainRows)
    {
        auto rowArea = area.removeFromTop (36);
        row.label.setBounds (rowArea.removeFromLeft (90));
        row.slider.setBounds (rowArea);
        area.removeFromTop (8);
    }

    return area.getY();
}

int MainComponent::layoutBassSection (juce::Rectangle<int> area)
{
    auto bassHeader = area.removeFromTop (28);
    bassPitchRangeLabel.setBounds (bassHeader.removeFromRight (100));
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

    // Each row, by default: label + slider, nothing else - identical
    // footprint to a plain ParamRow (see CurveableParamRow's comment in
    // MainComponent.h, "calm by default"). No separate indicator
    // component to position - the title text and value-box colour ARE
    // the indicator (see refreshCurveRow). The curve editor only
    // reserves space while row.curveExpanded is true for a row that
    // actually has a curve - most rows, most of the time, take zero
    // extra height. Interleaved with bassPlainRows (Volume/Delay Send) and
    // the three Transient module headings, same visual order as before
    // this became curveable.
    auto layoutCurveRow = [&] (CurveableParamRow& row)
    {
        auto rowArea = area.removeFromTop (36);
        row.label.setBounds (rowArea.removeFromLeft (90));
        row.slider.setBounds (rowArea);
        area.removeFromTop (8);

        bool showEditor = activeCurveExists (row) && row.curveExpanded;
        if (showEditor)
        {
            row.curveEditor.setBounds (area.removeFromTop (70));
            area.removeFromTop (6);
        }
    };

    layoutCurveRow (bassCurveRows[0]);
    layoutCurveRow (bassCurveRows[1]);
    layoutCurveRow (bassCurveRows[2]);
    layoutCurveRow (bassCurveRows[3]);
    layoutCurveRow (bassCurveRows[4]);
    layoutCurveRow (bassCurveRows[5]); // AM Depth

    for (auto& row : bassPlainRows)
    {
        auto rowArea = area.removeFromTop (36);
        row.label.setBounds (rowArea.removeFromLeft (90));
        row.slider.setBounds (rowArea);
        area.removeFromTop (8);
    }

    bassPitchTransientLabel.setBounds (area.removeFromTop (22));
    area.removeFromTop (4);
    layoutCurveRow (bassCurveRows[6]);
    layoutCurveRow (bassCurveRows[7]);

    area.removeFromTop (6);
    bassFilterTransientLabel.setBounds (area.removeFromTop (22));
    area.removeFromTop (4);
    layoutCurveRow (bassCurveRows[8]);
    layoutCurveRow (bassCurveRows[9]);

    area.removeFromTop (6);
    bassDriveTransientLabel.setBounds (area.removeFromTop (22));
    area.removeFromTop (4);
    layoutCurveRow (bassCurveRows[10]);

    layoutCurveRow (bassCurveRows[11]); // Delay Send

    return area.getY();
}

int MainComponent::layoutSkankSection (juce::Rectangle<int> area)
{
    auto skankHeader = area.removeFromTop (28);
    skankPitchRangeLabel.setBounds (skankHeader.removeFromRight (100));
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

    area.removeFromTop (4);

    auto skankChordQualityRow = area.removeFromTop (22);
    layoutSkankChordQualityLane (skankChordQualityRow);

    area.removeFromTop (10);

    auto skankChordRow = area.removeFromTop (28);
    skankMajorButton.setBounds (skankChordRow.removeFromLeft (70));
    skankChordRow.removeFromLeft (6);
    skankMinorButton.setBounds (skankChordRow.removeFromLeft (70));

    area.removeFromTop (8);

    auto layoutCurveRow = [&] (CurveableParamRow& row)
    {
        auto rowArea = area.removeFromTop (36);
        row.label.setBounds (rowArea.removeFromLeft (90));
        row.slider.setBounds (rowArea);
        area.removeFromTop (8);

        if (activeCurveExists (row) && row.curveExpanded)
        {
            row.curveEditor.setBounds (area.removeFromTop (70));
            area.removeFromTop (6);
        }
    };

    layoutCurveRow (skankCurveRows[0]); // Tune
    layoutCurveRow (skankCurveRows[1]); // Decay
    layoutCurveRow (skankCurveRows[2]); // Drive
    layoutCurveRow (skankCurveRows[3]); // Delay Send
    layoutCurveRow (skankCurveRows[4]); // SawMix

    for (auto& row : skankPlainRows)
    {
        auto rowArea = area.removeFromTop (36);
        row.label.setBounds (rowArea.removeFromLeft (90));
        row.slider.setBounds (rowArea);
        area.removeFromTop (8);
    }

    return area.getY();
}

int MainComponent::layoutSnareSection (juce::Rectangle<int> area)
{
    auto snareHeader = area.removeFromTop (28);
    snareTitleLabel.setBounds (snareHeader.removeFromLeft (200));
    snareTriggerButton.setBounds (snareHeader.removeFromLeft (90));
    snareHeader.removeFromLeft (6);
    snareClearButton.setBounds (snareHeader.removeFromLeft (70));

    area.removeFromTop (8);

    auto snareLengthRow = area.removeFromTop (28);
    snareLengthLabel.setBounds (snareLengthRow.removeFromLeft (60));
    for (auto& btn : snareLengthButtons)
    {
        btn.setBounds (snareLengthRow.removeFromLeft (44));
        snareLengthRow.removeFromLeft (6);
    }
    snareLengthRow.removeFromLeft (20);
    int snareNumPages = (engine.snarePattern().getActiveLength() + RawDub::numSteps - 1) / RawDub::numSteps;
    for (int p = 0; p < maxPages; ++p)
    {
        snarePageButtons[(size_t) p].setVisible (p < snareNumPages && snareNumPages > 1);
        snarePageButtons[(size_t) p].setBounds (snareLengthRow.removeFromLeft (64));
        snareLengthRow.removeFromLeft (6);
    }

    area.removeFromTop (8);

    auto snarePatternBankRow = area.removeFromTop (28);
    snarePatternBankLabel.setBounds (snarePatternBankRow.removeFromLeft (50));
    for (auto& btn : snarePatternBankButtons)
    {
        btn.setBounds (snarePatternBankRow.removeFromLeft (22));
        snarePatternBankRow.removeFromLeft (2);
    }
    snarePatternBankRow.removeFromLeft (8);
    snareSharedLabel.setBounds (snarePatternBankRow.removeFromLeft (130));
    snareMakeUniqueButton.setBounds (snarePatternBankRow.removeFromLeft (90));

    area.removeFromTop (8);

    auto snareStepRow = area.removeFromTop (50);
    layoutStepRow (snareStepRow, snareStepButtons, engine.snarePattern().getActiveLength(), snareViewPage);

    area.removeFromTop (10);

    auto layoutCurveRow = [&] (CurveableParamRow& row)
    {
        auto rowArea = area.removeFromTop (36);
        row.label.setBounds (rowArea.removeFromLeft (90));
        row.slider.setBounds (rowArea);
        area.removeFromTop (8);

        if (activeCurveExists (row) && row.curveExpanded)
        {
            row.curveEditor.setBounds (area.removeFromTop (70));
            area.removeFromTop (6);
        }
    };
    for (auto& row : snareCurveRows)
        layoutCurveRow (row);

    for (auto& row : snarePlainRows)
    {
        auto rowArea = area.removeFromTop (36);
        row.label.setBounds (rowArea.removeFromLeft (90));
        row.slider.setBounds (rowArea);
        area.removeFromTop (8);
    }

    return area.getY();
}

int MainComponent::layoutHiHatSection (juce::Rectangle<int> area)
{
    auto hihatHeader = area.removeFromTop (28);
    hihatTitleLabel.setBounds (hihatHeader.removeFromLeft (200));
    hihatTriggerButton.setBounds (hihatHeader.removeFromLeft (90));
    hihatHeader.removeFromLeft (6);
    hihatClearButton.setBounds (hihatHeader.removeFromLeft (70));

    area.removeFromTop (8);

    auto hihatLengthRow = area.removeFromTop (28);
    hihatLengthLabel.setBounds (hihatLengthRow.removeFromLeft (60));
    for (auto& btn : hihatLengthButtons)
    {
        btn.setBounds (hihatLengthRow.removeFromLeft (44));
        hihatLengthRow.removeFromLeft (6);
    }
    hihatLengthRow.removeFromLeft (20);
    int hihatNumPages = (engine.hihatPattern().getActiveLength() + RawDub::numSteps - 1) / RawDub::numSteps;
    for (int p = 0; p < maxPages; ++p)
    {
        hihatPageButtons[(size_t) p].setVisible (p < hihatNumPages && hihatNumPages > 1);
        hihatPageButtons[(size_t) p].setBounds (hihatLengthRow.removeFromLeft (64));
        hihatLengthRow.removeFromLeft (6);
    }

    area.removeFromTop (8);

    auto hihatPatternBankRow = area.removeFromTop (28);
    hihatPatternBankLabel.setBounds (hihatPatternBankRow.removeFromLeft (50));
    for (auto& btn : hihatPatternBankButtons)
    {
        btn.setBounds (hihatPatternBankRow.removeFromLeft (22));
        hihatPatternBankRow.removeFromLeft (2);
    }
    hihatPatternBankRow.removeFromLeft (8);
    hihatSharedLabel.setBounds (hihatPatternBankRow.removeFromLeft (130));
    hihatMakeUniqueButton.setBounds (hihatPatternBankRow.removeFromLeft (90));

    area.removeFromTop (8);

    auto hihatStepRow = area.removeFromTop (50);
    layoutStepRow (hihatStepRow, hihatStepButtons, engine.hihatPattern().getActiveLength(), hihatViewPage);

    area.removeFromTop (10);

    auto layoutCurveRow = [&] (CurveableParamRow& row)
    {
        auto rowArea = area.removeFromTop (36);
        row.label.setBounds (rowArea.removeFromLeft (90));
        row.slider.setBounds (rowArea);
        area.removeFromTop (8);

        if (activeCurveExists (row) && row.curveExpanded)
        {
            row.curveEditor.setBounds (area.removeFromTop (70));
            area.removeFromTop (6);
        }
    };
    for (auto& row : hihatCurveRows)
        layoutCurveRow (row);

    for (auto& row : hihatPlainRows)
    {
        auto rowArea = area.removeFromTop (36);
        row.label.setBounds (rowArea.removeFromLeft (90));
        row.slider.setBounds (rowArea);
        area.removeFromTop (8);
    }

    return area.getY();
}

int MainComponent::layoutDelaySection (juce::Rectangle<int> area)
{
    auto delayHeader = area.removeFromTop (28);
    delayTitleLabel.setBounds (delayHeader.removeFromLeft (150));
    delayBypassButton.setBounds (delayHeader.removeFromLeft (90));

    area.removeFromTop (8);

    auto delayLengthRow = area.removeFromTop (28);
    delayLengthLabel.setBounds (delayLengthRow.removeFromLeft (60));
    for (auto& btn : delayLengthButtons)
    {
        btn.setBounds (delayLengthRow.removeFromLeft (44));
        delayLengthRow.removeFromLeft (6);
    }

    area.removeFromTop (16);

    {
        auto rowArea = area.removeFromTop (36);
        delayTimeRow.label.setBounds (rowArea.removeFromLeft (90));
        delayTimeRow.slider.setBounds (rowArea);
        area.removeFromTop (8);
    }

    auto layoutCurveRow = [&] (CurveableParamRow& row)
    {
        auto rowArea = area.removeFromTop (36);
        row.label.setBounds (rowArea.removeFromLeft (90));
        row.slider.setBounds (rowArea);
        area.removeFromTop (8);

        if (activeCurveExists (row) && row.curveExpanded)
        {
            row.curveEditor.setBounds (area.removeFromTop (70));
            area.removeFromTop (6);
        }
    };
    for (auto& row : delayCurveRows)
        layoutCurveRow (row);

    return area.getY();
}

void MainComponent::timerCallback()
{
    updatePlayButtonText();

    int kickStep = engine.isPlaying() ? engine.getCurrentKickStep() : -1;
    int bassStep = engine.isPlaying() ? engine.getCurrentBassStep() : -1;
    int skankStep = engine.isPlaying() ? engine.getCurrentSkankStep() : -1;
    int snareStep = engine.isPlaying() ? engine.getCurrentSnareStep() : -1;
    int hihatStep = engine.isPlaying() ? engine.getCurrentHiHatStep() : -1;
    int delayStep = engine.isPlaying() ? engine.getCurrentDelayStep() : -1;

    if (kickStep != kickPlayheadStep || bassStep != bassPlayheadStep || skankStep != skankPlayheadStep
        || snareStep != snarePlayheadStep || hihatStep != hihatPlayheadStep || delayStep != delayPlayheadStep)
    {
        kickPlayheadStep = kickStep;
        bassPlayheadStep = bassStep;
        skankPlayheadStep = skankStep;
        snarePlayheadStep = snareStep;
        hihatPlayheadStep = hihatStep;
        delayPlayheadStep = delayStep;
        refreshStepColours();
    }

    // edge-scroll while a pitch drag holds in the top/bottom band - see
    // bassPitchEdgeScrollDirection/StepButton::edgeZonePx
    if (bassPitchEdgeScrollDirection != 0)
    {
        tickPitchEdgeScroll (engine.bassPattern(), bassPitchViewportMin, bassPitchEdgeScrollDirection, bassPitchEdgeScrollStepIndex);
        refreshBassPitchViewport (false);
        refreshStepColours();
    }
    if (skankPitchEdgeScrollDirection != 0)
    {
        tickPitchEdgeScroll (engine.skankPattern(), skankPitchViewportMin, skankPitchEdgeScrollDirection, skankPitchEdgeScrollStepIndex);
        refreshSkankPitchViewport (false);
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
    refreshBassGuideLines();

    int skankLength = engine.skankPattern().getActiveLength();
    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        auto& qualityBtn = skankChordQualityButtons[(size_t) s];
        int skankIndex = skankViewPage * RawDub::numSteps + s;
        if (skankIndex >= skankLength)
        {
            qualityBtn.setVisible (false);
            continue;
        }

        auto& skankBtn = skankStepButtons[(size_t) s];
        skankBtn.setOn (engine.skankPattern().isOn (skankIndex));
        skankBtn.setLevel (engine.skankPattern().getLevel (skankIndex));
        skankBtn.setSemitoneOffset (engine.skankPattern().getSemitoneOffset (skankIndex));
        skankBtn.setPlayhead (skankIndex == skankPlayheadStep);

        // chord quality only means anything for an active step - and
        // only when Skank is actually the tab being viewed. This runs
        // every timer tick regardless of which tab is active (same as
        // the rest of this function), so without the tab check it would
        // force these buttons back on over whatever switchInstrumentTab
        // just hid, the moment any Skank step happens to be on.
        if (currentInstrumentTab == 2 && engine.skankPattern().isOn (skankIndex))
        {
            bool minor = engine.skankPatternSlot().getChordIsMinor (skankIndex);
            qualityBtn.setVisible (true);
            qualityBtn.setButtonText (minor ? "min" : "Maj");
            qualityBtn.setColour (juce::TextButton::buttonColourId, minor ? juce::Colours::black : juce::Colours::white);
            qualityBtn.setColour (juce::TextButton::textColourOffId, minor ? juce::Colours::white : juce::Colours::black);
        }
        else
        {
            qualityBtn.setVisible (false);
        }
    }
    refreshSkankGuideLines();

    int snareLength = engine.snarePattern().getActiveLength();
    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        int snareIndex = snareViewPage * RawDub::numSteps + s;
        if (snareIndex >= snareLength)
            continue;

        auto& snareBtn = snareStepButtons[(size_t) s];
        snareBtn.setOn (engine.snarePattern().isOn (snareIndex));
        snareBtn.setLevel (engine.snarePattern().getLevel (snareIndex));
        snareBtn.setPlayhead (snareIndex == snarePlayheadStep);
    }

    int hihatLength = engine.hihatPattern().getActiveLength();
    for (int s = 0; s < RawDub::numSteps; ++s)
    {
        int hihatIndex = hihatViewPage * RawDub::numSteps + s;
        if (hihatIndex >= hihatLength)
            continue;

        auto& hihatBtn = hihatStepButtons[(size_t) s];
        hihatBtn.setOn (engine.hihatPattern().isOn (hihatIndex));
        hihatBtn.setLevel (engine.hihatPattern().getLevel (hihatIndex));
        hihatBtn.setPlayhead (hihatIndex == hihatPlayheadStep);
    }

    for (auto& row : kickCurveRows)
        row.curveEditor.setPlayheadStep (kickPlayheadStep);
    for (auto& row : bassCurveRows)
        row.curveEditor.setPlayheadStep (bassPlayheadStep);
    for (auto& row : skankCurveRows)
        row.curveEditor.setPlayheadStep (skankPlayheadStep);
    for (auto& row : snareCurveRows)
        row.curveEditor.setPlayheadStep (snarePlayheadStep);
    for (auto& row : hihatCurveRows)
        row.curveEditor.setPlayheadStep (hihatPlayheadStep);
    for (auto& row : delayCurveRows)
        row.curveEditor.setPlayheadStep (delayPlayheadStep);

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
    refreshPageButtons (snarePageButtons, snareViewPage, snarePlayheadStep);
    refreshPageButtons (hihatPageButtons, hihatViewPage, hihatPlayheadStep);

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
    refreshLengthButtons (snareLengthButtons, engine.snarePattern().getActiveLength());
    refreshLengthButtons (hihatLengthButtons, engine.hihatPattern().getActiveLength());
    refreshLengthButtons (delayLengthButtons, engine.delayPattern.getActiveLength());

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
    refreshBankButtons (snarePatternBankButtons, engine.getCurrentSnarePatternIndex());
    refreshBankButtons (hihatPatternBankButtons, engine.getCurrentHiHatPatternIndex());

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

// Every CurveableParamRow's slider value/curve/override state is
// resynced generically by refreshAllOverrideControls (loops
// allCurveableRows) - only the plain (non-curveable) rows and the
// handful of non-slider controls (AM mode/ratio, chord quality, delay
// bypass) need touching individually here.
void MainComponent::refreshParamSlidersFromEngine()
{
    refreshAllOverrideControls();

    kickPlainRows[0].slider.setValue ((double) engine.kick.volume.load(), juce::dontSendNotification);

    bassPlainRows[0].slider.setValue ((double) engine.bass.volume.load(), juce::dontSendNotification);

    bool useAM = engine.bass.useAMMode.load();
    bassHarmonicModeButton.setButtonText (useAM ? "Harmonic: AM" : "Harmonic: Drive");
    float ratio = engine.bass.amRatio.load();
    bassAmRatioButtons[0].setToggleState (ratio == 1.0f, juce::dontSendNotification);
    bassAmRatioButtons[1].setToggleState (ratio == 2.0f, juce::dontSendNotification);
    bassAmRatioButtons[2].setToggleState (ratio == 3.0f, juce::dontSendNotification);

    skankPlainRows[0].slider.setValue ((double) engine.skank.volume.load(), juce::dontSendNotification);
    refreshPatternSharingIndicators();
    bool isMinor = engine.skank.minorChord.load();
    skankMajorButton.setToggleState (! isMinor, juce::dontSendNotification);
    skankMinorButton.setToggleState (isMinor, juce::dontSendNotification);

    snarePlainRows[0].slider.setValue ((double) engine.snare.volume.load(), juce::dontSendNotification);

    hihatPlainRows[0].slider.setValue ((double) engine.hihat.volume.load(), juce::dontSendNotification);

    delayTimeRow.slider.setValue ((double) engine.delay.timeMs.load(), juce::dontSendNotification);
    bool bypassed = engine.delay.bypass.load();
    delayBypassButton.setColour (juce::TextButton::buttonColourId, bypassed ? juce::Colours::black : juce::Colours::white);
    delayBypassButton.setColour (juce::TextButton::textColourOffId, bypassed ? juce::Colours::white : juce::Colours::black);
}

// Slider's numeric VALUE only - see MainComponent.h's comment on why
// this is separate from refreshCurveRow. Safe to call on a row with no
// override mechanism at all (early-returns, slider value untouched).
void MainComponent::refreshOverrideControls (CurveableParamRow& row)
{
    if (! row.getOverrideMap)
        return;
    auto* ov = RawDub::AudioEngine::findOverride (row.getOverrideMap(), row.paramId);
    bool active = ov != nullptr && ov->active.load();
    row.slider.setValue (active ? (double) ov->value.load() : (double) row.baseParam->load(), juce::dontSendNotification);
    refreshCurveRow (row);
}

void MainComponent::refreshAllOverrideControls()
{
    for (auto* row : allCurveableRows)
        refreshOverrideControls (*row);
}

// Wires one CurveableParamRow to a specific parameter, on any
// instrument - "any continuous parameter can become curve-capable" (see
// project_raw_dub_song_architecture memory) made concrete: this one
// function is what makes every row behave identically, regardless of
// which parameter or instrument it's for. Interaction model: title =
// phrase-level (curve), value box = section-level (override), slider =
// edit whichever is selected - see MainComponent.h's comment on
// CurveableParamRow for the full reasoning. getPattern/getOverrideMap
// resolve dynamically (see CurveableParamRow's own comment) - a bare
// std::function, not a reference, since the actual pattern/GlobalPattern
// each points to can change independently of this row's own lifetime.
void MainComponent::setupCurveableParam (CurveableParamRow& row, const juce::String& name, std::atomic<float>& baseParam,
                                          double minV, double maxV, double step, int paramId, int ownerTab,
                                          std::function<RawDub::StepPattern& ()> getPattern,
                                          std::function<RawDub::AudioEngine::OverrideMap& ()> getOverrideMap)
{
    row.paramId = paramId;
    row.ownerTab = ownerTab;
    row.baseName = name;
    row.baseParam = &baseParam;
    row.rangeMin = (float) minV;
    row.rangeMax = (float) maxV;
    row.getPattern = std::move (getPattern);
    row.getOverrideMap = std::move (getOverrideMap);

    pageContent.addAndMakeVisible (row.label);
    row.label.setText (name, juce::dontSendNotification);

    pageContent.addAndMakeVisible (row.slider);
    row.slider.setSliderStyle (juce::Slider::LinearHorizontal);
    row.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
    row.slider.setOverrideCapable (static_cast<bool> (row.getOverrideMap));
    row.slider.setRange (minV, maxV, step);
    row.slider.setValue ((double) baseParam.load(), juce::dontSendNotification);
    row.slider.setScrollWheelEnabled (false); // page now scrolls under the mouse - a slider must never eat that wheel event

    pageContent.addChildComponent (row.curveEditor); // starts hidden - see refreshCurveRow

    // TITLE click - phrase-level. No curve yet: create one, seeded flat
    // at whatever the slider currently shows (never a silent jump in
    // sound - "a fixed value is simply a flat curve"), and expand it
    // immediately. Curve already exists: toggle the editor open/closed -
    // the curve itself stays fully active for playback either way.
    row.label.onLabelClick = [this, &row]
    {
        if (row.curveExpanded)
        {
            // collapsing - if nothing was ever actually drawn (still
            // just the flat seed from creation, or flattened back down
            // since), remove it outright rather than leave a title still
            // reading "~" for a curve nobody ever shaped - same "a
            // flattened curve is no curve" rule as the slider/point-edit
            // paths use.
            row.curveExpanded = false;
            if (auto* curve = activeCurve (row))
                if (curve->isFlat())
                    removeActiveCurve (row);
        }
        else
        {
            // expanding - materialize into the active scope if it isn't
            // already there (idempotent: no-op if it already has its own
            // curve). This is what lets an active Override "pick up" a
            // dormant pattern curve the first time it's actually opened,
            // seeded from that curve's own shape (see
            // curveAvailableHere's comment) - never a silent jump, and
            // never leaves the editor showing default/blank data for a
            // curve curveAvailableHere just promised was there.
            float v = (float) row.slider.getValue();
            float normalized = row.rangeMax > row.rangeMin ? (v - row.rangeMin) / (row.rangeMax - row.rangeMin) : 0.0f;
            activeCurveOrCreate (row, normalized);
            row.curveExpanded = true;
        }
        refreshCurveRow (row);
        resized();
    };

    // VALUE BOX click - section-level. Only wired for rows that actually
    // have an override mechanism (ValueBoxSlider ignores this entirely
    // otherwise, per setOverrideCapable above).
    row.slider.onValueBoxClick = [this, &row]
    {
        if (! row.getOverrideMap)
            return;
        auto& ov = RawDub::AudioEngine::getOrCreateOverride (row.getOverrideMap(), row.paramId);
        bool newActive = ! ov.active.load();
        // seed with the current base value so turning an override on
        // never silently jumps the sound - it starts as a no-op. Any
        // curve the override already owns (from a previous time it was
        // active) is deliberately left alone either way - toggling off
        // doesn't discard it, so toggling back on later picks up right
        // where you left it, same as the flat value already does.
        if (newActive)
            ov.value.store (row.baseParam->load());
        else
            row.curveExpanded = false; // leaving this row's curve scope - nothing to show open
        ov.active.store (newActive);
        refreshOverrideControls (row);
        resized();
    };

    // SLIDER - always just edits whichever value is currently selected
    // (section override if active, else the instrument's base value) -
    // unchanged from before curves existed. If a curve is ALSO active,
    // touching the slider REMOVES it outright rather than merely
    // flattening it - a flattened curve is behaviourally identical to no
    // curve, so it doesn't linger as a hidden object once the slider has
    // been used (see MainComponent.h's comment on CurveableParamRow).
    row.slider.onValueChange = [this, &row]
    {
        float v = (float) row.slider.getValue();
        if (row.getOverrideMap)
        {
            auto& ov = RawDub::AudioEngine::getOrCreateOverride (row.getOverrideMap(), row.paramId);
            if (ov.active.load())
                ov.value.store (v);
            else
                row.baseParam->store (v);
        }
        else
        {
            row.baseParam->store (v);
        }

        if (activeCurveExists (row))
        {
            removeActiveCurve (row);
            row.curveExpanded = false;
            refreshCurveRow (row);
            resized();
        }
    };

    row.curveEditor.getPointCount = [this, &row] { auto* c = activeCurve (row); return c ? c->getPointCount() : 2; };
    row.curveEditor.getPointPosition = [this, &row] (int i) { auto* c = activeCurve (row); return c ? c->getPointPosition (i) : (float) i; };
    row.curveEditor.getPointValue = [this, &row] (int i) { auto* c = activeCurve (row); return c ? c->getPointValue (i) : 0.5f; };
    // Point-value edits and removals can flatten a curve too (dragging
    // points together, or removing all but two equal-value anchors) -
    // same auto-removal rule as the slider, checked here via isFlat()
    // since resetToValue() isn't involved on this path.
    row.curveEditor.onPointValueChanged = [this, &row] (int i, float v)
    {
        auto* c = activeCurve (row);
        if (c == nullptr)
            return;
        c->setPointValue (i, v);
        if (c->isFlat())
        {
            removeActiveCurve (row);
            row.curveExpanded = false;
            refreshCurveRow (row);
            resized();
        }
    };
    row.curveEditor.onPointPositionChanged = [this, &row] (int i, float p) { if (auto* c = activeCurve (row)) c->setPointPosition (i, p); };
    row.curveEditor.onAddPoint = [this, &row] (float p, float v) { auto* c = activeCurve (row); return c ? c->insertPoint (p, v) : -1; };
    row.curveEditor.onRemovePoint = [this, &row] (int i)
    {
        auto* c = activeCurve (row);
        if (c == nullptr)
            return;
        c->removePoint (i);
        if (c->isFlat())
        {
            removeActiveCurve (row);
            row.curveExpanded = false;
            refreshCurveRow (row);
            resized();
        }
    };

    refreshCurveRow (row);
    allCurveableRows.push_back (&row);
}

bool MainComponent::rowOverrideActive (const CurveableParamRow& row) const
{
    if (! row.getOverrideMap)
        return false;
    auto* ov = RawDub::AudioEngine::findOverride (row.getOverrideMap(), row.paramId);
    return ov != nullptr && ov->active.load();
}

RawDub::PointCurve* MainComponent::activeCurve (CurveableParamRow& row)
{
    if (rowOverrideActive (row))
    {
        auto* ov = RawDub::AudioEngine::findOverride (row.getOverrideMap(), row.paramId);
        return (ov != nullptr && ov->hasCurve.load()) ? &ov->curve : nullptr;
    }
    return row.getPattern().findCurve (row.paramId);
}

RawDub::PointCurve& MainComponent::activeCurveOrCreate (CurveableParamRow& row, float seedNormalized)
{
    if (rowOverrideActive (row))
    {
        auto& ov = RawDub::AudioEngine::getOrCreateOverride (row.getOverrideMap(), row.paramId);
        if (! ov.hasCurve.load())
        {
            // seed from the pattern's own curve shape if it has one -
            // "start editing under Override" begins from what you were
            // already hearing, not a silent jump to flat - otherwise
            // start flat, same "never a silent jump" rule as everywhere
            // else a curve gets created.
            if (auto* patternCurve = row.getPattern().findCurve (row.paramId))
                ov.curve.copyFrom (*patternCurve);
            else
                ov.curve.resetToValue (seedNormalized);
            ov.hasCurve.store (true);
        }
        return ov.curve;
    }
    return row.getPattern().getOrCreateCurve (row.paramId, seedNormalized);
}

bool MainComponent::activeCurveExists (const CurveableParamRow& row) const
{
    if (rowOverrideActive (row))
    {
        auto* ov = RawDub::AudioEngine::findOverride (row.getOverrideMap(), row.paramId);
        return ov != nullptr && ov->hasCurve.load();
    }
    return row.getPattern().hasCurve (row.paramId);
}

void MainComponent::removeActiveCurve (CurveableParamRow& row)
{
    if (rowOverrideActive (row))
    {
        if (auto* ov = RawDub::AudioEngine::findOverride (row.getOverrideMap(), row.paramId))
            ov->hasCurve.store (false);
    }
    else
    {
        row.getPattern().removeCurve (row.paramId);
    }
}

bool MainComponent::curveAvailableHere (const CurveableParamRow& row) const
{
    if (activeCurveExists (row))
        return true;
    // override active but has no curve of its own YET - the pattern's
    // curve is still there underneath, just dormant (see this function's
    // header comment). Only relevant when override is active: when it's
    // not, activeCurveExists above already checked the pattern directly.
    if (rowOverrideActive (row))
        return row.getPattern().hasCurve (row.paramId);
    return false;
}

// Resyncs one row's title text, value-box colour, and curve-editor
// visibility to its instrument's current pattern state - called
// whenever that state may have changed out from under the UI (pattern
// switch, project load, title/value-box click). Does NOT touch the
// slider's numeric value - that stays whoever last set it
// (refreshParamSlidersFromEngine / refreshOverrideControls), since
// the slider always represents "the last explicit value," independent
// of whatever the curve is doing during playback.
void MainComponent::refreshCurveRow (CurveableParamRow& row)
{
    // Label uses the broader "available here" check (includes a dormant
    // pattern curve sitting under an active override) so the indicator
    // never silently vanishes just because Override was switched on -
    // see curveAvailableHere's comment. Editor visibility below stays on
    // the STRICT in-scope check - a dormant curve isn't directly on
    // screen until the title is actually clicked to materialize it.
    bool hasCurve = activeCurveExists (row);
    row.label.setText (curveAvailableHere (row) ? ("~ " + row.baseName) : row.baseName, juce::dontSendNotification);

    // Gated on this row's OWN instrument tab actually being the one
    // showing - this runs from more than just that tab's local code
    // (e.g. switchInstrumentTab calls it to re-correct its own blanket
    // show/hide), so without this check it would happily re-show a
    // curve editor while a different instrument tab is active, the same
    // visibility bug this project has hit before with other per-tab
    // components.
    bool showEditor = hasCurve && row.curveExpanded && currentInstrumentTab == row.ownerTab;
    row.curveEditor.setVisible (showEditor);
    row.curveEditor.setGridDivisions (row.getPattern().getActiveLength());
    row.curveEditor.repaint();

    bool overrideActive = rowOverrideActive (row);
    row.slider.setColour (juce::Slider::textBoxBackgroundColourId, overrideActive ? juce::Colours::black : juce::Colours::white);
    row.slider.setColour (juce::Slider::textBoxTextColourId, overrideActive ? juce::Colours::white : juce::Colours::black);
    row.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::black);
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

    // This gets called from many places that don't know/care which tab
    // is active (pattern-bank clicks, Global Pattern recall, New/Open,
    // Make Unique - not just switchInstrumentTab), so each instrument's
    // visibility must be gated on its own tab being the current one -
    // otherwise a Bass action could force Kick's indicator visible again
    // over whatever switchInstrumentTab had already hidden.
    auto kickRefs = engine.globalPatternsReferencingKick (engine.getCurrentKickPatternIndex());
    bool kickShared = currentInstrumentTab == 0 && kickRefs.size() > 1;
    kickSharedLabel.setVisible (kickShared);
    kickMakeUniqueButton.setVisible (kickShared);
    if (kickShared)
        kickSharedLabel.setText (formatSharedWith (kickRefs), juce::dontSendNotification);

    auto bassRefs = engine.globalPatternsReferencingBass (engine.getCurrentBassPatternIndex());
    bool bassShared = currentInstrumentTab == 1 && bassRefs.size() > 1;
    bassSharedLabel.setVisible (bassShared);
    bassMakeUniqueButton.setVisible (bassShared);
    if (bassShared)
        bassSharedLabel.setText (formatSharedWith (bassRefs), juce::dontSendNotification);

    auto skankRefs = engine.globalPatternsReferencingSkank (engine.getCurrentSkankPatternIndex());
    bool skankShared = currentInstrumentTab == 2 && skankRefs.size() > 1;
    skankSharedLabel.setVisible (skankShared);
    skankMakeUniqueButton.setVisible (skankShared);
    if (skankShared)
        skankSharedLabel.setText (formatSharedWith (skankRefs), juce::dontSendNotification);

    auto snareRefs = engine.globalPatternsReferencingSnare (engine.getCurrentSnarePatternIndex());
    bool snareShared = currentInstrumentTab == 3 && snareRefs.size() > 1;
    snareSharedLabel.setVisible (snareShared);
    snareMakeUniqueButton.setVisible (snareShared);
    if (snareShared)
        snareSharedLabel.setText (formatSharedWith (snareRefs), juce::dontSendNotification);

    auto hihatRefs = engine.globalPatternsReferencingHiHat (engine.getCurrentHiHatPatternIndex());
    bool hihatShared = currentInstrumentTab == 4 && hihatRefs.size() > 1;
    hihatSharedLabel.setVisible (hihatShared);
    hihatMakeUniqueButton.setVisible (hihatShared);
    if (hihatShared)
        hihatSharedLabel.setText (formatSharedWith (hihatRefs), juce::dontSendNotification);
}

void MainComponent::setVoiceLength (RawDub::StepPattern& pattern, int newLength, int& viewPage)
{
    pattern.setActiveLength (newLength);
    viewPage = 0;
    refreshStepColours();
    resized();
}

// "I should immediately see the musical material [a pattern] contains"
// - if its active steps all fit inside the default root-centered view,
// leave the view alone; otherwise shift just enough to include them,
// centering on the pattern's own pitch range if it's wider than one
// window's worth.
int MainComponent::computeAutoCenteredPitchViewportMin (const RawDub::StepPattern& pattern)
{
    constexpr int defaultMin = -12;

    int lo = RawDub::StepPattern::maxSemitoneOffset + 1;
    int hi = -RawDub::StepPattern::maxSemitoneOffset - 1;

    for (int i = 0; i < RawDub::StepPattern::maxLength; ++i)
    {
        if (! pattern.isOn (i))
            continue;

        int off = pattern.getSemitoneOffset (i);
        lo = juce::jmin (lo, off);
        hi = juce::jmax (hi, off);
    }

    if (lo > hi) // no active steps at all
        return defaultMin;

    if (lo >= defaultMin && hi <= defaultMin + pitchViewportSize)
        return defaultMin;

    int center = (lo + hi) / 2;
    return center - pitchViewportSize / 2;
}

// Guide lines only mean anything as a live reference while you're
// actually looking at an existing note or placing/modifying one - never
// all the time (clutter on a grid at rest), and always exactly one
// line, never a survey of the whole pattern: hovering shows that step's
// own pitch, dragging shows the dragged step's live pitch (which sweeps
// through as you drag, so you can see it align against another step's
// bar without needing every pitch drawn at once).
void MainComponent::refreshBassGuideLines()
{
    std::vector<int> pitches;
    if (bassDraggingStepIndex >= 0)
        pitches = { engine.bassPattern().getSemitoneOffset (bassDraggingStepIndex) };
    else if (bassHoveredStepIndex >= 0 && engine.bassPattern().isOn (bassHoveredStepIndex))
        pitches = { engine.bassPattern().getSemitoneOffset (bassHoveredStepIndex) };

    for (auto& btn : bassStepButtons)
        btn.setGuidePitches (pitches);
}

void MainComponent::refreshSkankGuideLines()
{
    std::vector<int> pitches;
    if (skankDraggingStepIndex >= 0)
        pitches = { engine.skankPattern().getSemitoneOffset (skankDraggingStepIndex) };
    else if (skankHoveredStepIndex >= 0 && engine.skankPattern().isOn (skankHoveredStepIndex))
        pitches = { engine.skankPattern().getSemitoneOffset (skankHoveredStepIndex) };

    for (auto& btn : skankStepButtons)
        btn.setGuidePitches (pitches);
}

void MainComponent::refreshBassPitchViewport (bool autoCenter)
{
    if (autoCenter)
        bassPitchViewportMin = computeAutoCenteredPitchViewportMin (engine.bassPattern());
    for (auto& btn : bassStepButtons)
        btn.setPitchViewport (bassPitchViewportMin, bassPitchViewportMin + pitchViewportSize);
    bassPitchRangeLabel.setText (juce::String (bassPitchViewportMin) + ".." + juce::String (bassPitchViewportMin + pitchViewportSize) + " st",
                                  juce::dontSendNotification);
}

void MainComponent::refreshSkankPitchViewport (bool autoCenter)
{
    if (autoCenter)
        skankPitchViewportMin = computeAutoCenteredPitchViewportMin (engine.skankPattern());
    for (auto& btn : skankStepButtons)
        btn.setPitchViewport (skankPitchViewportMin, skankPitchViewportMin + pitchViewportSize);
    skankPitchRangeLabel.setText (juce::String (skankPitchViewportMin) + ".." + juce::String (skankPitchViewportMin + pitchViewportSize) + " st",
                                   juce::dontSendNotification);
}

// shared by both instruments' timerCallback ticks - advances the
// dragged step's pitch by one semitone and scrolls the viewport to
// match, for as long as the edge zone is held (see StepButton::edgeZonePx)
void MainComponent::tickPitchEdgeScroll (RawDub::StepPattern& pattern, int& viewportMin, int direction, int stepIndex)
{
    int newOffset = juce::jlimit (-RawDub::StepPattern::maxSemitoneOffset, RawDub::StepPattern::maxSemitoneOffset,
                                   pattern.getSemitoneOffset (stepIndex) + direction);
    pattern.setSemitoneOffset (stepIndex, newOffset);
    viewportMin = juce::jlimit (-RawDub::StepPattern::maxSemitoneOffset, RawDub::StepPattern::maxSemitoneOffset - pitchViewportSize,
                                 viewportMin + direction);
}
