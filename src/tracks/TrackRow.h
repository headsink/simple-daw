#pragma once

#include <JuceHeader.h>
#include "AudioTrack.h"
#include "../plugin/PluginHost.h"
#include "../ui/TrackMeterComponent.h"

class PluginEditorWindow;

struct TrackRowDragStarter : public juce::MouseListener
{
    explicit TrackRowDragStarter(class TrackRow* r) : row(r) {}

    void mouseDown(const juce::MouseEvent& e) override;

    class TrackRow* row = nullptr;
};

class TrackRow : public juce::Component,
                   public juce::FileDragAndDropTarget,
                   public juce::DragAndDropTarget
{
public:
    TrackRow(AudioTrack& track, PluginHost& host,
             std::function<void(TrackRow*)> onRemove,
             std::function<void()> onLayoutChanged,
             std::function<void(TrackRow* from, TrackRow* to, bool insertAbove)> onReorderRequested);
    ~TrackRow();

    void paint(juce::Graphics& g) override;
    void resized() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragMove(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragExit(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDropped(const juce::DragAndDropTarget::SourceDetails& details) override;

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
    std::function<void(TrackRow* from, TrackRow* to, bool insertAbove)> onReorderRequested;

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
    bool dragHighlight = false;
    bool dropHoverActive = false;
    bool dropInsertAbove = false;

    PluginEditorWindow* editorWindow = nullptr;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    std::unique_ptr<TrackRowDragStarter> dragStarter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackRow)
};
