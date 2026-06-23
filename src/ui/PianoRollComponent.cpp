#include "PianoRollComponent.h"

PianoRollContent::PianoRollContent(PianoRollComponent& o, juce::MidiKeyboardState& keys, MidiClip& c)
    : owner(o), keyboardState(keys), clip(c) {}

int PianoRollContent::gridHeight() const { return owner.getNumPitches() * owner.getRowHeight(); }
int PianoRollContent::velocityAreaY() const { return gridHeight(); }
int PianoRollContent::velocityAreaHeight() const { return 70; }
int PianoRollContent::contentTotalHeight() const { return gridHeight() + velocityAreaHeight(); }

int PianoRollContent::beatToX(double beat) const
{
    return (int) (beat * owner.getBeatWidth());
}

double PianoRollContent::xToBeat(int x) const
{
    double b = (double) x / (double) owner.getBeatWidth();
    if (b < 0.0) b = 0.0;
    const double maxBeat = clip.getLengthBeats();
    if (b > maxBeat) b = maxBeat;
    return b;
}

int PianoRollContent::pitchToY(int pitch) const
{
    const int top = 0;
    const int row = (owner.getFirstPitch() + owner.getNumPitches() - 1) - pitch;
    return top + row * owner.getRowHeight();
}

int PianoRollContent::yToPitch(int y) const
{
    const int row = y / owner.getRowHeight();
    int pitch = (owner.getFirstPitch() + owner.getNumPitches() - 1) - row;
    if (pitch < owner.getFirstPitch()) pitch = owner.getFirstPitch();
    if (pitch >= owner.getFirstPitch() + owner.getNumPitches())
        pitch = owner.getFirstPitch() + owner.getNumPitches() - 1;
    return pitch;
}

double PianoRollContent::snapBeat(double beat) const
{
    return std::round(beat / owner.getSnapBeats()) * owner.getSnapBeats();
}

int PianoRollContent::velocityToY(uint8_t velocity) const
{
    const int h = velocityAreaHeight();
    return velocityAreaY() + h - (int) (((float) velocity / 127.0f) * (float) h);
}

uint8_t PianoRollContent::yToVelocity(int y) const
{
    const int h = velocityAreaHeight();
    const int rel = y - velocityAreaY();
    if (rel < 0) return 127;
    if (rel > h) return 1;
    const float frac = 1.0f - (float) rel / (float) h;
    return (uint8_t) juce::jlimit(1, 127, (int) (frac * 127.0f));
}

bool PianoRollContent::isBlackKey(int pitch) const
{
    const int n = pitch % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

juce::String PianoRollContent::noteName(int pitch)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String(names[pitch % 12]) + juce::String(pitch / 12 - 1);
}

int PianoRollContent::findNoteAt(int x, int y) const
{
    const juce::ScopedLock sl(clip.lock);
    const auto& notes = clip.getNotes();
    for (int i = (int) notes.size() - 1; i >= 0; --i)
    {
        const auto& n = notes[i];
        const int nx = beatToX(n.startBeat);
        const int nw = std::max(4, beatToX(n.startBeat + n.lengthBeats) - nx);
        const int ny = pitchToY(n.pitch);
        if (x >= nx && x < nx + nw && y >= ny && y < ny + owner.getRowHeight())
            return i;
    }
    return -1;
}

int PianoRollContent::findNoteAtX(int x) const
{
    const juce::ScopedLock sl(clip.lock);
    const auto& notes = clip.getNotes();
    for (int i = (int) notes.size() - 1; i >= 0; --i)
    {
        const auto& n = notes[i];
        const int nx = beatToX(n.startBeat);
        const int nw = std::max(4, beatToX(n.startBeat + n.lengthBeats) - nx);
        if (x >= nx && x < nx + nw) return i;
    }
    return -1;
}

bool PianoRollContent::isOnResizeEdge(int x, int noteIndex) const
{
    const juce::ScopedLock sl(clip.lock);
    const auto& notes = clip.getNotes();
    if (noteIndex < 0 || noteIndex >= (int) notes.size()) return false;
    const auto& n = notes[noteIndex];
    const int nx = beatToX(n.startBeat);
    const int nw = beatToX(n.startBeat + n.lengthBeats) - nx;
    return x >= nx + nw - 6 && x <= nx + nw;
}

void PianoRollContent::startAudition(int pitch)
{
    stopAudition();
    keyboardState.noteOn(1, pitch, 0.8f);
    auditionPitch = pitch;
}

void PianoRollContent::stopAudition()
{
    if (auditionPitch >= 0)
    {
        keyboardState.noteOff(1, auditionPitch, 0.8f);
        auditionPitch = -1;
    }
}

void PianoRollContent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
    drawGrid(g);
    drawNotes(g);
    drawVelocityBars(g);
    drawPlayhead(g);
}

void PianoRollContent::drawGrid(juce::Graphics& g)
{
    const int gh = gridHeight();
    const int w = getWidth();
    const double beatsVisible = clip.getLengthBeats();

    for (int i = 0; i < owner.getNumPitches(); ++i)
    {
        const int pitch = owner.getFirstPitch() + i;
        const int y = pitchToY(pitch);
        g.setColour(isBlackKey(pitch) ? juce::Colour(0xff1e1e1e) : juce::Colour(0xff282828));
        g.fillRect(0, y, w, owner.getRowHeight());
    }

    for (int b = 0; b <= (int) beatsVisible; ++b)
    {
        const int x = beatToX((double) b);
        g.setColour(b % 4 == 0 ? juce::Colour(0xff606060) : juce::Colour(0xff383838));
        g.drawVerticalLine(x, 0.0f, (float) gh);
    }

    g.setColour(juce::Colour(0xff383838));
    for (int i = 0; i <= owner.getNumPitches(); ++i)
    {
        const int y = i * owner.getRowHeight();
        g.drawHorizontalLine(y, 0.0f, (float) w);
    }

    const int vY = velocityAreaY();
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(0, vY, w, velocityAreaHeight());
    g.setColour(juce::Colour(0xff606060));
    g.drawHorizontalLine(vY, 0.0f, (float) w);
    g.setColour(juce::Colour(0xff505050));
    g.drawHorizontalLine(vY + velocityAreaHeight(), 0.0f, (float) w);

    g.setColour(juce::Colour(0xff808080));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("Velocity", 4, vY + 2, 60, 14, juce::Justification::centredLeft);
}

void PianoRollContent::drawNotes(juce::Graphics& g)
{
    const juce::ScopedLock sl(clip.lock);
    const auto& notes = clip.getNotes();

    for (int i = 0; i < (int) notes.size(); ++i)
    {
        const auto& n = notes[i];
        const int x = beatToX(n.startBeat);
        const int w = std::max(4, beatToX(n.startBeat + n.lengthBeats) - x);
        const int y = pitchToY(n.pitch);
        const bool selected = (dragMode == DragMode::moving || dragMode == DragMode::resizing)
                              && dragNoteIndex == i;

        g.setColour(selected ? juce::Colour(0xff80a0f0) : juce::Colour(0xff5080d0));
        g.fillRoundedRectangle((float) x + 1, (float) y + 1, (float) w - 2, (float) owner.getRowHeight() - 2, 3.0f);
    g.drawRoundedRectangle((float) x + 1, (float) y + 1, (float) w - 2, (float) owner.getRowHeight() - 2, 3.0f, 1.0f);

        if (w > 30)
        {
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(10.0f));
            g.drawText(noteName(n.pitch), x + 4, y + 1, w - 8, owner.getRowHeight() - 2,
                       juce::Justification::centredLeft);
        }
    }

    if (creatingNote)
    {
        const int x = beatToX(tempStart);
        const int w = std::max(4, beatToX(tempStart + tempLength) - x);
        const int y = pitchToY(tempPitch);
        g.setColour(juce::Colour(0x9080a0f0));
        g.fillRoundedRectangle((float) x + 1, (float) y + 1, (float) w - 2, (float) owner.getRowHeight() - 2, 3.0f);
    }
}

void PianoRollContent::drawVelocityBars(juce::Graphics& g)
{
    const juce::ScopedLock sl(clip.lock);
    const auto& notes = clip.getNotes();
    const int vY = velocityAreaY();
    const int vH = velocityAreaHeight();

    for (int i = 0; i < (int) notes.size(); ++i)
    {
        const auto& n = notes[i];
        const int x = beatToX(n.startBeat);
        const int w = std::max(4, beatToX(n.startBeat + n.lengthBeats) - x);
        const int barH = juce::jmax(2, (int) (((float) n.velocity / 127.0f) * (float) vH));
        const int barY = vY + vH - barH;
        const bool selected = dragMode == DragMode::velocity && dragNoteIndex == i;

        g.setColour(selected ? juce::Colour(0xff80a0f0) : juce::Colour(0xff5080d0));
        g.fillRect(x + 1, barY, w - 2, barH);
        g.setColour(juce::Colour(0xffa0c0ff));
        g.drawRect(x + 1, barY, w - 2, barH, 1);
    }
}

void PianoRollContent::drawPlayhead(juce::Graphics& g)
{
    if (! owner.isPlaying() && owner.getPlayheadBeat() <= 0.0) return;
    const int x = beatToX(owner.getPlayheadBeat());
    g.setColour(juce::Colour(0xffff6060));
    g.drawVerticalLine(x, 0.0f, (float) contentTotalHeight());
}

void PianoRollContent::mouseDown(const juce::MouseEvent& e)
{
    const int y = e.y;

    if (y >= velocityAreaY())
    {
        const int noteIdx = findNoteAtX(e.x);
        if (noteIdx >= 0)
        {
            {
                const juce::ScopedLock sl(clip.lock);
                dragOrigVelocity = clip.getNotes()[noteIdx].velocity;
            }
            dragMode = DragMode::velocity;
            dragNoteIndex = noteIdx;
            {
                const juce::ScopedLock sl(clip.lock);
                auto& notes = clip.getNotes();
                notes[noteIdx].velocity = yToVelocity(e.y);
            }
            repaint();
        }
        return;
    }

    if (e.mods.isRightButtonDown())
    {
        const int noteIdx = findNoteAt(e.x, e.y);
        if (noteIdx >= 0)
        {
            const juce::ScopedLock sl(clip.lock);
            clip.removeNote(noteIdx);
        }
        repaint();
        return;
    }

    const int noteIdx = findNoteAt(e.x, e.y);
    if (noteIdx >= 0)
    {
        if (isOnResizeEdge(e.x, noteIdx))
        {
            dragMode = DragMode::resizing;
            dragNoteIndex = noteIdx;
            const juce::ScopedLock sl(clip.lock);
            const auto& n = clip.getNotes()[noteIdx];
            dragOrigStart = n.startBeat;
            dragOrigLength = n.lengthBeats;
        }
        else
        {
            dragMode = DragMode::moving;
            dragNoteIndex = noteIdx;
            const juce::ScopedLock sl(clip.lock);
            const auto& n = clip.getNotes()[noteIdx];
            dragOrigStart = n.startBeat;
            dragOrigLength = n.lengthBeats;
            dragOrigPitch = n.pitch;
            startAudition(n.pitch);
        }
        dragStartBeat = xToBeat(e.x);
        dragStartY = e.y;
    }
    else
    {
        const int pitch = yToPitch(e.y);
        const double beat = snapBeat(xToBeat(e.x));
        creatingNote = true;
        tempPitch = pitch;
        tempStart = beat;
        tempLength = owner.getSnapBeats();
        dragMode = DragMode::creating;
        startAudition(pitch);
    }

    repaint();
}

void PianoRollContent::mouseDrag(const juce::MouseEvent& e)
{
    if (dragMode == DragMode::creating)
    {
        const double currentBeat = snapBeat(xToBeat(e.x));
        tempLength = std::max(owner.getSnapBeats(), currentBeat - tempStart);
        repaint();
    }
    else if (dragMode == DragMode::moving)
    {
        const double deltaBeats = xToBeat(e.x) - dragStartBeat;
        const int deltaPitch = (e.y - dragStartY) / owner.getRowHeight();
        const double newStart = snapBeat(dragOrigStart + deltaBeats);
        const int newPitch = juce::jlimit(owner.getFirstPitch(),
            owner.getFirstPitch() + owner.getNumPitches() - 1,
            dragOrigPitch + deltaPitch);

        if (auditionPitch >= 0 && auditionPitch != newPitch)
        {
            stopAudition();
            startAudition(newPitch);
        }

        {
            const juce::ScopedLock sl(clip.lock);
            auto& notes = clip.getNotes();
            if (dragNoteIndex >= 0 && dragNoteIndex < (int) notes.size())
            {
                notes[dragNoteIndex].startBeat = newStart;
                notes[dragNoteIndex].pitch = newPitch;
            }
        }
        repaint();
    }
    else if (dragMode == DragMode::resizing)
    {
        const double deltaBeats = xToBeat(e.x) - dragStartBeat;
        const double newLength = std::max(owner.getSnapBeats(), snapBeat(dragOrigLength + deltaBeats));

        {
            const juce::ScopedLock sl(clip.lock);
            auto& notes = clip.getNotes();
            if (dragNoteIndex >= 0 && dragNoteIndex < (int) notes.size())
                notes[dragNoteIndex].lengthBeats = newLength;
        }
        repaint();
    }
    else if (dragMode == DragMode::velocity)
    {
        {
            const juce::ScopedLock sl(clip.lock);
            auto& notes = clip.getNotes();
            if (dragNoteIndex >= 0 && dragNoteIndex < (int) notes.size())
                notes[dragNoteIndex].velocity = yToVelocity(e.y);
        }
        repaint();
    }
}

void PianoRollContent::mouseUp(const juce::MouseEvent& e)
{
    if (dragMode == DragMode::creating && creatingNote)
    {
        const juce::ScopedLock sl(clip.lock);
        MidiNote n;
        n.pitch = tempPitch;
        n.startBeat = tempStart;
        n.lengthBeats = tempLength;
        n.velocity = 80;
        clip.addNote(n);
        creatingNote = false;
    }

    stopAudition();
    dragMode = DragMode::none;
    dragNoteIndex = -1;
    repaint();
    (void) e;
}

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

    addAndMakeVisible(zoomOutButton);
    zoomOutButton.onClick = [this]
    {
        beatWidth = juce::jmax((double) minBeatWidth, beatWidth - 20.0);
        if (content != nullptr)
            content->setSize(juce::roundToInt(clip.getLengthBeats() * beatWidth),
                             content->getHeight());
    };

    addAndMakeVisible(zoomInButton);
    zoomInButton.onClick = [this]
    {
        beatWidth = juce::jmin((double) maxBeatWidth, beatWidth + 20.0);
        if (content != nullptr)
            content->setSize(juce::roundToInt(clip.getLengthBeats() * beatWidth),
                             content->getHeight());
    };

    addAndMakeVisible(clearButton);
    clearButton.onClick = [this]
    {
        const juce::ScopedLock sl(clip.lock);
        clip.clearNotes();
        if (content != nullptr) content->repaint();
    };

    addAndMakeVisible(infoLabel);
    infoLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    infoLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(viewport);
    viewport.setScrollBarsShown(true, false);
    content = std::make_unique<PianoRollContent>(*this, keyboardState, clip);
    viewport.setViewedComponent(content.get());
    content->setSize(juce::roundToInt(clip.getLengthBeats() * beatWidth),
                     numPitches * rowHeight + velocityLaneHeight);

    startTimerHz(30);
}

void PianoRollComponent::timerCallback()
{
    const double beat = midiTrack.getCurrentBeat();
    if (std::abs(beat - playheadBeat) > 0.01)
    {
        playheadBeat = beat;
        if (content != nullptr) content->repaint();
    }
    infoLabel.setText(juce::String(clip.getLengthBeats(), 1) + " beats  |  " +
                      juce::String((int) beatWidth) + " px/beat",
                      juce::dontSendNotification);
}

int PianoRollComponent::keyYForPitch(int pitch) const
{
    const int top = toolbarHeight;
    const int row = (firstPitch + numPitches - 1) - pitch;
    return top + row * rowHeight;
}

int PianoRollComponent::pitchAtKeyY(int y) const
{
    const int row = (y - toolbarHeight) / rowHeight;
    int pitch = (firstPitch + numPitches - 1) - row;
    if (pitch < firstPitch) pitch = firstPitch;
    if (pitch >= firstPitch + numPitches) pitch = firstPitch + numPitches - 1;
    return pitch;
}

void PianoRollComponent::drawKeys(juce::Graphics& g)
{
    for (int i = 0; i < numPitches; ++i)
    {
        const int pitch = firstPitch + i;
        const int y = keyYForPitch(pitch);
        const bool black = (pitch % 12) == 1 || (pitch % 12) == 3 || (pitch % 12) == 6
                        || (pitch % 12) == 8 || (pitch % 12) == 10;
        g.setColour(black ? juce::Colour(0xff202020) : juce::Colour(0xffd0d0d0));
        g.fillRect(0, y, keyWidth, rowHeight);
        g.setColour(juce::Colour(0xff404040));
        g.drawHorizontalLine(y, 0.0f, (float) keyWidth);

        if (pitch % 12 == 0)
        {
            g.setColour(black ? juce::Colour(0xffd0d0d0) : juce::Colour(0xff202020));
            g.setFont(juce::FontOptions(10.0f));
            static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            g.drawText(juce::String(names[pitch % 12]) + juce::String(pitch / 12 - 1),
                       2, y + 1, keyWidth - 4, rowHeight - 2, juce::Justification::centredLeft);
        }
    }

    g.setColour(juce::Colour(0xff505050));
    g.drawVerticalLine(keyWidth, (float) toolbarHeight,
                       (float) keyYForPitch(firstPitch) + rowHeight);
}

void PianoRollComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
    drawKeys(g);
}

void PianoRollComponent::resized()
{
    auto area = getLocalBounds();
    auto toolbarArea = area.removeFromTop(toolbarHeight);
    snapComboBox.setBounds(toolbarArea.removeFromLeft(96).reduced(4, 4));
    toolbarArea.removeFromLeft(6);
    zoomOutButton.setBounds(toolbarArea.removeFromLeft(28).reduced(2, 4));
    zoomInButton.setBounds(toolbarArea.removeFromLeft(28).reduced(2, 4));
    toolbarArea.removeFromLeft(8);
    clearButton.setBounds(toolbarArea.removeFromLeft(60).reduced(2, 4));
    toolbarArea.removeFromLeft(8);
    infoLabel.setBounds(toolbarArea.removeFromLeft(220).reduced(0, 4));

    area.removeFromLeft(keyWidth);
    viewport.setBounds(area);

    if (content != nullptr)
        content->setSize(juce::roundToInt(clip.getLengthBeats() * beatWidth),
                         numPitches * rowHeight + velocityLaneHeight);
}

void PianoRollComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.y >= toolbarHeight && e.x < keyWidth)
    {
        const int pitch = pitchAtKeyY(e.y);
        if (pitch >= firstPitch && pitch < firstPitch + numPitches)
        {
            keyboardState.noteOn(1, pitch, 0.8f);
            auditionPitch = pitch;
        }
    }
}

void PianoRollComponent::mouseUp(const juce::MouseEvent& e)
{
    if (auditionPitch >= 0)
    {
        keyboardState.noteOff(1, auditionPitch, 0.8f);
        auditionPitch = -1;
    }
    (void) e;
}
