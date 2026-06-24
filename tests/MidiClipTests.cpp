#include <juce_core/juce_core.h>
#include "../src/midi/MidiClip.h"

class MidiClipTests : public juce::UnitTest
{
public:
    MidiClipTests() : juce::UnitTest("MidiClip") {}

    static MidiNote makeNote(int pitch, double startBeat, double lengthBeats, uint8_t velocity)
    {
        MidiNote n;
        n.pitch = pitch;
        n.startBeat = startBeat;
        n.lengthBeats = lengthBeats;
        n.velocity = velocity;
        return n;
    }

    void runTest() override
    {
        beginTest("default constructor loads demo melody (8 notes, C major scale)");
        {
            MidiClip clip;
            expectEquals((int) clip.getNotes().size(), 8);
            expectEquals(clip.getNotes()[0].pitch, 60);
            expectEquals(clip.getNotes()[7].pitch, 72);
            // loadDemoMelody() explicitly clears the undo/redo stacks,
            // so a freshly-constructed clip cannot undo its demo content.
            expect(! clip.canUndo());
            expect(! clip.canRedo());
        }

        beginTest("addNote / addNotes (with empty no-op) / removeNote (with OOB no-op) / removeNotes (with empty no-op) / clearNotes");
        {
            MidiClip clip;
            clip.clearNotes();
            clip.clearUndoHistory();
            expectEquals((int) clip.getNotes().size(), 0);

            clip.addNote(makeNote(60, 0.0, 1.0, 100));
            expectEquals((int) clip.getNotes().size(), 1);

            std::vector<MidiNote> batch = {
                makeNote(62, 1.0, 1.0, 100),
                makeNote(64, 2.0, 1.0, 100)
            };
            clip.addNotes(batch);
            expectEquals((int) clip.getNotes().size(), 3);

            clip.addNotes({});
            expectEquals((int) clip.getNotes().size(), 3);

            clip.removeNote(0);
            expectEquals((int) clip.getNotes().size(), 2);
            expectEquals(clip.getNotes()[0].pitch, 62);

            clip.removeNote(999);
            clip.removeNote(-1);
            expectEquals((int) clip.getNotes().size(), 2);

            clip.removeNotes({0});
            expectEquals((int) clip.getNotes().size(), 1);

            clip.removeNotes({});
            expectEquals((int) clip.getNotes().size(), 1);

            clip.clearNotes();
            expectEquals((int) clip.getNotes().size(), 0);
        }

        beginTest("removeNotes with multiple indices handles re-indexing correctly");
        {
            MidiClip clip;
            clip.clearNotes();
            clip.clearUndoHistory();
            for (int i = 0; i < 5; ++i)
                clip.addNote(makeNote(60 + i, (double) i, 1.0, 100));
            expectEquals((int) clip.getNotes().size(), 5);

            clip.removeNotes({0, 2});
            expectEquals((int) clip.getNotes().size(), 3);
            expectEquals(clip.getNotes()[0].pitch, 61);
            expectEquals(clip.getNotes()[1].pitch, 63);
            expectEquals(clip.getNotes()[2].pitch, 64);
        }

        beginTest("undo / redo round-trip");
        {
            MidiClip clip;
            clip.clearNotes();
            clip.clearUndoHistory();

            clip.addNote(makeNote(60, 0.0, 1.0, 100));
            clip.addNote(makeNote(62, 1.0, 1.0, 100));
            expectEquals((int) clip.getNotes().size(), 2);
            expect(clip.canUndo());
            expect(! clip.canRedo());

            clip.undo();
            expectEquals((int) clip.getNotes().size(), 1);
            expectEquals(clip.getNotes()[0].pitch, 60);
            expect(clip.canRedo());

            clip.undo();
            expectEquals((int) clip.getNotes().size(), 0);

            clip.undo();
            expectEquals((int) clip.getNotes().size(), 0);

            clip.redo();
            expectEquals((int) clip.getNotes().size(), 1);
            expectEquals(clip.getNotes()[0].pitch, 60);

            clip.redo();
            expectEquals((int) clip.getNotes().size(), 2);
            expectEquals(clip.getNotes()[1].pitch, 62);

            clip.redo();
            expectEquals((int) clip.getNotes().size(), 2);
        }

        beginTest("new mutation clears the redo stack");
        {
            MidiClip clip;
            clip.clearNotes();
            clip.clearUndoHistory();

            clip.addNote(makeNote(60, 0.0, 1.0, 100));
            clip.addNote(makeNote(62, 1.0, 1.0, 100));
            clip.undo();
            clip.undo();
            expectEquals((int) clip.getNotes().size(), 0);
            expect(clip.canRedo());

            clip.addNote(makeNote(64, 0.0, 1.0, 100));
            expect(! clip.canRedo());
            expectEquals((int) clip.getNotes().size(), 1);
            expectEquals(clip.getNotes()[0].pitch, 64);
        }

        beginTest("clearUndoHistory");
        {
            MidiClip clip;
            clip.clearNotes();
            clip.clearUndoHistory();

            clip.addNote(makeNote(60, 0.0, 1.0, 100));
            clip.addNote(makeNote(62, 1.0, 1.0, 100));
            clip.undo();
            // After 2 adds + 1 undo: undoStack has [[]], notes=[60], redoStack=[[60,62]].
            expect(clip.canUndo());
            expect(clip.canRedo());

            clip.clearUndoHistory();
            expect(! clip.canUndo());
            expect(! clip.canRedo());
        }

        beginTest("replaceAllNotes snapshots previous state for undo");
        {
            MidiClip clip;
            clip.clearNotes();
            clip.clearUndoHistory();

            std::vector<MidiNote> batch = {
                makeNote(60, 0.0, 1.0, 100),
                makeNote(64, 1.0, 1.0, 100),
                makeNote(67, 2.0, 1.0, 100)
            };
            clip.replaceAllNotes(batch);
            expectEquals((int) clip.getNotes().size(), 3);
            expectEquals(clip.getNotes()[2].pitch, 67);

            clip.undo();
            expectEquals((int) clip.getNotes().size(), 0);

            clip.redo();
            expectEquals((int) clip.getNotes().size(), 3);
        }

        beginTest("undo cap is 200 (oldest snapshots dropped)");
        {
            MidiClip clip;
            clip.clearNotes();
            clip.clearUndoHistory();

            for (int i = 0; i < 250; ++i)
                clip.addNote(makeNote(60, (double) i, 0.1, 100));
            expectEquals((int) clip.getNotes().size(), 250);

            for (int i = 0; i < 200; ++i)
                clip.undo();

            // After 250 adds the undo stack holds the 200 most recent snapshots
            // (i.e. states *after* adds #51..#250). After 200 undos we land on
            // the state after add #50, which is 50 notes. We cannot undo past
            // that (the older snapshots were dropped to enforce the cap).
            const int after = (int) clip.getNotes().size();
            expect(after > 0);
            expect(after < 250);
            expectEquals(after, 50);
        }

        beginTest("lengthBeats is atomic double");
        {
            MidiClip clip;
            expectEquals(clip.getLengthBeats(), 8.0);
            clip.setLengthBeats(16.0);
            expectEquals(clip.getLengthBeats(), 16.0);
        }

        beginTest("startBeat and id");
        {
            MidiClip clip;
            expectEquals(clip.getStartBeat(), 0.0);
            clip.setStartBeat(4.0);
            expectEquals(clip.getStartBeat(), 4.0);

            expectEquals(clip.getId(), 0);
            clip.setId(42);
            expectEquals(clip.getId(), 42);
        }

        beginTest("beginEdit pushes the current state without mutating");
        {
            MidiClip clip;
            clip.clearNotes();
            clip.clearUndoHistory();
            expect(! clip.canUndo());

            clip.beginEdit();
            expect(clip.canUndo());

            clip.addNote(makeNote(60, 0.0, 1.0, 100));
            expectEquals((int) clip.getNotes().size(), 1);

            clip.undo();
            expectEquals((int) clip.getNotes().size(), 0);
        }

        beginTest("concurrent reads under the lock (snapshot pattern)");
        {
            MidiClip clip;
            clip.clearNotes();
            clip.clearUndoHistory();
            for (int i = 0; i < 100; ++i)
                clip.addNote(makeNote(60, (double) i, 0.1, 100));

            // Mimic the audio thread's tryLock + copy pattern from MidiTrack.
            std::vector<MidiNote> snapshotA;
            {
                const juce::ScopedTryLock sl(clip.lock);
                if (sl.isLocked())
                    snapshotA = clip.getNotes();
            }
            expectEquals((int) snapshotA.size(), 100);

            // Mutate on the GUI thread.
            clip.addNote(makeNote(72, 100.0, 0.1, 100));

            // Take a new snapshot. Either it sees the new note (101) or the old
            // one (100); the contract is "atomic snapshot, never torn", never
            // "exception / crash / partial copy".
            std::vector<MidiNote> snapshotB;
            {
                const juce::ScopedTryLock sl(clip.lock);
                if (sl.isLocked())
                    snapshotB = clip.getNotes();
            }
            expect(snapshotB.size() == 100 || snapshotB.size() == 101);
        }
    }
};

static MidiClipTests midiClipTests;

class StdOutLogger : public juce::Logger
{
    void logMessage(const juce::String& message) override
    {
        std::cout << message << std::endl;
    }
};

int main()
{
    StdOutLogger logger;
    juce::Logger::setCurrentLogger(&logger);

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure(false);
    runner.runAllTests();

    int totalFailures = 0;
    int totalPasses = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        totalFailures += runner.getResult(i)->failures;
        totalPasses   += runner.getResult(i)->passes;
    }

    juce::Logger::setCurrentLogger(nullptr);
    return totalFailures > 0 ? 1 : 0;
}
