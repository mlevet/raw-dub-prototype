#pragma once
#include <JuceHeader.h>
#include <functional>

namespace RawDub
{
// A juce::Slider whose text-box area reports a plain single click,
// distinguished from a double-click - used as a parameter row's VALUE
// BOX, which controls section-level (override) behaviour: single click
// toggles the section override, double-click still opens JUCE's native
// text-edit mode to type an exact value. See MainComponent.h's
// CurveableParamRow comment for the full interaction model (title =
// phrase, value box = section, slider = edit).
//
// Two JUCE internals make this trickier than it looks, both confirmed
// by reading JUCE's own source rather than assumed:
//
// 1. Slider's text box is NOT part of the Slider component itself -
//    it's a separate child Label created internally by the LookAndFeel
//    (see LookAndFeel_V2::createSliderTextBox), added as a genuine
//    child. Overriding Slider::mouseDown on the outer component never
//    sees clicks landing on that child - they're dispatched straight to
//    it. Fixed with Component::addMouseListener(...,
//    wantsEventsForAllNestedChildComponents=true) via a SEPARATE
//    watcher object (not self-registration, which would double-fire for
//    clicks landing directly on the slider's own track/thumb).
//
// 2. Slider configures its text box for edit-ON-SINGLE-CLICK, not
//    double-click (Slider::Pimpl::updateTextBoxEnablement calls
//    Label::setEditable(shouldBeEditable) with only one argument, which
//    sets editOnSingleClick and leaves editOnDoubleClick at its default
//    of false) - and Label::mouseUp fires showEditor() unconditionally
//    on every click's release once editSingleClick is true, with no
//    click-count check at all. A plain "observe, don't block" approach
//    can't produce single-click-toggles-instead-of-edits: the label's
//    own mouseUp already commits to editing before any observer gets a
//    chance to react. The fix is to actually disable native auto-edit
//    entirely (Slider::setTextBoxIsEditable(false)) and drive editing
//    manually instead (Slider::showTextBox(), which works regardless of
//    that flag - only needs it true for the duration of the call, per
//    JUCE's own jassert in Pimpl::showTextBox) on a genuine double-click.
//
// onValueBoxClick, and the single/double-click distinction at all, only
// applies to rows that actually have a section-override mechanism - see
// setOverrideCapable. For rows without one (most Bass params), the value
// box is left as a completely ordinary, native Slider text box.
class ValueBoxSlider : public juce::Slider, private juce::Timer
{
public:
    ValueBoxSlider() { addMouseListener (&clickWatcher, true); }

    std::function<void()> onValueBoxClick;

    void setOverrideCapable (bool capable)
    {
        overrideCapable = capable;
        // native single-click auto-edit only gets disabled for rows that
        // actually need the value box to mean something else on a
        // single click - everywhere else stays a completely ordinary
        // editable Slider text box
        setTextBoxIsEditable (! capable);
    }

private:
    struct ClickWatcher : public juce::MouseListener
    {
        explicit ClickWatcher (ValueBoxSlider& o) : owner (o) {}

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! owner.overrideCapable)
                return;
            // e.eventComponent is the slider itself for a click on the
            // track/thumb (leave that alone, ordinary drag behaviour) -
            // anything else at this point is the internal text box child.
            if (e.eventComponent == &owner)
                return;
            if (e.getNumberOfClicks() != 1)
                return; // this mouseDown is already part of a double-click - mouseDoubleClick handles it

            owner.pendingClick = true;
            owner.startTimer (300);
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            owner.stopTimer();
            owner.pendingClick = false;

            if (owner.overrideCapable && e.eventComponent != &owner)
            {
                // native auto-edit is disabled for this row (see
                // setOverrideCapable) - drive it manually instead, since
                // this is a genuine double-click on the value box
                owner.setTextBoxIsEditable (true);
                owner.showTextBox();
                owner.setTextBoxIsEditable (false); // safe while an editor is already open - see class comment
            }
        }

        ValueBoxSlider& owner;
    };

    void timerCallback() override
    {
        stopTimer();
        if (pendingClick && onValueBoxClick)
            onValueBoxClick();
        pendingClick = false;
    }

    ClickWatcher clickWatcher { *this };
    bool overrideCapable = false;
    bool pendingClick = false;
};
}
