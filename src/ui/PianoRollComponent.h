#pragma once

#include <JuceHeader.h>
#include "../midi/MidiTrack.h"

class PianoRollComponent;

class PianoRollContent : public juce::Component
{
public:
    PianoRollContent(PianoRollComponent& owner, juce::MidiKeyboardState& keys, MidiClip& clip);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    int gridHeight() const;
    int velocityAreaY() const;
    int velocityAreaHeight() const;
    int contentTotalHeight() const;

    int beatToX(double beat) const;
    double xToBeat(int x) const;
    int pitchToY(int pitch) const;
    int yToPitch(int y) const;
    double snapBeat(double beat) const;
    int velocityToY(uint8_t velocity) const;
    uint8_t yToVelocity(int y) const;
    int findNoteAt(int x, int y) const;
    int findNoteAtX(int x) const;
    bool isOnResizeEdge(int x, int noteIndex) const;
    bool isBlackKey(int pitch) const;
    static juce::String noteName(int pitch);

    void drawGrid(juce::Graphics& g);
    void drawNotes(juce::Graphics& g);
    void drawVelocityBars(juce::Graphics& g);
    void drawPlayhead(juce::Graphics& g);

    void startAudition(int pitch);
    void stopAudition();

    PianoRollComponent& owner;
    juce::MidiKeyboardState& keyboardState;
    MidiClip& clip;

    enum class DragMode { none, creating, moving, resizing, velocity };
    DragMode dragMode = DragMode::none;
    int dragNoteIndex = -1;
    double dragStartBeat = 0.0;
    int dragStartY = 0;
    double dragOrigStart = 0.0;
    double dragOrigLength = 0.0;
    int dragOrigPitch = 0;
    uint8_t dragOrigVelocity = 80;

    bool creatingNote = false;
    int tempPitch = 0;
    double tempStart = 0.0;
    double tempLength = 0.0;

    int auditionPitch = -1;
};

class PianoRollComponent : public juce::Component,
                          public juce::Timer
{
public:
    PianoRollComponent(MidiTrack& track, juce::MidiKeyboardState& keys);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    void timerCallback() override;

    int getBeatWidth() const { return (int) beatWidth; }
    double getSnapBeats() const { return snapBeats; }
    double getPlayheadBeat() const { return playheadBeat; }
    bool isPlaying() const { return midiTrack.isPlaying(); }
    MidiClip& getClip() { return clip; }
    int getKeyWidth() const { return keyWidth; }
    int getRowHeight() const { return rowHeight; }
    int getNumPitches() const { return numPitches; }
    int getFirstPitch() const { return firstPitch; }

private:
    static constexpr int keyWidth = 56;
    static constexpr int rowHeight = 14;
    static constexpr int toolbarHeight = 28;
    static constexpr int numPitches = 25;
    static constexpr int firstPitch = 48;
    static constexpr int velocityLaneHeight = 70;
    static constexpr int minBeatWidth = 30;
    static constexpr int maxBeatWidth = 200;
    static constexpr int defaultBeatWidth = 82;

    void drawKeys(juce::Graphics& g);
    int keyYForPitch(int pitch) const;
    int pitchAtKeyY(int y) const;

    MidiTrack& midiTrack;
    MidiClip& clip;
    juce::MidiKeyboardState& keyboardState;

    juce::ComboBox snapComboBox;
    juce::TextButton zoomOutButton {"-"};
    juce::TextButton zoomInButton {"+"};
    juce::TextButton clearButton {"Clear"};
    juce::Label infoLabel;

    double snapBeats = 0.25;
    double beatWidth = defaultBeatWidth;
    double playheadBeat = 0.0;
    int auditionPitch = -1;

    juce::Viewport viewport;
    std::unique_ptr<PianoRollContent> content;

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
        setSize(760, 540);
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
