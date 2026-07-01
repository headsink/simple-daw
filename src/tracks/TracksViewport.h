#pragma once

#include <JuceHeader.h>
#include "AudioTrack.h"
#include "TrackRow.h"
#include "../plugin/PluginHost.h"

class TracksViewport : public juce::Component
{
public:
    TracksViewport(juce::AudioDeviceManager& dm,
                   PluginHost& host,
                   std::function<void(TrackRow*)> onTrackRemoved,
                   std::function<void()> onLayoutChanged);
    ~TracksViewport() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void addTrack();
    TrackRow* createTrackFromFileAsync(const juce::File& audioFile, float initialGain,
                                       int loopStart = -1, int loopEnd = -1);
    void removeTrack(TrackRow* row);
    void reorderTrack(TrackRow* from, TrackRow* to, bool insertAbove);
    void clearAll();

    void prepareAllToPlay(int samplesPerBlockExpected, double sampleRate);
    void releaseAllResources();

    void tryRenderAll(juce::AudioBuffer<float>& dest, int startSample,
                      int numSamples, bool anyTrackSoloed);

    bool anyTrackSoloed() const;

    const std::vector<std::unique_ptr<AudioTrack>>& getTracks() const { return tracks; }
    const std::vector<std::unique_ptr<TrackRow>>& getTrackRows() const { return trackRows; }

    juce::Viewport& getViewport() { return viewport; }

private:
    void refreshLayout();
    void resolveCurrentDevice(double& outSampleRate, int& outBlockSize) const;

    PluginHost& pluginHost;
    juce::AudioDeviceManager& deviceManager;
    std::function<void(TrackRow*)> onTrackRemoved;
    std::function<void()> onLayoutChanged;

    juce::SpinLock tracksLock;
    std::vector<std::unique_ptr<AudioTrack>> tracks;
    std::vector<std::unique_ptr<TrackRow>> trackRows;

    std::atomic<std::shared_ptr<const std::vector<AudioTrack*>>> tracksSnapshot{
        std::make_shared<const std::vector<AudioTrack*>>() };

    void publishSnapshot();

    juce::Viewport viewport;
    juce::Component container;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TracksViewport)
};
