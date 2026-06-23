#include "MidiClip.h"

MidiClip::MidiClip()
{
    loadDemoMelody();
}

void MidiClip::addNote(const MidiNote& note) { notes.push_back(note); }

void MidiClip::removeNote(int index)
{
    if (index >= 0 && index < (int) notes.size())
        notes.erase(notes.begin() + index);
}

void MidiClip::clearNotes() { notes.clear(); }

void MidiClip::loadDemoMelody()
{
    notes.clear();
    lengthBeats.store(8.0);

    const int scale[] = { 60, 62, 64, 65, 67, 69, 71, 72 };
    for (int i = 0; i < 8; ++i)
    {
        MidiNote n;
        n.pitch = scale[i];
        n.startBeat = (double) i;
        n.lengthBeats = 0.9;
        n.velocity = 80;
        notes.push_back(n);
    }
}
