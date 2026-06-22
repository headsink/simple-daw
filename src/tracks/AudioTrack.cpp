#include "AudioTrack.h"

AudioTrack::AudioTrack()
{
    source = std::make_unique<AudioTrackSource>();
}

void AudioTrack::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    source->prepareToPlay(samplesPerBlockExpected, sampleRate);
    scratchBuffer.setSize(2, samplesPerBlockExpected);
}

void AudioTrack::releaseResources()
{
    source->releaseResources();
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

void AudioTrack::renderInto(juce::AudioBuffer<float>& dest, int startSample, int numSamples,
                            bool anyOtherTrackSoloed)
{
    if (mute.load()) return;
    if (anyOtherTrackSoloed && !solo.load()) return;
    if (!source->isLoaded()) return;

    scratchBuffer.clear();
    juce::AudioSourceChannelInfo scratch(&scratchBuffer, 0, numSamples);
    source->getNextAudioBlock(scratch);

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
