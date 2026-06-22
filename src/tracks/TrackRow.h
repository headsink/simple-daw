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
    int getPreferredHeight() const { return 76; }

    void refreshTimeLabel();

private:
    void openFile();
    void updateButtons();
    static juce::String formatTime(double seconds);

    AudioTrack& track;
    std::function<void(TrackRow*)> onRemove;

    juce::Label nameLabel;
    juce::TextButton loadButton {"Load"};
    juce::TextButton playButton {"Play"};
    juce::TextButton stopButton {"Stop"};
    juce::Label timeLabel;
    juce::Slider gainSlider;
    juce::Slider panSlider;
    juce::TextButton muteButton {"Mute"};
    juce::TextButton soloButton {"Solo"};
    juce::TextButton loopButton {"Loop"};
    juce::TextButton removeButton {"X"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackRow)
};
