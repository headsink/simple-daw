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

    bool isLoaded() const;
    bool isPlaying() const;
    const juce::String& getLoadedFileName() const;
    int getNumChannels() const;
    int getNumSamples() const;

private:
    juce::AudioBuffer<float> fileBuffer;
    int playPosition = 0;
    bool playing = false;
    juce::String loadedFileName;
};
