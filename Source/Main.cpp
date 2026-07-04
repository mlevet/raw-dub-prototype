#include <JuceHeader.h>
#include "MainComponent.h"

class RawDubApplication : public juce::JUCEApplication
{
public:
    RawDubApplication() = default;

    const juce::String getApplicationName() override    { return "Raw Dub Prototype"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }

    void initialise (const juce::String&) override
    {
        mainWindow.reset (new MainWindow (getApplicationName()));
    }

    void shutdown() override { mainWindow = nullptr; }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, juce::Colours::white, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);

            // The instrument list keeps growing (Kick/Bass/AM/Skank...),
            // so the content is taller than most screens can show in one
            // fixed window. Scroll instead of resizing the window every
            // time - content keeps its own natural size, the window
            // stays a reasonable, screen-safe fixed size.
            auto* viewport = new juce::Viewport();
            viewport->setViewedComponent (new MainComponent(), true);
            viewport->setScrollBarsShown (true, false);

            setContentOwned (viewport, true);
            centreWithSize (860, 950);
            setResizable (true, false);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (RawDubApplication)
