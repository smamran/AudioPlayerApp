#include "AacAudioFormat.h"

// ==============================================================================
// AacAudioReader Implementation
// ==============================================================================
AacAudioReader::AacAudioReader(juce::InputStream* input, NeAACDecHandle handle, juce::File file)
    : juce::AudioFormatReader(input, "AAC/M4A FAAD2 Format"),
      hDecoder(handle),
      audioFile(file)
{
    // ফাইল মেমোরিতে লোড করা
    audioFile.loadFileAsData(fileData);

    if (fileData.getSize() > 0)
    {
        unsigned long samplerate = 0;
        unsigned char channels = 0;

        auto* dataPtr = static_cast<unsigned char*>(fileData.getData());
        unsigned long dataSize = static_cast<unsigned long>(fileData.getSize());

        // M4A atom / ftyp ফিল্টারিং করে মেইন অডিও ডাটার অবস্থান খোঁজা
        unsigned long offset = 0;
        if (dataSize > 8 && dataPtr[4] == 'f' && dataPtr[5] == 't' && dataPtr[6] == 'y' && dataPtr[7] == 'p')
        {
            // M4A মেটাডেটা এড়িয়ে mdat সংকেত খোঁজা
            for (unsigned long i = 0; i < dataSize - 8; ++i)
            {
                if (dataPtr[i] == 'm' && dataPtr[i + 1] == 'd' && dataPtr[i + 2] == 'a' && dataPtr[i + 3] == 't')
                {
                    offset = i + 4; // mdat ট্যাগের পর থেকে অডিও ডাটা শুরু
                    break;
                }
            }
        }

        // FAAD2 ইনবাউন্ড হেডার বা বিটস্ট্রিম পার্সিং
        long initResult = NeAACDecInit(hDecoder, dataPtr + offset, dataSize - offset, &samplerate, &channels);

        if (initResult < 0)
        {
            // ডিফল্ট ফলব্যাক ভ্যালু
            sampleRate = 44100;
            numChannels = 2;
        }
        else
        {
            sampleRate = samplerate > 0 ? samplerate : 44100;
            numChannels = channels > 0 ? channels : 2;
        }

        bitsPerSample = 16;
        // স্যাম্পল দৈর্ঘ্য অনুমান করা
        lengthInSamples = (static_cast<juce::int64>(dataSize) * 8) / (bitsPerSample * numChannels);
    }
}

AacAudioReader::~AacAudioReader()
{
    if (hDecoder != nullptr)
    {
        NeAACDecClose(hDecoder);
        hDecoder = nullptr;
    }
}

bool AacAudioReader::readSamples(int* const* destChannels, int numDestChannels, int startOffsetInDestBuffer,
                                 juce::int64 startSampleInFile, int numSamplesToRead)
{
    juce::ignoreUnused(startSampleInFile);

    if (hDecoder == nullptr || fileData.getSize() == 0 || numSamplesToRead <= 0)
        return false;

    // বাফার ক্লিয়ার/জিরো ফিল করা
    for (int ch = 0; ch < numDestChannels; ++ch)
    {
        if (destChannels != nullptr && destChannels[ch] != nullptr)
            std::memset(destChannels[ch] + startOffsetInDestBuffer, 0, static_cast<size_t>(numSamplesToRead) * sizeof(int));
    }

    NeAACDecFrameInfo frameInfo;
    auto* bufferPtr = static_cast<unsigned char*>(fileData.getData());
    unsigned long bufferSize = static_cast<unsigned long>(fileData.getSize());

    // ডিকোড করে সরাসরি 16-bit PCM ডেটা সংগ্রহ
    void* sampleBuffer = NeAACDecDecode(hDecoder, &frameInfo, bufferPtr, bufferSize);

    if (frameInfo.error == 0 && sampleBuffer != nullptr && frameInfo.samples > 0)
    {
        auto* pcmData = static_cast<int16_t*>(sampleBuffer);
        int samplesRead = static_cast<int>(frameInfo.samples / numChannels);
        int samplesToCopy = juce::jmin(numSamplesToRead, samplesRead);

        for (int ch = 0; ch < numDestChannels; ++ch)
        {
            if (destChannels != nullptr && destChannels[ch] != nullptr)
            {
                int* dest = destChannels[ch] + startOffsetInDestBuffer;
                for (int i = 0; i < samplesToCopy; ++i)
                {
                    int sampleIndex = i * numChannels + (ch % numChannels);
                    dest[i] = static_cast<int>(pcmData[sampleIndex]) << 16;
                }
            }
        }
    }

    return true;
}

// ==============================================================================
// AacAudioFormat Implementation
// ==============================================================================
AacAudioFormat::AacAudioFormat()
    : juce::AudioFormat("AAC / M4A Audio", ".aac .m4a")
{
}

juce::Array<int> AacAudioFormat::getPossibleSampleRates()  { return { 44100, 48000, 88200, 96000 }; }
juce::Array<int> AacAudioFormat::getPossibleBitDepths()   { return { 16, 24 }; }
bool AacAudioFormat::canDoStereo()                        { return true; }
bool AacAudioFormat::canDoMono()                          { return true; }
bool AacAudioFormat::isCompressed()                       { return true; }

juce::AudioFormatReader* AacAudioFormat::createReaderFor(juce::InputStream* sourceStream, bool destroyStreamIfOpeningFails)
{
    if (sourceStream == nullptr)
        return nullptr;

    auto fileStream = dynamic_cast<juce::FileInputStream*>(sourceStream);

    if (fileStream == nullptr)
    {
        if (destroyStreamIfOpeningFails) delete sourceStream;
        return nullptr;
    }

    NeAACDecHandle hDecoder = NeAACDecOpen();
    if (hDecoder == nullptr)
    {
        if (destroyStreamIfOpeningFails) delete sourceStream;
        return nullptr;
    }

    NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration(hDecoder);
    config->outputFormat = FAAD_FMT_16BIT;
    NeAACDecSetConfiguration(hDecoder, config);

    return new AacAudioReader(sourceStream, hDecoder, fileStream->getFile());
}
