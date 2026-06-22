#include "MidiTrack.h"

MidiTrack::MidiTrack()
{
    for (int i = 0; i < 8; ++i)
        synth.addVoice(new SineVoice());

    synth.clearSounds();
    synth.addSound(new DemoSound());
}

void MidiTrack::prepareToPlay(int, double sr)
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void MidiTrack::releaseResources()
{
    heldNotes.clear();
}

void MidiTrack::play()
{
    if (! playing.load())
    {
        currentBeat.store(0.0);
        heldNotes.clear();
        playing.store(true);
    }
}

void MidiTrack::stop()
{
    playing.store(false);
    currentBeat.store(0.0);
    heldNotes.clear();
}

void MidiTrack::setPlaying(bool shouldPlay)
{
    if (shouldPlay) play();
    else stop();
}

int MidiTrack::beatToSample(double beat, double beatRangeStart,
                            double beatRangeEnd, int numSamples) const
{
    const double span = beatRangeEnd - beatRangeStart;
    if (span <= 0.0) return 0;
    double frac = (beat - beatRangeStart) / span;
    if (frac < 0.0) frac = 0.0;
    if (frac > 1.0) frac = 1.0;
    return (int) (frac * (double) numSamples);
}

void MidiTrack::emitPendingNoteOffs(juce::MidiBuffer& midi, double beatRangeStart,
                                    double beatRangeEnd, int numSamples)
{
    for (auto it = heldNotes.begin(); it != heldNotes.end(); )
    {
        if (it->endBeat >= beatRangeStart && it->endBeat < beatRangeEnd)
        {
            const int offset = beatToSample(it->endBeat, beatRangeStart,
                                            beatRangeEnd, numSamples);
            midi.addEvent(juce::MidiMessage::noteOff(1, it->pitch, 0.8f), offset);
            it = heldNotes.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void MidiTrack::emitNoteOns(juce::MidiBuffer& midi, double beatRangeStart,
                            double beatRangeEnd, int numSamples)
{
    const juce::ScopedTryLock sl(clip.lock);
    if (! sl.isLocked()) return;

    for (const auto& note : clip.getNotes())
    {
        if (note.startBeat >= beatRangeStart && note.startBeat < beatRangeEnd)
        {
            const int offset = beatToSample(note.startBeat, beatRangeStart,
                                            beatRangeEnd, numSamples);
            midi.addEvent(juce::MidiMessage::noteOn(1, note.pitch,
                                                    (float) note.velocity / 127.0f),
                          offset);
            heldNotes.push_back({ note.pitch, note.endBeat() });
        }
    }
}

void MidiTrack::killAllHeldNotes(juce::MidiBuffer& midi)
{
    for (const auto& hn : heldNotes)
        midi.addEvent(juce::MidiMessage::noteOff(1, hn.pitch, 0.8f), 0);
    heldNotes.clear();
}

void MidiTrack::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    info.buffer->clear();

    if (! playing.load())
        return;

    const int numSamples = info.numSamples;
    const double sr = sampleRate;
    const double beatsPerSecond = bpm.load() / 60.0;
    const double beatsPerSample = beatsPerSecond / sr;
    const double deltaBeats = beatsPerSample * (double) numSamples;

    const double beatStart = currentBeat.load();
    const double beatEnd = beatStart + deltaBeats;
    const double clipLen = clip.getLengthBeats();

    juce::MidiBuffer midi;

    if (looping.load() && beatEnd >= clipLen)
    {
        const double remainingBeats = clipLen - beatStart;
        const int remainingSamples = (int) ((remainingBeats / deltaBeats) * (double) numSamples);

        emitPendingNoteOffs(midi, beatStart, clipLen, remainingSamples);
        emitNoteOns(midi, beatStart, clipLen, remainingSamples);

        synth.renderNextBlock(*info.buffer, midi, info.startSample, remainingSamples);

        midi.clear();
        killAllHeldNotes(midi);

        const int wrapSamples = numSamples - remainingSamples;
        const double wrapBeatStart = 0.0;
        const double wrapBeatEnd = deltaBeats - remainingBeats;

        emitNoteOns(midi, wrapBeatStart, wrapBeatEnd, wrapSamples);
        emitPendingNoteOffs(midi, wrapBeatStart, wrapBeatEnd, wrapSamples);

        synth.renderNextBlock(*info.buffer, midi,
                              info.startSample + remainingSamples, wrapSamples);

        currentBeat.store(wrapBeatEnd);
    }
    else
    {
        emitPendingNoteOffs(midi, beatStart, beatEnd, numSamples);
        emitNoteOns(midi, beatStart, beatEnd, numSamples);

        synth.renderNextBlock(*info.buffer, midi, info.startSample, numSamples);

        if (beatEnd >= clipLen)
        {
            juce::MidiBuffer killMidi;
            killAllHeldNotes(killMidi);
            synth.renderNextBlock(*info.buffer, killMidi,
                                  info.startSample, numSamples);
            playing.store(false);
            currentBeat.store(0.0);
        }
        else
        {
            currentBeat.store(beatEnd);
        }
    }
}
