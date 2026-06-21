#include "AudioTrackSource.h"

void AudioTrackSource::prepareToPlay(int, double) {}
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
    playPosition = 0;
    playing = false;
    loadedFileName = file.getFileName() + "  (" + juce::String(reader->numChannels)
        + " ch, " + juce::String(reader->lengthInSamples) + " samples, "
        + juce::String((int)reader->sampleRate) + " Hz)";
}

void AudioTrackSource::setPlaying(bool shouldPlay) { playing = shouldPlay; }
void AudioTrackSource::stop() { playing = false; playPosition = 0; }
void AudioTrackSource::togglePlay() { playing = !playing; }
void AudioTrackSource::seekToStart() { playPosition = 0; }

bool AudioTrackSource::isLoaded() const { return fileBuffer.getNumSamples() > 0; }
bool AudioTrackSource::isPlaying() const { return playing; }
const juce::String& AudioTrackSource::getLoadedFileName() const { return loadedFileName; }
int AudioTrackSource::getNumChannels() const { return fileBuffer.getNumChannels(); }
int AudioTrackSource::getNumSamples() const { return fileBuffer.getNumSamples(); }

void AudioTrackSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    info.buffer->clear();
    if (!playing || fileBuffer.getNumSamples() == 0)
        return;

    const int numSamples = info.numSamples;
    const int remaining = fileBuffer.getNumSamples() - playPosition;
    if (remaining <= 0)
    {
        playing = false;
        playPosition = 0;
        return;
    }

    const int toCopy = std::min(numSamples, remaining);
    for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
    {
        const int srcCh = ch % fileBuffer.getNumChannels();
        info.buffer->copyFrom(ch, info.startSample, fileBuffer, srcCh,
                              playPosition, toCopy);
    }
    playPosition += toCopy;
}
