#include "Recorder.h"

void Recorder::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device != nullptr)
    {
        recordSampleRate = device->getCurrentSampleRate();
        deviceBufferSize = device->getCurrentBufferSizeSamples();
    }
}

void Recorder::audioDeviceStopped() {}

void Recorder::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                 int numInputChannels,
                                                 float* const* /*outputChannelData*/,
                                                 int /*numOutputChannels*/,
                                                 int numSamples,
                                                 const juce::AudioIODeviceCallbackContext& /*context*/)
{
    if (! recording.load() || numInputChannels == 0 || inputChannelData == nullptr)
        return;

    const juce::ScopedLock sl(bufferLock);

    const int neededLength = recordBufferLength + numSamples;
    const int currentCapacity = recordBuffer.getNumSamples();
    if (currentCapacity < neededLength || recordBuffer.getNumChannels() != numInputChannels)
    {
        const int newCapacity = juce::jmax(neededLength, currentCapacity * 2 + deviceBufferSize * 4);
        recordBuffer.setSize(numInputChannels, newCapacity, true, true);
    }

    for (int ch = 0; ch < numInputChannels; ++ch)
    {
        if (inputChannelData[ch] != nullptr)
            recordBuffer.copyFrom(ch, recordBufferLength, inputChannelData[ch], numSamples);
    }
    recordBufferLength += numSamples;
}

void Recorder::startRecording()
{
    const juce::ScopedLock sl(bufferLock);
    recordBufferLength = 0;
    recordBuffer.setSize(0, 0);
    recording.store(true);
}

juce::String Recorder::stopRecording()
{
    recording.store(false);

    const juce::ScopedLock sl(bufferLock);

    if (recordBufferLength == 0 || recordBuffer.getNumChannels() == 0)
        return {};

    const juce::String filename = "simple-daw-recording-" +
        juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S") + ".wav";
    juce::File outFile(juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile(filename));

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> stream(outFile.createOutputStream());
    if (stream == nullptr) return {};

    auto* rawWriter = wavFormat.createWriterFor(stream.get(), recordSampleRate,
        (unsigned int) recordBuffer.getNumChannels(), 16, {}, 0);
    if (rawWriter == nullptr) return {};

    std::unique_ptr<juce::AudioFormatWriter> writer(rawWriter);
    writer->writeFromAudioSampleBuffer(recordBuffer, 0, recordBufferLength);
    writer->flush();

    return outFile.getFullPathName();
}
