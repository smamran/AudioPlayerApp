#include "AacAudioFormat.h"
#include <neaacdec.h>
#include <vector>

#if JUCE_WINDOWS
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>

namespace
{
struct MFInitializer
{
    MFInitializer()  { MFStartup (MF_VERSION, MFSTARTUP_NOSOCKET); }
    ~MFInitializer() { MFShutdown(); }
};

bool decodeWithWMF (const juce::File& file, juce::AudioBuffer<float>& outBuffer, double& outSampleRate, unsigned int& outChannels)
{
    static MFInitializer mfInit;

    IMFSourceReader* pReader = nullptr;
    HRESULT hr = MFCreateSourceReaderFromURL (file.getFullPathName().toWideCharPointer(), nullptr, &pReader);
    if (FAILED (hr) || pReader == nullptr)
        return false;

    pReader->SetStreamSelection ((DWORD) MF_SOURCE_READER_ALL_STREAMS, FALSE);
    pReader->SetStreamSelection ((DWORD) MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);

    // Request Float audio first
    IMFMediaType* pPartialType = nullptr;
    hr = MFCreateMediaType (&pPartialType);
    if (FAILED (hr))
    {
        pReader->Release();
        return false;
    }

    pPartialType->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pPartialType->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_Float);
    hr = pReader->SetCurrentMediaType ((DWORD) MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPartialType);
    pPartialType->Release();

    bool isFloat = true;
    if (FAILED (hr))
    {
        // Try 16-bit PCM if float is not supported
        isFloat = false;
        IMFMediaType* pPcmType = nullptr;
        MFCreateMediaType (&pPcmType);
        pPcmType->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        pPcmType->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_PCM);
        hr = pReader->SetCurrentMediaType ((DWORD) MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPcmType);
        pPcmType->Release();
        if (FAILED (hr))
        {
            pReader->Release();
            return false;
        }
    }

    IMFMediaType* pActualType = nullptr;
    hr = pReader->GetCurrentMediaType ((DWORD) MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pActualType);
    if (FAILED (hr))
    {
        pReader->Release();
        return false;
    }

    UINT32 sampleRate = 0, numChannels = 0, bitsPerSample = 0;
    pActualType->GetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
    pActualType->GetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, &numChannels);
    pActualType->GetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample);
    pActualType->Release();

    if (sampleRate == 0 || numChannels == 0)
    {
        pReader->Release();
        return false;
    }

    std::vector<std::vector<float>> channelsData (numChannels);

    while (true)
    {
        DWORD flags = 0;
        LONGLONG timeStamp = 0;
        IMFSample* pSample = nullptr;
        hr = pReader->ReadSample ((DWORD) MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, &timeStamp, &pSample);
        if (FAILED (hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM))
            break;

        if (pSample != nullptr)
        {
            IMFMediaBuffer* pBuffer = nullptr;
            if (SUCCEEDED (pSample->ConvertToContiguousBuffer (&pBuffer)))
            {
                BYTE* pData = nullptr;
                DWORD cbLength = 0;
                if (SUCCEEDED (pBuffer->Lock (&pData, nullptr, &cbLength)))
                {
                    if (isFloat && bitsPerSample == 32)
                    {
                        const float* floatData = reinterpret_cast<const float*> (pData);
                        size_t numFrames = cbLength / (numChannels * sizeof (float));
                        for (size_t f = 0; f < numFrames; ++f)
                        {
                            for (UINT32 ch = 0; ch < numChannels; ++ch)
                                channelsData[ch].push_back (floatData[f * numChannels + ch]);
                        }
                    }
                    else if (bitsPerSample == 16)
                    {
                        const int16_t* pcm16 = reinterpret_cast<const int16_t*> (pData);
                        size_t numFrames = cbLength / (numChannels * sizeof (int16_t));
                        for (size_t f = 0; f < numFrames; ++f)
                        {
                            for (UINT32 ch = 0; ch < numChannels; ++ch)
                                channelsData[ch].push_back (pcm16[f * numChannels + ch] / 32768.0f);
                        }
                    }
                    pBuffer->Unlock();
                }
                pBuffer->Release();
            }
            pSample->Release();
        }
    }
    pReader->Release();

    if (channelsData.empty() || channelsData[0].empty())
        return false;

    int totalSamples = (int) channelsData[0].size();
    outBuffer.setSize ((int) numChannels, totalSamples);
    for (UINT32 ch = 0; ch < numChannels; ++ch)
        juce::FloatVectorOperations::copy (outBuffer.getWritePointer ((int) ch), channelsData[ch].data(), totalSamples);

    outSampleRate = sampleRate;
    outChannels = numChannels;
    return true;
}
}
#endif

namespace
{
bool decodeWithFAAD2 (const juce::MemoryBlock& memory, juce::AudioBuffer<float>& outBuffer, double& outSampleRate, unsigned int& outChannels)
{
    if (memory.getSize() < 4)
        return false;

    NeAACDecHandle hDecoder = NeAACDecOpen();
    if (hDecoder == nullptr)
        return false;

    NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration (hDecoder);
    config->outputFormat = FAAD_FMT_FLOAT;
    NeAACDecSetConfiguration (hDecoder, config);

    unsigned long sRate = 0;
    unsigned char chans = 0;
    auto* dataPtr = static_cast<unsigned char*> (const_cast<void*> (memory.getData()));
    unsigned long dataSize = (unsigned long) memory.getSize();

    // Look for ADTS syncword (0xFFF)
    unsigned long startOffset = 0;
    while (startOffset + 1 < dataSize)
    {
        if (dataPtr[startOffset] == 0xFF && (dataPtr[startOffset + 1] & 0xF6) == 0xF0)
            break;
        startOffset++;
    }

    if (startOffset >= dataSize)
        startOffset = 0;

    long initRes = NeAACDecInit (hDecoder, dataPtr + startOffset, dataSize - startOffset, &sRate, &chans);
    if (initRes < 0 || sRate == 0 || chans == 0)
    {
        NeAACDecClose (hDecoder);
        return false;
    }

    std::vector<std::vector<float>> channelsData (chans);
    unsigned long curOffset = startOffset + (unsigned long) initRes;

    while (curOffset < dataSize)
    {
        NeAACDecFrameInfo frameInfo;
        void* pcm = NeAACDecDecode (hDecoder, &frameInfo, dataPtr + curOffset, dataSize - curOffset);

        if (frameInfo.bytesconsumed == 0 && frameInfo.error != 0)
        {
            curOffset++;
            continue;
        }

        curOffset += frameInfo.bytesconsumed;

        if (frameInfo.error == 0 && frameInfo.samples > 0 && pcm != nullptr)
        {
            const float* pcmFloat = static_cast<const float*> (pcm);
            unsigned long framesDecoded = frameInfo.samples / chans;
            for (unsigned long f = 0; f < framesDecoded; ++f)
            {
                for (unsigned char ch = 0; ch < chans; ++ch)
                    channelsData[ch].push_back (pcmFloat[f * chans + ch]);
            }
        }
    }

    NeAACDecClose (hDecoder);

    if (channelsData.empty() || channelsData[0].empty())
        return false;

    int totalSamples = (int) channelsData[0].size();
    outBuffer.setSize ((int) chans, totalSamples);
    for (unsigned char ch = 0; ch < chans; ++ch)
        juce::FloatVectorOperations::copy (outBuffer.getWritePointer ((int) ch), channelsData[ch].data(), totalSamples);

    outSampleRate = sRate;
    outChannels = chans;
    return true;
}

class DecodedAacReader : public juce::AudioFormatReader
{
public:
    DecodedAacReader (juce::InputStream* in,
                      const juce::String& formatName,
                      juce::AudioBuffer<float> buffer,
                      double sRate,
                      unsigned int channels)
        : juce::AudioFormatReader (in, formatName),
          audioData (std::move (buffer))
    {
        sampleRate = sRate;
        numChannels = channels;
        bitsPerSample = 16;
        lengthInSamples = audioData.getNumSamples();
    }

    bool readSamples (int* const* destChannels, int numDestChannels, int startOffsetInDestBuffer,
                      juce::int64 startSampleInFile, int numSamples) override
    {
        if (destChannels == nullptr || numSamples <= 0)
            return true;

        if (startSampleInFile >= lengthInSamples || startSampleInFile < 0)
        {
            for (int ch = 0; ch < numDestChannels; ++ch)
                if (destChannels[ch] != nullptr)
                    std::memset (destChannels[ch] + startOffsetInDestBuffer, 0, (size_t) numSamples * sizeof (int));
            return true;
        }

        int samplesToRead = (int) juce::jmin ((juce::int64) numSamples, lengthInSamples - startSampleInFile);

        for (int ch = 0; ch < numDestChannels; ++ch)
        {
            if (destChannels[ch] != nullptr)
            {
                int srcChannel = juce::jmin (ch, (int) numChannels - 1);
                const float* src = audioData.getReadPointer (srcChannel, (int) startSampleInFile);
                int* dest = destChannels[ch] + startOffsetInDestBuffer;

                for (int i = 0; i < samplesToRead; ++i)
                {
                    float s = juce::jlimit (-1.0f, 1.0f, src[i]);
                    dest[i] = (int) (s * 2147483647.0f);
                }

                for (int i = samplesToRead; i < numSamples; ++i)
                    dest[i] = 0;
            }
        }
        return true;
    }

private:
    juce::AudioBuffer<float> audioData;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DecodedAacReader)
};
}

AacAudioFormat::AacAudioFormat()
    : juce::AudioFormat ("AAC/M4A Format", ".aac .m4a")
{
}

juce::Array<int> AacAudioFormat::getPossibleSampleRates() { return { 44100, 48000, 88200, 96000, 32000, 22050 }; }
juce::Array<int> AacAudioFormat::getPossibleBitDepths()   { return { 16, 24, 32 }; }
bool AacAudioFormat::canDoStereo()                       { return true; }
bool AacAudioFormat::canDoMono()                         { return true; }
bool AacAudioFormat::isCompressed()                      { return true; }

juce::AudioFormatReader* AacAudioFormat::createReaderFor (juce::InputStream* sourceStream, bool destroyStreamIfOpeningFails)
{
    if (sourceStream == nullptr)
        return nullptr;

    juce::AudioBuffer<float> decodedBuffer;
    double sRate = 0.0;
    unsigned int chans = 0;
    bool success = false;

#if JUCE_WINDOWS
    if (auto* fs = dynamic_cast<juce::FileInputStream*> (sourceStream))
    {
        if (decodeWithWMF (fs->getFile(), decodedBuffer, sRate, chans))
            success = true;
    }
#endif

    if (! success)
    {
        sourceStream->setPosition (0);
        juce::MemoryBlock mem;
        auto totalLen = sourceStream->getTotalLength();
        if (totalLen > 0)
        {
            mem.ensureSize ((size_t) totalLen);
            auto bytesRead = sourceStream->read (mem.getData(), (int) totalLen);
            if (bytesRead > 0)
            {
                mem.setSize ((size_t) bytesRead);
                if (decodeWithFAAD2 (mem, decodedBuffer, sRate, chans))
                    success = true;
            }
        }
    }

    if (success && sRate > 0.0 && chans > 0 && decodedBuffer.getNumSamples() > 0)
        return new DecodedAacReader (sourceStream, getFormatName(), std::move (decodedBuffer), sRate, chans);

    if (destroyStreamIfOpeningFails)
        delete sourceStream;

    return nullptr;
}
