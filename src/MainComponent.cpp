#include "MainComponent.h"
#include "AacAudioFormat.h"
#include "DrMp3AudioFormat.h"

MainComponent::MainComponent()
{
    setWantsKeyboardFocus (true);

    // Register basic formats (WAV, AIFF, FLAC, MP3, etc.)
    formatManager.registerBasicFormats();
    formatManager.registerFormat (new DrMp3AudioFormat(), false);
    formatManager.registerFormat (new AacAudioFormat(), true);

    transportSource.addChangeListener (this);
    thumbnail.addChangeListener (this);

    // Setup GUI Buttons (disable keyboard focus on buttons so space/o/l always go to MainComponent)
    addAndMakeVisible (openButton);
    openButton.setWantsKeyboardFocus (false);
    openButton.onClick = [this] { openButtonClicked(); };

    addAndMakeVisible (playButton);
    playButton.setWantsKeyboardFocus (false);
    playButton.onClick = [this] { playButtonClicked(); };
    playButton.setEnabled (false);

    addAndMakeVisible (stopButton);
    stopButton.setWantsKeyboardFocus (false);
    stopButton.onClick = [this] { stopButtonClicked(); };
    stopButton.setEnabled (false);

    addAndMakeVisible (loopButton);
    loopButton.setWantsKeyboardFocus (false);
    loopButton.onClick = [this] { loopButtonClicked(); };
    loopButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3a3a44));

    // Seekbar / Position Slider
    addAndMakeVisible (positionSlider);
    positionSlider.setWantsKeyboardFocus (false);
    positionSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    positionSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    positionSlider.setRange (0.0, 1.0, 0.01);
    positionSlider.setEnabled (false);
    positionSlider.onValueChange = [this] {
        if (positionSlider.isMouseButtonDown())
        {
            transportSource.setPosition (positionSlider.getValue());
            updateTimeLabel();
            repaint();
        }
    };

    // Time Label
    addAndMakeVisible (timeLabel);
    timeLabel.setText ("00:00 / 00:00", juce::dontSendNotification);
    timeLabel.setFont (juce::FontOptions (14.0f));
    timeLabel.setColour (juce::Label::textColourId, juce::Colours::cyan);
    timeLabel.setJustificationType (juce::Justification::centred);

    // Setup Volume Label & Slider
    addAndMakeVisible (volumeLabel);
    volumeLabel.setFont (juce::FontOptions (14.0f));
    volumeLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

    addAndMakeVisible (volumeSlider);
    volumeSlider.setWantsKeyboardFocus (false);
    volumeSlider.setRange (0.0, 1.0, 0.01);
    volumeSlider.setValue (0.8);
    volumeSlider.setScrollWheelEnabled (true);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 55, 24);
    volumeSlider.textFromValueFunction = [] (double v) {
        return juce::String (juce::roundToInt (v * 100.0)) + " %";
    };
    volumeSlider.valueFromTextFunction = [] (const juce::String& s) {
        return s.upToFirstOccurrenceOf ("%", false, false).getDoubleValue() / 100.0;
    };
    volumeSlider.onValueChange = [this] {
        float gain = (float) volumeSlider.getValue();
        transportSource.setGain (gain);
    };

    setSize (760, 390);
    setAudioChannels (0, 2); // 0 inputs, 2 stereo outputs

    startTimerHz (30); // 30 FPS timer for playhead, seekbar and state updates
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    transportSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (readerSource == nullptr || ! transportSource.isPlaying())
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    transportSource.getNextAudioBlock (bufferToFill);
}

void MainComponent::releaseResources()
{
    transportSource.releaseResources();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181820)); // Dark background

    // Draw waveform container panel
    g.setColour (juce::Colour (0xff22222c));
    g.fillRoundedRectangle (thumbnailBounds.toFloat(), 6.0f);

    g.setColour (juce::Colour (0xff32323e));
    g.drawRoundedRectangle (thumbnailBounds.toFloat(), 6.0f, 1.0f);

    if (thumbnail.getNumChannels() > 0)
    {
        auto audioLength = transportSource.getLengthInSeconds();
        auto currentPos = transportSource.getCurrentPosition();

        // Draw waveform
        g.setColour (juce::Colour (0xff00b4d8));
        thumbnail.drawChannels (g, thumbnailBounds.reduced (4), 0.0, thumbnail.getTotalLength(), 1.0f);

        // Highlight played region
        if (audioLength > 0.0)
        {
            float drawRatio = (float) juce::jlimit (0.0, 1.0, currentPos / audioLength);
            int playedWidth = (int) (drawRatio * thumbnailBounds.getWidth());

            g.setColour (juce::Colour (0x2200b4d8));
            g.fillRect (thumbnailBounds.getX(), thumbnailBounds.getY(), playedWidth, thumbnailBounds.getHeight());

            // Playhead Marker line
            auto currentX = thumbnailBounds.getX() + playedWidth;
            g.setColour (juce::Colours::white);
            g.drawLine ((float) currentX, (float) thumbnailBounds.getY(),
                        (float) currentX, (float) thumbnailBounds.getBottom(), 2.0f);

            // Playhead handle marker
            g.setColour (juce::Colour (0xff00f0ff));
            g.fillEllipse ((float) currentX - 4.0f, (float) thumbnailBounds.getY() + 2.0f, 8.0f, 8.0f);
        }
    }
    else
    {
        g.setColour (juce::Colours::grey);
        g.setFont (15.0f);
        g.drawText ("No Audio File Loaded (Drag & Drop file here, press 'O', or click Open)", thumbnailBounds, juce::Justification::centred, true);
    }

    // Drag and drop visual feedback overlay
    if (isDragOver)
    {
        g.setColour (juce::Colour (0xee161b22));
        g.fillRoundedRectangle (thumbnailBounds.toFloat(), 6.0f);

        g.setColour (juce::Colour (0xff00e5ff));
        juce::Path outline, dashedOutline;
        outline.addRoundedRectangle (thumbnailBounds.toFloat().reduced (3.0f), 6.0f);
        float dashes[] = { 8.0f, 4.0f };
        juce::PathStrokeType (2.0f).createDashedStroke (dashedOutline, outline, dashes, 2);
        g.fillPath (dashedOutline);

        g.setColour (juce::Colour (0xff00e5ff));
        g.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
        g.drawText ("Drop audio file here to start playing", thumbnailBounds, juce::Justification::centred, true);
    }
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (20);

    // Top Controls Row
    auto controlsRow = area.removeFromTop (36);
    openButton.setBounds (controlsRow.removeFromLeft (110));
    controlsRow.removeFromLeft (10);

    playButton.setBounds (controlsRow.removeFromLeft (85));
    controlsRow.removeFromLeft (8);

    stopButton.setBounds (controlsRow.removeFromLeft (85));
    controlsRow.removeFromLeft (8);

    loopButton.setBounds (controlsRow.removeFromLeft (100));
    controlsRow.removeFromLeft (20);

    volumeLabel.setBounds (controlsRow.removeFromLeft (60));
    volumeSlider.setBounds (controlsRow.removeFromLeft (220));

    area.removeFromTop (14);

    // Seekbar & Time Row
    auto seekRow = area.removeFromTop (26);
    timeLabel.setBounds (seekRow.removeFromRight (110));
    seekRow.removeFromRight (10);
    positionSlider.setBounds (seekRow);

    area.removeFromTop (12);

    // Waveform Bounds
    thumbnailBounds = area;
}

void MainComponent::openButtonClicked()
{
    juce::File initialDir = lastMediaFolder.isDirectory()
                                ? lastMediaFolder
                                : juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    auto chooser = std::make_shared<juce::FileChooser> (
        "Select Audio File...",
        initialDir,
        "*.wav;*.mp3;*.flac;*.aac;*.m4a;*.ogg;*.aif;*.aiff;*.wma");

    chooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                lastMediaFolder = file.getParentDirectory();
                loadFile (file);
            }
        });
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& filePath : files)
    {
        juce::File file (filePath);
        if (file.isDirectory())
            return true;

        auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".aac" ||
            ext == ".m4a" || ext == ".ogg" || ext == ".aif" || ext == ".aiff" ||
            ext == ".wma" || formatManager.findFormatForFileExtension (ext) != nullptr)
        {
            return true;
        }
    }
    return false;
}

void MainComponent::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused (files, x, y);
    isDragOver = true;
    repaint();
}

void MainComponent::fileDragMove (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused (files, x, y);
    if (! isDragOver)
    {
        isDragOver = true;
        repaint();
    }
}

void MainComponent::fileDragExit (const juce::StringArray& files)
{
    juce::ignoreUnused (files);
    isDragOver = false;
    repaint();
}

void MainComponent::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused (x, y);
    isDragOver = false;
    repaint();
    grabKeyboardFocus();

    for (const auto& filePath : files)
    {
        juce::File file (filePath);
        if (file.isDirectory())
        {
            auto audioFiles = file.findChildFiles (juce::File::findFiles, false,
                                                   "*.wav;*.mp3;*.flac;*.aac;*.m4a;*.ogg;*.aif;*.aiff;*.wma");
            if (! audioFiles.isEmpty())
            {
                loadFile (audioFiles[0]);
                return;
            }
        }
        else if (file.existsAsFile())
        {
            auto ext = file.getFileExtension().toLowerCase();
            if (ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".aac" ||
                ext == ".m4a" || ext == ".ogg" || ext == ".aif" || ext == ".aiff" ||
                ext == ".wma" || formatManager.findFormatForFileExtension (ext) != nullptr)
            {
                loadFile (file);
                return;
            }
        }
    }

    // Fallback if none matched extension explicitly: try first existing file
    if (files.size() > 0)
    {
        juce::File fallback (files[0]);
        if (fallback.existsAsFile())
            loadFile (fallback);
    }
}

void MainComponent::loadFile (const juce::File& file)
{
    auto* reader = formatManager.createReaderFor (file);
    if (reader != nullptr)
    {
        transportSource.stop();
        transportSource.setSource (nullptr);

        lastMediaFolder = file.getParentDirectory();

        auto newSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
        newSource->setLooping (isLooping);

        transportSource.setSource (newSource.get(), 0, nullptr, reader->sampleRate);
        thumbnail.setSource (new juce::FileInputSource (file));

        readerSource.reset (newSource.release());

        double totalLen = transportSource.getLengthInSeconds();
        positionSlider.setRange (0.0, totalLen > 0.0 ? totalLen : 1.0, 0.01);
        positionSlider.setValue (0.0, juce::dontSendNotification);
        positionSlider.setEnabled (true);

        playButton.setEnabled (true);
        stopButton.setEnabled (true);

        // When open/dropped new file it should auto start playing
        transportSource.setPosition (0.0);
        transportSource.start();
        changeState (Playing);

        // File name in window title bar: "JUCE Audio Player - <filename>"
        if (onTitleChange)
            onTitleChange ("JUCE Audio Player - " + file.getFileName());

        updateTimeLabel();
        repaint();
    }
}

void MainComponent::playButtonClicked()
{
    if (state == Playing)
    {
        // 2. Pause will pause the current play
        transportSource.stop();
        changeState (Paused);
    }
    else // Stopped or Paused
    {
        if (readerSource != nullptr)
        {
            if (transportSource.getCurrentPosition() >= transportSource.getLengthInSeconds())
                transportSource.setPosition (0.0);

            // 1. Play button when pressed it will change to pause.
            transportSource.start();
            changeState (Playing);
        }
    }
}

void MainComponent::stopButtonClicked()
{
    // 3. Stop button will be enabled and pressed it will stopped and seekbar will be in start position
    transportSource.stop();
    transportSource.setPosition (0.0);
    changeState (Stopped);
    positionSlider.setValue (0.0, juce::dontSendNotification);
    stopButton.setEnabled (true);
    updateTimeLabel();
    repaint();
}

void MainComponent::loopButtonClicked()
{
    // 3. press 'l' for loop toggle on off
    isLooping = ! isLooping;
    loopButton.setButtonText (isLooping ? "Loop: ON" : "Loop: OFF");
    loopButton.setColour (juce::TextButton::buttonColourId,
                          isLooping ? juce::Colour (0xff2e7d32) : juce::Colour (0xff3a3a44));

    if (readerSource != nullptr)
        readerSource->setLooping (isLooping);
}

void MainComponent::changeState (TransportState newState)
{
    state = newState;
    switch (state)
    {
    case Stopped:
        playButton.setButtonText ("Play");
        stopButton.setEnabled (readerSource != nullptr);
        break;

    case Playing:
        playButton.setButtonText ("Pause");
        stopButton.setEnabled (true);
        break;

    case Paused:
        playButton.setButtonText ("Play");
        stopButton.setEnabled (true);
        break;
    }
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &transportSource)
    {
        if (transportSource.isPlaying())
        {
            changeState (Playing);
        }
        else if (state == Playing && transportSource.hasStreamFinished())
        {
            if (isLooping)
            {
                transportSource.setPosition (0.0);
                transportSource.start();
                positionSlider.setValue (0.0, juce::dontSendNotification);
            }
            else
            {
                stopButtonClicked();
            }
        }
    }
    else if (source == &thumbnail)
    {
        repaint();
    }
}

void MainComponent::timerCallback()
{
    if (state == Playing)
    {
        double length = transportSource.getLengthInSeconds();
        double current = transportSource.getCurrentPosition();

        if (transportSource.hasStreamFinished() || (length > 0.0 && current >= length))
        {
            if (isLooping)
            {
                transportSource.setPosition (0.0);
                transportSource.start();
                positionSlider.setValue (0.0, juce::dontSendNotification);
            }
            else
            {
                stopButtonClicked();
            }
        }
        else
        {
            if (! positionSlider.isMouseButtonDown())
                positionSlider.setValue (current, juce::dontSendNotification);
        }
    }

    updateTimeLabel();
    repaint();
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    auto ch = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());

    // 2. press 'o' to open from last selected folder for media
    if (ch == 'o' || key.isKeyCode ('O') || key.isKeyCode ('o'))
    {
        openButtonClicked();
        return true;
    }

    // 3. press 'l' for loop toggle on off
    if (ch == 'l' || key.isKeyCode ('L') || key.isKeyCode ('l'))
    {
        loopButtonClicked();
        return true;
    }

    // 4. press 'space bar' for play/pause
    if (key.getKeyCode() == juce::KeyPress::spaceKey || key.getTextCharacter() == ' ')
    {
        playButtonClicked();
        return true;
    }

    return false;
}

void MainComponent::mouseDown (const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    seekToMousePosition (event);
}

void MainComponent::mouseDrag (const juce::MouseEvent& event)
{
    seekToMousePosition (event);
}

void MainComponent::seekToMousePosition (const juce::MouseEvent& event)
{
    if (thumbnailBounds.contains (event.getPosition()) ||
        (event.x >= thumbnailBounds.getX() && event.x <= thumbnailBounds.getRight() &&
         event.y >= thumbnailBounds.getY() - 5 && event.y <= thumbnailBounds.getBottom() + 5))
    {
        double audioLength = transportSource.getLengthInSeconds();
        if (audioLength > 0.0)
        {
            double ratio = juce::jlimit (0.0, 1.0, (double) (event.x - thumbnailBounds.getX()) / (double) thumbnailBounds.getWidth());
            double newPos = ratio * audioLength;
            transportSource.setPosition (newPos);
            positionSlider.setValue (newPos, juce::dontSendNotification);
            updateTimeLabel();
            repaint();
        }
    }
}

void MainComponent::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    juce::ignoreUnused (event);
    if (std::abs (wheel.deltaY) > 0.0001f)
    {
        double step = (wheel.deltaY > 0.0f ? 0.05 : -0.05);
        double newVol = juce::jlimit (0.0, 1.0, volumeSlider.getValue() + step);
        volumeSlider.setValue (newVol, juce::sendNotificationAsync);
    }
}

juce::String MainComponent::formatTime (double seconds)
{
    if (seconds < 0.0)
        seconds = 0.0;

    int totalSec = (int) std::floor (seconds);
    int mins = totalSec / 60;
    int secs = totalSec % 60;
    return juce::String::formatted ("%02d:%02d", mins, secs);
}

void MainComponent::updateTimeLabel()
{
    double cur = transportSource.getCurrentPosition();
    double len = transportSource.getLengthInSeconds();
    timeLabel.setText (formatTime (cur) + " / " + formatTime (len), juce::dontSendNotification);
}
