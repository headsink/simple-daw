#include "SineVoice.h"

bool SineVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<juce::SynthesiserSound*>(sound) != nullptr;
}

void SineVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    currentAngle = 0.0;
    level = velocity * 0.15;
    const double cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    const double cyclesPerSample = cyclesPerSecond / getSampleRate();
    angleDelta = cyclesPerSample * juce::MathConstants<double>::twoPi;
}

void SineVoice::stopNote(float, bool)
{
    level = 0.0;
    clearCurrentNote();
}

void SineVoice::pitchWheelMoved(int) {}
void SineVoice::controllerMoved(int, int) {}

void SineVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (level <= 0.0)
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float currentSample = (float)(level * std::sin(currentAngle));
        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
            outputBuffer.addSample(ch, startSample + sample, currentSample);

        currentAngle += angleDelta;
    }
}
