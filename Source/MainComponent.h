#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "BWLookAndFeel.h"
#include "StepButton.h"
#include "CurveLaneEditor.h"
#include "ParamID.h"
#include "ClickableLabel.h"
#include "ValueBoxSlider.h"
#include "ProjectIO.h"
#include <array>
#include <vector>
#include <functional>

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

    // Pitch viewport (Bass/Skank only, Kick has no pitch) - see
    // project_raw_dub_song_architecture memory. A fixed-size (24-
    // semitone) window onto StepPattern's wider +/-36 range; only its
    // position (viewportMin) moves, never its size, so a given interval
    // always draws the same height regardless of what else is in the
    // pattern. Two distinct ways this moves, never conflated:
    // (1) auto-centered on a pattern's actual content whenever a
    //     DIFFERENT pattern becomes the one you're looking at (switching
    //     slots, recalling a Global Pattern, Make Unique, New/Open) -
    //     see computeAutoCenteredPitchViewportMin.
    // (2) scrolled by the minimum amount needed to keep the note you're
    //     dragging in view, while editing an already-visible pattern -
    //     see the onPitchDrag handlers.
    // Deliberately UI-only, not saved - reset/recomputed fresh each time
    // rather than persisted per slot (simpler, and since it's computed
    // from content it lands in a sensible place anyway).
    static constexpr int pitchViewportSize = 24; // same visible height as the original +/-12
    int bassPitchViewportMin = -12;
    int skankPitchViewportMin = -12;
    static int computeAutoCenteredPitchViewportMin (const RawDub::StepPattern& pattern);
    void refreshBassPitchViewport (bool autoCenter);
    void refreshSkankPitchViewport (bool autoCenter);

    // Guide lines are only shown while actively relevant - hovering an
    // existing active step, or drawing/dragging a new one - not visible
    // all the time (would clutter the grid at rest). -1 = not hovering/
    // dragging any step right now. Both cases show exactly one line -
    // that step's own (live, if dragging) pitch - never the whole
    // pattern's pitch set; while dragging, that one line sweeping
    // through is what lets you see it align against another step's bar.
    int bassHoveredStepIndex = -1;
    int bassDraggingStepIndex = -1;
    int skankHoveredStepIndex = -1;
    int skankDraggingStepIndex = -1;
    void refreshBassGuideLines();
    void refreshSkankGuideLines();

    // Edge-scroll: while a pitch drag holds within the top/bottom band
    // of the step (see StepButton::edgeZonePx), the timer (already
    // running at 30Hz for the playhead) keeps advancing the dragged
    // step's pitch and scrolling the viewport every tick, for as long as
    // the direction stays non-zero - continuing even if the cursor isn't
    // moving, which a purely per-mouse-event scroll can't do. Cleared
    // (direction = 0) on mouseUp or once the cursor leaves the zone.
    int bassPitchEdgeScrollDirection = 0;
    int bassPitchEdgeScrollStepIndex = -1;
    int skankPitchEdgeScrollDirection = 0;
    int skankPitchEdgeScrollStepIndex = -1;
    static void tickPitchEdgeScroll (RawDub::StepPattern& pattern, int& viewportMin, int direction, int stepIndex);

    // Wheel/trackpad deltas are usually well under 1.0 per event - an
    // accumulator carries the fractional remainder across events so a
    // gentle scroll still adds up to a whole semitone eventually,
    // instead of every single event rounding to zero and being discarded.
    float bassPitchWheelAccumulator = 0.0f;
    float skankPitchWheelAccumulator = 0.0f;

    // Minimal feedback on where the viewport currently sits - see
    // refreshBassPitchViewport/refreshSkankPitchViewport
    juce::Label bassPitchRangeLabel;
    juce::Label skankPitchRangeLabel;

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

    // "Any continuous parameter can become curve-capable" (see
    // project_raw_dub_song_architecture memory) - the reusable building
    // block, one instance per continuous parameter, on ANY instrument
    // (originally Bass-only, generalized to every instrument once asked
    // for explicitly - see that memory's 2026-07-10 dated entries).
    // Interaction model: the TITLE controls phrase-level behaviour (click
    // creates/expands/collapses a curve), the VALUE BOX controls
    // section-level behaviour (single click toggles the section
    // override, double-click still types an exact value), the SLIDER
    // always just edits whichever of those is currently selected - it
    // never changes role itself. No separate indicator component
    // needed: the title text itself ("Cutoff" vs "~ Cutoff") shows curve
    // state, and the value box's own colour (normal vs inverted) shows
    // override state - both ARE the indicator, calm by default with
    // nothing extra to manage.
    //
    // curveExpanded is UI-only state, separate from "does a curve
    // exist": a curve can exist on the pattern while its editor is
    // collapsed - it stays fully active for playback, just off-screen.
    // Moving the slider writes the instrument's base atomic (or the
    // section override, if active) exactly as before curves existed -
    // AND, if a curve is currently active, REMOVES it outright rather
    // than merely flattening it: a flattened curve is behaviourally
    // identical to no curve at all, so it doesn't linger as a hidden
    // object once the slider has been touched. Manually flattening a
    // curve by dragging its points together (in the editor) triggers
    // the same removal - see PointCurve::isFlat.
    struct CurveableParamRow
    {
        RawDub::ClickableLabel label;
        RawDub::ValueBoxSlider slider;
        RawDub::CurveLaneEditor curveEditor;
        // Cast from whichever instrument's own ParamID enum (BassParamID,
        // KickParamID, etc) at setup time - bare int from here on, since
        // it only ever needs to be unique within getPattern()'s own
        // curves map / getOverrideMap()'s own override map, never
        // globally (see ParamID.h).
        int paramId = 0;
        juce::String baseName; // e.g. "Cutoff" - label text is derived from this plus curve state
        std::atomic<float>* baseParam = nullptr;
        float rangeMin = 0.0f, rangeMax = 1.0f;
        bool curveExpanded = false;
        // Which instrument tab this row belongs to (0=Kick..4=Hi-Hat) -
        // needed generically now that curve editors exist on every
        // instrument, not just Bass, to gate curve-editor visibility on
        // the right tab (see refreshCurveRow).
        int ownerTab = -1;
        // Resolves to whichever StepPattern is CURRENTLY live for this
        // row's instrument (e.g. engine.bassPattern()) - a function, not
        // a cached reference, because which pattern that is changes with
        // the instrument's own pattern-bank selection.
        std::function<RawDub::StepPattern&()> getPattern;
        // Resolves to whichever Global Pattern's override map applies to
        // this row's instrument (e.g. current-slot's bassOverrides) - a
        // function, not a cached reference, because which Global Pattern
        // is current changes independently. Empty function = no
        // section-override mechanism for this row at all (none currently,
        // but kept optional rather than assumed).
        std::function<RawDub::AudioEngine::OverrideMap& ()> getOverrideMap;
    };
    void setupCurveableParam (CurveableParamRow& row, const juce::String& name, std::atomic<float>& baseParam,
                               double minV, double maxV, double step, int paramId, int ownerTab,
                               std::function<RawDub::StepPattern& ()> getPattern,
                               std::function<RawDub::AudioEngine::OverrideMap& ()> getOverrideMap = {});
    bool rowOverrideActive (const CurveableParamRow& row) const;
    // Resyncs one row's title text/value-box colour/curve-editor
    // visibility to its instrument's current pattern state - called
    // whenever that state may have changed out from under the UI
    // (pattern switch, project load, title/value-box click).
    void refreshCurveRow (CurveableParamRow& row);
    // Slider's numeric VALUE (override-or-base, per rowOverrideActive) is
    // deliberately a separate refresh from refreshCurveRow above - only
    // needs recomputing when the section (Global Pattern slot) itself
    // may have changed, not on every curve-editor-local refresh (see
    // refreshCurveRow's own comment: it never touches slider value).
    void refreshOverrideControls (CurveableParamRow& row);
    void refreshAllOverrideControls(); // loops allCurveableRows - see its own comment

    // The curve currently "in scope" for a row: if the section override
    // is active, that's the override's OWN curve (independent of the
    // pattern's) - otherwise it's the pattern's curve. This is what lets
    // Override own a shaped curve variant without touching the pattern's
    // curve that other sections/patterns still see. Returns nullptr if
    // the in-scope curve doesn't exist yet (flat override, or no pattern
    // curve). Non-const: callers mutate through the returned pointer.
    RawDub::PointCurve* activeCurve (CurveableParamRow& row);
    // Same in-scope logic, but creates the curve if it doesn't exist yet
    // - seeded from the pattern's curve shape if the override is active
    // and the pattern already has one (so "start editing under Override"
    // begins from what you were already hearing), else flat at
    // seedNormalized (0-1, already normalized into the curve's own
    // space - same convention as PointCurve::resetToValue).
    RawDub::PointCurve& activeCurveOrCreate (CurveableParamRow& row, float seedNormalized);
    bool activeCurveExists (const CurveableParamRow& row) const;
    void removeActiveCurve (CurveableParamRow& row);
    // Distinct from activeCurveExists (which is strictly in-scope: only
    // true if the override, when active, already has its OWN materialized
    // curve). This is true if curve material exists ANYWHERE relevant to
    // the row - including a pattern curve sitting dormant underneath an
    // active override that hasn't been opened yet. Drives the "~" label
    // only - never what the curve editor actually shows/edits or whether
    // a slider touch should auto-remove a curve (both of those must stay
    // strictly in-scope, or a dormant pattern curve would get silently
    // wiped by moving the override's slider). See setupCurveableParam's
    // title-click handler, which uses this to decide whether to
    // materialize-and-seed a pattern curve into the override on first
    // open, vs. create a genuinely fresh flat one.
    bool curveAvailableHere (const CurveableParamRow& row) const;

    // Every CurveableParamRow across every instrument, populated once in
    // the constructor after all five instruments' rows are set up -
    // lets refreshAllOverrideControls (and anything else that needs
    // "touch every curveable row") be one generic loop instead of one
    // hand-written loop per instrument.
    std::vector<CurveableParamRow*> allCurveableRows;

    // Only one instrument's section is shown at a time - the others are
    // hidden entirely, never stacked. Each instrument's full set of
    // components is collected into one of these vectors (populated once,
    // in the constructor) so switching tabs is just "hide these, show
    // those" instead of touching every component individually inline.
    // currentInstrumentTab: 0=Kick, 1=Bass, 2=Skank, 3=Snare, 4=Hi-Hat, 5=Delay.
    void switchInstrumentTab (int tab);
    // Each returns the total height it laid out its components into
    // (area.getY() after every removeFromTop call) - resized() uses this
    // to size pageContent so pageViewport's scrollbar knows exactly how
    // far there is to scroll for whichever tab is currently showing. All
    // six take a page-content-relative area (already narrower than the
    // window - see pageContent's comment) with a generous, deliberately
    // oversized height; nothing about the row layout itself changed,
    // only where these components live (pageContent, not MainComponent
    // directly) and how much of it is actually used.
    int layoutKickSection (juce::Rectangle<int> area);
    int layoutBassSection (juce::Rectangle<int> area);
    int layoutSkankSection (juce::Rectangle<int> area);
    int layoutSnareSection (juce::Rectangle<int> area);
    int layoutHiHatSection (juce::Rectangle<int> area);
    int layoutDelaySection (juce::Rectangle<int> area);
    int currentInstrumentTab = 0;
    std::array<juce::TextButton, 6> instrumentTabButtons; // Kick / Bass / Skank / Snare / Hi-Hat / Delay
    std::vector<juce::Component*> kickComponents, bassComponents, skankComponents, snareComponents, hihatComponents, delayComponents;

    // Bass in particular has grown too tall to fit one window (curves,
    // transient modules) - every per-instrument-tab component
    // (kick/bass/skank/snare/hihat/delay's own labels/sliders/step grids/
    // curve editors) is a child of pageContent rather than of MainComponent
    // directly, and pageContent is the viewed component of pageViewport,
    // which is what actually makes the tall tabs scrollable. The
    // transport bar / Global Patterns row / instrument tab buttons stay
    // direct children of MainComponent - only the per-tab content below
    // them scrolls. pageContent's width is deliberately narrower than
    // pageViewport's (see resized()) so the scrollbar has its own gutter
    // on the right rather than overlapping the rightmost slider/value
    // box. Every slider inside pageContent has scroll-wheel-to-nudge
    // disabled (setScrollWheelEnabled(false)) - once the page itself
    // scrolls under the mouse, a slider silently eating that wheel
    // event and changing its value instead would be exactly the kind of
    // surprise this system otherwise goes out of its way to avoid.
    juce::Viewport pageViewport;
    juce::Component pageContent;

    // Kick - length selectable 4/16/32/64, shown/edited 16 steps per page,
    // same paging mechanism as Bass.
    juce::TextButton kickTriggerButton { "Trigger" };
    juce::TextButton kickClearButton { "Clear" };
    std::array<RawDub::StepButton, RawDub::numSteps> kickStepButtons;
    juce::Label kickTitleLabel { {}, "Kick" };
    std::array<CurveableParamRow, 5> kickCurveRows; // Tune, Punch, Decay, Drive, Delay Send
    std::array<ParamRow, 1> kickPlainRows; // Volume
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
    // Every continuous Bass param except Volume/Delay Send (see
    // ParamID.h for why) - Tune(0), Drive(1), Cutoff(2), Resonance(3),
    // Length(4), AM Depth(5), Pitch Amount(6), Pitch Decay(7), Filter
    // Amount(8), Filter Decay(9), Drive Transient Amount(10) - matches
    // BassParamID's declaration order exactly. Each row can independently
    // grow a curve editor - see CurveableParamRow.
    std::array<CurveableParamRow, 12> bassCurveRows; // ...DriveTransientAmount(10), Delay Send(11)
    // Volume - continuously-read per-block mixing control, not a
    // trigger-time snapshot, so it doesn't fit this mechanism (see
    // ParamID.h) - headed to a future mixer page instead. Delay Send
    // used to live here too but is curve/override-capable now (see
    // bassCurveRows[11]) - it's still continuously-read per-block, but
    // resolved fresh every block via curve/override instead of a raw
    // knob read, see AudioEngine::resolveDelaySends.
    std::array<ParamRow, 1> bassPlainRows;
    // Section headings for the Transient Behaviour modules - drawn above
    // bassCurveRows[6-7] (Pitch), [8-9] (Filter), [10] (Drive), same
    // three-independent-modules layout as before, just now indexing into
    // bassCurveRows instead of the old flat bassParamRows.
    juce::Label bassPitchTransientLabel { {}, "Pitch Transient" };
    juce::Label bassFilterTransientLabel { {}, "Filter Transient" };
    juce::Label bassDriveTransientLabel { {}, "Drive Transient" };
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

    // Section-level voicing overrides - see AudioEngine::ParamOverride
    // and project_raw_dub_song_architecture memory. Off: the slider
    // edits the instrument's base value, same as every other param. On:
    // the slider instead edits the CURRENT Global Pattern's override,
    // base left untouched - so the same pattern can voice differently
    // per section without ever touching every pattern that shares it.
    // No standalone button for this - toggled by clicking the row's
    // value box (single click), see CurveableParamRow/ValueBoxSlider.
    // The value box's own colour (inverted when active) is the only
    // indicator. Refresh functions declared earlier alongside
    // CurveableParamRow (refreshOverrideControls/refreshAllOverrideControls) -
    // generic across every instrument now, not Bass-specific.

    int kickPlayheadStep = -1;
    int bassPlayheadStep = -1;

    // Skank - now sequenced like Kick/Bass (variable length, paging,
    // per-step pitch for progressions - see SkankSynth.h). Chord
    // voicing itself is deliberately still fixed (Major/Minor triad
    // shape, not arbitrary voicings) - synthesis is not finished, this
    // is about judging the instrument in musical context, not a final
    // build. Chord QUALITY (major vs minor), however, is per-step (see
    // skankChordQualityButtons) - a real limitation surfaced by actually
    // composing: alternating major/minor on a repeated root needed to
    // be sequenceable, not a manual performance.
    juce::TextButton skankTriggerButton { "Trigger" };
    juce::TextButton skankClearButton { "Clear" };
    std::array<RawDub::StepButton, RawDub::numSteps> skankStepButtons;
    juce::Label skankTitleLabel { {}, "Skank" };
    // Now only: (a) the default a newly-activated step's chord quality
    // is seeded with, (b) what an unsequenced manual Trigger hit uses -
    // no longer read by sequenced playback at all, see
    // AudioEngine::advanceStep and SkankPatternSlot::chordIsMinor.
    juce::TextButton skankMajorButton { "Major" };
    juce::TextButton skankMinorButton { "Minor" };
    // Per-step chord quality lane, one button per visible step position
    // (same pagination as skankStepButtons) - only shown for steps that
    // are actually on. Click cycles Major/Minor for that step.
    std::array<juce::TextButton, RawDub::numSteps> skankChordQualityButtons;
    void layoutSkankChordQualityLane (juce::Rectangle<int> laneRow);
    std::array<CurveableParamRow, 5> skankCurveRows; // Tune, Decay, Drive, Delay Send, SawMix
    // Volume - see bassPlainRows' comment, same reasoning.
    std::array<ParamRow, 1> skankPlainRows;
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

    // Snare - noise + a small tonal body, one shared filter/envelope, see
    // SnareSynth.h. Same pattern-bank/paging mechanism as Kick - no
    // pitch (StepButton::hasPitch stays false), same as Kick.
    juce::TextButton snareTriggerButton { "Trigger" };
    juce::TextButton snareClearButton { "Clear" };
    std::array<RawDub::StepButton, RawDub::numSteps> snareStepButtons;
    juce::Label snareTitleLabel { {}, "Snare" };
    std::array<CurveableParamRow, 7> snareCurveRows; // Tune, Noise Mix, Cutoff, Resonance, Decay, Drive, Delay Send
    std::array<ParamRow, 1> snarePlainRows; // Volume
    juce::Label snareLengthLabel { {}, "Length" };
    std::array<juce::TextButton, 4> snareLengthButtons;
    std::array<juce::TextButton, maxPages> snarePageButtons;
    int snareViewPage = 0;
    int snarePlayheadStep = -1;

    juce::Label snarePatternBankLabel { {}, "Pattern" };
    std::array<juce::TextButton, RawDub::AudioEngine::bankSize> snarePatternBankButtons;

    // "Make Unique" - see kickSharedLabel above for the full explanation
    juce::Label snareSharedLabel;
    juce::TextButton snareMakeUniqueButton { "Make Unique" };

    // Hi-Hat (closed) - noise + filter + envelope, even simpler than
    // Snare (no tonal body) - see HiHatSynth.h. Same pattern-bank/paging
    // mechanism as Kick.
    juce::TextButton hihatTriggerButton { "Trigger" };
    juce::TextButton hihatClearButton { "Clear" };
    std::array<RawDub::StepButton, RawDub::numSteps> hihatStepButtons;
    juce::Label hihatTitleLabel { {}, "Hi-Hat" };
    std::array<CurveableParamRow, 5> hihatCurveRows; // Cutoff, Resonance, Decay, Drive, Delay Send
    std::array<ParamRow, 1> hihatPlainRows; // Volume
    juce::Label hihatLengthLabel { {}, "Length" };
    std::array<juce::TextButton, 4> hihatLengthButtons;
    std::array<juce::TextButton, maxPages> hihatPageButtons;
    int hihatViewPage = 0;
    int hihatPlayheadStep = -1;

    juce::Label hihatPatternBankLabel { {}, "Pattern" };
    std::array<juce::TextButton, RawDub::AudioEngine::bankSize> hihatPatternBankButtons;

    juce::Label hihatSharedLabel;
    juce::TextButton hihatMakeUniqueButton { "Make Unique" };

    // Delay - a send effect on the whole mix, not a sequenced instrument
    // (see DubDelay.h) - no step grid, no pattern bank (see
    // AudioEngine::delayPattern's comment: a single curve+Length
    // container, not a bank of saved patterns). Feedback/Tone/Drive/Wet
    // are curve+override-capable, sampled by the GLOBAL step counter
    // over this Length (see ParamID.h's DelayParamID); Time stays a
    // plain slider (see its own comment on why).
    juce::Label delayTitleLabel { {}, "Delay" };
    ParamRow delayTimeRow;
    std::array<CurveableParamRow, 4> delayCurveRows; // Feedback, Tone, Drive, Wet
    juce::TextButton delayBypassButton { "Bypass" };
    juce::Label delayLengthLabel { {}, "Length" };
    std::array<juce::TextButton, 4> delayLengthButtons;
    int delayPlayheadStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
