#pragma once

#include <JuceHeader.h>

class AudioTrackSource : public juce::AudioSource
{
public:
    AudioTrackSource() = default;
    ~AudioTrackSource() override;

    void prepareToPlay(int, double) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;

    void loadFile(const juce::File& file);
    void loadFileAsync(const juce::File& file,
                       std::function<void(bool, const juce::String&)> onComplete = {});
    bool isLoading() const { return loadingFlag.load() > 0; }

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
    void applyLoadedBuffer(juce::AudioBuffer<float> newBuffer, double sr,
                           juce::String newName, juce::String newPath);

    juce::AudioBuffer<float> fileBuffer;
    std::atomic<int> playPosition{0};
    std::atomic<bool> playing{false};
    std::atomic<bool> looping{false};
    std::atomic<int> loopStart{0};
    std::atomic<int> loopEnd{0};
    std::atomic<int> loadingFlag{0};
    std::atomic<int> loadGeneration{0};
    std::shared_ptr<std::atomic<int>> lifetimeToken;
    double currentSampleRate = 44100.0;
    juce::String loadedFileName;
    juce::String loadedFilePath;
    juce::SpinLock bufferLock;
};
