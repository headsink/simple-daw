#pragma once

#include <JuceHeader.h>
#include "PeakMeterComponent.h"

class TransportBar : public juce::Component
{
public:
    static inline const juce::String noneOutputEntry { "(none)" };

    TransportBar(std::atomic<float>& masterPeak,
                 std::function<void()> onAddTrack,
                 std::function<void()> onScanPlugins,
                 std::function<void()> onSettings,
                 std::function<void()> onSave,
                 std::function<void()> onLoad,
                 std::function<void()> onRecordToggle,
                 std::function<void(const juce::String&)> onMidiOutputChanged,
                 std::function<void(float)> onMasterGainChanged);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setStatusText(const juce::String& s);
    void setMasterGain(float v, juce::NotificationType nt);
    float getMasterGain() const { return (float) masterGainSlider.getValue(); }
    void setRecToggleState(bool on, juce::NotificationType nt);
    void setScanButtonEnabled(bool on);
    void refreshMidiOutputCombo(const juce::StringArray& names, const juce::String& selected);
    void setMidiOutputSelection(const juce::String& name, juce::NotificationType nt);
    juce::String getMidiOutputSelection() const { return midiOutputCombo.getText(); }

private:
    std::atomic<float>& masterPeakRef;

    std::function<void()> cbAddTrack;
    std::function<void()> cbScanPlugins;
    std::function<void()> cbSettings;
    std::function<void()> cbSave;
    std::function<void()> cbLoad;
    std::function<void()> cbRecordToggle;
    std::function<void(const juce::String&)> cbMidiOutputChanged;
    std::function<void(float)> cbMasterGainChanged;

    juce::Label statusLabel;
    juce::TextButton addTrackButton;
    juce::TextButton scanPluginsButton;
    juce::TextButton settingsButton;
    juce::TextButton saveButton;
    juce::TextButton loadButton;
    juce::TextButton recordButton;
    juce::Label midiOutputLabel;
    juce::ComboBox midiOutputCombo;
    juce::Label masterGainLabel;
    juce::Slider masterGainSlider;
    juce::Label peakLabel;
    PeakMeterComponent peakMeter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};
