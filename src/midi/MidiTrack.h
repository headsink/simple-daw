#pragma once

#include <JuceHeader.h>
#include "MidiClip.h"
#include "SineVoice.h"
#include "DemoSound.h"

class MidiTrack : public juce::AudioSource
{
public:
    MidiTrack();

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;

    void play();
    void stop();
    void setPlaying(bool shouldPlay);
    bool isPlaying() const { return playing.load(); }

    void setBpm(double newBpm) { bpm.store(newBpm); }
    double getBpm() const { return bpm.load(); }

    void setLooping(bool shouldLoop) { looping.store(shouldLoop); }
    bool isLooping() const { return looping.load(); }

    double getCurrentBeat() const { return currentBeat.load(); }
    double getClipLengthBeats() const { return clip.getLengthBeats(); }

    MidiClip& getClip() { return clip; }

private:
    void emitPendingNoteOffs(juce::MidiBuffer& midi, double beatRangeStart,
                             double beatRangeEnd, int numSamples);
    void emitNoteOns(juce::MidiBuffer& midi, double beatRangeStart,
                     double beatRangeEnd, int numSamples);
    void killAllHeldNotes(juce::MidiBuffer& midi);

    int beatToSample(double beat, double beatRangeStart,
                     double beatRangeEnd, int numSamples) const;

    MidiClip clip;
    juce::Synthesiser synth;

    std::atomic<bool> playing{false};
    std::atomic<bool> looping{true};
    std::atomic<double> bpm{120.0};
    std::atomic<double> currentBeat{0.0};

    double sampleRate = 44100.0;

    struct HeldNote
    {
        int pitch;
        double endBeat;
    };
    std::vector<HeldNote> heldNotes;
};
