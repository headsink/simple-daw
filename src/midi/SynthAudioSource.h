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

    // MIDI events generated from the keyboard state during the last block.
    // Read by MainComponent to forward to external MIDI outputs.
    const juce::MidiBuffer& getLastMidiBuffer() const { return lastMidiBuffer; }

private:
    juce::Synthesiser synth;
    juce::MidiBuffer lastMidiBuffer;
};
