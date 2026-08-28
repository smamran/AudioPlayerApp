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
                auto totalSize = in->getTotalLength();
                memoryData.ensureSize ((size_t) totalSize);
                in->read (memoryData.getData(), (int) totalSize);

                if (! drmp3_init_memory (&mp3, memoryData.getData(), memoryData.getSize(), nullptr))
                    return;

                sampleRate = mp3.sampleRate;
                numChannels = mp3.channels;
                bitsPerSample = 16;
                lengthInSamples = (juce::int64) drmp3_get_pcm_frame_count (&mp3);

                // Pre-allocate buffer for chunk decoding (up to 8192 frames)
                scratchBuffer.resize (8192 * numChannels);
            }

            ~Reader() override
            {
                drmp3_uninit (&mp3);
            }

            bool readSamples (int* const* destChannels, int numDestChannels, int startSampleInDestBuffer,
                              juce::int64 startSampleInFile, int numSamples) override
            {
                if (startSampleInFile < 0 || startSampleInFile >= lengthInSamples)
                {
                    for (int ch = 0; ch < numDestChannels; ++ch)
                        if (destChannels != nullptr && destChannels[ch] != nullptr)
                            juce::FloatVectorOperations::clear ((float*) destChannels[ch] + startSampleInDestBuffer, numSamples);
                    return true;
                }

                // Only seek if JUCE jumped to a non-sequential position (e.g., user scrubbing)
                if (currentFramePos != startSampleInFile)
                {
                    drmp3_seek_to_pcm_frame (&mp3, (uint64_t) startSampleInFile);
                    currentFramePos = startSampleInFile;
                }

                // Resize scratch buffer if needed
                size_t neededSize = (size_t) numSamples * numChannels;
                if (scratchBuffer.size() < neededSize)
                    scratchBuffer.resize (neededSize);

                uint64_t framesRead = drmp3_read_pcm_frames_f32 (&mp3, (uint64_t) numSamples, scratchBuffer.data());
                currentFramePos += (juce::int64) framesRead;

                for (int ch = 0; ch < numDestChannels; ++ch)
                {
                    if (destChannels != nullptr && destChannels[ch] != nullptr)
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
            juce::MemoryBlock memoryData;
            juce::int64 currentFramePos { -1 };
            std::vector<float> scratchBuffer;
        };

        std::unique_ptr<Reader> reader (new Reader (sourceStream, getFormatName()));

        if (reader->sampleRate > 0)
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
