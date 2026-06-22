#pragma once

#include <JuceHeader.h>
#include "AudioTrack.h"

class TrackRow : public juce::Component
{
public:
    explicit TrackRow(AudioTrack& track, std::function<void(TrackRow*)> onRemove);

    void paint(juce::Graphics& g) override;
    void resized() override;

    AudioTrack& getTrack() { return track; }
    int getPreferredHeight() const { return 56; }

private:
    void openFile();
    void updateButtons();

    AudioTrack& track;
    std::function<void(TrackRow*)> onRemove;

    juce::Label nameLabel;
    juce::TextButton loadButton {"Load"};
    juce::TextButton playButton {"Play"};
    juce::TextButton stopButton {"Stop"};
    juce::Slider gainSlider;
    juce::TextButton muteButton {"Mute"};
    juce::TextButton removeButton {"X"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackRow)
};
