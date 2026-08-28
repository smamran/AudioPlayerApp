#pragma once

#include <JuceHeader.h>

class MainComponent : public juce::AudioAppComponent,
                      public juce::ChangeListener,
                      public juce::Timer {
public:
  MainComponent();
  ~MainComponent() override;

  // Audio lifecycle callbacks
  void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
  void
  getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) override;
  void releaseResources() override;

  // GUI callbacks
  void paint(juce::Graphics &g) override;
  void resized() override;
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;
  void timerCallback() override;

private:
  enum TransportState { Stopped, Starting, Playing, Stopping };

  void changeState(TransportState newState);
  void openButtonClicked();
  void playButtonClicked();
  void stopButtonClicked();

  // UI Controls
  juce::TextButton openButton{"Open File..."};
  juce::TextButton playButton{"Play"};
  juce::TextButton stopButton{"Stop"};
  juce::Slider volumeSlider;
  juce::Label volumeLabel{{}, "Volume"};

  // Audio Reader & Formats
  juce::AudioFormatManager formatManager;
  std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
  juce::AudioTransportSource transportSource;
  TransportState state{Stopped};

  // 50 টি ফাইলের পিক ক্যাশ করার জন্য
  juce::AudioThumbnailCache thumbnailCache{50};
  juce::AudioThumbnail thumbnail{512, formatManager, thumbnailCache};
  bool isStoppingWithDelay{false};
  double stopTime{0.0};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
