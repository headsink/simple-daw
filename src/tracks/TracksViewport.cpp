#include "TracksViewport.h"

TracksViewport::TracksViewport(juce::AudioDeviceManager& dm,
                               PluginHost& host,
                               std::function<void(TrackRow*)> onTrackRemovedCb,
                               std::function<void()> onLayoutChangedCb)
    : pluginHost(host),
      deviceManager(dm),
      onTrackRemoved(std::move(onTrackRemovedCb)),
      onLayoutChanged(std::move(onLayoutChangedCb))
{
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&container);
    viewport.setScrollBarsShown(true, false);
}

TracksViewport::~TracksViewport()
{
    {
        const juce::SpinLock::ScopedLockType sl(tracksLock);
        trackRows.clear();
        tracks.clear();
    }
}

void TracksViewport::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
}

void TracksViewport::resized()
{
    viewport.setBounds(getLocalBounds());
    refreshLayout();
}

void TracksViewport::resolveCurrentDevice(double& outSampleRate, int& outBlockSize) const
{
    outSampleRate = 48000.0;
    outBlockSize = 512;
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        outSampleRate = device->getCurrentSampleRate();
        outBlockSize = device->getCurrentBufferSizeSamples();
    }
}

void TracksViewport::addTrack()
{
    double sr = 48000.0;
    int bs = 512;
    resolveCurrentDevice(sr, bs);

    auto track = std::make_unique<AudioTrack>();
    track->prepareToPlay(bs, sr);

    auto* trackPtr = track.get();
    auto row = std::make_unique<TrackRow>(*trackPtr, pluginHost,
        [this](TrackRow* r) { removeTrack(r); },
        [this] { refreshLayout(); if (onLayoutChanged) onLayoutChanged(); },
        [this](TrackRow* from, TrackRow* to, bool above) { reorderTrack(from, to, above); });

    {
        const juce::SpinLock::ScopedLockType sl(tracksLock);
        tracks.push_back(std::move(track));
        trackRows.push_back(std::move(row));
    }
    publishSnapshot();
    container.addAndMakeVisible(trackRows.back().get());
    refreshLayout();
}

TrackRow* TracksViewport::createTrackFromFileAsync(const juce::File& audioFile, float initialGain,
                                                int loopStart, int loopEnd)
{
    double sr = 48000.0;
    int bs = 512;
    resolveCurrentDevice(sr, bs);

    auto track = std::make_unique<AudioTrack>();
    track->prepareToPlay(bs, sr);
    track->setGain(initialGain);

    auto token = track->getLifetimeToken();
    const int myVersion = token->load();

    auto row = std::make_unique<TrackRow>(*track, pluginHost,
        [this](TrackRow* r) { removeTrack(r); },
        [this] { refreshLayout(); if (onLayoutChanged) onLayoutChanged(); },
        [this](TrackRow* from, TrackRow* to, bool above) { reorderTrack(from, to, above); });
    row->setNameText("(loading...) " + audioFile.getFileName());

    {
        const juce::SpinLock::ScopedLockType sl(tracksLock);
        tracks.push_back(std::move(track));
        trackRows.push_back(std::move(row));
    }
    publishSnapshot();
    TrackRow* rowPtr = trackRows.back().get();
    container.addAndMakeVisible(rowPtr);
    AudioTrack* trackRaw = tracks.back().get();

    trackRaw->loadFileAsync(audioFile,
        [this, rowPtr, token, myVersion, loopStart, loopEnd](bool ok, const juce::String& err)
        {
            juce::MessageManager::getInstance()->callAsync(
                [this, rowPtr, token, myVersion, loopStart, loopEnd, ok, err]
                {
                    if (token->load() != myVersion) return;
                    if (rowPtr == nullptr) return;
                    if (ok)
                    {
                        auto& source = rowPtr->getTrack().getSource();
                        rowPtr->setNameText(source.getLoadedFileName());
                        if (loopStart >= 0 || loopEnd >= 0)
                        {
                            if (loopStart >= 0)
                                rowPtr->getTrack().setLoopStart(loopStart);
                            if (loopEnd >= 0)
                                rowPtr->getTrack().setLoopEnd(loopEnd);
                        }
                    }
                    else
                    {
                        rowPtr->setNameText("(failed: " + err + ")");
                    }
                    rowPtr->refreshTimeLabel();
                    rowPtr->updateButtons();
                    refreshLayout();
                });
        });

    return rowPtr;
}

void TracksViewport::removeTrack(TrackRow* row)
{
    container.removeChildComponent(row);

    juce::MessageManager::getInstance()->callAsync(
        [this, row]
        {
            for (size_t i = 0; i < trackRows.size(); ++i)
            {
                if (trackRows[i].get() == row)
                {
                    const juce::SpinLock::ScopedLockType sl(tracksLock);
                    trackRows.erase(trackRows.begin() + i);
                    tracks.erase(tracks.begin() + i);
                    publishSnapshot();
                    break;
                }
            }
            refreshLayout();
            if (onLayoutChanged) onLayoutChanged();
        });
}

void TracksViewport::reorderTrack(TrackRow* from, TrackRow* to, bool insertAbove)
{
    if (from == to) return;
    size_t fromIdx = SIZE_MAX, toIdx = SIZE_MAX;
    for (size_t i = 0; i < trackRows.size(); ++i)
    {
        if (trackRows[i].get() == from) fromIdx = i;
        if (trackRows[i].get() == to)   toIdx = i;
    }
    if (fromIdx == SIZE_MAX || toIdx == SIZE_MAX) return;

    const juce::SpinLock::ScopedLockType sl(tracksLock);

    size_t newIdx = insertAbove ? toIdx : toIdx + 1;

    auto movedTrack = std::move(tracks[fromIdx]);
    auto movedRow   = std::move(trackRows[fromIdx]);
    tracks.erase(tracks.begin() + fromIdx);
    trackRows.erase(trackRows.begin() + fromIdx);

    if (newIdx > fromIdx) --newIdx;

    tracks.insert(tracks.begin() + newIdx, std::move(movedTrack));
    trackRows.insert(trackRows.begin() + newIdx, std::move(movedRow));

    publishSnapshot();
    refreshLayout();
}

void TracksViewport::clearAll()
{
    {
        const juce::SpinLock::ScopedLockType sl(tracksLock);
        trackRows.clear();
        tracks.clear();
    }
    publishSnapshot();
}

void TracksViewport::prepareAllToPlay(int samplesPerBlockExpected, double sampleRate)
{
    for (auto& t : tracks)
        t->prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void TracksViewport::releaseAllResources()
{
    for (auto& t : tracks)
        t->releaseResources();
}

bool TracksViewport::anyTrackSoloed() const
{
    const juce::SpinLock::ScopedTryLockType sl(tracksLock);
    if (! sl.isLocked()) return false;
    for (const auto& t : tracks)
        if (t->isSolo()) return true;
    return false;
}

void TracksViewport::tryRenderAll(juce::AudioBuffer<float>& dest, int startSample,
                                  int numSamples, bool anyTrackSoloedFlag)
{
    auto snap = tracksSnapshot.load(std::memory_order_acquire);
    for (auto* t : *snap)
        t->renderInto(dest, startSample, numSamples, anyTrackSoloedFlag);
}

void TracksViewport::publishSnapshot()
{
    auto snap = std::make_shared<std::vector<AudioTrack*>>();
    {
        const juce::SpinLock::ScopedLockType sl(tracksLock);
        snap->reserve(tracks.size());
        for (const auto& t : tracks)
            snap->push_back(t.get());
    }
    tracksSnapshot.store(std::move(snap), std::memory_order_release);
}

void TracksViewport::refreshLayout()
{
    if (trackRows.empty())
    {
        container.setSize(viewport.getWidth(), viewport.getHeight());
        return;
    }
    int totalHeight = 0;
    for (size_t i = 0; i < trackRows.size(); ++i)
    {
        const int h = trackRows[i]->getCurrentPreferredHeight();
        trackRows[i]->setBounds(0, totalHeight, viewport.getWidth() - 4, h);
        totalHeight += h;
    }
    container.setSize(viewport.getWidth(), std::max(totalHeight, viewport.getHeight()));
}
