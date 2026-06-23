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

    void setLoopStart(int sample);
    void setLoopEnd(int sample);
    void clearLoopRegion();
    int getLoopStart() const { return loopStart.load(); }
    int getLoopEnd() const { return loopEnd.load(); }

    bool isLoaded() const;
    bool isPlaying() const;
    bool isLooping() const;
    const juce::String& getLoadedFileName() const;
    const juce::String& getFilePath() const { return loadedFilePath; }
    int getNumChannels() const;
    int getNumSamples() const;
    int getPlayPosition() const;
    double getSampleRate() const { return currentSampleRate; }

private:
    int clampLoopBound(int sample) const;
    bool loopRegionIsFullFile() const;

    juce::AudioBuffer<float> fileBuffer;
    std::atomic<int> playPosition{0};
    std::atomic<bool> playing{false};
    std::atomic<bool> looping{false};
    std::atomic<int> loopStart{0};
    std::atomic<int> loopEnd{0};
    double currentSampleRate = 44100.0;
    juce::String loadedFileName;
    juce::String loadedFilePath;
};
