#include "PianoRollComponent.h"

PianoRollComponent::PianoRollComponent(MidiTrack& track, juce::MidiKeyboardState& keys)
    : midiTrack(track), clip(track.getClip()), keyboardState(keys)
{
    addAndMakeVisible(snapComboBox);
    snapComboBox.addItem("1/4", 1);
    snapComboBox.addItem("1/8", 2);
    snapComboBox.addItem("1/16", 3);
    snapComboBox.addItem("1/16T", 4);
    snapComboBox.setSelectedId(3);
    snapComboBox.onChange = [this]
    {
        switch (snapComboBox.getSelectedId())
        {
            case 1: snapBeats = 1.0; break;
            case 2: snapBeats = 0.5; break;
            case 3: snapBeats = 0.25; break;
            case 4: snapBeats = 1.0 / 6.0; break;
            default: break;
        }
    };

    startTimerHz(30);
}

void PianoRollComponent::timerCallback()
{
    const double beat = midiTrack.getCurrentBeat();
    if (std::abs(beat - playheadBeat) > 0.01)
    {
        playheadBeat = beat;
        repaint();
    }
}

juce::Rectangle<int> PianoRollComponent::getGridArea() const
{
    return getLocalBounds().withTrimmedTop(toolbarHeight)
                           .withTrimmedLeft(keyWidth);
}

int PianoRollComponent::beatToX(double beat) const
{
    const auto grid = getGridArea();
    const double beatsVisible = clip.getLengthBeats();
    return grid.getX() + (int) ((beat / beatsVisible) * (double) grid.getWidth());
}

double PianoRollComponent::xToBeat(int x) const
{
    const auto grid = getGridArea();
    const double beatsVisible = clip.getLengthBeats();
    const int gx = x - grid.getX();
    if (grid.getWidth() <= 0) return 0.0;
    double beat = (double) gx / (double) grid.getWidth() * beatsVisible;
    if (beat < 0.0) beat = 0.0;
    if (beat > beatsVisible) beat = beatsVisible;
    return beat;
}

int PianoRollComponent::pitchToY(int pitch) const
{
    const int top = toolbarHeight;
    const int row = (firstPitch + numPitches - 1) - pitch;
    return top + row * rowHeight;
}

int PianoRollComponent::yToPitch(int y) const
{
    const int row = (y - toolbarHeight) / rowHeight;
    int pitch = (firstPitch + numPitches - 1) - row;
    if (pitch < firstPitch) pitch = firstPitch;
    if (pitch >= firstPitch + numPitches) pitch = firstPitch + numPitches - 1;
    return pitch;
}

double PianoRollComponent::snapBeat(double beat) const
{
    return std::round(beat / snapBeats) * snapBeats;
}

bool PianoRollComponent::isBlackKey(int pitch) const
{
    const int n = pitch % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

juce::String PianoRollComponent::noteName(int pitch)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String(names[pitch % 12]) + juce::String(pitch / 12 - 1);
}

int PianoRollComponent::findNoteAt(int x, int y) const
{
    const juce::ScopedLock sl(clip.lock);
    const auto& notes = clip.getNotes();
    for (int i = (int) notes.size() - 1; i >= 0; --i)
    {
        const auto& n = notes[i];
        const int nx = beatToX(n.startBeat);
        const int nw = std::max(4, beatToX(n.startBeat + n.lengthBeats) - nx);
        const int ny = pitchToY(n.pitch);
        if (x >= nx && x < nx + nw && y >= ny && y < ny + rowHeight)
            return i;
    }
    return -1;
}

bool PianoRollComponent::isOnResizeEdge(int x, int noteIndex) const
{
    const juce::ScopedLock sl(clip.lock);
    const auto& notes = clip.getNotes();
    if (noteIndex < 0 || noteIndex >= (int) notes.size()) return false;
    const auto& n = notes[noteIndex];
    const int nx = beatToX(n.startBeat);
    const int nw = beatToX(n.startBeat + n.lengthBeats) - nx;
    return x >= nx + nw - 6 && x <= nx + nw;
}

void PianoRollComponent::startAudition(int pitch)
{
    stopAudition();
    keyboardState.noteOn(1, pitch, 0.8f);
    auditionPitch = pitch;
}

void PianoRollComponent::stopAudition()
{
    if (auditionPitch >= 0)
    {
        keyboardState.noteOff(1, auditionPitch, 0.8f);
        auditionPitch = -1;
    }
}

void PianoRollComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
    drawBackground(g);
    drawKeys(g);
    drawNotes(g);
    drawPlayhead(g);
}

void PianoRollComponent::drawBackground(juce::Graphics& g)
{
    const auto grid = getGridArea();
    const double beatsVisible = clip.getLengthBeats();

    for (int i = 0; i < numPitches; ++i)
    {
        const int pitch = firstPitch + i;
        const int y = pitchToY(pitch);
        g.setColour(isBlackKey(pitch) ? juce::Colour(0xff1e1e1e) : juce::Colour(0xff282828));
        g.fillRect(grid.getX(), y, grid.getWidth(), rowHeight);
    }

    for (int b = 0; b <= (int) beatsVisible; ++b)
    {
        const int x = beatToX((double) b);
        if (b % 4 == 0)
            g.setColour(juce::Colour(0xff606060));
        else
            g.setColour(juce::Colour(0xff383838));
        g.drawVerticalLine(x, (float) grid.getY(), (float) grid.getBottom());
    }

    g.setColour(juce::Colour(0xff383838));
    for (int i = 0; i <= numPitches; ++i)
    {
        const int y = toolbarHeight + i * rowHeight;
        g.drawHorizontalLine(y, (float) grid.getX(), (float) grid.getRight());
    }

    g.setColour(juce::Colour(0xff404040));
    g.drawVerticalLine(grid.getX(), (float) toolbarHeight, (float) grid.getBottom());
}

void PianoRollComponent::drawKeys(juce::Graphics& g)
{
    for (int i = 0; i < numPitches; ++i)
    {
        const int pitch = firstPitch + i;
        const int y = pitchToY(pitch);
        if (isBlackKey(pitch))
        {
            g.setColour(juce::Colour(0xff202020));
            g.fillRect(0, y, keyWidth, rowHeight);
        }
        else
        {
            g.setColour(juce::Colour(0xffd0d0d0));
            g.fillRect(0, y, keyWidth, rowHeight);
        }
        g.setColour(juce::Colour(0xff404040));
        g.drawHorizontalLine(y, 0.0f, (float) keyWidth);

        if (pitch % 12 == 0)
        {
            g.setColour(juce::Colour(0xff202020));
            g.setFont(juce::FontOptions(10.0f));
            g.drawText(noteName(pitch), 2, y + 1, keyWidth - 4, rowHeight - 2,
                       juce::Justification::centredLeft);
        }
    }

    g.setColour(juce::Colour(0xff505050));
    g.drawVerticalLine(keyWidth, (float) toolbarHeight, (float) pitchToY(firstPitch) + rowHeight);
}

void PianoRollComponent::drawNotes(juce::Graphics& g)
{
    const juce::ScopedLock sl(clip.lock);
    const auto& notes = clip.getNotes();

    for (const auto& n : notes)
    {
        const int x = beatToX(n.startBeat);
        const int w = std::max(4, beatToX(n.startBeat + n.lengthBeats) - x);
        const int y = pitchToY(n.pitch);
        const bool selected = (dragMode == DragMode::moving || dragMode == DragMode::resizing)
                              && dragNoteIndex == (int) (&n - &notes[0]);

        g.setColour(selected ? juce::Colour(0xff80a0f0) : juce::Colour(0xff5080d0));
        g.fillRoundedRectangle((float) x + 1, (float) y + 1, (float) w - 2, (float) rowHeight - 2, 3.0f);
        g.setColour(juce::Colour(0xffa0c0ff));
        g.drawRoundedRectangle((float) x + 1, (float) y + 1, (float) w - 2, (float) rowHeight - 2, 3.0f, 1.0f);

        if (w > 30)
        {
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(10.0f));
            g.drawText(noteName(n.pitch), x + 4, y + 1, w - 8, rowHeight - 2,
                       juce::Justification::centredLeft);
        }
    }

    if (creatingNote)
    {
        const int x = beatToX(tempStart);
        const int w = std::max(4, beatToX(tempStart + tempLength) - x);
        const int y = pitchToY(tempPitch);
        g.setColour(juce::Colour(0x9080a0f0));
        g.fillRoundedRectangle((float) x + 1, (float) y + 1, (float) w - 2, (float) rowHeight - 2, 3.0f);
    }
}

void PianoRollComponent::drawPlayhead(juce::Graphics& g)
{
    if (! midiTrack.isPlaying() && playheadBeat <= 0.0) return;
    const int x = beatToX(playheadBeat);
    const auto grid = getGridArea();
    g.setColour(juce::Colour(0xffff6060));
    g.drawVerticalLine(x, (float) grid.getY(), (float) grid.getBottom());
}

void PianoRollComponent::resized()
{
    snapComboBox.setBounds(8, 4, 80, 22);
}

void PianoRollComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.x < keyWidth && e.y >= toolbarHeight)
    {
        const int pitch = yToPitch(e.y);
        startAudition(pitch);
        dragMode = DragMode::auditioning;
        return;
    }

    if (e.y < toolbarHeight) return;

    const int noteIndex = findNoteAt(e.x, e.y);

    if (e.mods.isRightButtonDown() && noteIndex >= 0)
    {
        {
            const juce::ScopedLock sl(clip.lock);
            clip.removeNote(noteIndex);
        }
        repaint();
        return;
    }

    if (noteIndex >= 0)
    {
        if (isOnResizeEdge(e.x, noteIndex))
        {
            dragMode = DragMode::resizing;
            dragNoteIndex = noteIndex;
            const juce::ScopedLock sl(clip.lock);
            const auto& n = clip.getNotes()[noteIndex];
            dragOrigStart = n.startBeat;
            dragOrigLength = n.lengthBeats;
        }
        else
        {
            dragMode = DragMode::moving;
            dragNoteIndex = noteIndex;
            const juce::ScopedLock sl(clip.lock);
            const auto& n = clip.getNotes()[noteIndex];
            dragOrigStart = n.startBeat;
            dragOrigLength = n.lengthBeats;
            dragOrigPitch = n.pitch;
            startAudition(n.pitch);
        }
        dragStartBeat = xToBeat(e.x);
        dragStartPitch = yToPitch(e.y);
    }
    else
    {
        const int pitch = yToPitch(e.y);
        const double beat = snapBeat(xToBeat(e.x));
        creatingNote = true;
        tempPitch = pitch;
        tempStart = beat;
        tempLength = snapBeats;
        dragMode = DragMode::creating;
        startAudition(pitch);
    }

    repaint();
}

void PianoRollComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (dragMode == DragMode::creating)
    {
        const double currentBeat = snapBeat(xToBeat(e.x));
        tempLength = std::max(snapBeats, currentBeat - tempStart);
        repaint();
    }
    else if (dragMode == DragMode::moving)
    {
        const double deltaBeats = xToBeat(e.x) - dragStartBeat;
        const int deltaPitch = yToPitch(e.y) - dragStartPitch;
        const double newStart = snapBeat(dragOrigStart + deltaBeats);
        const int newPitch = dragOrigPitch + deltaPitch;
        const int clampedPitch = juce::jlimit(firstPitch, firstPitch + numPitches - 1, newPitch);

        if (auditionPitch >= 0 && auditionPitch != clampedPitch)
        {
            stopAudition();
            startAudition(clampedPitch);
        }

        {
            const juce::ScopedLock sl(clip.lock);
            auto& notes = clip.getNotes();
            if (dragNoteIndex >= 0 && dragNoteIndex < (int) notes.size())
            {
                notes[dragNoteIndex].startBeat = newStart;
                notes[dragNoteIndex].pitch = clampedPitch;
            }
        }
        repaint();
    }
    else if (dragMode == DragMode::resizing)
    {
        const double deltaBeats = xToBeat(e.x) - dragStartBeat;
        const double newLength = std::max(snapBeats, snapBeat(dragOrigLength + deltaBeats));

        {
            const juce::ScopedLock sl(clip.lock);
            auto& notes = clip.getNotes();
            if (dragNoteIndex >= 0 && dragNoteIndex < (int) notes.size())
                notes[dragNoteIndex].lengthBeats = newLength;
        }
        repaint();
    }
}

void PianoRollComponent::mouseUp(const juce::MouseEvent& e)
{
    if (dragMode == DragMode::creating && creatingNote)
    {
        {
            const juce::ScopedLock sl(clip.lock);
            MidiNote n;
            n.pitch = tempPitch;
            n.startBeat = tempStart;
            n.lengthBeats = tempLength;
            n.velocity = 80;
            clip.addNote(n);
        }
        creatingNote = false;
    }

    stopAudition();
    dragMode = DragMode::none;
    dragNoteIndex = -1;
    repaint();
}
