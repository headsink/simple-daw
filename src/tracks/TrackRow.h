#pragma once

#include <JuceHeader.h>
#include "AudioTrack.h"
#include "../plugin/PluginHost.h"

class TrackRow : public juce::Component
{
public:
    TrackRow(AudioTrack& track, PluginHost& host, std::function<void(TrackRow*)> onRemove);

    void paint(juce::Graphics& g) override;
    void resized() override;

    AudioTrack& getTrack() { return track; }
    static int getPreferredHeight() { return 76; }

    void refreshTimeLabel();
    void refreshPluginLabel();

private:
    void openFile();
    void openPluginChooser();
    void showPluginEditor();
    void updateButtons();
    static juce::String formatTime(double seconds);

    AudioTrack& track;
    PluginHost& pluginHost;
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
    juce::TextButton loadPluginButton {"VST3"};
    juce::TextButton bypassButton {"Bypass"};
    juce::TextButton editPluginButton {"Edit"};
    juce::Label pluginLabel;
    juce::TextButton removeButton {"X"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackRow)
};
