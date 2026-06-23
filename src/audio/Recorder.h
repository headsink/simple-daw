#pragma once

#include <JuceHeader.h>

class Recorder : public juce::AudioIODeviceCallback
{
public:
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

    void startRecording();
    juce::String stopRecording();
    bool isRecording() const { return recording.load(); }

private:
    std::atomic<bool> recording{false};
    juce::AudioBuffer<float> recordBuffer;
    int recordBufferLength = 0;
    double recordSampleRate = 0.0;
    int deviceBufferSize = 512;
    juce::CriticalSection bufferLock;
};
