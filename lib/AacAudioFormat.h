#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

class AacAudioFormat : public juce::AudioFormat
{
public:
    AacAudioFormat();
    ~AacAudioFormat() override = default;

    juce::Array<int> getPossibleSampleRates() override;
    juce::Array<int> getPossibleBitDepths() override;
    bool canDoStereo() override;
    bool canDoMono() override;
    bool isCompressed() override;

    juce::AudioFormatReader* createReaderFor (juce::InputStream* sourceStream,
                                             bool destroyStreamIfOpeningFails) override;

    using juce::AudioFormat::createWriterFor;

    std::unique_ptr<juce::AudioFormatWriter> createWriterFor (std::unique_ptr<juce::OutputStream>& streamToWriteTo,
                                                              const juce::AudioFormatWriterOptions& options) override
    {
        juce::ignoreUnused (streamToWriteTo, options);
        return nullptr;
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AacAudioFormat)
};
