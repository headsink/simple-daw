#include "PluginHost.h"

PluginHost::PluginHost()
{
    formatManager.addFormat(std::make_unique<juce::VST3PluginFormat>());
}

void PluginHost::scanForPlugins()
{
    juce::AudioPluginFormat* vst3 = nullptr;
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        if (formatManager.getFormat(i)->getName().equalsIgnoreCase("VST3"))
        {
            vst3 = formatManager.getFormat(i);
            break;
        }
    }
    if (vst3 == nullptr)
        return;

    juce::FileSearchPath searchPath;
    const juce::File defaultVst3Dir("C:\\Program Files\\Common Files\\VST3");
    if (defaultVst3Dir.exists())
        searchPath.add(defaultVst3Dir);
    const juce::File userVst3Dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                       .getChildFile("VST3");
    if (userVst3Dir.exists())
        searchPath.add(userVst3Dir);

    if (searchPath.getNumPaths() == 0)
        return;

    juce::File deadMansPedal = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                   .getChildFile("simple-daw-plugin-scan.log");

    juce::PluginDirectoryScanner scanner(knownList, *vst3, searchPath, true,
                                         deadMansPedal, false);
    juce::String nameBeingScanned;
    while (scanner.scanNextFile(true, nameBeingScanned)) {}
}

void PluginHost::clearKnownList()
{
    knownList.clear();
    knownList.sendChangeMessage();
}

void PluginHost::createInstanceAsync(const juce::PluginDescription& desc, InstanceCallback cb)
{
    formatManager.createPluginInstanceAsync(desc, sampleRate, blockSize, std::move(cb));
}
