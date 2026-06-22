#pragma once

#include <JuceHeader.h>

class PluginHost
{
public:
    PluginHost();

    juce::AudioPluginFormatManager& getFormatManager() { return formatManager; }
    juce::KnownPluginList& getKnownList() { return knownList; }

    void setSampleRate(double r) { sampleRate = r; }
    void setBlockSize(int b) { blockSize = b; }

    void scanForPlugins();
    void clearKnownList();

    using InstanceCallback =
        std::function<void(std::unique_ptr<juce::AudioPluginInstance>, const juce::String&)>;

    void createInstanceAsync(const juce::PluginDescription& desc, InstanceCallback cb);

private:
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownList;
    double sampleRate = 48000.0;
    int blockSize = 512;
};
