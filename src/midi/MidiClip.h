#pragma once

#include <JuceHeader.h>
#include "MidiNote.h"

class MidiClip
{
public:
    MidiClip();

    int getId() const { return id; }
    void setId(int newId) { id = newId; }

    double getStartBeat() const { return startBeat; }
    void setStartBeat(double b) { startBeat = b; }

    double getLengthBeats() const { return lengthBeats; }
    void setLengthBeats(double b) { lengthBeats = b; }

    const std::vector<MidiNote>& getNotes() const { return notes; }
    std::vector<MidiNote>& getNotes() { return notes; }

    void addNote(const MidiNote& note);
    void removeNote(int index);
    void clearNotes();

    void loadDemoMelody();

    juce::CriticalSection lock;

private:
    int id = 0;
    double startBeat = 0.0;
    double lengthBeats = 8.0;
    std::vector<MidiNote> notes;
};
