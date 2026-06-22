#pragma once

#include <JuceHeader.h>

class AudioTrackSource : public juce::AudioSource
{
public:
    void prepareToPlay(int, double) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;

    void loadFile(const juce::File& file);

    void setPlaying(bool shouldPlay);
    void stop();
    void togglePlay();
    void seekToStart();
    void setLooping(bool shouldLoop);

    bool isLoaded() const;
    bool isPlaying() const;
    bool isLooping() const;
    const juce::String& getLoadedFileName() const;
    int getNumChannels() const;
    int getNumSamples() const;
    int getPlayPosition() const;
    double getSampleRate() const { return currentSampleRate; }

private:
    juce::AudioBuffer<float> fileBuffer;
    std::atomic<int> playPosition{0};
    std::atomic<bool> playing{false};
    std::atomic<bool> looping{false};
    double currentSampleRate = 44100.0;
    juce::String loadedFileName;
};
