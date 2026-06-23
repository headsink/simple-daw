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

    double getLengthBeats() const { return lengthBeats.load(); }
    void setLengthBeats(double b) { lengthBeats.store(b); }

    const std::vector<MidiNote>& getNotes() const { return notes; }
    std::vector<MidiNote>& getNotes() { return notes; }

    void addNote(const MidiNote& note);
    void addNotes(const std::vector<MidiNote>& newNotes);
    void removeNote(int index);
    void removeNotes(const std::set<int>& indices);
    void clearNotes();
    void replaceAllNotes(const std::vector<MidiNote>& newNotes);
    void beginEdit() { const juce::ScopedLock sl(lock); pushState(); }

    void loadDemoMelody();

    bool canUndo() const { return (int) undoStack.size() > 0; }
    bool canRedo() const { return (int) redoStack.size() > 0; }
    void undo();
    void redo();
    void clearUndoHistory();

    juce::CriticalSection lock;

private:
    void pushState();

    int id = 0;
    double startBeat = 0.0;
    std::atomic<double> lengthBeats{8.0};
    std::vector<MidiNote> notes;

    static constexpr int maxUndoSteps = 200;
    std::vector<std::vector<MidiNote>> undoStack;
    std::vector<std::vector<MidiNote>> redoStack;
};
