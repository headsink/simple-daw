#include "MidiClip.h"

MidiClip::MidiClip()
{
    loadDemoMelody();
}

void MidiClip::pushState()
{
    undoStack.push_back(notes);
    if ((int) undoStack.size() > maxUndoSteps)
        undoStack.erase(undoStack.begin());
    redoStack.clear();
}

void MidiClip::addNote(const MidiNote& note)
{
    const juce::ScopedLock sl(lock);
    pushState();
    notes.push_back(note);
}

void MidiClip::addNotes(const std::vector<MidiNote>& newNotes)
{
    if (newNotes.empty()) return;
    const juce::ScopedLock sl(lock);
    pushState();
    for (const auto& n : newNotes)
        notes.push_back(n);
}

void MidiClip::removeNote(int index)
{
    const juce::ScopedLock sl(lock);
    if (index < 0 || index >= (int) notes.size()) return;
    pushState();
    notes.erase(notes.begin() + index);
}

void MidiClip::removeNotes(const std::set<int>& indices)
{
    if (indices.empty()) return;
    const juce::ScopedLock sl(lock);
    pushState();
    for (auto it = indices.rbegin(); it != indices.rend(); ++it)
    {
        const int idx = *it;
        if (idx >= 0 && idx < (int) notes.size())
            notes.erase(notes.begin() + idx);
    }
}

void MidiClip::clearNotes()
{
    const juce::ScopedLock sl(lock);
    pushState();
    notes.clear();
}

void MidiClip::replaceAllNotes(const std::vector<MidiNote>& newNotes)
{
    const juce::ScopedLock sl(lock);
    pushState();
    notes = newNotes;
}

void MidiClip::undo()
{
    const juce::ScopedLock sl(lock);
    if (undoStack.empty()) return;
    redoStack.push_back(notes);
    notes = undoStack.back();
    undoStack.pop_back();
}

void MidiClip::redo()
{
    const juce::ScopedLock sl(lock);
    if (redoStack.empty()) return;
    undoStack.push_back(notes);
    notes = redoStack.back();
    redoStack.pop_back();
}

void MidiClip::clearUndoHistory()
{
    const juce::ScopedLock sl(lock);
    undoStack.clear();
    redoStack.clear();
}

void MidiClip::loadDemoMelody()
{
    const juce::ScopedLock sl(lock);
    notes.clear();
    undoStack.clear();
    redoStack.clear();
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
