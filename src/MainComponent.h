#pragma once

#include <JuceHeader.h>
#include <functional>

class MainComponent : public juce::AudioAppComponent,
                      public juce::ChangeListener,
                      public juce::Timer,
                      public juce::FileDragAndDropTarget
{
public:
    MainComponent();
    ~MainComponent() override;

    // Audio lifecycle callbacks
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    // GUI callbacks
    void paint (juce::Graphics& g) override;
    void resized() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void timerCallback() override;

    // Drag and Drop callbacks
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragMove (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    // Keyboard & Mouse interactions
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    // Public helper to load a file
    void loadFile (const juce::File& file);

    // Callback to update parent window title
    std::function<void (const juce::String&)> onTitleChange;

private:
    enum TransportState
    {
        Stopped,
        Playing,
        Paused
    };

    void changeState (TransportState newState);
    void openButtonClicked();
    void playButtonClicked();
    void stopButtonClicked();
    void loopButtonClicked();
    void updateTimeLabel();
    void seekToMousePosition (const juce::MouseEvent& event);
    static juce::String formatTime (double seconds);

    // UI Controls
    juce::TextButton openButton { "Open File..." };
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton loopButton { "Loop: OFF" };

    juce::Slider positionSlider;
    juce::Label timeLabel;

    juce::Label volumeLabel { {}, "Volume:" };
    juce::Slider volumeSlider;

    // Audio reader & sources
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;
    TransportState state { Stopped };
    bool isLooping { false };

    // Remember last selected directory for media opening
    juce::File lastMediaFolder;

    // Drag and drop state
    bool isDragOver { false };

    // Audio Thumbnail / Waveform
    juce::AudioThumbnailCache thumbnailCache { 50 };
    juce::AudioThumbnail thumbnail { 512, formatManager, thumbnailCache };
    juce::Rectangle<int> thumbnailBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
