#include "PluginHost.h"

PluginScanThread::PluginScanThread(juce::KnownPluginList& resultList,
                                   juce::AudioPluginFormat& fmt,
                                   const juce::FileSearchPath& paths)
    : juce::Thread("PluginScanThread"),
      resultList(resultList),
      format(fmt),
      searchPaths(paths)
{
}

PluginScanThread::~PluginScanThread()
{
    stopThread(1000);
}

void PluginScanThread::run()
{
    juce::Array<juce::File> files;
    for (int i = 0; i < searchPaths.getNumPaths(); ++i)
    {
        const juce::File dir = searchPaths[i];
        if (! dir.isDirectory()) continue;
        for (juce::DirectoryIterator it(dir, true, "*.vst3"); it.next(); )
            files.add(it.getFile());
    }

    for (int i = 0; i < files.size() && ! threadShouldExit(); ++i)
    {
        const juce::File f = files[i];

        juce::OwnedArray<juce::PluginDescription> found;
        try
        {
            format.findAllTypesForFile(found, f.getFullPathName());
        }
        catch (...) {}

        for (auto* desc : found)
        {
            if (threadShouldExit()) break;
            const int count = foundCount.fetch_add(1) + 1;
            juce::MessageManager::getInstance()->callAsync(
                [this, d = *desc, count]() mutable
                {
                    if (resultList.getTypeForFile(d.fileOrIdentifier) == nullptr)
                    {
                        resultList.addType(d);
                        resultList.sendChangeMessage();
                    }
                });
        }
    }
}

PluginHost::PluginHost()
{
    formatManager.addFormat(std::make_unique<juce::VST3PluginFormat>());
}

static juce::FileSearchPath getVst3SearchPath()
{
    juce::FileSearchPath searchPath;
    const juce::File defaultVst3Dir("C:\\Program Files\\Common Files\\VST3");
    if (defaultVst3Dir.exists())
        searchPath.add(defaultVst3Dir);
    const juce::File userVst3Dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                       .getChildFile("VST3");
    if (userVst3Dir.exists())
        searchPath.add(userVst3Dir);
    return searchPath;
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
    if (vst3 == nullptr) return;

    const auto searchPath = getVst3SearchPath();
    if (searchPath.getNumPaths() == 0) return;

    juce::File deadMansPedal(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                 .getChildFile("simple-daw-plugin-scan.log"));

    juce::PluginDirectoryScanner scanner(knownList, *vst3, searchPath, true,
                                         deadMansPedal, false);
    juce::String nameBeingScanned;
    while (scanner.scanNextFile(true, nameBeingScanned)) {}
}

void PluginHost::scanForPluginsAsync()
{
    if (activeScan != nullptr && activeScan->isThreadRunning())
        return;

    juce::AudioPluginFormat* vst3 = nullptr;
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        if (formatManager.getFormat(i)->getName().equalsIgnoreCase("VST3"))
        {
            vst3 = formatManager.getFormat(i);
            break;
        }
    }
    if (vst3 == nullptr) return;

    const auto searchPath = getVst3SearchPath();
    if (searchPath.getNumPaths() == 0) return;

    activeScan = std::make_unique<PluginScanThread>(knownList, *vst3, searchPath);
    activeScan->startThread();
}

bool PluginHost::isScanning() const
{
    return activeScan != nullptr && activeScan->isThreadRunning();
}

int PluginHost::getScanFoundCount() const
{
    if (activeScan == nullptr) return (int) knownList.getNumTypes();
    return activeScan->getFoundCount() + (int) knownList.getNumTypes();
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
