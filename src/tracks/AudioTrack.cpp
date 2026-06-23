#include "AudioTrack.h"

AudioTrack::AudioTrack()
{
    source = std::make_unique<AudioTrackSource>();
}

void AudioTrack::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    currentBlockSize = samplesPerBlockExpected > 0 ? samplesPerBlockExpected : 512;

    source->prepareToPlay(samplesPerBlockExpected, sampleRate);
    scratchBuffer.setSize(2, samplesPerBlockExpected);

    if (plugin)
    {
        plugin->prepareToPlay(currentSampleRate, currentBlockSize);
        juce::AudioProcessor::BusesLayout stereo;
        stereo.inputBuses.add(juce::AudioChannelSet::stereo());
        stereo.outputBuses.add(juce::AudioChannelSet::stereo());
        plugin->setBusesLayout(stereo);
    }
}

void AudioTrack::releaseResources()
{
    source->releaseResources();
    if (plugin) plugin->releaseResources();
    scratchBuffer.setSize(0, 0);
}

void AudioTrack::loadFile(const juce::File& file) { source->loadFile(file); }
void AudioTrack::setPlaying(bool shouldPlay) { source->setPlaying(shouldPlay); }
void AudioTrack::togglePlay() { source->togglePlay(); }
void AudioTrack::stop() { source->stop(); }

void AudioTrack::setGain(float g) { gain.store(juce::jlimit(0.0f, 2.0f, g)); }
void AudioTrack::setPan(float p)  { pan.store(juce::jlimit(-1.0f, 1.0f, p)); }
void AudioTrack::setMute(bool m)  { mute.store(m); }
void AudioTrack::setSolo(bool s)  { solo.store(s); }

void AudioTrack::setPlugin(std::unique_ptr<juce::AudioPluginInstance> p)
{
    const juce::SpinLock::ScopedLockType sl(pluginLock);
    if (plugin) plugin->releaseResources();
    plugin = std::move(p);
    if (plugin)
    {
        plugin->prepareToPlay(currentSampleRate, currentBlockSize);
        juce::AudioProcessor::BusesLayout stereo;
        stereo.inputBuses.add(juce::AudioChannelSet::stereo());
        stereo.outputBuses.add(juce::AudioChannelSet::stereo());
        plugin->setBusesLayout(stereo);
    }
}

void AudioTrack::clearPlugin()
{
    const juce::SpinLock::ScopedLockType sl(pluginLock);
    if (plugin) plugin->releaseResources();
    plugin.reset();
    pluginBypass.store(false);
}

juce::String AudioTrack::getPluginName() const
{
    return plugin ? plugin->getName() : juce::String();
}

void AudioTrack::renderInto(juce::AudioBuffer<float>& dest, int startSample, int numSamples,
                            bool anyOtherTrackSoloed)
{
    if (mute.load()) return;
    if (anyOtherTrackSoloed && !solo.load()) return;
    if (!source->isLoaded() && !plugin) return;

    scratchBuffer.clear();
    juce::AudioSourceChannelInfo scratch(&scratchBuffer, 0, numSamples);
    source->getNextAudioBlock(scratch);

    if (plugin && !pluginBypass.load())
    {
        const juce::SpinLock::ScopedTryLockType sl(pluginLock);
        if (sl.isLocked())
        {
            pluginMidiBuffer.clear();
            plugin->processBlock(scratchBuffer, pluginMidiBuffer);
        }
    }

    const float g = gain.load();
    const float p = pan.load();
    const int destChans = dest.getNumChannels();

    for (int ch = 0; ch < destChans; ++ch)
    {
        const int srcCh = ch % scratchBuffer.getNumChannels();
        float panGain = 1.0f;
        if (destChans == 2)
        {
            if (ch == 0) panGain = (p <= 0.0f) ? 1.0f : (1.0f - p);
            else         panGain = (p >= 0.0f) ? 1.0f : (1.0f + p);
        }
        dest.addFrom(ch, startSample, scratchBuffer, srcCh, 0, numSamples, g * panGain);
    }
}
