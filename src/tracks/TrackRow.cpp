#include "TrackRow.h"
#include "../plugin/PluginWindows.h"

TrackRow::TrackRow(AudioTrack& t, PluginHost& host, std::function<void(TrackRow*)> removeCb)
    : track(t), pluginHost(host), onRemove(std::move(removeCb))
{
    addAndMakeVisible(nameLabel);
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    nameLabel.setText("(no file loaded)", juce::dontSendNotification);

    addAndMakeVisible(loadButton);
    loadButton.onClick = [this] { openFile(); };

    addAndMakeVisible(playButton);
    playButton.onClick = [this]
    {
        if (!track.getSource().isLoaded()) return;
        track.togglePlay();
        updateButtons();
    };

    addAndMakeVisible(stopButton);
    stopButton.onClick = [this]
    {
        track.stop();
        updateButtons();
    };

    addAndMakeVisible(timeLabel);
    timeLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    timeLabel.setJustificationType(juce::Justification::centred);
    timeLabel.setText("0:00 / 0:00", juce::dontSendNotification);

    addAndMakeVisible(gainSlider);
    gainSlider.setRange(0.0, 2.0, 0.01);
    gainSlider.setValue(0.25);
    gainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    gainSlider.setTextBoxIsEditable(false);
    gainSlider.onValueChange = [this]
    {
        track.setGain((float)gainSlider.getValue());
    };

    addAndMakeVisible(panSlider);
    panSlider.setRange(-1.0, 1.0, 0.01);
    panSlider.setValue(0.0);
    panSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    panSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    panSlider.setTextBoxIsEditable(false);
    panSlider.onValueChange = [this]
    {
        track.setPan((float)panSlider.getValue());
    };

    addAndMakeVisible(muteButton);
    muteButton.setClickingTogglesState(true);
    muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffaa3030));
    muteButton.onClick = [this] { track.setMute(muteButton.getToggleState()); };

    addAndMakeVisible(soloButton);
    soloButton.setClickingTogglesState(true);
    soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff3060a0));
    soloButton.onClick = [this] { track.setSolo(soloButton.getToggleState()); };

    addAndMakeVisible(loopButton);
    loopButton.setClickingTogglesState(true);
    loopButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff308030));
    loopButton.onClick = [this] { track.getSource().setLooping(loopButton.getToggleState()); };

    addAndMakeVisible(loadPluginButton);
    loadPluginButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff504020));
    loadPluginButton.onClick = [this] { openPluginChooser(); };

    addAndMakeVisible(bypassButton);
    bypassButton.setClickingTogglesState(true);
    bypassButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff806020));
    bypassButton.onClick = [this] { track.setPluginBypass(bypassButton.getToggleState()); };
    bypassButton.setEnabled(false);

    addAndMakeVisible(editPluginButton);
    editPluginButton.onClick = [this] { showPluginEditor(); };
    editPluginButton.setEnabled(false);

    addAndMakeVisible(pluginLabel);
    pluginLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    pluginLabel.setText("(no plugin)", juce::dontSendNotification);

    addAndMakeVisible(removeButton);
    removeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff803030));
    removeButton.onClick = [this]
    {
        if (onRemove) onRemove(this);
    };

    refreshTimeLabel();
    refreshPluginLabel();
    updateButtons();
}

void TrackRow::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff252525));
    g.fillRect(getLocalBounds());
    g.setColour(juce::Colour(0xff404040));
    g.drawLine(0.0f, 0.0f, (float)getWidth(), 0.0f, 1.0f);
}

void TrackRow::resized()
{
    auto area = getLocalBounds().reduced(6);
    const int buttonW = 56;
    const int playW = 56;
    const int stopW = 56;
    const int timeW = 100;
    const int removeW = 28;
    const int gainW = 120;
    const int panW = 100;
    const int muteW = 46;
    const int soloW = 46;
    const int loopW = 46;
    const int pluginW = 50;
    const int bypassW = 56;
    const int editW = 44;

    nameLabel.setBounds(area.removeFromTop(16));
    area.removeFromTop(2);

    auto buttonRow = area.removeFromTop(26);
    loadButton.setBounds(buttonRow.removeFromLeft(buttonW).reduced(2, 3));
    buttonRow.removeFromLeft(4);
    playButton.setBounds(buttonRow.removeFromLeft(playW).reduced(2, 3));
    buttonRow.removeFromLeft(4);
    stopButton.setBounds(buttonRow.removeFromLeft(stopW).reduced(2, 3));
    buttonRow.removeFromLeft(6);
    timeLabel.setBounds(buttonRow.removeFromLeft(timeW).reduced(2, 3));
    buttonRow.removeFromLeft(6);
    gainSlider.setBounds(buttonRow.removeFromLeft(gainW).reduced(0, 5));
    buttonRow.removeFromLeft(4);
    panSlider.setBounds(buttonRow.removeFromLeft(panW).reduced(0, 5));
    buttonRow.removeFromLeft(4);
    muteButton.setBounds(buttonRow.removeFromLeft(muteW).reduced(2, 3));
    buttonRow.removeFromLeft(3);
    soloButton.setBounds(buttonRow.removeFromLeft(soloW).reduced(2, 3));
    buttonRow.removeFromLeft(3);
    loopButton.setBounds(buttonRow.removeFromLeft(loopW).reduced(2, 3));
    buttonRow.removeFromLeft(4);
    loadPluginButton.setBounds(buttonRow.removeFromLeft(pluginW).reduced(2, 3));
    buttonRow.removeFromLeft(3);
    bypassButton.setBounds(buttonRow.removeFromLeft(bypassW).reduced(2, 3));
    buttonRow.removeFromLeft(3);
    editPluginButton.setBounds(buttonRow.removeFromLeft(editW).reduced(2, 3));
    buttonRow.removeFromLeft(8);
    removeButton.setBounds(buttonRow.removeFromRight(removeW).reduced(2, 3));

    auto pluginRow = area.removeFromTop(14);
    pluginLabel.setBounds(pluginRow);
}

void TrackRow::openFile()
{
    juce::FileChooser chooser("Select a file to load",
                              juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                              "*.wav;*.aif;*.aiff;*.flac;*.mp3");
    if (chooser.browseForFileToOpen())
    {
        auto file = chooser.getResult();
        if (file.existsAsFile())
        {
            track.loadFile(file);
            nameLabel.setText(track.getSource().getLoadedFileName(), juce::dontSendNotification);
            updateButtons();
            refreshTimeLabel();
        }
    }
}

void TrackRow::openPluginChooser()
{
    auto* dlg = new PluginChooserDialog(pluginHost,
        [this](const juce::PluginDescription& desc)
        {
            pluginHost.createInstanceAsync(desc,
                [this](std::unique_ptr<juce::AudioPluginInstance> inst, const juce::String& err)
                {
                    if (inst)
                    {
                        track.setPlugin(std::move(inst));
                        juce::MessageManager::getInstance()->callAsync(
                            [this] { refreshPluginLabel(); updateButtons(); });
                    }
                    else
                    {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Plugin Load Error",
                            err.isEmpty() ? "Unknown error." : err);
                    }
                });
        });
    (void) dlg;
}

void TrackRow::showPluginEditor()
{
    auto* p = track.getPlugin();
    if (p == nullptr) return;
    new PluginEditorWindow(*p);
}

void TrackRow::updateButtons()
{
    const bool loaded = track.getSource().isLoaded();
    playButton.setEnabled(loaded);
    stopButton.setEnabled(loaded);
    playButton.setButtonText(track.getSource().isPlaying() ? "Pause" : "Play");

    const bool hasPlugin = track.hasPlugin();
    bypassButton.setEnabled(hasPlugin);
    editPluginButton.setEnabled(hasPlugin);
    if (!hasPlugin)
    {
        bypassButton.setToggleState(false, juce::dontSendNotification);
        track.setPluginBypass(false);
    }
}

void TrackRow::refreshPluginLabel()
{
    if (track.hasPlugin())
    {
        juce::String txt = track.getPluginName();
        if (track.isPluginBypassed()) txt += "  [bypassed]";
        pluginLabel.setText(txt, juce::dontSendNotification);
        pluginLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    }
    else
    {
        pluginLabel.setText("(no plugin)", juce::dontSendNotification);
        pluginLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    }
}

void TrackRow::refreshTimeLabel()
{
    const auto& src = track.getSource();
    if (!src.isLoaded())
    {
        timeLabel.setText("0:00 / 0:00", juce::dontSendNotification);
        return;
    }
    const double rate = src.getSampleRate() > 0.0 ? src.getSampleRate() : 44100.0;
    const double total = (double)src.getNumSamples() / rate;
    const double current = (double)src.getPlayPosition() / rate;
    timeLabel.setText(formatTime(current) + " / " + formatTime(total),
                      juce::dontSendNotification);
}

juce::String TrackRow::formatTime(double seconds)
{
    if (seconds < 0.0) seconds = 0.0;
    const int total = (int)seconds;
    const int mins = total / 60;
    const int secs = total % 60;
    return juce::String(mins) + ":" + (secs < 10 ? "0" : "") + juce::String(secs);
}
