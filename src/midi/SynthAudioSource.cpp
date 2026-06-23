#include "SynthAudioSource.h"

SynthAudioSource::SynthAudioSource()
{
    for (int i = 0; i < 8; ++i)
        synth.addVoice(new SineVoice());

    synth.clearSounds();
    synth.addSound(new DemoSound());
}

void SynthAudioSource::prepareToPlay(int, double sampleRate)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void SynthAudioSource::releaseResources() {}

void SynthAudioSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    info.buffer->clear();
    lastMidiBuffer.clear();
    keyboardState.processNextMidiBuffer(lastMidiBuffer, info.startSample, info.numSamples, true);
    synth.renderNextBlock(*info.buffer, lastMidiBuffer, info.startSample, info.numSamples);
}
