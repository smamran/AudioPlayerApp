#include "MainComponent.h"
#include "AacAudioFormat.h"
#include "DrMp3AudioFormat.h"

MainComponent::MainComponent() {
  formatManager.registerBasicFormats();
  formatManager.registerFormat(new DrMp3AudioFormat(), true); // Custom MP3 Decoder
  formatManager.registerFormat(new AacAudioFormat(), true);   // FAAD2 AAC/M4A Decoder

  transportSource.addChangeListener(this);
  thumbnail.addChangeListener(this);

  // Setup GUI Buttons
  addAndMakeVisible(openButton);
  openButton.onClick = [this] { openButtonClicked(); };

  addAndMakeVisible(playButton);
  playButton.onClick = [this] { playButtonClicked(); };
  playButton.setEnabled(false);

  addAndMakeVisible(stopButton);
  stopButton.onClick = [this] { stopButtonClicked(); };
  stopButton.setEnabled(false);

  // Setup Volume Slider
  addAndMakeVisible(volumeSlider);
  volumeSlider.setRange(0.0, 1.0, 0.01);
  volumeSlider.setValue(0.8);
  volumeSlider.onValueChange = [this] {
    transportSource.setGain((float)volumeSlider.getValue());
  };

  addAndMakeVisible(volumeLabel);
  volumeLabel.attachToComponent(&volumeSlider, true);

  setSize(700, 350);
  setAudioChannels(0, 2); // 0 inputs, 2 stereo outputs

  startTimerHz(30); // 30 FPS timer for playhead & smooth fade-out repainting
}

MainComponent::~MainComponent() { shutdownAudio(); }

void MainComponent::prepareToPlay(int samplesPerBlockExpected,
                                  double sampleRate) {
  transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) {
    if (!transportSource.isPlaying() || transportSource.getCurrentPosition() >= transportSource.getLengthInSeconds()) {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    // 0.5 সেকেন্ডের স্মুথ ফেইড-আউট হ্যান্ডলিং
    if (isStoppingWithDelay) {
        double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        double elapsedTime = currentTime - stopTime;

        if (elapsedTime >= 0.5) {
            transportSource.stop();
            transportSource.setPosition(0.0);
            isStoppingWithDelay = false;
            bufferToFill.clearActiveBufferRegion();
            return;
        } else {
            float fadeGain = static_cast<float>(1.0 - (elapsedTime / 0.5));
            transportSource.getNextAudioBlock(bufferToFill);
            bufferToFill.buffer->applyGainRamp(bufferToFill.startSample, bufferToFill.numSamples, fadeGain, fadeGain);
            return;
        }
    }

    transportSource.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources() { transportSource.releaseResources(); }

void MainComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff1e1e24)); // Background color

  juce::Rectangle<int> thumbnailBounds(30, 120, getWidth() - 60, 180);

  g.setColour(juce::Colour(0xff2a2a32));
  g.fillRect(thumbnailBounds);

  g.setColour(juce::Colours::cyan);
  if (thumbnail.getNumChannels() > 0) {
    thumbnail.drawChannels(g, thumbnailBounds, 0.0, thumbnail.getTotalLength(), 1.0f);

    // Playhead Marker
    g.setColour(juce::Colours::white);
    auto audioLength = transportSource.getLengthInSeconds();
    if (audioLength > 0.0) {
      auto drawPosition = (float)transportSource.getCurrentPosition() / (float)audioLength;
      auto currentX = thumbnailBounds.getX() + (drawPosition * thumbnailBounds.getWidth());
      g.drawVerticalLine((int)currentX, (float)thumbnailBounds.getY(), (float)thumbnailBounds.getBottom());
    }
  } else {
    g.setFont(18.0f);
    g.drawText("No Audio File Loaded", thumbnailBounds, juce::Justification::centred, true);
  }
}

void MainComponent::resized() {
  openButton.setBounds(30, 30, 120, 40);
  playButton.setBounds(160, 30, 100, 40);
  stopButton.setBounds(270, 30, 100, 40);
  volumeSlider.setBounds(460, 30, 200, 40);
}

void MainComponent::openButtonClicked() {
  auto chooser = std::make_shared<juce::FileChooser>(
      "Select Audio File...", juce::File{},
      "*.wav;*.mp3;*.flac;*.ogg;*.aac;*.m4a");

  chooser->launchAsync(
      juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
      [this, chooser](const juce::FileChooser &fc) {
        auto file = fc.getResult();
        if (file.existsAsFile()) {
          auto *reader = formatManager.createReaderFor(file);
          if (reader != nullptr) {
            auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
            transportSource.setSource(newSource.get(), 0, nullptr, reader->sampleRate);
            thumbnail.setSource(new juce::FileInputSource(file));

            playButton.setEnabled(true);
            readerSource.reset(newSource.release());
            changeState(Stopped);
          }
        }
      });
}

void MainComponent::playButtonClicked() {
  if (state == Stopped)
    changeState(Starting);
  else if (state == Playing)
    changeState(Stopping);
}

void MainComponent::stopButtonClicked() {
    if (state == Playing || state == Starting) {
        stopTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        isStoppingWithDelay = true;
        changeState(Stopping);
    } else {
        transportSource.setPosition(0.0);
        repaint();
    }
}

void MainComponent::changeState(TransportState newState) {
    if (state != newState) {
        state = newState;
        switch (state) {
        case Stopped:
            stopButton.setEnabled(false);
            playButton.setButtonText("Play");
            transportSource.setPosition(0.0);
            isStoppingWithDelay = false;
            repaint();
            break;

        case Starting:
            isStoppingWithDelay = false;
            playButton.setEnabled(true);
            stopButton.setEnabled(true);
            playButton.setButtonText("Pause");
            transportSource.start();
            state = Playing;
            break;

        case Playing:
            playButton.setButtonText("Pause");
            stopButton.setEnabled(true);
            break;

        case Stopping:
            stopButton.setEnabled(false);
            playButton.setButtonText("Play");
            break;
        }
    }
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == &transportSource) {
    if (transportSource.isPlaying()) {
      changeState(Playing);
    } else {
      changeState(Stopped);
    }
  } else if (source == &thumbnail) {
    repaint();
  }
}

void MainComponent::timerCallback() {
    if (isStoppingWithDelay) {
        double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        if (currentTime - stopTime >= 0.5) {
            changeState(Stopped);
        }
    }

    if (transportSource.isPlaying() || isStoppingWithDelay)
        repaint();
}
