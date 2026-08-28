#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <neaacdec.h>

// ==============================================================================
// 1. AacAudioReader Class
// ==============================================================================
class AacAudioReader : public juce::AudioFormatReader
{
public:
    AacAudioReader(juce::InputStream* input, NeAACDecHandle handle, juce::File file);
    ~AacAudioReader() override;

    bool readSamples(int* const* destChannels, int numDestChannels, int startOffsetInDestBuffer,
                     juce::int64 startSampleInFile, int numSamplesToRead) override;

private:
    NeAACDecHandle hDecoder = nullptr;
    juce::File audioFile;
    juce::MemoryBlock fileData;
    juce::int64 currentSamplePosition = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AacAudioReader)
};

// ==============================================================================
// 2. AacAudioFormat Class
// ==============================================================================
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

    juce::AudioFormatReader* createReaderFor(juce::InputStream* sourceStream, bool destroyStreamIfOpeningFails) override;

    using juce::AudioFormat::createWriterFor;

    std::unique_ptr<juce::AudioFormatWriter> createWriterFor(std::unique_ptr<juce::OutputStream>& streamToWriteTo,
                                                             const juce::AudioFormatWriterOptions& options) override
    {
        juce::ignoreUnused(streamToWriteTo, options);
        return nullptr;
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AacAudioFormat)
};
