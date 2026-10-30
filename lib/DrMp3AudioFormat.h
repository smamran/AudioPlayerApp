#pragma once

#include <JuceHeader.h>
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

class DrMp3AudioFormat : public juce::AudioFormat
{
public:
    DrMp3AudioFormat()
        : juce::AudioFormat ("MP3 Format", ".mp3") {}

    juce::Array<int> getPossibleSampleRates() override { return { 44100, 48000, 32000, 22050, 16000, 11025, 8000 }; }
    juce::Array<int> getPossibleBitDepths() override { return { 16 }; }
    bool canDoStereo() override { return true; }
    bool canDoMono() override { return true; }

    juce::AudioFormatReader* createReaderFor (juce::InputStream* sourceStream, bool deleteStreamIfOpeningFails) override
    {
        class Reader : public juce::AudioFormatReader
        {
        public:
            Reader (juce::InputStream* in, const juce::String& formatName)
                : juce::AudioFormatReader (in, formatName)
            {
                if (in == nullptr)
                    return;

                in->setPosition (0);
                auto totalSize = in->getTotalLength();
                if (totalSize <= 0)
                    return;

                memoryData.ensureSize ((size_t) totalSize);
                auto bytesRead = in->read (memoryData.getData(), (int) totalSize);
                if (bytesRead <= 0)
                    return;

                memoryData.setSize ((size_t) bytesRead);

                if (! drmp3_init_memory (&mp3, memoryData.getData(), memoryData.getSize(), nullptr))
                    return;

                initialized = true;
                sampleRate = mp3.sampleRate;
                numChannels = mp3.channels;
                bitsPerSample = 16;
                lengthInSamples = (juce::int64) drmp3_get_pcm_frame_count (&mp3);

                if (lengthInSamples <= 0 && sampleRate > 0)
                    lengthInSamples = (juce::int64) ((memoryData.getSize() * 8.0 * sampleRate) / 128000.0);

                scratchBuffer.resize (8192 * numChannels);
            }

            ~Reader() override
            {
                if (initialized)
                    drmp3_uninit (&mp3);
            }

            bool readSamples (int* const* destChannels, int numDestChannels, int startSampleInDestBuffer,
                              juce::int64 startSampleInFile, int numSamples) override
            {
                if (destChannels == nullptr || numSamples <= 0)
                    return true;

                if (startSampleInFile < 0 || startSampleInFile >= lengthInSamples)
                {
                    for (int ch = 0; ch < numDestChannels; ++ch)
                        if (destChannels[ch] != nullptr)
                            std::memset (destChannels[ch] + startSampleInDestBuffer, 0, (size_t) numSamples * sizeof (int));
                    return true;
                }

                if (currentFramePos != startSampleInFile)
                {
                    drmp3_seek_to_pcm_frame (&mp3, (uint64_t) startSampleInFile);
                    currentFramePos = startSampleInFile;
                }

                size_t neededSize = (size_t) numSamples * numChannels;
                if (scratchBuffer.size() < neededSize)
                    scratchBuffer.resize (neededSize);

                uint64_t framesRead = drmp3_read_pcm_frames_f32 (&mp3, (uint64_t) numSamples, scratchBuffer.data());
                currentFramePos += (juce::int64) framesRead;

                for (int ch = 0; ch < numDestChannels; ++ch)
                {
                    if (destChannels[ch] != nullptr)
                    {
                        auto* dest = destChannels[ch] + startSampleInDestBuffer;
                        int sourceChannel = juce::jmin (ch, (int) numChannels - 1);

                        for (int i = 0; i < (int) framesRead; ++i)
                        {
                            float sample = scratchBuffer[(size_t) i * numChannels + sourceChannel];
                            float clamped = juce::jlimit (-1.0f, 1.0f, sample);
                            dest[i] = (int) (clamped * 2147483647.0f);
                        }

                        for (int i = (int) framesRead; i < numSamples; ++i)
                            dest[i] = 0;
                    }
                }

                return true;
            }

        private:
            drmp3 mp3 {};
            bool initialized { false };
            juce::MemoryBlock memoryData;
            juce::int64 currentFramePos { -1 };
            std::vector<float> scratchBuffer;
        };

        if (sourceStream == nullptr)
            return nullptr;

        std::unique_ptr<Reader> reader (new Reader (sourceStream, getFormatName()));

        if (reader->sampleRate > 0 && reader->lengthInSamples > 0)
            return reader.release();

        if (deleteStreamIfOpeningFails)
            delete sourceStream;

        return nullptr;
    }

    std::unique_ptr<juce::AudioFormatWriter> createWriterFor (
        std::unique_ptr<juce::OutputStream>& streamToWriteTo,
        const juce::AudioFormatWriterOptions& options) override
    {
        juce::ignoreUnused (streamToWriteTo, options);
        return nullptr;
    }
};
