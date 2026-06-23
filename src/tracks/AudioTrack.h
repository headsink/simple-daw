#pragma once

#include <JuceHeader.h>
#include "../audio/AudioTrackSource.h"

class AudioTrack
{
public:
    AudioTrack();
    ~AudioTrack();

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate);
    void releaseResources();

    void loadFile(const juce::File& file);
    void loadFileAsync(const juce::File& file,
                       std::function<void(bool, const juce::String&)> onComplete = {});
    bool isLoading() const { return source->isLoading(); }
    std::shared_ptr<std::atomic<int>> getLifetimeToken() const { return lifetimeToken; }
    void setPlaying(bool shouldPlay);
    void togglePlay();
    void stop();

    void setGain(float g);
    void setPan(float p);
    void setMute(bool m);
    void setSolo(bool s);

    void setLoopStart(int sample) { source->setLoopStart(sample); }
    void setLoopEnd(int sample) { source->setLoopEnd(sample); }
    void clearLoopRegion() { source->clearLoopRegion(); }
    int getLoopStart() const { return source->getLoopStart(); }
    int getLoopEnd() const { return source->getLoopEnd(); }
    void setLoopStartFromPlayhead() { source->setLoopStart(source->getPlayPosition()); }
    void setLoopEndFromPlayhead() { source->setLoopEnd(source->getPlayPosition()); }

    void setPlugin(std::unique_ptr<juce::AudioPluginInstance> p);
    void clearPlugin();
    bool hasPlugin() const { return plugin != nullptr; }
    juce::AudioPluginInstance* getPlugin() const { return plugin.get(); }
    void setPluginBypass(bool b) { pluginBypass.store(b); }
    bool isPluginBypassed() const { return pluginBypass.load(); }
    juce::String getPluginName() const;

    void renderInto(juce::AudioBuffer<float>& dest, int startSample, int numSamples,
                    bool anyOtherTrackSoloed);

    AudioTrackSource& getSource() { return *source; }
    const AudioTrackSource& getSource() const { return *source; }

    float getGain() const { return gain.load(); }
    float getPan() const { return pan.load(); }
    bool isMuted() const { return mute.load(); }
    bool isSolo() const { return solo.load(); }

    std::atomic<float>& getPeakRef() { return peak; }

private:
    std::unique_ptr<AudioTrackSource> source;
    juce::AudioBuffer<float> scratchBuffer;
    std::shared_ptr<std::atomic<int>> lifetimeToken;

    std::atomic<float> gain{1.0f};
    std::atomic<float> pan{0.0f};
    std::atomic<bool> mute{false};
    std::atomic<bool> solo{false};
    std::atomic<float> peak{0.0f};

    std::unique_ptr<juce::AudioPluginInstance> plugin;
    std::atomic<bool> pluginBypass{false};
    juce::MidiBuffer pluginMidiBuffer;
    juce::SpinLock pluginLock;
    double currentSampleRate = 48000.0;
    int currentBlockSize = 512;
};
