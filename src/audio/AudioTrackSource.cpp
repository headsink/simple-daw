#include "AudioTrackSource.h"

void AudioTrackSource::prepareToPlay(int, double sampleRate)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

void AudioTrackSource::releaseResources() {}

void AudioTrackSource::loadFile(const juce::File& file)
{
    juce::AudioFormatManager mgr;
    mgr.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(mgr.createReaderFor(file));
    if (reader == nullptr)
    {
        fileBuffer.setSize(0, 0);
        loadedFileName = "(failed to load)";
        return;
    }

    fileBuffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
    reader->read(&fileBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
    playPosition.store(0);
    playing.store(false);
    currentSampleRate = reader->sampleRate;
    loadedFileName = file.getFileName() + "  (" + juce::String(reader->numChannels)
        + " ch, " + juce::String(reader->lengthInSamples) + " samples, "
        + juce::String((int)reader->sampleRate) + " Hz)";
}

void AudioTrackSource::setPlaying(bool shouldPlay) { playing.store(shouldPlay); }
void AudioTrackSource::stop() { playing.store(false); playPosition.store(0); }
void AudioTrackSource::togglePlay() { playing.store(!playing.load()); }
void AudioTrackSource::seekToStart() { playPosition.store(0); }
void AudioTrackSource::setLooping(bool shouldLoop) { looping.store(shouldLoop); }

bool AudioTrackSource::isLoaded() const { return fileBuffer.getNumSamples() > 0; }
bool AudioTrackSource::isPlaying() const { return playing.load(); }
bool AudioTrackSource::isLooping() const { return looping.load(); }
const juce::String& AudioTrackSource::getLoadedFileName() const { return loadedFileName; }
int AudioTrackSource::getNumChannels() const { return fileBuffer.getNumChannels(); }
int AudioTrackSource::getNumSamples() const { return fileBuffer.getNumSamples(); }
int AudioTrackSource::getPlayPosition() const { return playPosition.load(); }

void AudioTrackSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    info.buffer->clear();
    if (!playing.load() || fileBuffer.getNumSamples() == 0)
        return;

    const int pos = playPosition.load();
    const int numSamples = info.numSamples;
    const int total = fileBuffer.getNumSamples();
    int remaining = total - pos;
    if (remaining <= 0)
    {
        if (looping.load())
        {
            playPosition.store(0);
            remaining = total;
        }
        else
        {
            playing.store(false);
            playPosition.store(0);
            return;
        }
    }

    int toCopy = std::min(numSamples, remaining);
    for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
    {
        const int srcCh = ch % fileBuffer.getNumChannels();
        info.buffer->copyFrom(ch, info.startSample, fileBuffer, srcCh,
                              pos, toCopy);
    }
    int newPos = pos + toCopy;
    if (looping.load() && newPos >= total)
        newPos = 0;
    playPosition.store(newPos);
}
