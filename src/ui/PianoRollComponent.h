#pragma once

#include <JuceHeader.h>
#include "../midi/MidiTrack.h"

class PianoRollComponent : public juce::Component,
                          public juce::Timer
{
public:
    PianoRollComponent(MidiTrack& track, juce::MidiKeyboardState& keys);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    void timerCallback() override;

private:
    static constexpr int keyWidth = 56;
    static constexpr int rowHeight = 14;
    static constexpr int toolbarHeight = 28;
    static constexpr int numPitches = 25;
    static constexpr int firstPitch = 48;

    juce::Rectangle<int> getGridArea() const;
    int beatToX(double beat) const;
    double xToBeat(int x) const;
    int pitchToY(int pitch) const;
    int yToPitch(int y) const;
    double snapBeat(double beat) const;
    int findNoteAt(int x, int y) const;
    bool isOnResizeEdge(int x, int noteIndex) const;
    bool isBlackKey(int pitch) const;
    static juce::String noteName(int pitch);

    void drawBackground(juce::Graphics& g);
    void drawKeys(juce::Graphics& g);
    void drawNotes(juce::Graphics& g);
    void drawPlayhead(juce::Graphics& g);
    void startAudition(int pitch);
    void stopAudition();

    MidiTrack& midiTrack;
    MidiClip& clip;
    juce::MidiKeyboardState& keyboardState;

    juce::ComboBox snapComboBox;
    double snapBeats = 0.25;

    enum class DragMode { none, creating, moving, resizing, auditioning };
    DragMode dragMode = DragMode::none;
    int dragNoteIndex = -1;
    double dragStartBeat = 0.0;
    int dragStartPitch = 0;
    double dragOrigStart = 0.0;
    double dragOrigLength = 0.0;
    int dragOrigPitch = 0;

    bool creatingNote = false;
    int tempPitch = 0;
    double tempStart = 0.0;
    double tempLength = 0.0;

    int auditionPitch = -1;
    double playheadBeat = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollComponent)
};

class PianoRollWindow : public juce::DocumentWindow
{
public:
    using OnClosed = std::function<void()>;

    PianoRollWindow(MidiTrack& track, juce::MidiKeyboardState& keys, OnClosed cb)
        : juce::DocumentWindow("Piano Roll",
                               juce::Desktop::getInstance().getDefaultLookAndFeel()
                                   .findColour(juce::ResizableWindow::backgroundColourId),
                               juce::DocumentWindow::allButtons),
          onClosed(std::move(cb))
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new PianoRollComponent(track, keys), true);
        setResizable(true, false);
        setSize(760, 440);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
        toFront(true);
    }

    void closeButtonPressed() override
    {
        if (onClosed) onClosed();
        delete this;
    }

private:
    OnClosed onClosed;
};
