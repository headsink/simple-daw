#include "TransportBar.h"

TransportBar::TransportBar(std::atomic<float>& masterPeak,
                           std::function<void()> onAddTrack,
                           std::function<void()> onScanPlugins,
                           std::function<void()> onSettings,
                           std::function<void()> onSave,
                           std::function<void()> onLoad,
                           std::function<void()> onRecordToggle,
                           std::function<void(const juce::String&)> onMidiOutputChanged,
                           std::function<void(float)> onMasterGainChanged)
    : masterPeakRef(masterPeak),
      cbAddTrack(std::move(onAddTrack)),
      cbScanPlugins(std::move(onScanPlugins)),
      cbSettings(std::move(onSettings)),
      cbSave(std::move(onSave)),
      cbLoad(std::move(onLoad)),
      cbRecordToggle(std::move(onRecordToggle)),
      cbMidiOutputChanged(std::move(onMidiOutputChanged)),
      cbMasterGainChanged(std::move(onMasterGainChanged)),
      peakMeter(masterPeak)
{
    addAndMakeVisible(statusLabel);
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(addTrackButton);
    addTrackButton.setButtonText("+ Add Track");
    addTrackButton.onClick = [this] { if (cbAddTrack) cbAddTrack(); };

    addAndMakeVisible(scanPluginsButton);
    scanPluginsButton.setButtonText("Scan VST3");
    scanPluginsButton.onClick = [this] { if (cbScanPlugins) cbScanPlugins(); };

    addAndMakeVisible(settingsButton);
    settingsButton.setButtonText("Settings");
    settingsButton.onClick = [this] { if (cbSettings) cbSettings(); };

    addAndMakeVisible(saveButton);
    saveButton.setButtonText("Save");
    saveButton.onClick = [this] { if (cbSave) cbSave(); };

    addAndMakeVisible(loadButton);
    loadButton.setButtonText("Load");
    loadButton.onClick = [this] { if (cbLoad) cbLoad(); };

    addAndMakeVisible(recordButton);
    recordButton.setButtonText("Rec");
    recordButton.setClickingTogglesState(true);
    recordButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffaa3030));
    recordButton.onClick = [this] { if (cbRecordToggle) cbRecordToggle(); };

    addAndMakeVisible(midiOutputLabel);
    midiOutputLabel.setText("MIDI Out", juce::dontSendNotification);
    midiOutputLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    midiOutputLabel.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(midiOutputCombo);
    midiOutputCombo.setTooltip("Route on-screen keyboard and sequencer notes to a hardware/software MIDI output");
    midiOutputCombo.onChange = [this]
    {
        if (cbMidiOutputChanged) cbMidiOutputChanged(midiOutputCombo.getText());
    };

    addAndMakeVisible(masterGainLabel);
    masterGainLabel.setText("Master", juce::dontSendNotification);
    masterGainLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    masterGainLabel.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(masterGainSlider);
    masterGainSlider.setRange(0.0, 2.0, 0.01);
    masterGainSlider.setValue(0.8);
    masterGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    masterGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    masterGainSlider.setTextBoxIsEditable(false);
    masterGainSlider.onValueChange = [this]
    {
        if (cbMasterGainChanged) cbMasterGainChanged((float) masterGainSlider.getValue());
    };

    addAndMakeVisible(peakMeter);
    addAndMakeVisible(peakLabel);
    peakLabel.setText("Peak", juce::dontSendNotification);
    peakLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    peakLabel.setJustificationType(juce::Justification::centredRight);
}

void TransportBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
}

void TransportBar::resized()
{
    auto area = getLocalBounds();
    statusLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(6);

    auto topRow = area.removeFromTop(28);
    addTrackButton.setBounds(topRow.removeFromRight(110));
    topRow.removeFromRight(8);
    scanPluginsButton.setBounds(topRow.removeFromRight(90));
    topRow.removeFromRight(8);
    settingsButton.setBounds(topRow.removeFromRight(80));
    topRow.removeFromRight(16);
    masterGainLabel.setBounds(topRow.removeFromRight(60));
    topRow.removeFromRight(4);
    masterGainSlider.setBounds(topRow.removeFromRight(180));
    topRow.removeFromRight(12);
    peakLabel.setBounds(topRow.removeFromRight(40));
    topRow.removeFromRight(4);
    peakMeter.setBounds(topRow.removeFromRight(140).reduced(0, 6));
    saveButton.setBounds(topRow.removeFromLeft(50).reduced(2, 3));
    topRow.removeFromLeft(4);
    loadButton.setBounds(topRow.removeFromLeft(50).reduced(2, 3));
    topRow.removeFromLeft(4);
    recordButton.setBounds(topRow.removeFromLeft(50).reduced(2, 3));
    topRow.removeFromLeft(8);
    midiOutputLabel.setBounds(topRow.removeFromLeft(56));
    topRow.removeFromLeft(4);
    midiOutputCombo.setBounds(topRow.removeFromLeft(160).reduced(0, 4));
}

void TransportBar::setStatusText(const juce::String& s)
{
    statusLabel.setText(s, juce::dontSendNotification);
}

void TransportBar::setMasterGain(float v, juce::NotificationType nt)
{
    masterGainSlider.setValue((double) v, nt);
}

void TransportBar::setRecToggleState(bool on, juce::NotificationType nt)
{
    recordButton.setToggleState(on, nt);
}

void TransportBar::setScanButtonEnabled(bool on)
{
    scanPluginsButton.setEnabled(on);
}

void TransportBar::refreshMidiOutputCombo(const juce::StringArray& names, const juce::String& selected)
{
    midiOutputCombo.clear();
    midiOutputCombo.addItem(noneOutputEntry, 1);
    for (int i = 0; i < names.size(); ++i)
        midiOutputCombo.addItem(names[i], i + 2);
    midiOutputCombo.setText(selected, juce::dontSendNotification);
}

void TransportBar::setMidiOutputSelection(const juce::String& name, juce::NotificationType nt)
{
    midiOutputCombo.setText(name, nt);
}
