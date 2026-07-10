#pragma once
#include <JuceHeader.h>
#include <functional>

namespace RawDub
{
// A plain juce::Label that also reports single clicks - used as a
// parameter row's TITLE, which controls phrase-level (curve) behaviour:
// click to create/expand/collapse a curve. See MainComponent.h's
// CurveableParamRow comment for the full interaction model (title =
// phrase, value box = section, slider = edit).
class ClickableLabel : public juce::Label
{
public:
    std::function<void()> onLabelClick;

    void mouseDown (const juce::MouseEvent&) override
    {
        if (onLabelClick)
            onLabelClick();
    }
};
}
