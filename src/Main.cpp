#include "MainComponent.h"
#include <JuceHeader.h>

class AudioPlayerApplication : public juce::JUCEApplication
{
public:
    AudioPlayerApplication()
    {
    }

    const juce::String getApplicationName() override
    {
        return "JUCE Audio Player";
    }

    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String& commandLine) override
    {
        juce::ignoreUnused (commandLine);
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name)
            : DocumentWindow (
                name,
                juce::Desktop::getInstance().getDefaultLookAndFeel().findColour (
                    juce::ResizableWindow::backgroundColourId),
                DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);

            auto* comp = new MainComponent();
            comp->onTitleChange = [this] (const juce::String& newTitle) {
                setName (newTitle);
            };

            setContentOwned (comp, true);
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());

            // Load and set application icon for title bar and taskbar
            auto iconImage = juce::ImageFileFormat::loadFrom (
                BinaryData::player_webp,
                (size_t) BinaryData::player_webpSize);

            if (! iconImage.isValid())
            {
                juce::File iconFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                                          .getParentDirectory()
                                          .getChildFile ("res/player.webp");
                if (! iconFile.existsAsFile())
                    iconFile = juce::File::getCurrentWorkingDirectory().getChildFile ("res/player.webp");

                if (iconFile.existsAsFile())
                    iconImage = juce::ImageFileFormat::loadFrom (iconFile);
            }

            if (iconImage.isValid())
                setIcon (iconImage);

            setVisible (true);

            if (iconImage.isValid())
            {
                if (auto* peer = getPeer())
                    peer->setIcon (iconImage);
            }

            comp->grabKeyboardFocus();
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (AudioPlayerApplication)
