#pragma once

#include <cstdint>

struct MidiNote
{
    int pitch = 60;
    double startBeat = 0.0;
    double lengthBeats = 1.0;
    uint8_t velocity = 80;

    double endBeat() const { return startBeat + lengthBeats; }
};
