#include "AudioTrackSource.h"

void AudioTrackSource::prepareToPlay(int, double sampleRate)
{
    if (! lifetimeToken)
        lifetimeToken = std::make_shared<std::atomic<int>>(0);
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

AudioTrackSource::~AudioTrackSource()
{
    if (lifetimeToken)
        lifetimeToken->fetch_add(1);
}

void AudioTrackSource::releaseResources() {}

void AudioTrackSource::loadFile(const juce::File& file)
{
    juce::AudioFormatManager mgr;
    mgr.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(mgr.createReaderFor(file));
    if (reader == nullptr)
    {
        applyLoadedBuffer({}, 0.0, "(failed to load)", juce::String());
        return;
    }

    juce::AudioBuffer<float> newBuffer((int)reader->numChannels, (int)reader->lengthInSamples);
    reader->read(&newBuffer, 0, (int)reader->lengthInSamples, 0, true, true);

    const double sr = reader->sampleRate;
    const juce::String name = file.getFileName() + "  (" + juce::String(reader->numChannels)
        + " ch, " + juce::String(reader->lengthInSamples) + " samples, "
        + juce::String((int)reader->sampleRate) + " Hz)";
    const juce::String path = file.getFullPathName();

    applyLoadedBuffer(std::move(newBuffer), sr, name, path);
}

void AudioTrackSource::loadFileAsync(const juce::File& file,
                                     std::function<void(bool, const juce::String&)> onComplete)
{
    if (! lifetimeToken)
        lifetimeToken = std::make_shared<std::atomic<int>>(0);

    const int myGen = loadGeneration.fetch_add(1) + 1;
    loadingFlag.fetch_add(1);
    auto token = lifetimeToken;
    const int myVersion = token->load();

    std::thread([this, token, myVersion, myGen, file, cb = std::move(onComplete)]() mutable
    {
        if (token->load() != myVersion) return;

        juce::AudioFormatManager mgr;
        mgr.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader(mgr.createReaderFor(file));
        if (reader == nullptr)
        {
            juce::MessageManager::getInstance()->callAsync(
                [this, token, myVersion, myGen, cb]()
                mutable
                {
                    if (token->load() != myVersion) return;
                    if (loadGeneration.load() == myGen)
                    {
                        loadingFlag.store(0);
                        if (cb) cb(false, "Failed to decode file");
                    }
                });
            return;
        }

        if (token->load() != myVersion) return;

        juce::AudioBuffer<float> newBuffer((int)reader->numChannels, (int)reader->lengthInSamples);
        reader->read(&newBuffer, 0, (int)reader->lengthInSamples, 0, true, true);

        const double sr = reader->sampleRate;
        const juce::String name = file.getFileName() + "  (" + juce::String(reader->numChannels)
            + " ch, " + juce::String(reader->lengthInSamples) + " samples, "
            + juce::String((int)reader->sampleRate) + " Hz)";
        const juce::String path = file.getFullPathName();

        juce::MessageManager::getInstance()->callAsync(
            [this, token, myVersion, myGen, cb, newBuffer = std::move(newBuffer), sr, name, path]() mutable
            {
                if (token->load() != myVersion) return;
                if (loadGeneration.load() != myGen) return;
                applyLoadedBuffer(std::move(newBuffer), sr, name, path);
                loadingFlag.store(0);
                if (cb) cb(true, juce::String());
            });
    }).detach();
}

void AudioTrackSource::applyLoadedBuffer(juce::AudioBuffer<float> newBuffer, double sr,
                                         juce::String newName, juce::String newPath)
{
    const juce::SpinLock::ScopedLockType sl(bufferLock);
    fileBuffer = std::move(newBuffer);
    playPosition.store(0);
    playing.store(false);
    if (sr > 0.0) currentSampleRate = sr;
    loopStart.store(0);
    loopEnd.store(fileBuffer.getNumSamples());
    loadedFileName = newName;
    loadedFilePath = newPath;
}

void AudioTrackSource::setPlaying(bool shouldPlay) { playing.store(shouldPlay); }
void AudioTrackSource::stop() { playing.store(false); playPosition.store(0); }
void AudioTrackSource::togglePlay() { playing.store(!playing.load()); }
void AudioTrackSource::seekToStart() { playPosition.store(0); }
void AudioTrackSource::setLooping(bool shouldLoop) { looping.store(shouldLoop); }

void AudioTrackSource::setLoopStart(int sample) { loopStart.store(clampLoopBound(sample)); }
void AudioTrackSource::setLoopEnd(int sample)   { loopEnd.store(clampLoopBound(sample)); }
void AudioTrackSource::clearLoopRegion()
{
    loopStart.store(0);
    loopEnd.store(fileBuffer.getNumSamples());
}

int AudioTrackSource::clampLoopBound(int sample) const
{
    const int total = fileBuffer.getNumSamples();
    if (total <= 0) return 0;
    if (sample < 0) return 0;
    if (sample > total) return total;
    return sample;
}

void AudioTrackSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    info.buffer->clear();

    const juce::SpinLock::ScopedTryLockType sl(bufferLock);
    if (! sl.isLocked())
        return;

    if (!playing.load() || fileBuffer.getNumSamples() == 0)
        return;

    const int total = fileBuffer.getNumSamples();
    int pos = playPosition.load();
    const int numSamples = info.numSamples;

    int a = loopStart.load();
    int b = loopEnd.load();
    if (a < 0) a = 0;
    if (b > total) b = total;
    if (a >= b) { a = 0; b = total; }

    if (pos < a) pos = a;
    if (pos >= b) pos = looping.load() ? a : total;

    int remaining = b - pos;
    if (remaining <= 0)
    {
        playing.store(false);
        playPosition.store(0);
        return;
    }

    int toCopy = std::min(numSamples, remaining);
    for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
    {
        const int srcCh = ch % fileBuffer.getNumChannels();
        info.buffer->copyFrom(ch, info.startSample, fileBuffer, srcCh,
                              pos, toCopy);
    }
    int newPos = pos + toCopy;
    if (looping.load() && newPos >= b)
        newPos = a;
    playPosition.store(newPos);
}

bool AudioTrackSource::isLoaded() const { return fileBuffer.getNumSamples() > 0; }
bool AudioTrackSource::isPlaying() const { return playing.load(); }
bool AudioTrackSource::isLooping() const { return looping.load(); }
const juce::String& AudioTrackSource::getLoadedFileName() const { return loadedFileName; }
int AudioTrackSource::getNumChannels() const { return fileBuffer.getNumChannels(); }
int AudioTrackSource::getNumSamples() const { return fileBuffer.getNumSamples(); }
int AudioTrackSource::getPlayPosition() const { return playPosition.load(); }
