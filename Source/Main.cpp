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

            // Used to wrap this in a scrolling juce::Viewport, from when
            // the whole instrument list was always stacked (content was
            // ~2100px tall). Since the instrument tabs redesign, only one
            // instrument's section is shown at a time and the content
            // fits comfortably inside a normal fixed window (~900px) -
            // removed the Viewport entirely rather than leave it as dead
            // weight, since it was also capturing mouse wheel events
            // meant for the pitch grid's own scroll handling.
            setContentOwned (new MainComponent(), true);
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
