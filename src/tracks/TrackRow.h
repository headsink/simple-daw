#pragma once

#include <JuceHeader.h>
#include "AudioTrack.h"
#include "../plugin/PluginHost.h"
#include "../ui/TrackMeterComponent.h"

class PluginEditorWindow;

class TrackRow : public juce::Component
{
public:
    TrackRow(AudioTrack& track, PluginHost& host,
             std::function<void(TrackRow*)> onRemove,
             std::function<void()> onLayoutChanged);
    ~TrackRow();

    void paint(juce::Graphics& g) override;
    void resized() override;

    AudioTrack& getTrack() { return track; }
    static int getPreferredHeight() { return 76; }
    int getPreferredHeightForState(bool abExpanded) const { return abExpanded ? 126 : 76; }
    int getCurrentPreferredHeight() const { return abOpen ? 126 : 76; }

    void refreshTimeLabel();
    void refreshPluginLabel();
    void refreshAbLabels();
    void setNameText(const juce::String& text) { nameLabel.setText(text, juce::dontSendNotification); }
    void updateButtons();

private:
    void openFile();
    void openPluginChooser();
    void showPluginEditor();
    void toggleAbPanel();
    void layoutTopRow(juce::Rectangle<int> area);
    void layoutAbRow(juce::Rectangle<int> area);
    static juce::String formatTime(double seconds);
    static juce::String formatSamples(int samples, double sampleRate);

    AudioTrack& track;
    PluginHost& pluginHost;
    std::function<void(TrackRow*)> onRemove;
    std::function<void()> onLayoutChanged;

    juce::Label nameLabel;
    juce::TextButton loadButton {"Load"};
    juce::TextButton playButton {"Play"};
    juce::TextButton stopButton {"Stop"};
    juce::Label timeLabel;
    TrackMeterComponent trackMeter;
    juce::Slider gainSlider;
    juce::Slider panSlider;
    juce::TextButton muteButton {"Mute"};
    juce::TextButton soloButton {"Solo"};
    juce::TextButton loopButton {"Loop"};
    juce::TextButton abButton {"A/B"};
    juce::TextButton loadPluginButton {"VST3"};
    juce::TextButton bypassButton {"Bypass"};
    juce::TextButton editPluginButton {"Edit"};
    juce::Label pluginLabel;
    juce::TextButton removeButton {"X"};

    juce::TextButton setAButton {"Set A"};
    juce::TextButton setBButton {"Set B"};
    juce::TextButton clearAbButton {"Clear"};
    juce::Slider startSlider;
    juce::Slider endSlider;
    juce::Label startValueLabel;
    juce::Label endValueLabel;
    juce::Label abStatusLabel;

    bool abOpen = false;

    PluginEditorWindow* editorWindow = nullptr;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackRow)
};
