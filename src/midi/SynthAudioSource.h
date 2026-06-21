#pragma once

#include <JuceHeader.h>
#include "SineVoice.h"
#include "DemoSound.h"

class SynthAudioSource : public juce::AudioSource
{
public:
    SynthAudioSource();

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;

    juce::MidiKeyboardState keyboardState;

private:
    juce::Synthesiser synth;
};
