#pragma once

#include <JuceHeader.h>

class PluginScanThread : public juce::Thread
{
public:
    PluginScanThread(juce::KnownPluginList& resultList,
                     juce::AudioPluginFormat& fmt,
                     const juce::FileSearchPath& paths);
    ~PluginScanThread() override;

    void run() override;

    int getFoundCount() const { return foundCount.load(); }

private:
    juce::KnownPluginList& resultList;
    juce::AudioPluginFormat& format;
    juce::FileSearchPath searchPaths;
    std::atomic<int> foundCount{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginScanThread)
};

class PluginHost
{
public:
    PluginHost();

    juce::AudioPluginFormatManager& getFormatManager() { return formatManager; }
    juce::KnownPluginList& getKnownList() { return knownList; }

    void setSampleRate(double r) { sampleRate = r; }
    void setBlockSize(int b) { blockSize = b; }

    void scanForPlugins();
    void scanForPluginsAsync();
    bool isScanning() const;
    int getScanFoundCount() const;
    void clearKnownList();

    using InstanceCallback =
        std::function<void(std::unique_ptr<juce::AudioPluginInstance>, const juce::String&)>;

    void createInstanceAsync(const juce::PluginDescription& desc, InstanceCallback cb);

private:
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownList;
    double sampleRate = 48000.0;
    int blockSize = 512;

    std::unique_ptr<PluginScanThread> activeScan;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHost)
};
